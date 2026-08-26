#!/usr/bin/env python3
"""Generate and optionally import the 300-matrix Peng-inspired A_c,50 pilot."""

from __future__ import annotations

import argparse
from functools import reduce
import hashlib
from math import gcd
from pathlib import Path
import sqlite3

import numpy as np


N = 50
C_VALUES = ((1, 100), (1, 50))
NEGATIVE_INERTIAS = (1, 5, 25)
SEEDS = range(1, 51)
DECIMAL_SCALE = 100_000_000
FAMILY_PREFIX = "Peng-inspired A_c,n spectral pilot"


def generate_matrix(c_numerator: int, c_denominator: int, negative_inertia: int, seed: int) -> tuple[str, float]:
    rng = np.random.default_rng(np.random.SeedSequence([2022, N, c_numerator, c_denominator, negative_inertia, seed]))
    gaussian = rng.standard_normal((N, N))
    orthogonal, triangular = np.linalg.qr(gaussian)
    orthogonal *= np.where(np.diag(triangular) < 0.0, -1.0, 1.0)

    positive = rng.uniform(0.5, 1.5, N - negative_inertia)
    negative = rng.uniform(0.5, 1.5, negative_inertia)
    negative *= (c_numerator / c_denominator) * positive.sum() / negative.sum()
    eigenvalues = np.concatenate((-negative, positive))
    rng.shuffle(eigenvalues)

    matrix = orthogonal.T @ (eigenvalues[:, None] * orthogonal)
    matrix = (matrix + matrix.T) * 0.5
    rounded = np.rint(matrix * DECIMAL_SCALE).astype(np.int64)
    rounded = np.triu(rounded) + np.triu(rounded, 1).T

    upper = [int(rounded[i, j]) for i in range(N) for j in range(i, N)]
    divisor = reduce(gcd, map(abs, upper))
    upper = [value // divisor for value in upper]

    stored = np.array(rounded, dtype=np.float64) / divisor
    stored_eigenvalues = np.linalg.eigvalsh(stored)
    observed_h = -stored_eigenvalues[stored_eigenvalues <= 0.0].sum() / stored_eigenvalues[stored_eigenvalues > 0.0].sum()
    if abs(observed_h - c_numerator / c_denominator) > 1e-7:
        raise RuntimeError(f"seed {seed}: rationalization changed h from {c_numerator / c_denominator} to {observed_h}")
    return ",".join(map(str, upper)), observed_h


def build_rows() -> list[tuple[int, str, str, str]]:
    rows = []
    for c_numerator, c_denominator in C_VALUES:
        c_label = f"{c_numerator / c_denominator:.2f}"
        for negative_inertia in NEGATIVE_INERTIAS:
            family = f"{FAMILY_PREFIX}; n={N}; c={c_label}; negative_inertia={negative_inertia}"
            for seed in SEEDS:
                matrix, observed_h = generate_matrix(c_numerator, c_denominator, negative_inertia, seed)
                source = (
                    "Local exact rationalization of Peng 2022 Section 3.1 A_c,n construction; "
                    f"n={N}; c={c_label}; negative_inertia={negative_inertia}; seed={seed}; "
                    "orthogonal matrix from NumPy PCG64 Gaussian QR with positive R diagonal; "
                    "positive and unscaled negative eigenvalue magnitudes sampled uniformly from [0.5,1.5); "
                    f"negative spectrum rescaled to h(Q)=c; entries rounded to 1/{DECIMAL_SCALE} then reduced by positive common scale; "
                    f"observed_h={observed_h:.12g}; NumPy {np.__version__}; these are not Peng's unpublished instances"
                )
                rows.append((N, matrix, source, family))
    if len(rows) != 300 or len({matrix for _, matrix, _, _ in rows}) != 300:
        raise RuntimeError("the pilot must contain 300 distinct matrices")
    return rows


def rows_sha256(rows: list[tuple[int, str, str, str]]) -> str:
    digest = hashlib.sha256()
    for dimension, matrix, source, family in rows:
        digest.update(f"{dimension}\0{matrix}\0{source}\0{family}\n".encode())
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
        source = connection.execute(
            "SELECT source_id,title FROM sources WHERE source_id=45"
        ).fetchone()
        if source != (45, "Performance comparison of two recently proposed copositivity tests"):
            raise RuntimeError(f"unexpected Peng source record: {source!r}")

        existing_rows = connection.execute(
            "SELECT matrix_id,matrix FROM matrices WHERE family LIKE ? ORDER BY matrix_id",
            (f"{FAMILY_PREFIX}%",),
        ).fetchall()
        if existing_rows:
            if [matrix for _, matrix in existing_rows] != [matrix for _, matrix, _, _ in rows]:
                raise RuntimeError(f"refusing import: found {len(existing_rows)} nonmatching existing pilot rows")
            print(f"already present as IDs {existing_rows[0][0]}-{existing_rows[-1][0]}; sha256={digest}")
            return

        existing_matrices = {matrix for (matrix,) in connection.execute("SELECT matrix FROM matrices")}
        overlaps = [matrix for _, matrix, _, _ in rows if matrix in existing_matrices]
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
