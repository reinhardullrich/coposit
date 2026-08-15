#!/usr/bin/env python3
"""Classify eight small imported matrices by exact enumeration of every simplex face."""

from __future__ import annotations

import argparse
import itertools
import sqlite3
from collections import Counter
from fractions import Fraction
from pathlib import Path

from classify_constructed_literature_truth_2026_08_14 import dense_matrix


DATABASE = Path(__file__).parents[1] / "copos_testdata.sqlite3"
EVIDENCE_PREFIX = "truth_evidence=exact small-simplex enumeration on 2026-08-14: "
EXPECTED = {
    10691: (Fraction(1, 10), (1, 3), (Fraction(7, 10), Fraction(3, 10))),
    10693: (Fraction(1, 5), (2, 3), (Fraction(3, 5), Fraction(2, 5))),
    10696: (Fraction(2, 17), (1, 2, 4), (Fraction(5, 17), Fraction(8, 17), Fraction(4, 17))),
    10697: (Fraction(0), (1, 2, 3), (Fraction(1, 4), Fraction(1, 2), Fraction(1, 4))),
    10715: (Fraction(0), (2, 3, 4), (Fraction(1, 3), Fraction(1, 3), Fraction(1, 3))),
    10716: (
        Fraction(4867891, 14759327), (1, 2, 4),
        (Fraction(5462843, 14759327), Fraction(3908122, 14759327), Fraction(5388362, 14759327)),
    ),
    10767: (Fraction(0), (2, 3, 4), (Fraction(1, 3), Fraction(1, 3), Fraction(1, 3))),
    10772: (
        Fraction(580481644, 14759327), (1, 2, 4),
        (Fraction(5462843, 14759327), Fraction(3908122, 14759327), Fraction(5388362, 14759327)),
    ),
}
EXPECTED_RESULTS = Counter({(1, 1): 5, (0, 1): 3})


def solve_unique(matrix: list[list[int]], right_hand_side: list[int]) -> list[Fraction] | None:
    work = [[Fraction(value) for value in row] + [Fraction(rhs)] for row, rhs in zip(matrix, right_hand_side)]
    rank = 0
    columns = len(matrix[0])
    for column in range(columns):
        pivot = next((row for row in range(rank, len(work)) if work[row][column]), None)
        if pivot is None:
            continue
        work[rank], work[pivot] = work[pivot], work[rank]
        divisor = work[rank][column]
        work[rank] = [value / divisor for value in work[rank]]
        for row in range(len(work)):
            if row == rank or not work[row][column]:
                continue
            multiplier = work[row][column]
            work[row] = [value - multiplier * pivot_value for value, pivot_value in zip(work[row], work[rank])]
        rank += 1

    if any(all(value == 0 for value in row[:-1]) and row[-1] != 0 for row in work):
        return None
    if rank != columns:
        raise ValueError("underdetermined stationary system prevents the short finite proof")
    solution = [Fraction(0)] * columns
    for row in work:
        pivot = next((column for column, value in enumerate(row[:-1]) if value), None)
        if pivot is not None:
            solution[pivot] = row[-1]
    return solution


def exact_simplex_minimum(matrix: list[list[int]]) -> tuple[Fraction, tuple[int, ...], tuple[Fraction, ...]]:
    candidates = []
    n = len(matrix)
    for cardinality in range(1, n + 1):
        for zero_based_support in itertools.combinations(range(n), cardinality):
            stationary_system = [
                [matrix[i][j] for j in zero_based_support] + [-1]
                for i in zero_based_support
            ] + [[1] * cardinality + [0]]
            solution = solve_unique(stationary_system, [0] * cardinality + [1])
            if solution is None:
                continue
            weights, value = tuple(solution[:-1]), solution[-1]
            if all(weight > 0 for weight in weights):
                support = tuple(i + 1 for i in zero_based_support)
                candidates.append((value, support, weights))
    if not candidates:
        raise ValueError("no simplex stationary candidate found")
    return min(candidates, key=lambda candidate: candidate[0])


def fraction_text(value: Fraction) -> str:
    return str(value.numerator) if value.denominator == 1 else f"{value.numerator}/{value.denominator}"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dry-run", action="store_true")
    arguments = parser.parse_args()

    placeholders = ",".join("?" for _ in EXPECTED)
    with sqlite3.connect(DATABASE) as connection:
        rows = {
            matrix_id: (n, storage, strict, copositive, source or "")
            for matrix_id, n, storage, strict, copositive, source in connection.execute(
                f"""SELECT matrix_id, dimension, matrix, is_strictly_copositive, is_copositive, source
                    FROM matrices WHERE matrix_id IN ({placeholders})""",
                sorted(EXPECTED),
            )
        }
        if set(rows) != set(EXPECTED):
            raise ValueError(f"missing target rows: {sorted(set(EXPECTED) - set(rows))}")

        pending = []
        matching = 0
        results = Counter()
        for matrix_id, expected in EXPECTED.items():
            n, storage, old_strict, old_copositive, source = rows[matrix_id]
            minimum = exact_simplex_minimum(dense_matrix(n, storage))
            if minimum != expected:
                raise ValueError(f"matrix {matrix_id}: exact minimum changed from {expected} to {minimum}")
            value, support, weights = minimum
            strict, copositive = (1, 1) if value > 0 else (0, 1)
            if old_strict is not None and old_strict != strict or old_copositive is not None and old_copositive != copositive:
                raise ValueError(f"matrix {matrix_id}: exact minimum contradicts stored truth")
            results[(strict, copositive)] += 1
            weight_text = ", ".join(f"x[{index}]={fraction_text(weight)}" for index, weight in zip(support, weights))
            evidence = (f"all {2 ** n - 1} nonempty faces were checked through their exact stationary systems; the global simplex "
                        f"minimum is {fraction_text(value)} at support {support} with {weight_text}")
            suffix = f"; {EVIDENCE_PREFIX}{evidence}"
            if suffix in source:
                if (old_strict, old_copositive) != (strict, copositive):
                    raise ValueError(f"matrix {matrix_id}: evidence exists without its truth values")
                matching += 1
            elif EVIDENCE_PREFIX in source:
                raise ValueError(f"matrix {matrix_id}: stored small-simplex evidence differs from the exact reconstruction")
            else:
                pending.append((strict, copositive, source + suffix, matrix_id))

        if results != EXPECTED_RESULTS:
            raise ValueError(f"unexpected classification counts: {results}")
        if pending and matching:
            raise ValueError(f"partial small-simplex migration found: {matching} applied, {len(pending)} pending")
        for truth, count in sorted(results.items()):
            print(f"strict={truth[0]} copositive={truth[1]}: {count}")
        print(f"pending={len(pending)} already_applied={matching}")
        if arguments.dry_run:
            return
        if not pending:
            print("all small exact classifications are already applied")
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
