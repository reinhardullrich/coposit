#!/usr/bin/env python3
"""Generate and optionally import the 960-matrix variable-order Peng A_c,n extension."""

from __future__ import annotations

import argparse
from functools import reduce
import hashlib
from math import gcd
from pathlib import Path
import sqlite3

import numpy as np


ORDERS = range(10, 50, 5)
C_VALUES = ((1, 100), (1, 50))
SEEDS = range(1, 21)
DECIMAL_SCALE = 100_000_000
FAMILY_PREFIX = "Peng-inspired variable-order A_c,n spectral extension"


def negative_inertias(order: int) -> tuple[int, int, int]:
    return 1, max(2, (order + 5) // 10), order // 2


def generate_matrix(order: int, c_numerator: int, c_denominator: int, negative_inertia: int, seed: int) -> tuple[str, float]:
    rng = np.random.default_rng(
        np.random.SeedSequence([2022, order, c_numerator, c_denominator, negative_inertia, seed])
    )
    gaussian = rng.standard_normal((order, order))
    orthogonal, triangular = np.linalg.qr(gaussian)
    orthogonal *= np.where(np.diag(triangular) < 0.0, -1.0, 1.0)

    positive = rng.uniform(0.5, 1.5, order - negative_inertia)
    negative = rng.uniform(0.5, 1.5, negative_inertia)
    negative *= (c_numerator / c_denominator) * positive.sum() / negative.sum()
    eigenvalues = np.concatenate((-negative, positive))
    rng.shuffle(eigenvalues)

    matrix = orthogonal.T @ (eigenvalues[:, None] * orthogonal)
    matrix = (matrix + matrix.T) * 0.5
    rounded = np.rint(matrix * DECIMAL_SCALE).astype(np.int64)
    rounded = np.triu(rounded) + np.triu(rounded, 1).T

    upper = [int(rounded[row, column]) for row in range(order) for column in range(row, order)]
    divisor = reduce(gcd, map(abs, upper))
    upper = [value // divisor for value in upper]

    stored_eigenvalues = np.linalg.eigvalsh(np.array(rounded, dtype=np.float64) / divisor)
    if np.count_nonzero(stored_eigenvalues < 0.0) != negative_inertia or np.any(stored_eigenvalues == 0.0):
        raise RuntimeError(f"order {order}, inertia {negative_inertia}, seed {seed}: rationalization changed the intended inertia")
    observed_h = -stored_eigenvalues[stored_eigenvalues <= 0.0].sum() / stored_eigenvalues[stored_eigenvalues > 0.0].sum()
    if abs(observed_h - c_numerator / c_denominator) > 1e-7:
        raise RuntimeError(
            f"order {order}, inertia {negative_inertia}, seed {seed}: rationalization changed h "
            f"from {c_numerator / c_denominator} to {observed_h}"
        )
    return ",".join(map(str, upper)), observed_h


def build_rows() -> list[tuple[int, str, str, str]]:
    rows = []
    for order in ORDERS:
        for c_numerator, c_denominator in C_VALUES:
            c_label = f"{c_numerator / c_denominator:.2f}"
            for negative_inertia in negative_inertias(order):
                family = f"{FAMILY_PREFIX}; n={order}; c={c_label}; negative_inertia={negative_inertia}"
                for seed in SEEDS:
                    matrix, observed_h = generate_matrix(order, c_numerator, c_denominator, negative_inertia, seed)
                    source = (
                        "Local exact variable-order extension of Peng 2022 Section 3.1 A_c,n construction; "
                        f"n={order}; c={c_label}; negative_inertia={negative_inertia}; seed={seed}; "
                        "orthogonal matrix from NumPy PCG64 Gaussian QR with positive R diagonal; "
                        "positive and unscaled negative eigenvalue magnitudes sampled uniformly from [0.5,1.5); "
                        f"negative spectrum rescaled to h(Q)=c; entries rounded to 1/{DECIMAL_SCALE} then reduced by positive common scale; "
                        f"observed_h={observed_h:.12g}; NumPy {np.__version__}; these are not Peng's unpublished instances"
                    )
                    rows.append((order, matrix, source, family))
    expected = len(ORDERS) * len(C_VALUES) * 3 * len(SEEDS)
    if len(rows) != expected or len({(order, matrix) for order, matrix, _, _ in rows}) != expected:
        raise RuntimeError(f"the extension must contain {expected} distinct matrices")
    return rows


def rows_sha256(rows: list[tuple[int, str, str, str]]) -> str:
    digest = hashlib.sha256()
    for order, matrix, source, family in rows:
        digest.update(f"{order}\0{matrix}\0{source}\0{family}\n".encode())
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
            "SELECT matrix_id,dimension,matrix,source,family FROM matrices WHERE family LIKE ? ORDER BY matrix_id",
            (f"{FAMILY_PREFIX}%",),
        ).fetchall()
        if existing_rows:
            if [(order, matrix, source_text, family) for _, order, matrix, source_text, family in existing_rows] != rows:
                raise RuntimeError(f"refusing import: found {len(existing_rows)} nonmatching existing extension rows")
            print(f"already present as IDs {existing_rows[0][0]}-{existing_rows[-1][0]}; sha256={digest}")
            return

        existing_matrices = {(order, matrix) for order, matrix in connection.execute("SELECT dimension,matrix FROM matrices")}
        overlaps = [(order, matrix) for order, matrix, _, _ in rows if (order, matrix) in existing_matrices]
        if overlaps:
            raise RuntimeError(f"refusing import: {len(overlaps)} generated matrices already exist elsewhere")

        next_id = connection.execute("SELECT COALESCE(MAX(matrix_id),0)+1 FROM matrices").fetchone()[0]
        with connection:
            connection.executemany(
                """INSERT INTO matrices(
                       matrix_id,dimension,matrix,is_strictly_copositive,is_copositive,source,source_id,family
                   ) VALUES (?,?,?,NULL,NULL,?,45,?)""",
                ((next_id + offset, *row) for offset, row in enumerate(rows)),
            )
        print(f"inserted {len(rows)} rows as IDs {next_id}-{next_id + len(rows) - 1}; sha256={digest}")
    finally:
        connection.close()


if __name__ == "__main__":
    main()
