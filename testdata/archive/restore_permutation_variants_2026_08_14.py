#!/usr/bin/env python3
"""Restore historical coordinate orderings while retaining direct positive-scale deduplication."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
import shutil
import sqlite3
from collections import defaultdict
from pathlib import Path

from deduplicate_literature_import_2026_08_14 import fingerprints, load_values, permutation_equivalent, read_matrix_market


PROJECT = Path(__file__).resolve().parents[2]
DATABASE = PROJECT / "testdata/copos_testdata.sqlite3"
TRASH = Path.home() / ".local/share/Trash/files"
PRE_PROJECTIVE = TRASH / "Copos_testdata.before_projective_dedup_20260809T142700Z.sqlite3"
POST_PROJECTIVE = Path.home() / "projects/fracessa/external/coposit/testdata/copos_testdata.sqlite3"
PRE_LITERATURE = TRASH / "copos_testdata.before_literature_dedup_20260814T180542+0300.sqlite3"
INITIAL_TSV = PROJECT / ".local-tmp/permutation-restore/reduced_b.tsv"


def projective_key(dimension: int, values: tuple[int, ...]) -> tuple[int, tuple[int, ...]]:
    divisor = 0
    for value in values:
        divisor = math.gcd(divisor, value)
    divisor = divisor or 1
    return dimension, tuple(value // divisor for value in values)


def inline_values(dimension: int, text: str) -> tuple[int, ...]:
    values = tuple(map(int, text.split(",")))
    if len(values) != dimension * (dimension + 1) // 2:
        raise ValueError(f"wrong packed length for order {dimension}")
    return values


def row_values(row: sqlite3.Row) -> tuple[int, ...]:
    if row["matrix"].startswith("file:"):
        relative = row["matrix"].removeprefix("file:")
        active = PROJECT / "testdata" / relative
        if not active.is_file():
            fallback = TRASH / Path(relative).name
            values = read_matrix_market(fallback, row["dimension"])
            return projective_key(row["dimension"], values)[1]
    return load_values(row["dimension"], row["matrix"])


def current_rows(connection: sqlite3.Connection) -> list[sqlite3.Row]:
    return list(connection.execute("SELECT * FROM matrices ORDER BY matrix_id"))


def build_permutation_index(rows: list[sqlite3.Row], dimensions: set[int]) -> dict[tuple, list[tuple[sqlite3.Row, tuple[int, ...]]]]:
    index: dict[tuple, list[tuple[sqlite3.Row, tuple[int, ...]]]] = defaultdict(list)
    for row in rows:
        if row["dimension"] not in dimensions:
            continue
        values = row_values(row)
        index[fingerprints(row["dimension"], values)[1]].append((row, values))
    return index


def equivalent_representative(
    dimension: int,
    values: tuple[int, ...],
    index: dict[tuple, list[tuple[sqlite3.Row, tuple[int, ...]]]],
) -> sqlite3.Row:
    invariant = fingerprints(dimension, values)[1]
    for row, other in index.get(invariant, []):
        if permutation_equivalent(dimension, values, other):
            return row
    raise ValueError(f"no retained permutation representative for order {dimension}")


def initial_occurrences(path: Path) -> tuple[list[dict], int]:
    exact = set()
    by_scale: dict[tuple[int, tuple[int, ...]], dict] = {}
    with path.open(encoding="ascii") as stream:
        for original_id, line in enumerate(stream, 1):
            source_matrix_id, candidate_id, support, extended_support, dimension, strict, matrix = line.rstrip("\n").split("\t")
            dimension_value = int(dimension)
            values = inline_values(dimension_value, matrix)
            exact.add((dimension_value, values))
            key = projective_key(dimension_value, values)
            by_scale.setdefault(key, {
                "original_id": original_id,
                "dimension": dimension_value,
                "matrix": matrix,
                "values": key[1],
                "strict": int(strict),
                "source_matrix_id": int(source_matrix_id),
                "candidate_id": int(candidate_id),
                "support": support,
                "extended_support": extended_support,
            })
    return list(by_scale.values()), len(exact)


def source_id_for_old_row(row: sqlite3.Row, representative: sqlite3.Row) -> int | None:
    source = row["source"] or ""
    if source.startswith("FracESSA:"):
        return 91
    if "Dickinson-de Zeeuw" in source:
        return 16
    return representative["source_id"]


def normalized_old_results(connection: sqlite3.Connection, matrix_ids: set[int]) -> list[tuple]:
    if not matrix_ids:
        return []
    placeholders = ",".join("?" for _ in matrix_ids)
    rows = list(connection.execute(
        f"SELECT rowid,* FROM results WHERE matrix_id IN ({placeholders})",
        sorted(matrix_ids),
    ))
    preprocessing_names = {
        "preprocessing=connected_components": "connected_components",
        "preprocessing=pre_checks": "pre_checks",
        "preprocessing=both": "both",
    }
    selected: dict[tuple, tuple] = {}
    ranks: dict[tuple, tuple] = {}
    for row in rows:
        preprocessing = preprocessing_names.get(row["parameters"], "none")
        key = (row["matrix_id"], row["model_id"], "strictly_copositive", preprocessing, row["binary_sha256"])
        rank = (row["status"] == "ok", row["timeout_ns"], row["recorded_at"], row["rowid"])
        if key in ranks and rank <= ranks[key]:
            continue
        ranks[key] = rank
        selected[key] = (
            row["matrix_id"], row["model_id"], "strictly_copositive", preprocessing, row["binary_sha256"], row["status"], None,
            row["is_strictly_copositive"], row["elapsed_ns"], row["timeout_ns"], row["recorded_at"], row["message"],
        )
    return list(selected.values())


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--database", type=Path, default=DATABASE)
    parser.add_argument("--initial-tsv", type=Path, default=INITIAL_TSV)
    parser.add_argument("--pre-projective", type=Path, default=PRE_PROJECTIVE)
    parser.add_argument("--post-projective", type=Path, default=POST_PROJECTIVE)
    parser.add_argument("--pre-literature", type=Path, default=PRE_LITERATURE)
    parser.add_argument("--dry-run", action="store_true")
    arguments = parser.parse_args()

    required = (arguments.database, arguments.initial_tsv, arguments.pre_projective, arguments.post_projective, arguments.pre_literature)
    missing = [str(path) for path in required if not path.is_file()]
    if missing:
        raise FileNotFoundError(f"missing recovery inputs: {missing}")

    with sqlite3.connect(arguments.database) as connection, sqlite3.connect(arguments.pre_projective) as before_projective, \
            sqlite3.connect(arguments.post_projective) as after_projective, sqlite3.connect(arguments.pre_literature) as before_literature:
        for database in (connection, before_projective, after_projective, before_literature):
            database.row_factory = sqlite3.Row
        connection.execute("PRAGMA foreign_keys=ON")

        current = current_rows(connection)
        current_by_id = {row["matrix_id"]: row for row in current}
        used_ids = set(current_by_id)
        next_id = max(used_ids) + 1

        initial, initial_exact = initial_occurrences(arguments.initial_tsv)
        if initial_exact != 2480:
            raise ValueError(f"expected 2480 exact initial arrays, found {initial_exact}")
        initial_dimensions = {row["dimension"] for row in initial}
        permutation_index = build_permutation_index(current, initial_dimensions)

        direct_keys: set[tuple[int, tuple[int, ...]]] = set()
        direct_owners: dict[tuple[int, tuple[int, ...]], int] = {}
        loaded_dimensions: set[int] = set()

        def load_current_dimension(dimension: int) -> None:
            if dimension in loaded_dimensions:
                return
            for row in current:
                if row["dimension"] == dimension:
                    key = projective_key(dimension, row_values(row))
                    if key in direct_owners:
                        raise ValueError(f"existing direct-scale duplicate IDs {direct_owners[key]} and {row['matrix_id']}")
                    direct_keys.add(key)
                    direct_owners[key] = row["matrix_id"]
            loaded_dimensions.add(dimension)

        for dimension in initial_dimensions:
            load_current_dimension(dimension)

        planned: list[dict] = []
        planned_by_key: dict[tuple[int, tuple[int, ...]], dict] = {}

        def allocate(original_id: int) -> int:
            nonlocal next_id
            if original_id not in used_ids:
                used_ids.add(original_id)
                return original_id
            while next_id in used_ids:
                next_id += 1
            allocated = next_id
            used_ids.add(allocated)
            next_id += 1
            return allocated

        for item in sorted(initial, key=lambda row: row["original_id"]):
            key = (item["dimension"], item["values"])
            if key in direct_keys:
                continue
            representative = equivalent_representative(item["dimension"], item["values"], permutation_index)
            if representative["is_strictly_copositive"] != item["strict"]:
                raise ValueError(f"strict truth conflict for initial occurrence {item['original_id']}")
            matrix_id = allocate(item["original_id"])
            record = {
                "matrix_id": matrix_id,
                "original_id": item["original_id"],
                "dimension": item["dimension"],
                "matrix": item["matrix"],
                "file_sha256": None,
                "is_strictly_copositive": item["strict"],
                "is_copositive": representative["is_copositive"],
                "source": (
                    f"FracESSA:{item['source_matrix_id']}; restored permutation-sensitive reduced-B occurrence; "
                    f"candidate_id={item['candidate_id']}; support={item['support']}; extended_support={item['extended_support']}; "
                    f"original_occurrence_matrix_id={item['original_id']}"
                ),
                "source_id": 91,
                "family": None,
                "smoke_set": 0,
                "representative_core": 0,
                "stress_test": 0,
                "scale_set": 0,
                "timeout_5s_strict_set": 0,
                "additional_source_ids": "[]",
                "references_solved": "[]",
                "references_unsolved": "[]",
                "phase": "initial",
            }
            planned.append(record)
            planned_by_key[key] = record
            direct_keys.add(key)
            direct_owners[key] = matrix_id

        old_projective_rows = {row["matrix_id"]: row for row in before_projective.execute("SELECT * FROM matrices")}
        post_projective_ids = {row[0] for row in after_projective.execute("SELECT matrix_id FROM matrices")}
        deleted_projective_ids = set(old_projective_rows) - post_projective_ids
        if len(deleted_projective_ids) != 155:
            raise ValueError(f"expected 155 projective deletions, found {len(deleted_projective_ids)}")

        restored_projective_ids: set[int] = set()
        for old_id in sorted(deleted_projective_ids):
            row = old_projective_rows[old_id]
            values = row_values(row)
            key = projective_key(row["dimension"], values)
            record = planned_by_key.get(key)
            if record is not None and record["matrix_id"] == old_id and record["matrix"] == row["matrix"]:
                record["source"] = row["source"]
                record["family"] = row["family"]
                restored_projective_ids.add(old_id)
                continue
            load_current_dimension(row["dimension"])
            if key in direct_keys:
                continue
            representative = equivalent_representative(row["dimension"], values, permutation_index)
            matrix_id = allocate(old_id)
            record = {
                "matrix_id": matrix_id,
                "original_id": old_id,
                "dimension": row["dimension"],
                "matrix": row["matrix"],
                "file_sha256": None,
                "is_strictly_copositive": row["is_strictly_copositive"],
                "is_copositive": representative["is_copositive"],
                "source": row["source"],
                "source_id": source_id_for_old_row(row, representative),
                "family": row["family"],
                "smoke_set": 0,
                "representative_core": 0,
                "stress_test": 0,
                "scale_set": 0,
                "timeout_5s_strict_set": 0,
                "additional_source_ids": "[]",
                "references_solved": "[]",
                "references_unsolved": "[]",
                "phase": "projective_2026_08_09",
            }
            planned.append(record)
            planned_by_key[key] = record
            direct_keys.add(key)
            direct_owners[key] = matrix_id
            if matrix_id == old_id:
                restored_projective_ids.add(old_id)

        annotation = re.compile(r"equivalent occurrence matrix_id=(\d+)")
        literature_survivors: dict[int, sqlite3.Row] = {}
        for row in current:
            for deleted_id in map(int, annotation.findall(row["source"] or "")):
                if deleted_id in literature_survivors:
                    raise ValueError(f"duplicate literature annotation for {deleted_id}")
                literature_survivors[deleted_id] = row
        if len(literature_survivors) != 333:
            raise ValueError(f"expected 333 literature deletions, found {len(literature_survivors)}")

        old_literature_rows = {row["matrix_id"]: row for row in before_literature.execute("SELECT * FROM matrices")}
        restored_literature_ids: set[int] = set()
        scaled_literature_ids: set[int] = set()
        for old_id, representative in sorted(literature_survivors.items()):
            row = old_literature_rows[old_id]
            values = row_values(row)
            other = row_values(representative)
            if not permutation_equivalent(row["dimension"], values, other):
                raise ValueError(f"invalid archived literature equivalence for {old_id}")
            key = projective_key(row["dimension"], values)
            load_current_dimension(row["dimension"])
            if key in direct_keys:
                scaled_literature_ids.add(old_id)
                continue
            matrix_id = allocate(old_id)
            record = {name: row[name] for name in (
                "dimension", "matrix", "file_sha256", "is_strictly_copositive", "is_copositive", "source", "source_id", "family",
                "smoke_set", "representative_core", "stress_test", "scale_set", "timeout_5s_strict_set", "additional_source_ids",
                "references_solved",
            )}
            record.update({
                "matrix_id": matrix_id,
                "original_id": old_id,
                "references_unsolved": "[]",
                "phase": "literature_2026_08_14",
            })
            planned.append(record)
            planned_by_key[key] = record
            direct_keys.add(key)
            direct_owners[key] = matrix_id
            if matrix_id == old_id:
                restored_literature_ids.add(old_id)

        result_rows = normalized_old_results(before_projective, restored_projective_ids)
        counts = defaultdict(int)
        for row in planned:
            counts[row["phase"]] += 1
        print(json.dumps({
            "initial_occurrences": 9171,
            "initial_exact_arrays": initial_exact,
            "initial_direct_scale_classes": len(initial),
            "deleted_projective_rows": len(deleted_projective_ids),
            "restored_projective_ids_with_own_results": len(restored_projective_ids),
            "deleted_literature_rows": len(literature_survivors),
            "literature_rows_kept_removed_as_direct_scales": len(scaled_literature_ids),
            "planned_matrices": len(planned),
            "planned_by_phase": dict(sorted(counts.items())),
            "planned_results": len(result_rows),
            "reassigned_ids": sum(row["matrix_id"] != row["original_id"] for row in planned),
            "restored_literature_ids": sorted(restored_literature_ids),
            "restored_projective_ids": sorted(restored_projective_ids),
            "restored_file_ids": sorted(row["matrix_id"] for row in planned if row["matrix"].startswith("file:")),
        }, indent=2, sort_keys=True))
        if arguments.dry_run:
            return

        for row in planned:
            if not row["matrix"].startswith("file:"):
                continue
            destination = PROJECT / "testdata" / row["matrix"].removeprefix("file:")
            if destination.is_file():
                continue
            source = TRASH / destination.name
            if hashlib.sha256(source.read_bytes()).hexdigest() != row["file_sha256"]:
                raise ValueError(f"payload hash mismatch for {source}")
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)

        matrix_columns = (
            "matrix_id", "dimension", "matrix", "file_sha256", "is_strictly_copositive", "is_copositive", "source", "source_id",
            "family", "smoke_set", "representative_core", "stress_test", "scale_set", "timeout_5s_strict_set", "additional_source_ids",
            "references_solved", "references_unsolved",
        )
        connection.execute("BEGIN IMMEDIATE")
        connection.executemany(
            f"INSERT INTO matrices ({','.join(matrix_columns)}) VALUES ({','.join('?' for _ in matrix_columns)})",
            [tuple(row[name] for name in matrix_columns) for row in planned],
        )
        connection.executemany(
            """INSERT OR IGNORE INTO results (
                matrix_id,model_id,mode,preprocessing,binary_sha256,status,is_copositive,is_strictly_copositive,
                elapsed_ns,timeout_ns,recorded_at,message
            ) VALUES (?,?,?,?,?,?,?,?,?,?,?,?)""",
            result_rows,
        )
        if list(connection.execute("PRAGMA foreign_key_check")):
            raise ValueError("foreign-key check failed")
        connection.commit()
        if connection.execute("PRAGMA integrity_check").fetchone()[0] != "ok":
            raise ValueError("integrity check failed")


if __name__ == "__main__":
    main()
