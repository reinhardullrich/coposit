#!/usr/bin/env python3
"""Classify imported rows whose stored construction gives a short exact proof."""

from __future__ import annotations

import argparse
import itertools
import re
import sqlite3
from collections import Counter
from pathlib import Path

from classify_obvious_literature_truth_2026_08_14 import matrix_entries


DATABASE = Path(__file__).parents[1] / "copos_testdata.sqlite3"
EVIDENCE_PREFIX = "truth_evidence=constructed exactly on 2026-08-14: "
EXPECTED_RESULTS = Counter({(0, 0): 41, (0, 1): 11, (1, 1): 3})

HORN_CYCLES = {10686, 10687, 10688, 10689, 10690, 12510}
TABLE_GRAPH_ROWS = set(range(10813, 10831)) | set(range(10832, 10851)) | {10876, 10877}
CLIQUE_BOUNDS = {
    "brock200_1": (21, True), "brock200_2": (12, True), "brock200_3": (15, True), "brock200_4": (17, True),
    "brock400_1": (27, True), "brock400_2": (29, True), "brock400_3": (31, True), "brock400_4": (33, True),
    "brock800_1": (23, True), "brock800_2": (24, True), "brock800_3": (25, True), "brock800_4": (26, True),
    "hamming10-2": (512, True), "hamming10-4": (40, True),
    "keller4": (11, True), "keller5": (27, True), "keller6": (59, False),
    "MANN_a27": (126, True), "MANN_a45": (345, True),
}
SMALL_GRAPHS = {
    10769: ("icosahedron", 3),
    10775: ("Brock14", 5),
    10776: ("1tc16", 8),
    10996: ("graph8", 3),
}
POSITIVE_DEFINITE = {
    10729: [2, 1],
    10739: [6, 11, 1],
    10746: [1, 1],
}
NEGATIVE_WITNESSES = {
    10863: {0: 1, 5: 1, 7: 1},
    10865: {0: 1, 1: 1, 2: 1},
    10867: {0: 1, 1: 1, 3: 1},
}


def dense_matrix(n: int, storage: str) -> list[list[int]]:
    matrix = [[0] * n for _ in range(n)]
    for i, j, value in matrix_entries(n, storage):
        matrix[i][j] = matrix[j][i] = value
    return matrix


def validate_graph_transform(n: int, storage: str, parameter: int) -> None:
    count = 0
    for i, j, value in matrix_entries(n, storage):
        expected = {parameter - 1} if i == j else {-1, parameter - 1}
        if value not in expected:
            raise ValueError(f"graph transform has A[{i + 1},{j + 1}]={value}, expected one of {sorted(expected)}")
        count += 1
    expected_count = n * (n + 1) // 2
    if count != expected_count:
        raise ValueError(f"graph transform stores {count} upper-triangle entries instead of {expected_count}")


def maximal_clique(matrix: list[list[int]]) -> tuple[int, tuple[int, ...]]:
    n = len(matrix)
    best: tuple[int, ...] = ()
    for mask in range(1, 1 << n):
        if mask.bit_count() <= len(best):
            continue
        vertices = tuple(i for i in range(n) if mask & (1 << i))
        if all(matrix[i][j] == -1 for i, j in itertools.combinations(vertices, 2)):
            best = vertices
    return len(best), tuple(i + 1 for i in best)


def determinant(matrix: list[list[int]]) -> int:
    work = [row[:] for row in matrix]
    previous = 1
    sign = 1
    for pivot_index in range(len(work) - 1):
        if work[pivot_index][pivot_index] == 0:
            replacement = next((i for i in range(pivot_index + 1, len(work)) if work[i][pivot_index]), None)
            if replacement is None:
                return 0
            work[pivot_index], work[replacement] = work[replacement], work[pivot_index]
            sign = -sign
        pivot = work[pivot_index][pivot_index]
        for i in range(pivot_index + 1, len(work)):
            for j in range(pivot_index + 1, len(work)):
                work[i][j] = (work[i][j] * pivot - work[i][pivot_index] * work[pivot_index][j]) // previous
        previous = pivot
    return sign * work[-1][-1]


def catalog_instance(source: str) -> str:
    match = re.search(r"(?:^|; )catalog_instance_id=([^;]+)", source)
    if not match:
        raise ValueError("missing catalog_instance_id")
    return match.group(1)


def graph_table_decision(matrix_id: int, n: int, storage: str, source: str) -> tuple[int, int, str]:
    instance = catalog_instance(source)
    match = re.fullmatch(r"(?:zilinskas_duer_2011|zilinskas_2011_programming):(.+)_t_(\d+)", instance)
    if not match:
        match = re.fullmatch(r"bomze_eichfelder_2013:(.+)_lambda_(\d+)", instance)
    if not match:
        raise ValueError(f"matrix {matrix_id}: unexpected graph instance {instance}")
    graph, parameter_text = match.groups()
    parameter = int(parameter_text)
    if graph not in CLIQUE_BOUNDS:
        raise ValueError(f"matrix {matrix_id}: no clique bound for {graph}")
    validate_graph_transform(n, storage, parameter)
    clique_bound, exact = CLIQUE_BOUNDS[graph]
    if parameter > clique_bound or parameter == clique_bound and not exact:
        raise ValueError(f"matrix {matrix_id}: clique information does not decide parameter {parameter}")
    evidence = (f"A={parameter}(E-Adj({graph}))-E, and Motzkin-Straus gives min(x^T A x)={parameter}/omega({graph})-1 "
                f"on the simplex; the paper's clique table gives omega({graph}){'=' if exact else '>='}{clique_bound}")
    if exact and parameter == clique_bound:
        return 0, 1, evidence + ", so the minimum is exactly zero"
    return 0, 0, evidence + f">{parameter}, so the minimum is negative"


def decisions(rows: dict[int, tuple[int, str, int | None, int | None, str]]) -> dict[int, tuple[int, int, str]]:
    result: dict[int, tuple[int, int, str]] = {}

    for matrix_id in HORN_CYCLES:
        n, storage, _, _, _ = rows[matrix_id]
        for i, j, value in matrix_entries(n, storage):
            distance = (j - i) % n
            expected = 1 if i == j or distance not in (1, n - 1) else -1
            if value != expected:
                raise ValueError(f"matrix {matrix_id}: not the expected cycle transform at ({i + 1},{j + 1})")
        result[matrix_id] = (0, 1, f"A=2(E-Adj(C_{n}))-E and omega(C_{n})=2, so Motzkin-Straus gives simplex minimum 0")

    for matrix_id in TABLE_GRAPH_ROWS:
        n, storage, _, _, source = rows[matrix_id]
        result[matrix_id] = graph_table_decision(matrix_id, n, storage, source)

    for matrix_id, (name, parameter) in SMALL_GRAPHS.items():
        n, storage, _, _, _ = rows[matrix_id]
        validate_graph_transform(n, storage, parameter)
        omega, clique = maximal_clique(dense_matrix(n, storage))
        if omega != parameter:
            raise ValueError(f"matrix {matrix_id}: exact clique number {omega} differs from parameter {parameter}")
        result[matrix_id] = (
            0, 1,
            f"A={parameter}(E-Adj({name}))-E; exhaustive enumeration of all 2^{n} vertex subsets gives omega({name})={omega} "
            f"(one maximum clique is {clique}), so Motzkin-Straus gives simplex minimum 0",
        )

    for matrix_id, expected_minors in POSITIVE_DEFINITE.items():
        n, storage, _, _, _ = rows[matrix_id]
        matrix = dense_matrix(n, storage)
        minors = [determinant([row[:k] for row in matrix[:k]]) for k in range(1, n + 1)]
        if minors != expected_minors:
            raise ValueError(f"matrix {matrix_id}: leading principal minors changed from {expected_minors} to {minors}")
        result[matrix_id] = (1, 1, f"all leading principal minors are positive ({minors}); Sylvester's criterion makes A positive definite")

    for matrix_id, witness in NEGATIVE_WITNESSES.items():
        n, storage, _, _, _ = rows[matrix_id]
        matrix = dense_matrix(n, storage)
        value = sum(xi * xj * matrix[i][j] for i, xi in witness.items() for j, xj in witness.items())
        if value >= 0:
            raise ValueError(f"matrix {matrix_id}: stored witness is no longer negative")
        displayed = ", ".join(f"x[{i + 1}]={value}" for i, value in witness.items())
        result[matrix_id] = (0, 0, f"the nonnegative vector with {displayed} and all other entries zero has x^T A x={value}<0")

    return result


def merge(old: int | None, proposed: int, matrix_id: int, field: str) -> int:
    if old is not None and old != proposed:
        raise ValueError(f"matrix {matrix_id}: exact construction contradicts stored {field}={old}")
    return proposed


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dry-run", action="store_true")
    arguments = parser.parse_args()

    target_ids = HORN_CYCLES | TABLE_GRAPH_ROWS | set(SMALL_GRAPHS) | set(POSITIVE_DEFINITE) | set(NEGATIVE_WITNESSES)
    placeholders = ",".join("?" for _ in target_ids)
    with sqlite3.connect(DATABASE) as connection:
        rows = {
            matrix_id: (n, storage, strict, copositive, source or "")
            for matrix_id, n, storage, strict, copositive, source in connection.execute(
                f"""SELECT matrix_id, dimension, matrix, is_strictly_copositive, is_copositive, source
                    FROM matrices WHERE matrix_id IN ({placeholders})""",
                sorted(target_ids),
            )
        }
        if set(rows) != target_ids:
            raise ValueError(f"missing target rows: {sorted(target_ids - set(rows))}")

        exact_decisions = decisions(rows)
        observed = Counter((strict, copositive) for strict, copositive, _ in exact_decisions.values())
        if observed != EXPECTED_RESULTS:
            raise ValueError(f"unexpected decision counts: {observed}")

        pending = []
        matching = 0
        for matrix_id, (proposed_strict, proposed_copositive, evidence) in exact_decisions.items():
            _, _, old_strict, old_copositive, source = rows[matrix_id]
            strict = merge(old_strict, proposed_strict, matrix_id, "strict truth")
            copositive = merge(old_copositive, proposed_copositive, matrix_id, "ordinary truth")
            suffix = f"; {EVIDENCE_PREFIX}{evidence}"
            if suffix in source:
                if (old_strict, old_copositive) != (strict, copositive):
                    raise ValueError(f"matrix {matrix_id}: evidence exists without its truth values")
                matching += 1
            elif EVIDENCE_PREFIX in source:
                raise ValueError(f"matrix {matrix_id}: stored construction evidence differs from the exact reconstruction")
            else:
                pending.append((strict, copositive, source + suffix, matrix_id))

        if pending and matching:
            raise ValueError(f"partial construction migration found: {matching} applied, {len(pending)} pending")
        for truth, count in sorted(EXPECTED_RESULTS.items()):
            print(f"strict={truth[0]} copositive={truth[1]}: {count}")
        print(f"pending={len(pending)} already_applied={matching}")
        if arguments.dry_run:
            return
        if not pending:
            print("all construction classifications are already applied")
            return

        connection.execute("BEGIN IMMEDIATE")
        connection.executemany(
            "UPDATE matrices SET is_strictly_copositive=?, is_copositive=?, source=? WHERE matrix_id=?",
            pending,
        )
        if list(connection.execute("PRAGMA foreign_key_check")):
            raise ValueError("foreign-key check failed")
        connection.commit()
        if connection.execute("PRAGMA integrity_check").fetchone()[0] != "ok":
            raise ValueError("integrity check failed")

    print(f"applied={len(pending)}")


if __name__ == "__main__":
    main()
