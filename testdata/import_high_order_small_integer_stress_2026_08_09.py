#!/usr/bin/env python3
"""Reproduce the 2026-08-09 randomized high-order small-integer stress matrices."""

from __future__ import annotations

import argparse
import hashlib
import io
import sqlite3
from pathlib import Path
from typing import Iterator


DIMENSIONS = (
    51, 168, 235, 331, 438, 542, 651, 747, 856, 952, 1057, 1151, 1263, 1360, 1468,
    1565, 1672, 1770, 1876, 1973, 2084, 2181, 2289, 2386, 2498, 2594, 2701, 2799, 2908, 2997,
)
MASK64 = (1 << 64) - 1
BASE_SEED = 0xC0A0517_20260809
KINDS = ("boundary", "strict", "not_copositive")
SALTS = {"boundary": 0xB0A0D, "strict": 0x57A1C7, "not_copositive": 0xBAD}
FAMILIES = {
    "boundary": "generated sparse PSD / copositive boundary",
    "strict": "generated sparse positive definite / strict copositive",
    "not_copositive": "generated sparse two-coordinate witness / not copositive",
}


def splitmix64(state: int) -> tuple[int, int]:
    state = (state + 0x9E3779B97F4A7C15) & MASK64
    value = state
    value = ((value ^ (value >> 30)) * 0xBF58476D1CE4E5B9) & MASK64
    value = ((value ^ (value >> 27)) * 0x94D049BB133111EB) & MASK64
    return state, value ^ (value >> 31)


def shuffled(vertices: range, state: int) -> tuple[list[int], int]:
    result = list(vertices)
    for index in range(len(result) - 1, 0, -1):
        state, word = splitmix64(state)
        selected = word % (index + 1)
        result[index], result[selected] = result[selected], result[index]
    return result, state


def add_gram_edge(diagonal: list[int], upper: list[dict[int, int]], first: int, second: int, sign: int) -> None:
    diagonal[first] += 1
    diagonal[second] += 1
    row, column = sorted((first, second))
    value = upper[row].get(column, 0) + sign
    if value:
        upper[row][column] = value
    else:
        upper[row].pop(column, None)


def add_two_signed_cycles(diagonal: list[int], upper: list[dict[int, int]], vertices: range, seed: int) -> int:
    state = seed
    for _ in range(2):
        permutation, state = shuffled(vertices, state)
        for index, first in enumerate(permutation):
            second = permutation[(index + 1) % len(permutation)]
            state, word = splitmix64(state)
            add_gram_edge(diagonal, upper, first, second, 1 if word & 1 else -1)
    return state


def build_sparse_matrix(dimension: int, kind: str) -> tuple[list[int], list[dict[int, int]], str]:
    seed = (BASE_SEED ^ (dimension * 0x9E3779B97F4A7C15) ^ SALTS[kind]) & MASK64
    diagonal = [0] * dimension
    upper = [{} for _ in range(dimension)]

    if kind == "boundary":
        diagonal[0] = diagonal[1] = 1
        upper[0][1] = -1
        for index in range(2, dimension):
            diagonal[index] = 1
        add_two_signed_cycles(diagonal, upper, range(2, dimension), seed)
        assert diagonal[0] + diagonal[1] + 2 * upper[0][1] == 0
        proof = "PSD direct sum; nonnegative zero witness e1+e2"
    else:
        diagonal[:] = [1] * dimension
        state = add_two_signed_cycles(diagonal, upper, range(dimension), seed)
        if kind == "strict":
            proof = "positive definite I plus a sum of signed edge outer products"
        else:
            state, word = splitmix64(state)
            first = word % dimension
            state, word = splitmix64(state)
            second = word % dimension
            if second == first:
                second = (second + 1) % dimension
            row, column = sorted((first, second))
            upper[row][column] = -(diagonal[first] + diagonal[second])
            value = diagonal[first] + diagonal[second] + 2 * upper[row][column]
            assert value < 0
            proof = f"positive diagonal; x=e{first + 1}+e{second + 1} gives x^T A x={value}"

    off_diagonal = [value for row in upper for value in row.values() if value]
    assert any(value > 0 for value in off_diagonal)
    assert any(value < 0 for value in off_diagonal)
    return diagonal, upper, f"seed=0x{seed:016x}; {proof}"


def upper_text(diagonal: list[int], upper: list[dict[int, int]]) -> tuple[str, int, int]:
    dimension = len(diagonal)
    output = io.StringIO()
    maximum = 0
    nonzero = 0
    for row_index in range(dimension):
        row = ["0"] * (dimension - row_index)
        row[0] = str(diagonal[row_index])
        maximum = max(maximum, abs(diagonal[row_index]))
        nonzero += diagonal[row_index] != 0
        for column, value in upper[row_index].items():
            row[column - row_index] = str(value)
            maximum = max(maximum, abs(value))
            nonzero += value != 0
        if row_index:
            output.write(",")
        output.write(",".join(row))
    return output.getvalue(), maximum, nonzero


def generated_rows() -> Iterator[tuple[int, str, int, str, str, int, int]]:
    prefix = "Coposit deterministic high-order sparse stress generator 2026-08-09"
    for dimension in DIMENSIONS:
        seen: set[str] = set()
        for kind in KINDS:
            diagonal, upper, proof = build_sparse_matrix(dimension, kind)
            matrix, maximum, nonzero = upper_text(diagonal, upper)
            digest = hashlib.sha256(matrix.encode("ascii")).hexdigest()
            assert digest not in seen
            seen.add(digest)
            strict = int(kind == "strict")
            source = f"{prefix}; dimension={dimension}; class={kind}; {proof}"
            yield dimension, matrix, strict, source, FAMILIES[kind], maximum, nonzero


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--database", type=Path, default=Path(__file__).with_name("Copos_testdata.sqlite3"))
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument("--import", dest="do_import", action="store_true", help="insert all checked rows in one transaction")
    modes.add_argument("--verify-existing", action="store_true", help="regenerate and compare the already imported rows exactly")
    args = parser.parse_args()

    connection = sqlite3.connect(args.database)
    connection.execute("PRAGMA foreign_keys = ON")
    next_id = connection.execute("SELECT COALESCE(MAX(matrix_id), 0) + 1 FROM matrices").fetchone()[0]
    count = strict_count = text_bytes = maximum = 0
    ids: list[int] = []

    try:
        with connection:
            for offset, (dimension, matrix, strict, source, family, row_maximum, _) in enumerate(generated_rows()):
                copositive = int(not family.endswith("/ not copositive"))
                count += 1
                strict_count += strict
                text_bytes += len(matrix)
                maximum = max(maximum, row_maximum)
                if args.verify_existing:
                    existing = connection.execute(
                        "SELECT matrix_id, dimension, matrix, is_strictly_copositive, is_copositive, family "
                        "FROM matrices WHERE source = ?",
                        (source,),
                    ).fetchall()
                    if len(existing) != 1 or existing[0][1:] != (dimension, matrix, strict, copositive, family):
                        raise RuntimeError(f"stored row does not exactly match regenerated {source}")
                    ids.append(existing[0][0])
                    continue
                overlap = connection.execute(
                    "SELECT matrix_id FROM matrices WHERE dimension = ? AND matrix = ?", (dimension, matrix)
                ).fetchone()
                if overlap:
                    raise RuntimeError(f"refusing import: generated matrix duplicates ID {overlap[0]}")
                if args.do_import:
                    matrix_id = next_id + offset
                    connection.execute(
                        "INSERT INTO matrices(matrix_id, dimension, matrix, is_strictly_copositive, is_copositive, source, family) "
                        "VALUES (?, ?, ?, ?, ?, ?, ?)",
                        (matrix_id, dimension, matrix, strict, copositive, source, family),
                    )
                    ids.append(matrix_id)
        assert count == len(DIMENSIONS) * 3 == 90
        assert strict_count == len(DIMENSIONS)
        action = "verified" if args.verify_existing else "inserted" if args.do_import else "checked"
        id_text = f" as IDs {min(ids)}-{max(ids)}" if ids else ""
        print(f"{action} {count} rows{id_text}; text_bytes={text_bytes}; max_abs_entry={maximum}")
        if not args.do_import and not args.verify_existing:
            print("pass --import to write them")
    finally:
        connection.close()


if __name__ == "__main__":
    main()
