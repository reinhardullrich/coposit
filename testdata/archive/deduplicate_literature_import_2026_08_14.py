#!/usr/bin/env python3
"""Deduplicate classes touched by the literature import and merge their provenance."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
import sqlite3
from collections import Counter, defaultdict
from pathlib import Path


PROJECT = Path(__file__).resolve().parents[2]
DATABASE = PROJECT / "testdata/copos_testdata.sqlite3"
EXPECTED_MATRICES = 3490
EXPECTED_RETAINED = 3157
EXPECTED_SOURCES = 94
EXPECTED_IMPORTED = 1048
EXPECTED_DUPLICATE_CLASSES = 256
EXPECTED_REMOVED = EXPECTED_MATRICES - EXPECTED_RETAINED
MODULUS = 2**127 - 1


def packed_index(i: int, j: int, n: int) -> int:
    if i > j:
        i, j = j, i
    return i * n - i * (i - 1) // 2 + j - i


def read_matrix_market(path: Path, n: int) -> tuple[int, ...]:
    with path.open(encoding="ascii") as stream:
        header = next(stream, "").lower().split()
        data = (line.strip() for line in stream if line.strip() and not line.lstrip().startswith("%"))
        shape = next(data, "").split()
        if header == ["%%matrixmarket", "matrix", "array", "integer", "symmetric"]:
            if shape != [str(n), str(n)]:
                raise ValueError(f"wrong Matrix Market shape: {path}")
            values = tuple(int(token) for token in data)
            if len(values) != n * (n + 1) // 2:
                raise ValueError(f"wrong Matrix Market value count: {path}")
            return values
        if header != ["%%matrixmarket", "matrix", "coordinate", "integer", "symmetric"]:
            raise ValueError(f"unsupported Matrix Market file: {path}")
        if len(shape) != 3 or shape[:2] != [str(n), str(n)]:
            raise ValueError(f"wrong Matrix Market shape: {path}")
        values = [0] * (n * (n + 1) // 2)
        count = 0
        for line in data:
            row, column, value = line.split()
            values[packed_index(int(row) - 1, int(column) - 1, n)] += int(value)
            count += 1
        if count != int(shape[2]):
            raise ValueError(f"wrong Matrix Market entry count: {path}")
        return tuple(values)


def load_values(n: int, storage: str) -> tuple[int, ...]:
    if not storage.startswith("file:"):
        values = tuple(map(int, storage.split(",")))
    else:
        values = read_matrix_market(PROJECT / "testdata" / storage.removeprefix("file:"), n)
    if len(values) != n * (n + 1) // 2:
        raise ValueError(f"matrix has wrong packed length for order {n}")
    divisor = 0
    for value in values:
        divisor = math.gcd(divisor, value)
    divisor = divisor or 1
    return tuple(value // divisor for value in values)


def fingerprints(n: int, values: tuple[int, ...]) -> tuple[str, tuple]:
    digest = hashlib.sha256()
    diagonal = [0] * n
    local = [[0, 0, 0, 0] for _ in range(n)]
    total = [0, 0, 0, 0]
    offset = 0
    for i in range(n):
        for j in range(i, n):
            value = values[offset]
            offset += 1
            digest.update(str(value).encode("ascii"))
            digest.update(b",")
            residue = value % MODULUS
            square = residue * residue % MODULUS
            cube = square * residue % MODULUS
            total[0] = (total[0] + residue) % MODULUS
            total[1] = (total[1] + square) % MODULUS
            total[2] = (total[2] + cube) % MODULUS
            total[3] += value == 0
            if i == j:
                diagonal[i] = value
                continue
            for vertex in (i, j):
                local[vertex][0] = (local[vertex][0] + residue) % MODULUS
                local[vertex][1] = (local[vertex][1] + square) % MODULUS
                local[vertex][2] = (local[vertex][2] + cube) % MODULUS
                local[vertex][3] += value == 0
    vertex_signatures = tuple(sorted((diagonal[i], *local[i]) for i in range(n)))
    return digest.hexdigest(), (n, tuple(sorted(diagonal)), tuple(total), vertex_signatures)


def assign_colors(signatures_a: list[tuple], signatures_b: list[tuple]) -> tuple[list[int], list[int]] | None:
    palette = {signature: index for index, signature in enumerate(sorted(set(signatures_a + signatures_b)))}
    colors_a = [palette[signature] for signature in signatures_a]
    colors_b = [palette[signature] for signature in signatures_b]
    if Counter(colors_a) != Counter(colors_b):
        return None
    return colors_a, colors_b


def permutation_equivalent(n: int, left: tuple[int, ...], right: tuple[int, ...]) -> bool:
    if left == right:
        return True

    def value(values: tuple[int, ...], i: int, j: int) -> int:
        return values[packed_index(i, j, n)]

    signatures_left = []
    signatures_right = []
    for values, output in ((left, signatures_left), (right, signatures_right)):
        for i in range(n):
            output.append((value(values, i, i), tuple(sorted(Counter(value(values, i, j) for j in range(n) if j != i).items()))))
    assigned = assign_colors(signatures_left, signatures_right)
    if assigned is None:
        return False
    colors_left, colors_right = assigned

    while True:
        refined_left = []
        refined_right = []
        for values, colors, output in ((left, colors_left, refined_left), (right, colors_right, refined_right)):
            for i in range(n):
                neighbors = Counter((value(values, i, j), colors[j]) for j in range(n) if j != i)
                output.append((colors[i], tuple(sorted(neighbors.items()))))
        assigned = assign_colors(refined_left, refined_right)
        if assigned is None:
            return False
        next_left, next_right = assigned
        if next_left == colors_left and next_right == colors_right:
            break
        colors_left, colors_right = next_left, next_right

    by_color: dict[int, list[int]] = defaultdict(list)
    for vertex, color in enumerate(colors_right):
        by_color[color].append(vertex)
    mapping = [-1] * n
    used = [False] * n

    def search(mapped: int) -> bool:
        if mapped == n:
            return True
        choice = -1
        candidates: list[int] = []
        for i in range(n):
            if mapping[i] >= 0:
                continue
            available = []
            for j in by_color[colors_left[i]]:
                if used[j]:
                    continue
                if all(mapping[k] < 0 or value(left, i, k) == value(right, j, mapping[k]) for k in range(n)):
                    available.append(j)
            if not available:
                return False
            if choice < 0 or len(available) < len(candidates):
                choice, candidates = i, available
                if len(candidates) == 1:
                    break
        for candidate in candidates:
            mapping[choice] = candidate
            used[candidate] = True
            if search(mapped + 1):
                return True
            used[candidate] = False
            mapping[choice] = -1
        return False

    return search(0)


class UnionFind:
    def __init__(self, items) -> None:
        self.parent = {item: item for item in items}

    def find(self, item: int) -> int:
        while self.parent[item] != item:
            self.parent[item] = self.parent[self.parent[item]]
            item = self.parent[item]
        return item

    def union(self, left: int, right: int) -> None:
        left, right = self.find(left), self.find(right)
        if left != right:
            self.parent[right] = left


def merge_text(values: list[str | None], separator: str) -> str | None:
    unique = []
    for value in values:
        if value and value not in unique:
            unique.append(value)
    return separator.join(unique) or None


def catalog_occurrences(rows) -> set[str]:
    return {
        match
        for row in rows
        for match in re.findall(r"catalog_instance_id=([^;|]+)", row["source"] or "")
    }


def self_test() -> None:
    left = (1, 2, 3, 4, 5, 6)
    # Simultaneous permutation (0, 1, 2) -> (2, 0, 1).
    right = (6, 3, 5, 1, 2, 4)
    assert permutation_equivalent(3, left, right)
    assert not permutation_equivalent(3, left, (6, 3, 4, 1, 2, 5))
    assert load_values(2, "2,4,6") == (1, 2, 3)
    print("self-test=ok")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apply", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--verbose", action="store_true")
    arguments = parser.parse_args()
    if arguments.self_test:
        self_test()
        return

    with sqlite3.connect(DATABASE) as connection:
        connection.execute("PRAGMA foreign_keys=ON")
        connection.row_factory = sqlite3.Row
        rows = list(connection.execute("SELECT * FROM matrices ORDER BY matrix_id"))
        has_unsolved = bool(rows) and "references_unsolved" in rows[0].keys()
        sources = {row[0]: row[1] for row in connection.execute("SELECT source_id, publication_year FROM sources")}
        if len(sources) != EXPECTED_SOURCES:
            raise ValueError(f"expected {EXPECTED_SOURCES} sources")
        if len(rows) == EXPECTED_RETAINED:
            if len(catalog_occurrences(rows)) != EXPECTED_IMPORTED:
                raise ValueError("post-deduplication corpus does not retain every catalog occurrence")
            print(f"already_deduplicated=1 matrices={EXPECTED_RETAINED} catalog_occurrences={EXPECTED_IMPORTED}")
            return
        if len(rows) != EXPECTED_MATRICES:
            raise ValueError(f"expected pre-deduplication count {EXPECTED_MATRICES} or retained count {EXPECTED_RETAINED}")
        imported = {row["matrix_id"] for row in rows if (row["source"] or "").startswith("catalog_instance_id=")}
        if len(imported) != EXPECTED_IMPORTED:
            raise ValueError(f"expected {EXPECTED_IMPORTED} imported occurrences, found {len(imported)}")

        imported_dimensions = {row["dimension"] for row in rows if row["matrix_id"] in imported}
        candidate_rows = [row for row in rows if row["dimension"] in imported_dimensions]
        exact_buckets: dict[tuple[int, str], list[int]] = defaultdict(list)
        invariant_buckets: dict[tuple, list[int]] = defaultdict(list)
        row_by_id = {row["matrix_id"]: row for row in rows}
        for position, row in enumerate(candidate_rows, 1):
            values = load_values(row["dimension"], row["matrix"])
            digest, invariant = fingerprints(row["dimension"], values)
            exact_buckets[(row["dimension"], digest)].append(row["matrix_id"])
            invariant_buckets[invariant].append(row["matrix_id"])
            if arguments.verbose and position % 250 == 0:
                print(f"fingerprinted={position}/{len(candidate_rows)}")

        union_find = UnionFind(row_by_id)
        for matrix_ids in exact_buckets.values():
            if len(matrix_ids) < 2:
                continue
            representatives: list[int] = []
            for matrix_id in matrix_ids:
                values = load_values(row_by_id[matrix_id]["dimension"], row_by_id[matrix_id]["matrix"])
                match = next((other for other in representatives
                              if values == load_values(row_by_id[other]["dimension"], row_by_id[other]["matrix"])), None)
                if match is None:
                    representatives.append(matrix_id)
                else:
                    union_find.union(match, matrix_id)

        checked_pairs = 0
        for matrix_ids in invariant_buckets.values():
            representatives = sorted({union_find.find(matrix_id) for matrix_id in matrix_ids})
            for index, left_id in enumerate(representatives):
                for right_id in representatives[index + 1:]:
                    if union_find.find(left_id) == union_find.find(right_id):
                        continue
                    checked_pairs += 1
                    left_row, right_row = row_by_id[left_id], row_by_id[right_id]
                    left = load_values(left_row["dimension"], left_row["matrix"])
                    right = load_values(right_row["dimension"], right_row["matrix"])
                    if permutation_equivalent(left_row["dimension"], left, right):
                        union_find.union(left_id, right_id)

        all_classes: dict[int, list[int]] = defaultdict(list)
        for matrix_id in row_by_id:
            all_classes[union_find.find(matrix_id)].append(matrix_id)
        classes = [sorted(matrix_ids) for matrix_ids in all_classes.values()
                   if len(matrix_ids) > 1 and any(matrix_id in imported for matrix_id in matrix_ids)]
        benchmarked = {
            row[0]
            for row in connection.execute("SELECT matrix_id FROM results UNION SELECT matrix_id FROM preprocessing_results")
        }

        plans = []
        for matrix_ids in classes:
            benchmarked_members = benchmarked.intersection(matrix_ids)
            if len(benchmarked_members) > 1:
                raise ValueError(f"duplicate class has multiple independently benchmarked representations: {matrix_ids}")
            survivor = min(matrix_ids, key=lambda matrix_id: (
                matrix_id not in benchmarked_members,
                sources[row_by_id[matrix_id]["source_id"]], row_by_id[matrix_id]["source_id"], matrix_id
            ))
            removed = [matrix_id for matrix_id in matrix_ids if matrix_id != survivor]
            class_rows = [row_by_id[survivor], *(row_by_id[matrix_id] for matrix_id in removed)]

            source_ids = set()
            solved: dict[int, list[str]] = defaultdict(list)
            unsolved: dict[int, list[str]] = defaultdict(list)
            for row in class_rows:
                source_ids.add(row["source_id"])
                source_ids.update(json.loads(row["additional_source_ids"]))
                for item in json.loads(row["references_solved"]):
                    comment = item.get("comment")
                    if comment and comment not in solved[item["source_id"]]:
                        solved[item["source_id"]].append(comment)
                    else:
                        solved.setdefault(item["source_id"], [])
                if has_unsolved:
                    for item in json.loads(row["references_unsolved"]):
                        comment = item.get("comment")
                        if comment and comment not in unsolved[item["source_id"]]:
                            unsolved[item["source_id"]].append(comment)
                        else:
                            unsolved.setdefault(item["source_id"], [])
            primary = min(source_ids, key=lambda source_id: (sources[source_id], source_id))
            additional = sorted(source_ids - {primary}, key=lambda source_id: (sources[source_id], source_id))
            solved_json = []
            for source_id in sorted(solved, key=lambda value: (sources[value], value)):
                item = {"source_id": source_id}
                if solved[source_id]:
                    item["comment"] = " | ".join(solved[source_id])
                solved_json.append(item)
            unsolved_json = []
            for source_id in sorted(unsolved, key=lambda value: (sources[value], value)):
                item = {"source_id": source_id}
                if unsolved[source_id]:
                    item["comment"] = " | ".join(unsolved[source_id])
                unsolved_json.append(item)

            truth = {}
            for column in ("is_strictly_copositive", "is_copositive"):
                known = {row[column] for row in class_rows if row[column] is not None}
                if len(known) > 1:
                    raise ValueError(f"conflicting {column} in duplicate class {matrix_ids}")
                truth[column] = next(iter(known), None)
            if truth["is_strictly_copositive"] == 1 and truth["is_copositive"] != 1:
                raise ValueError(f"strict/non-strict truth conflict in duplicate class {matrix_ids}")

            source_values = [class_rows[0]["source"]]
            source_values.extend(
                f"equivalent occurrence matrix_id={row['matrix_id']}: {row['source']}" for row in class_rows[1:] if row["source"]
            )
            families = []
            for row in class_rows:
                for family in (row["family"] or "").split(" | "):
                    if family and family not in families:
                        families.append(family)
            update = {
                "source": merge_text(source_values, " || "),
                "source_id": primary,
                "additional_source_ids": json.dumps(additional, separators=(",", ":")),
                "references_solved": json.dumps(solved_json, separators=(",", ":")),
                **({"references_unsolved": json.dumps(unsolved_json, separators=(",", ":"))} if has_unsolved else {}),
                "family": " | ".join(families) or None,
                **truth,
                **{column: max(row[column] for row in class_rows)
                   for column in ("smoke_set", "representative_core", "stress_test", "scale_set", "timeout_5s_strict_set")},
            }
            plans.append((survivor, removed, update))

        removed_ids = [matrix_id for _, removed, _ in plans for matrix_id in removed]
        if len(plans) != EXPECTED_DUPLICATE_CLASSES or len(removed_ids) != EXPECTED_REMOVED:
            raise ValueError(f"expected {EXPECTED_DUPLICATE_CLASSES} duplicate classes and {EXPECTED_REMOVED} removals")
        result_rows = connection.execute(
            f"SELECT count(*) FROM results WHERE matrix_id IN ({','.join('?' for _ in removed_ids) or 'NULL'})", removed_ids
        ).fetchone()[0]
        preprocessing_rows = connection.execute(
            f"SELECT count(*) FROM preprocessing_results WHERE matrix_id IN ({','.join('?' for _ in removed_ids) or 'NULL'})", removed_ids
        ).fetchone()[0]
        if result_rows or preprocessing_rows:
            raise ValueError(f"deduplication would remove {result_rows} result rows and {preprocessing_rows} preprocessing rows")
        external_files = [row_by_id[matrix_id]["matrix"].removeprefix("file:") for matrix_id in removed_ids
                          if row_by_id[matrix_id]["matrix"].startswith("file:")]

        print(f"duplicate_classes={len(plans)} removed_matrices={len(removed_ids)} checked_permutation_pairs={checked_pairs} "
              f"removed_results={result_rows} removed_preprocessing_results={preprocessing_rows} orphaned_payloads={len(external_files)}")
        if arguments.verbose:
            for survivor, removed, _ in plans:
                print(f"survivor={survivor} removed={','.join(map(str, removed))}")
            for path in external_files:
                print(f"orphaned_payload={path}")
        if not arguments.apply or not plans:
            return

        connection.execute("BEGIN IMMEDIATE")
        for survivor, removed, update in plans:
            update_unsolved = ", references_unsolved=:references_unsolved" if has_unsolved else ""
            connection.execute(f"""
                UPDATE matrices
                SET source=:source, source_id=:source_id, additional_source_ids=:additional_source_ids,
                    references_solved=:references_solved{update_unsolved}, family=:family,
                    is_strictly_copositive=:is_strictly_copositive, is_copositive=:is_copositive,
                    smoke_set=:smoke_set, representative_core=:representative_core, stress_test=:stress_test,
                    scale_set=:scale_set, timeout_5s_strict_set=:timeout_5s_strict_set
                WHERE matrix_id=:survivor
            """, {**update, "survivor": survivor})
            connection.executemany("DELETE FROM matrices WHERE matrix_id=?", ((matrix_id,) for matrix_id in removed))
        retained_rows = list(connection.execute("SELECT source FROM matrices"))
        if len(retained_rows) != EXPECTED_RETAINED or len(catalog_occurrences(retained_rows)) != EXPECTED_IMPORTED:
            raise ValueError("wrong retained matrix or catalog-occurrence count")
        if list(connection.execute("PRAGMA foreign_key_check")):
            raise ValueError("foreign-key check failed")
        connection.commit()


if __name__ == "__main__":
    main()
