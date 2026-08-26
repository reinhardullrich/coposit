#!/usr/bin/env python3
"""Generate and optionally import 80 exact Peng-style SPN_n + B_n matrices."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import random
import sqlite3

import numpy as np


ORDERS = (50, 100, 150, 200)
SAMPLES = range(1, 21)
H_MINIMUM = 0.14
NONNEGATIVE_EDGE_PROBABILITY = 0.05
FAMILY_PREFIX = "Peng-style SPN+B_n C5-blow-up sample"


def generate_matrix(order: int, sample: int) -> tuple[str, str, float, tuple[int, int]]:
    rng = random.Random(2_022_000_000 + 10_000 * order + sample)
    for attempt in range(1, 101):
        cuts = sorted(rng.sample(range(1, order), 4))
        part_sizes = tuple(right - left for left, right in zip((0, *cuts), (*cuts, order)))
        parts = [part for part, size in enumerate(part_sizes) for _ in range(size)]
        rng.shuffle(parts)
        signs = [rng.choice((-1, 1)) for _ in range(order)]

        upper: list[int] = []
        zero_support: tuple[int, int] | None = None
        matrix = np.empty((order, order), dtype=np.float64)
        for row in range(order):
            for column in range(row, order):
                if row == column:
                    value = 2  # B_ii=1 and (vv^T)_ii=1.
                else:
                    cycle_distance = (parts[row] - parts[column]) % 5
                    b_entry = -1 if cycle_distance in (1, 4) else 1
                    nonnegative_entry = int(b_entry == 1 and rng.random() < NONNEGATIVE_EDGE_PROBABILITY)
                    value = b_entry + signs[row] * signs[column] + nonnegative_entry
                    if b_entry == -1 and signs[row] != signs[column] and zero_support is None:
                        zero_support = (row, column)
                upper.append(value)
                matrix[row, column] = value
                matrix[column, row] = value

        if zero_support is None:
            continue
        row, column = zero_support
        if matrix[row, row] + matrix[column, column] + 2 * matrix[row, column] != 0:
            raise RuntimeError("constructed zero support failed its exact quadratic check")

        eigenvalues = np.linalg.eigvalsh(matrix)
        positive_sum = eigenvalues[eigenvalues > 0.0].sum()
        observed_h = -eigenvalues[eigenvalues <= 0.0].sum() / positive_sum
        if observed_h < H_MINIMUM:
            continue

        construction = (
            f"sample={sample}; attempt={attempt}; part_sizes={','.join(map(str, part_sizes))}; "
            f"zero_support={row + 1},{column + 1}"
        )
        return ",".join(map(str, upper)), construction, float(observed_h), zero_support
    raise RuntimeError(f"could not generate order {order}, sample {sample} with h(Q) >= {H_MINIMUM}")


def build_rows() -> list[tuple[int, str, int, int, str, str]]:
    rows = []
    for order in ORDERS:
        family = f"{FAMILY_PREFIX}; n={order}"
        for sample in SAMPLES:
            matrix, construction, observed_h, _zero_support = generate_matrix(order, sample)
            source = (
                "Local exact sample from Peng 2022 Section 3.2 SPN_n+B_n family; "
                "B_n uses a random five-part blow-up of C5 in the Hoffman-Pereira construction; "
                "the SPN summand is vv^T plus a seeded symmetric nonnegative 0/1 matrix supported on positive B_n entries; "
                f"n={order}; {construction}; nonnegative_probability={NONNEGATIVE_EDGE_PROBABILITY}; "
                f"observed_h={observed_h:.12g}; these are not Peng's unpublished instances"
            )
            rows.append((order, matrix, 0, 1, source, family))

    expected = len(ORDERS) * len(SAMPLES)
    if len(rows) != expected or len({(order, matrix) for order, matrix, *_ in rows}) != expected:
        raise RuntimeError(f"the sample must contain {expected} distinct matrices")
    return rows


def rows_sha256(rows: list[tuple[int, str, int, int, str, str]]) -> str:
    digest = hashlib.sha256()
    for row in rows:
        digest.update("\0".join(map(str, row)).encode())
        digest.update(b"\n")
    return digest.hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--database", type=Path, default=Path(__file__).resolve().parents[1] / "copos_testdata.sqlite3")
    parser.add_argument("--import", dest="do_import", action="store_true", help="insert the checked rows in one transaction")
    args = parser.parse_args()

    rows = build_rows()
    digest = rows_sha256(rows)
    if not args.do_import:
        print(f"checked {len(rows)} distinct rows; sha256={digest}; pass --import to write them")
        return

    connection = sqlite3.connect(args.database)
    try:
        connection.execute("PRAGMA foreign_keys = ON")
        source = connection.execute("SELECT source_id,title FROM sources WHERE source_id=45").fetchone()
        if source != (45, "Performance comparison of two recently proposed copositivity tests"):
            raise RuntimeError(f"unexpected Peng source record: {source!r}")

        existing_rows = connection.execute(
            "SELECT matrix_id,dimension,matrix,is_strictly_copositive,is_copositive,source,family "
            "FROM matrices WHERE family LIKE ? ORDER BY matrix_id",
            (f"{FAMILY_PREFIX}%",),
        ).fetchall()
        if existing_rows:
            if [row[1:] for row in existing_rows] != rows:
                raise RuntimeError(f"refusing import: found {len(existing_rows)} nonmatching existing sample rows")
            print(f"already present as IDs {existing_rows[0][0]}-{existing_rows[-1][0]}; sha256={digest}")
            return

        existing_matrices = {(order, matrix) for order, matrix in connection.execute("SELECT dimension,matrix FROM matrices")}
        overlaps = [(order, matrix) for order, matrix, *_ in rows if (order, matrix) in existing_matrices]
        if overlaps:
            raise RuntimeError(f"refusing import: {len(overlaps)} generated matrices already exist elsewhere")

        next_id = connection.execute("SELECT COALESCE(MAX(matrix_id),0)+1 FROM matrices").fetchone()[0]
        with connection:
            connection.executemany(
                """INSERT INTO matrices(
                       matrix_id,dimension,matrix,is_strictly_copositive,is_copositive,source,source_id,family
                   ) VALUES (?,?,?,?,?,?,45,?)""",
                ((next_id + offset, *row) for offset, row in enumerate(rows)),
            )
        print(f"inserted {len(rows)} rows as IDs {next_id}-{next_id + len(rows) - 1}; sha256={digest}")
    finally:
        connection.close()


if __name__ == "__main__":
    main()
