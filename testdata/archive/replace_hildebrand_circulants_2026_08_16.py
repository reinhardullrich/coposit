#!/usr/bin/env python3
"""Replace the large-digit order-15-25 Hildebrand panel with one varied representative per order."""

from __future__ import annotations

import argparse
from fractions import Fraction
import hashlib
import os
from pathlib import Path
import sqlite3

from import_exceptional_matrices_2026_08_07 import primitive_upper_text
from import_hard_literature_matrices_2026_08_07 import hildebrand_circulant


OLD_MATRIX_IDS = tuple(range(10288, 10305))
CORE_REFILL_IDS = (9230, 9239, 9990, 9224, 9227, 10042)
FAMILY = "exceptional boundary / Hildebrand circulant support n-2"
EXTERNAL_THRESHOLD = 500_000

# Search grid: reduced positive fractions with denominator <= 30, 1/15 < theta < 1/2, and 1/40 < alpha < 1/5.
# These choices stay near its low-digit frontier while deliberately varying theta and alpha.
REPRESENTATIVES = (
    (13024, 15, Fraction(3, 14), Fraction(1, 9), 294, 35_354),
    (13025, 16, Fraction(1, 5), Fraction(1, 9), 447, 60_943),
    (13026, 17, Fraction(4, 17), Fraction(1, 11), 530, 81_140),
    (13027, 18, Fraction(1, 4), Fraction(1, 13), 585, 100_142),
    (13028, 19, Fraction(2, 11), Fraction(1, 12), 926, 176_015),
    (13029, 20, Fraction(2, 9), Fraction(1, 13), 1_130, 237_409),
    (13030, 21, Fraction(2, 13), Fraction(1, 13), 1_716, 396_458),
    (13031, 22, Fraction(1, 5), Fraction(1, 15), 1_807, 457_302),
    (13032, 23, Fraction(3, 17), Fraction(1, 15), 2_500, 690_045),
    (13033, 24, Fraction(1, 7), Fraction(1, 15), 2_403, 721_103),
    (13034, 25, Fraction(2, 11), Fraction(1, 17), 2_423, 787_499),
)

INITIAL_COUNTS = (3519, 872, 1337, 845, 465, 49, 512, 1168, 17, 1256)
FINAL_COUNTS = (3513, 872, 1331, 845, 465, 49, 512, 1162, 17, 1250)


def corpus_counts(connection: sqlite3.Connection) -> tuple[int, ...]:
    return tuple(connection.execute("""
        SELECT count(*),
               count(*) FILTER (WHERE is_strictly_copositive = 1),
               count(*) FILTER (WHERE is_copositive = 1 AND is_strictly_copositive = 0),
               count(*) FILTER (WHERE is_copositive = 0),
               count(*) FILTER (WHERE is_copositive IS NULL),
               count(*) FILTER (WHERE smoke_set),
               count(*) FILTER (WHERE core_and_stress_test),
               count(*) FILTER (WHERE n_le_100),
               count(*) FILTER (WHERE n_gt_100_solved),
               count(*) FILTER (
                   WHERE smoke_set OR core_and_stress_test OR n_le_100 OR n_gt_100_solved
               )
        FROM matrices
    """).fetchone())


def build_replacements() -> list[dict[str, object]]:
    rows = []
    seen = set()
    for matrix_id, order, theta, alpha, expected_digits, expected_length in REPRESENTATIVES:
        matrix, _zero = hildebrand_circulant(order, theta, alpha)
        packed = primitive_upper_text(matrix)
        maximum_digits = max(len(token.lstrip("-")) for token in packed.split(","))
        if (maximum_digits, len(packed)) != (expected_digits, expected_length):
            raise RuntimeError(
                f"representative {matrix_id} changed: {(maximum_digits, len(packed))} "
                f"!= {(expected_digits, expected_length)}"
            )
        if packed in seen:
            raise RuntimeError(f"duplicate generated representative {matrix_id}")
        seen.add(packed)
        theorem = "Theorem 8 and Lemma 25" if order % 2 == 0 else "Theorem 9 and Lemma 26"
        source = (
            f"Hildebrand 2017, Copositive matrices with circulant zero support set, {theorem} "
            "<https://arxiv.org/abs/1603.05111>; "
            f"n={order}, tan(theta/4)={theta}, tan(alpha/4)={alpha}; "
            "2026-08-16 low-digit diversified exact representative"
        )
        rows.append({
            "matrix_id": matrix_id,
            "order": order,
            "theta": theta,
            "alpha": alpha,
            "packed": packed,
            "maximum_digits": maximum_digits,
            "source": source,
        })
    return rows


def write_external_matrix(database: Path, row: dict[str, object]) -> tuple[str, str | None, Path | None]:
    packed = str(row["packed"])
    if len(packed) <= EXTERNAL_THRESHOLD:
        return packed, None, None

    matrix_id = int(row["matrix_id"])
    order = int(row["order"])
    target = database.parent / "matrices" / f"{matrix_id}.mtx"
    if target.exists():
        raise FileExistsError(target)
    temporary = target.with_suffix(".mtx.tmp")
    try:
        with temporary.open("w", encoding="ascii", newline="\n") as output:
            output.write("%%MatrixMarket matrix array integer symmetric\n")
            output.write(f"{order} {order}\n")
            output.write(packed.replace(",", "\n"))
            output.write("\n")
        os.replace(temporary, target)
    finally:
        temporary.unlink(missing_ok=True)
    with target.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    return f"file:matrices/{matrix_id}.mtx", digest, target


def main() -> None:
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--database", type=Path, default=root / "testdata/copos_testdata.sqlite3")
    parser.add_argument("--diagnostics", type=Path, default=root / "experiments/diagnostics.sqlite3")
    parser.add_argument("--apply", action="store_true")
    arguments = parser.parse_args()

    database = arguments.database.resolve()
    diagnostics = arguments.diagnostics.resolve()
    replacements = build_replacements()
    connection = sqlite3.connect(database)
    connection.execute("PRAGMA foreign_keys = ON")
    diagnostics_attached = diagnostics.is_file()
    if diagnostics_attached:
        connection.execute("ATTACH DATABASE ? AS diagnostics", (str(diagnostics),))

    created_files: list[Path] = []
    committed = False
    try:
        old_rows = connection.execute(
            f"SELECT matrix_id,dimension,matrix,source_id,family,is_copositive,is_strictly_copositive,"
            f"smoke_set,core_and_stress_test,preprocessing_solved,additional_source_ids,references_solved,references_unsolved "
            f"FROM matrices WHERE matrix_id IN ({','.join('?' for _ in OLD_MATRIX_IDS)}) ORDER BY matrix_id",
            OLD_MATRIX_IDS,
        ).fetchall()
        new_ids = tuple(int(row["matrix_id"]) for row in replacements)
        existing_new = connection.execute(
            f"SELECT matrix_id FROM matrices WHERE matrix_id IN ({','.join('?' for _ in new_ids)}) ORDER BY matrix_id",
            new_ids,
        ).fetchall()
        if not old_rows and tuple(row[0] for row in existing_new) == new_ids:
            if corpus_counts(connection) != FINAL_COUNTS:
                raise RuntimeError(f"replacement IDs exist in an unexpected corpus state: {corpus_counts(connection)}")
            print("already_applied=1")
            return
        if existing_new:
            raise RuntimeError(f"replacement matrix IDs already exist: {[row[0] for row in existing_new]}")
        if corpus_counts(connection) != INITIAL_COUNTS:
            raise RuntimeError(f"unexpected initial corpus counts: {corpus_counts(connection)}")
        if tuple(row[0] for row in old_rows) != OLD_MATRIX_IDS:
            raise RuntimeError("the old Hildebrand panel is incomplete")

        expected_orders = (15, 16, 16, 16, 16, 17, 18, 18, 18, 19, 20, 20, 21, 22, 23, 24, 25)
        if tuple(row[1] for row in old_rows) != expected_orders:
            raise RuntimeError("the old Hildebrand order panel changed")
        if any(
            row[3] != 20 or row[4] != FAMILY or row[5:10] != (1, 0, 0, 1, 0)
            or row[10:] != ("[]", "[]", "[]")
            for row in old_rows
        ):
            raise RuntimeError("the old Hildebrand identity or benchmark membership changed")
        refill_rows = connection.execute(
            f"SELECT matrix_id,dimension,is_copositive,is_strictly_copositive,smoke_set,core_and_stress_test,preprocessing_solved "
            f"FROM matrices WHERE matrix_id IN ({','.join('?' for _ in CORE_REFILL_IDS)}) ORDER BY matrix_id",
            CORE_REFILL_IDS,
        ).fetchall()
        if tuple(row[0] for row in refill_rows) != tuple(sorted(CORE_REFILL_IDS)) or any(
            row[2:] != (1, 0, 0, 0, 0) for row in refill_rows
        ):
            raise RuntimeError("Core refill candidates changed")

        old_files = []
        for storage in (row[2] for row in old_rows if row[2].startswith("file:")):
            relative = Path(storage.removeprefix("file:"))
            if relative.is_absolute() or ".." in relative.parts:
                raise RuntimeError(f"unsafe old matrix path: {relative}")
            old_files.append(database.parent / relative)
        if len(old_files) != 4 or not all(path.is_file() for path in old_files):
            raise RuntimeError("expected four external old Hildebrand matrices")

        print("order matrix_id max_digits theta alpha storage")
        for row in replacements:
            storage = "external" if len(str(row["packed"])) > EXTERNAL_THRESHOLD else "inline"
            print(
                f"{row['order']} {row['matrix_id']} {row['maximum_digits']} "
                f"{row['theta']} {row['alpha']} {storage}"
            )
        if not arguments.apply:
            print("dry_run=1")
            return

        stored_replacements = []
        for row in replacements:
            storage, digest, created = write_external_matrix(database, row)
            if created is not None:
                created_files.append(created)
            stored_replacements.append((row, storage, digest))

        old_placeholders = ",".join("?" for _ in OLD_MATRIX_IDS)
        removed_results = 0
        removed_preprocessing_results = 0
        connection.execute("BEGIN IMMEDIATE")
        try:
            if diagnostics_attached:
                removed_results = connection.execute(
                    f"SELECT count(*) FROM diagnostics.results WHERE matrix_id IN ({old_placeholders})", OLD_MATRIX_IDS
                ).fetchone()[0]
                removed_preprocessing_results = connection.execute(
                    f"SELECT count(*) FROM diagnostics.preprocessing_results WHERE matrix_id IN ({old_placeholders})", OLD_MATRIX_IDS
                ).fetchone()[0]
                connection.execute(f"DELETE FROM diagnostics.results WHERE matrix_id IN ({old_placeholders})", OLD_MATRIX_IDS)
                connection.execute(
                    f"DELETE FROM diagnostics.preprocessing_results WHERE matrix_id IN ({old_placeholders})", OLD_MATRIX_IDS
                )
            connection.execute(f"DELETE FROM matrices WHERE matrix_id IN ({old_placeholders})", OLD_MATRIX_IDS)
            connection.executemany(
                "UPDATE matrices SET core_and_stress_test=1 WHERE matrix_id=?",
                ((matrix_id,) for matrix_id in CORE_REFILL_IDS),
            )
            connection.executemany(
                """
                INSERT INTO matrices(
                    matrix_id,dimension,matrix,file_sha256,is_strictly_copositive,is_copositive,source,source_id,family,
                    smoke_set,core_and_stress_test,preprocessing_solved,additional_source_ids,references_solved,references_unsolved,
                    fastest_elapsed_ns,fastest_result_ref
                ) VALUES (?, ?, ?, ?, 0, 1, ?, 20, ?, 0, 1, 0, '[]', '[]', '[]', NULL, NULL)
                """,
                (
                    (row["matrix_id"], row["order"], storage, digest, row["source"], FAMILY)
                    for row, storage, digest in stored_replacements
                ),
            )
            if corpus_counts(connection) != FINAL_COUNTS:
                raise RuntimeError(f"wrong final corpus counts: {corpus_counts(connection)}")
            core_split = connection.execute("""
                SELECT count(*) FILTER (WHERE is_strictly_copositive = 1),
                       count(*) FILTER (WHERE is_copositive = 1 AND is_strictly_copositive = 0),
                       count(*) FILTER (WHERE is_copositive = 0),
                       count(*) FILTER (WHERE is_copositive IS NULL)
                FROM matrices WHERE core_and_stress_test
            """).fetchone()
            if core_split != (256, 175, 81, 0):
                raise RuntimeError(f"wrong Core composition: {core_split}")
            if connection.execute("PRAGMA integrity_check").fetchone()[0] != "ok":
                raise RuntimeError("corpus integrity check failed")
            if list(connection.execute("PRAGMA foreign_key_check")):
                raise RuntimeError("foreign-key check failed")
            connection.commit()
            committed = True
        except BaseException:
            connection.rollback()
            raise

        for path in old_files:
            path.unlink()
        if diagnostics_attached and connection.execute("PRAGMA diagnostics.integrity_check").fetchone()[0] != "ok":
            raise RuntimeError("diagnostics integrity check failed")
        print(
            f"removed_matrices={len(old_rows)} inserted_matrices={len(replacements)} "
            f"core_refills={len(CORE_REFILL_IDS)} removed_files={len(old_files)} created_files={len(created_files)} "
            f"removed_results={removed_results} removed_preprocessing_results={removed_preprocessing_results}"
        )
    finally:
        connection.close()
        if not committed:
            for path in created_files:
                path.unlink(missing_ok=True)


if __name__ == "__main__":
    main()
