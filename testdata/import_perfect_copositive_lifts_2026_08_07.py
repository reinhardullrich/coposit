#!/usr/bin/env python3
"""Reproduce the 2026-08-07 Dannenberg--Schürmann lift import."""

from __future__ import annotations

import argparse
import sqlite3
from pathlib import Path

from import_exceptional_matrices_2026_08_07 import primitive_upper_text


I = [[2, -5, 4], [-5, 14, -9], [4, -9, 6]]
E = [
    [366, -300, 197, 147, -81],
    [-300, 246, -161, 123, 69],
    [197, -161, 106, -82, 39],
    [147, 123, -82, 66, -33],
    [-81, 69, 39, -33, 18],
]


def lift(matrix: list[list[int]]) -> list[list[int]]:
    return [row + [row[-1]] for row in matrix] + [matrix[-1] + [matrix[-1][-1]]]


def quadratic(matrix: list[list[int]], vector: list[int]) -> int:
    return sum(vector[i] * matrix[i][j] * vector[j] for i in range(len(vector)) for j in range(len(vector)))


def family_rows(
    seed: list[list[int]], witness: list[int], last_order: int, corollary: str, family: str
) -> list[tuple[int, str, int, str, str]]:
    paper = "Dannenberg-Schürmann 2023, Perfect Copositive Matrices <https://arxiv.org/abs/2303.17310>"
    order = len(seed)
    minimum = quadratic(seed, witness)
    rows = []
    matrix = seed
    while order <= last_order:
        assert witness[-1] >= 2 and quadratic(matrix, witness) == minimum
        lift_count = order - len(seed)
        source = f"{paper}; Lemma 5.1 and {corollary}; seed order {len(seed)}, lift count {lift_count}"
        rows.append((order, primitive_upper_text(matrix), 1, source, family))
        matrix = lift(matrix)
        witness = witness[:-1] + [0, witness[-1]]
        order += 1
    return rows


def build_rows() -> list[tuple[int, str, int, str, str]]:
    rows = family_rows(
        I,
        [0, 1, 2],
        102,
        "Corollary 5.6",
        "strict perfect copositive / Dannenberg-Schürmann SPN lift",
    ) + family_rows(
        E,
        [1, 0, 0, 0, 4],
        104,
        "Corollary 5.7",
        "strict exceptional perfect copositive / Dannenberg-Schürmann lift",
    )
    assert len(rows) == 200
    assert len({(dimension, matrix) for dimension, matrix, *_ in rows}) == 200
    return rows


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--database", type=Path, default=Path(__file__).with_name("Copos_testdata.sqlite3"))
    parser.add_argument("--import", dest="do_import", action="store_true", help="insert the checked rows in one transaction")
    args = parser.parse_args()

    rows = build_rows()
    connection = sqlite3.connect(args.database)
    try:
        existing = {(dimension, matrix) for dimension, matrix in connection.execute("SELECT dimension, matrix FROM matrices")}
        overlaps = [(dimension, matrix) for dimension, matrix, *_ in rows if (dimension, matrix) in existing]
        if overlaps:
            raise RuntimeError(f"refusing import: {len(overlaps)} exact matrix rows already exist")
        if not args.do_import:
            print(f"checked {len(rows)} new exact rows; pass --import to write them")
            return
        next_id = connection.execute("SELECT COALESCE(MAX(matrix_id), 0) + 1 FROM matrices").fetchone()[0]
        with connection:
            connection.executemany(
                "INSERT INTO matrices(matrix_id, dimension, matrix, is_strictly_copositive, is_copositive, source, family) "
                "VALUES (?, ?, ?, ?, 1, ?, ?)",
                ((next_id + offset, *row) for offset, row in enumerate(rows)),
            )
        print(f"inserted {len(rows)} rows as IDs {next_id}-{next_id + len(rows) - 1}")
    finally:
        connection.close()


if __name__ == "__main__":
    main()
