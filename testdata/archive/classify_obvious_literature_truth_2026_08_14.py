#!/usr/bin/env python3
"""Classify imported literature matrices using short exact certificates visible in the stored matrix."""

from __future__ import annotations

import argparse
import sqlite3
from collections import Counter
from pathlib import Path
from typing import Iterator


DATABASE = Path(__file__).parents[1] / "copos_testdata.sqlite3"
FIRST_IMPORTED_ID = 10685
LAST_IMPORTED_ID = 12522
EVIDENCE_PREFIX = "truth_evidence=self-classified exactly on 2026-08-14: "
EXPECTED_UPDATES = Counter({(0, 0): 289, (0, 1): 166, (0, None): 6, (1, 1): 237, (None, 1): 1})


def upper_positions(n: int) -> Iterator[tuple[int, int]]:
    for i in range(n):
        for j in range(i, n):
            yield i, j


def matrix_entries(n: int, storage: str) -> Iterator[tuple[int, int, int]]:
    if not storage.startswith("file:"):
        values = storage.split(",")
        expected = n * (n + 1) // 2
        if len(values) != expected:
            raise ValueError(f"inline matrix has {len(values)} values instead of {expected}")
        for (i, j), value in zip(upper_positions(n), values):
            yield i, j, int(value)
        return

    path = DATABASE.parent / storage.removeprefix("file:")
    with path.open() as stream:
        header = stream.readline().lower().split()
        if len(header) != 5 or header[:2] != ["%%matrixmarket", "matrix"] or header[3:] != ["integer", "symmetric"]:
            raise ValueError(f"unsupported Matrix Market header in {path}: {header}")
        lines = (line for line in stream if line.strip() and not line.lstrip().startswith("%"))
        shape = next(lines).split()
        if int(shape[0]) != n or int(shape[1]) != n:
            raise ValueError(f"wrong Matrix Market shape in {path}: {shape}")

        if header[2] == "array":
            positions = upper_positions(n)
            count = 0
            for line in lines:
                position = next(positions, None)
                if position is None:
                    raise ValueError(f"too many array values in {path}")
                count += 1
                yield *position, int(line)
            expected = n * (n + 1) // 2
            if count != expected:
                raise ValueError(f"array matrix has {count} values instead of {expected}: {path}")
            return

        if header[2] == "coordinate":
            expected = int(shape[2])
            count = 0
            for line in lines:
                row, column, value = map(int, line.split())
                i, j = sorted((row - 1, column - 1))
                count += 1
                yield i, j, value
            if count != expected:
                raise ValueError(f"coordinate matrix has {count} values instead of {expected}: {path}")
            return

        raise ValueError(f"unsupported Matrix Market format in {path}: {header[2]}")


def zero_pair_evidence(i: int, j: int, aii: int, aij: int, ajj: int) -> str:
    return (f"the nonnegative vector with x[{i + 1}]={-aij} and x[{j + 1}]={aii} has x^T A x=0 "
            f"because Aii={aii}, Aij={aij}, Ajj={ajj}, and Aij^2=Aii*Ajj")


def classify(n: int, storage: str) -> tuple[int | None, int | None, str] | None:
    diagonal = [0] * n
    negative_part = [0] * n
    all_nonnegative = True
    ones_value = 0

    for i, j, value in matrix_entries(n, storage):
        if i == j:
            diagonal[i] = value
            ones_value += value
        else:
            ones_value += 2 * value
            if value < 0:
                negative_part[i] -= value
                negative_part[j] -= value
        all_nonnegative &= value >= 0

    for i, value in enumerate(diagonal):
        if value < 0:
            return 0, 0, f"e[{i + 1}] is a nonnegative witness with x^T A x=A[{i + 1},{i + 1}]={value}<0"
    if ones_value < 0:
        return 0, 0, f"the all-ones vector is a nonnegative witness with x^T A x={ones_value}<0"

    zero_evidence = None
    for i, j, value in matrix_entries(n, storage):
        if i == j or value >= 0:
            continue
        aii, ajj = diagonal[i], diagonal[j]
        if value * value > aii * ajj:
            if aii:
                xi, xj = -value, aii
            else:
                xi, xj = ajj + 1, 1
            quadratic = aii * xi * xi + 2 * value * xi * xj + ajj * xj * xj
            return (0, 0, f"the two-coordinate nonnegative witness x[{i + 1}]={xi}, x[{j + 1}]={xj} has "
                    f"x^T A x={quadratic}<0 from Aii={aii}, Aij={value}, Ajj={ajj}")
        if value * value == aii * ajj and zero_evidence is None:
            zero_evidence = zero_pair_evidence(i, j, aii, value, ajj)
    if ones_value == 0 and zero_evidence is None:
        zero_evidence = "the all-ones vector is nonzero and nonnegative with x^T A x=0"

    if all_nonnegative:
        minimum = min(diagonal)
        if minimum > 0:
            return 1, 1, f"A is entrywise nonnegative and every diagonal entry is positive (minimum {minimum})"
        i = diagonal.index(0)
        return 0, 1, f"A is entrywise nonnegative and e[{i + 1}] is an exact zero because A[{i + 1},{i + 1}]=0"

    residuals = [diagonal[i] - negative_part[i] for i in range(n)]
    minimum = min(residuals)
    if minimum >= 0:
        evidence = ("for every row, Aii is at least the sum of absolute negative off-diagonal entries "
                    f"(minimum exact residual {minimum})")
        if minimum > 0:
            return 1, 1, evidence
        if 0 in diagonal:
            i = diagonal.index(0)
            return 0, 1, f"{evidence}, while e[{i + 1}] is an exact zero because A[{i + 1},{i + 1}]=0"
        if zero_evidence:
            return 0, 1, f"{evidence}, and {zero_evidence}"
        return None, 1, evidence

    if zero_evidence:
        return 0, None, zero_evidence
    return None


def merge(old: int | None, proposed: int | None, matrix_id: int, field: str) -> int | None:
    if proposed is None:
        return old
    if old is not None and old != proposed:
        raise ValueError(f"matrix {matrix_id}: exact construction contradicts stored {field}={old}")
    return proposed


def evidence_kind(evidence: str) -> str:
    if evidence.startswith("e["):
        return "negative diagonal witness"
    if evidence.startswith("the all-ones vector is a nonnegative witness"):
        return "negative all-ones witness"
    if evidence.startswith("the two-coordinate nonnegative witness"):
        return "negative two-coordinate witness"
    if evidence.startswith("A is entrywise nonnegative"):
        return "entrywise-nonnegative certificate"
    if evidence.startswith("for every row"):
        return "negative-part diagonal-dominance certificate"
    if "Aij^2=Aii*Ajj" in evidence:
        return "zero two-coordinate witness"
    return "zero all-ones witness"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dry-run", action="store_true")
    arguments = parser.parse_args()

    with sqlite3.connect(DATABASE) as connection:
        rows = list(connection.execute(
            """SELECT matrix_id, dimension, matrix, is_strictly_copositive, is_copositive, source
               FROM matrices
               WHERE matrix_id BETWEEN ? AND ?
               ORDER BY matrix_id""",
            (FIRST_IMPORTED_ID, LAST_IMPORTED_ID),
        ))
        pending = []
        matching = 0
        counts = Counter()
        reasons = Counter()
        partial = []
        for matrix_id, n, storage, old_strict, old_copositive, source in rows:
            certificate = classify(n, storage)
            if certificate is None:
                continue
            proposed_strict, proposed_copositive, evidence = certificate
            strict = merge(old_strict, proposed_strict, matrix_id, "strict truth")
            copositive = merge(old_copositive, proposed_copositive, matrix_id, "ordinary truth")
            suffix = f"; {EVIDENCE_PREFIX}{evidence}"
            if (strict, copositive) != (old_strict, old_copositive):
                if EVIDENCE_PREFIX in source:
                    raise ValueError(f"matrix {matrix_id}: stored evidence does not match stored truth")
                pending.append((strict, copositive, source + suffix, matrix_id))
                counts[(strict, copositive)] += 1
                reasons[evidence_kind(evidence)] += 1
                if strict is None or copositive is None:
                    partial.append((matrix_id, strict, copositive, evidence))
            elif EVIDENCE_PREFIX in source:
                if suffix not in source:
                    raise ValueError(f"matrix {matrix_id}: stored evidence differs from the exact reconstruction")
                matching += 1

        if pending and matching:
            raise ValueError(f"partial exact-classification migration found: {matching} applied, {len(pending)} pending")
        if pending and counts != EXPECTED_UPDATES:
            raise ValueError(f"unexpected exact-classification counts: {counts}")
        if not pending and matching != sum(EXPECTED_UPDATES.values()):
            raise ValueError(f"expected {sum(EXPECTED_UPDATES.values())} exact evidence comments, found {matching}")

        for truth, count in sorted(counts.items(), key=lambda item: str(item[0])):
            print(f"strict={truth[0]} copositive={truth[1]}: {count}")
        for reason, count in reasons.most_common():
            print(f"{reason}: {count}")
        for matrix_id, strict, copositive, evidence in partial:
            print(f"partial matrix={matrix_id} strict={strict} copositive={copositive}: {evidence}")
        print(f"pending={len(pending)} already_applied={matching}")
        if arguments.dry_run:
            return
        if not pending:
            print("all obvious exact classifications are already applied")
            return

        connection.execute("BEGIN IMMEDIATE")
        connection.executemany(
            """UPDATE matrices
               SET is_strictly_copositive=?, is_copositive=?, source=?
               WHERE matrix_id=?""",
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
