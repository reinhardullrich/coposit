#!/usr/bin/env python3
"""Add entrywise-decimal-rounding variants of two Chen--Burer RandQP Hessians."""

from __future__ import annotations

import argparse
import io
import math
import sqlite3
import zipfile
from decimal import Decimal, ROUND_HALF_UP, localcontext
from functools import reduce
from pathlib import Path

from scipy.io import loadmat


PROJECT = Path(__file__).resolve().parents[2]
ARCHIVE = PROJECT / "research/data/copositive_optimization_matrix_extraction_2026-08-14/upstream/quadprogIP/QuadProgBB_instances.zip"
PARENTS = (
    (12198, "instances/mat/randqp/qp40_20_2_1.mat"),
    (12214, "instances/mat/randqp/qp50_25_2_1.mat"),
)
DECIMAL_PLACES = (1, 2, 3, 4)
FAMILY = "Chen-Burer RandQP / entrywise decimal-rounding experiment"


def primitive_upper_text(matrix: object, places: int) -> tuple[int, str]:
    """Round the archived binary doubles normally, then clear the common decimal denominator."""
    n = int(matrix.shape[0])
    scale = 10**places
    with localcontext() as context:
        context.prec = 80
        rounded = [
            Decimal.from_float(float(matrix[i, j])).quantize(Decimal(1).scaleb(-places), rounding=ROUND_HALF_UP)
            for i in range(n) for j in range(i, n)
        ]
    integers = [int(value * scale) for value in rounded]
    divisor = reduce(math.gcd, map(abs, integers)) or 1
    return n, ",".join(str(value // divisor) for value in integers)


def rows() -> list[tuple[int, str, str, int]]:
    output: list[tuple[int, str, str, int]] = []
    with zipfile.ZipFile(ARCHIVE) as archive:
        for parent_id, member in PARENTS:
            matrix = loadmat(io.BytesIO(archive.read(member)))["H"]
            if matrix.shape[0] != matrix.shape[1] or not (matrix == matrix.T).all():
                raise RuntimeError(f"{member} does not contain a symmetric square H")
            for places in DECIMAL_PLACES:
                dimension, storage = primitive_upper_text(matrix, places)
                source = (
                    f"coposit experiment: entrywise decimal rounding to {places} places, ROUND_HALF_UP; "
                    f"parent matrix_id={parent_id}; parent archive member={member}; "
                    "derived from the stored IEEE-754 H, then scaled to a primitive integer matrix"
                )
                output.append((dimension, storage, source, parent_id))
    return output


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--database", type=Path, default=PROJECT / "testdata/copos_testdata.sqlite3")
    parser.add_argument("--import", dest="do_import", action="store_true", help="insert the eight checked experiment rows")
    parser.add_argument("--verify-existing", action="store_true", help="regenerate and compare the eight existing experiment rows")
    arguments = parser.parse_args()
    if arguments.do_import and arguments.verify_existing:
        parser.error("--import and --verify-existing cannot be combined")

    generated = rows()
    if len(generated) != 8 or len({(dimension, storage) for dimension, storage, _, _ in generated}) != 8:
        raise RuntimeError("rounding did not create eight distinct matrices")

    connection = sqlite3.connect(arguments.database)
    connection.execute("PRAGMA foreign_keys = ON")
    try:
        if arguments.verify_existing:
            for dimension, storage, source, parent_id in generated:
                existing = connection.execute(
                    "SELECT dimension, matrix, source, source_id, family, core_and_stress_test, smoke_set "
                    "FROM matrices WHERE source = ?",
                    (source,),
                ).fetchall()
                expected = (dimension, storage, source, 73, FAMILY, 0, 0)
                if existing != [expected]:
                    raise RuntimeError(f"stored rounding variant differs from regenerated parent {parent_id}")
            print("verified 8 Chen-Burer decimal-rounding experiment rows")
            return

        existing_sources = connection.execute(
            "SELECT count(*) FROM matrices WHERE family = ?", (FAMILY,)
        ).fetchone()[0]
        if existing_sources:
            raise RuntimeError(f"refusing import: {existing_sources} rows already use experiment family {FAMILY!r}")
        duplicates = [
            connection.execute("SELECT matrix_id FROM matrices WHERE dimension = ? AND matrix = ?", (dimension, storage)).fetchone()
            for dimension, storage, _, _ in generated
        ]
        if any(duplicates):
            raise RuntimeError("refusing import: a rounded variant duplicates an existing matrix")
        if not arguments.do_import:
            print("checked 8 new Chen-Burer decimal-rounding experiment rows; pass --import to write them")
            return

        first_id = connection.execute("SELECT COALESCE(MAX(matrix_id), 0) + 1 FROM matrices").fetchone()[0]
        with connection:
            for offset, (dimension, storage, source, _) in enumerate(generated):
                connection.execute(
                    "INSERT INTO matrices(matrix_id, dimension, matrix, source, source_id, family) VALUES (?, ?, ?, ?, 73, ?)",
                    (first_id + offset, dimension, storage, source, FAMILY),
                )
        print(f"inserted 8 Chen-Burer decimal-rounding experiment rows as IDs {first_id}-{first_id + 7}")
    finally:
        connection.close()


if __name__ == "__main__":
    main()
