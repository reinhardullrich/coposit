#!/usr/bin/env python3
"""Add one exact low-height independent-angle Hildebrand circulant for each order 15 through 30."""

from __future__ import annotations

import argparse
from fractions import Fraction
from pathlib import Path
import sqlite3

from import_exceptional_matrices_2026_08_07 import primitive_upper_text
from import_hard_literature_matrices_2026_08_07 import angle_components, complex_power, is_zero


FAMILY = "exceptional boundary / Hildebrand independent-angle circulant support n-2"
SOURCE_MARKER = "2026-08-19 exact independent-angle low-height representative"
FIRST_MATRIX_ID = 13795
EXTERNAL_THRESHOLD = 500_000

# For each order, these are independently selected rational values tan(zeta_j/4), followed by the maximum number of decimal digits
# in one entry of the primitive integer matrix. The search first tested the theorem's equally spaced target angles with every
# Fraction.limit_denominator bound from 5 through 100. It then used deterministic 20- and 30-second low-denominator refinements per
# order, plus a separate 60-second refinement for order 19. These are the best candidates found within that bounded search, not
# proved global minima.
CANDIDATES = (
    (15, "1/4,1/3,1/2,3/5,3/4,1", 52),
    (16, "1/5,1/3,1/2,2/3,3/4,5/6", 63),
    (17, "1/5,1/3,5/12,1/2,2/3,4/5,1", 84),
    (18, "1/5,1/3,7/17,1/2,2/3,3/4,6/7", 86),
    (19, "1/5,1/3,5/12,1/2,3/5,3/4,4/5,1", 118),
    (20, "1/5,1/4,1/3,1/2,3/5,2/3,3/4,8/9", 99),
    (21, "1/5,1/4,1/3,5/12,1/2,2/3,3/4,4/5,1", 133),
    (22, "1/5,1/4,1/3,3/7,1/2,3/5,2/3,3/4,20/21", 132),
    (23, "1/5,1/4,1/3,5/12,1/2,3/5,2/3,3/4,5/6,1", 174),
    (24, "1/7,1/5,1/3,3/8,5/11,1/2,2/3,3/4,7/9,8/9", 167),
    (25, "1/7,1/5,7/24,1/3,5/12,1/2,3/5,2/3,3/4,5/6,1", 193),
    (26, "1/7,1/5,3/11,1/3,3/7,1/2,4/7,2/3,3/4,4/5,20/21", 207),
    (27, "1/7,1/5,1/4,1/3,5/12,1/2,8/15,3/5,2/3,3/4,11/13,1", 240),
    (28, "1/7,1/5,1/4,1/3,2/5,3/7,1/2,3/5,2/3,3/4,4/5,20/21", 208),
    (29, "1/7,1/5,1/4,7/24,3/8,5/12,1/2,3/5,2/3,3/4,4/5,6/7,1", 360),
    (30, "1/8,1/5,2/9,3/11,1/3,2/5,1/2,4/7,7/11,2/3,7/9,5/6,20/21", 306),
)


def construct(order: int, tangents: tuple[Fraction, ...]) -> tuple[list[list[Fraction]], list[Fraction]]:
    expected_count = (order - 3) // 2
    if len(tangents) != expected_count or not all(Fraction(0) < value <= 1 for value in tangents):
        raise ValueError(f"order {order}: invalid angle count or range")
    if order % 2 == 0 and tangents[-1] == 1:
        raise ValueError(f"order {order}: even construction requires zeta_m < pi")

    half_angles = [angle_components((tangent,), (0,)) for tangent in tangents]
    cosines = [cosine * cosine - sine * sine for cosine, sine in half_angles]
    if not all(cosines[index] > cosines[index + 1] for index in range(len(cosines) - 1)):
        raise ValueError(f"order {order}: angles are not strictly increasing")

    sine_multiples = [complex_power(cosine, sine, order)[1] for cosine, sine in half_angles]
    if any(value == 0 for value in sine_multiples) or not all(
        (value > 0) == (index % 2 == 0) for index, value in enumerate(sine_multiples)
    ):
        raise ValueError(f"order {order}: Hildebrand alternating-angle condition failed")

    polynomial = [Fraction(1)]
    for cosine in cosines:
        factor = (Fraction(1), -2 * cosine, Fraction(1))
        polynomial = [
            sum(
                (polynomial[index - offset] * factor[offset] for offset in range(3) if 0 <= index - offset < len(polynomial)),
                Fraction(0),
            )
            for index in range(len(polynomial) + 2)
        ]
    if order % 2 == 0:
        polynomial = [
            sum((polynomial[index - offset] for offset in range(2) if 0 <= index - offset < len(polynomial)), Fraction(0))
            for index in range(len(polynomial) + 1)
        ]
    if len(polynomial) != order - 2 or not all(coefficient > 0 for coefficient in polynomial):
        raise ValueError(f"order {order}: Hildebrand zero polynomial is not positive")

    first_half = []
    for distance in range(order // 2 + 1):
        value = Fraction(0)
        for index, ((half_cosine, half_sine), cosine) in enumerate(zip(half_angles, cosines)):
            numerator = complex_power(cosine, 2 * half_cosine * half_sine, distance)[0]
            difference_product = Fraction(1)
            for other_index, other_cosine in enumerate(cosines):
                if index != other_index:
                    difference_product *= cosine - other_cosine
            first_sine = half_sine if order % 2 else 2 * half_sine * half_cosine
            value += numerator / (first_sine * sine_multiples[index] * difference_product)
        first_half.append(value)

    matrix = [
        [first_half[min((column - row) % order, (row - column) % order)] for column in range(order)]
        for row in range(order)
    ]
    zero = polynomial + [Fraction(0), Fraction(0)]
    if not is_zero(matrix, zero):
        raise ValueError(f"order {order}: exact Hildebrand zero verification failed")
    return matrix, zero


def build_rows() -> list[tuple[int, int, str, str]]:
    rows = []
    seen = set()
    for offset, (order, tangent_text, expected_digits) in enumerate(CANDIDATES):
        tangents = tuple(Fraction(token) for token in tangent_text.split(","))
        matrix, _zero = construct(order, tangents)
        packed = primitive_upper_text(matrix)
        maximum_digits = max(len(token.lstrip("-")) for token in packed.split(","))
        if maximum_digits != expected_digits:
            raise RuntimeError(f"order {order}: maximum digits changed from {expected_digits} to {maximum_digits}")
        if len(packed) > EXTERNAL_THRESHOLD:
            raise RuntimeError(f"order {order}: unexpected external-storage requirement ({len(packed)} bytes)")
        if packed in seen:
            raise RuntimeError(f"order {order}: duplicate independent-angle matrix")
        seen.add(packed)
        theorem = "Theorem 8 and Lemma 25" if order % 2 == 0 else "Theorem 9 and Lemma 26"
        source = (
            f"Hildebrand 2017, Copositive matrices with circulant zero support set, {theorem} "
            f"<https://arxiv.org/abs/1603.05111>; n={order}; independently selected "
            f"tan(zeta_j/4)=({tangent_text}); {SOURCE_MARKER}"
        )
        rows.append((FIRST_MATRIX_ID + offset, order, packed, source))
    return rows


def main() -> None:
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--database", type=Path, default=root / "testdata/copos_testdata.sqlite3")
    parser.add_argument("--apply", action="store_true")
    arguments = parser.parse_args()

    rows = build_rows()
    connection = sqlite3.connect(arguments.database.resolve())
    connection.execute("PRAGMA foreign_keys = ON")
    try:
        existing = connection.execute(
            """
            SELECT matrix_id,dimension,matrix,source,source_id,family,is_strictly_copositive,is_copositive,
                   smoke_set,core_and_stress_test,preprocessing_solved,additional_source_ids,references_solved,references_unsolved,
                   fastest_elapsed_ns,fastest_result_ref
            FROM matrices WHERE matrix_id BETWEEN ? AND ? OR source LIKE ? ORDER BY matrix_id
            """,
            (FIRST_MATRIX_ID, FIRST_MATRIX_ID + len(rows) - 1, f"%{SOURCE_MARKER}%"),
        ).fetchall()
        expected_ids = tuple(range(FIRST_MATRIX_ID, FIRST_MATRIX_ID + len(rows)))
        if tuple(row[0] for row in existing) == expected_ids:
            expected_by_id = {matrix_id: (order, packed, source) for matrix_id, order, packed, source in rows}
            if all(
                row[1:4] == expected_by_id[row[0]]
                and row[4:11] == (20, FAMILY, 0, 1, 0, 0, 0)
                and row[11:14] == ("[]", "[]", "[]")
                and row[14:] == (None, None)
                for row in existing
            ):
                print("already_applied=1")
                return
        if existing:
            raise RuntimeError(f"independent-angle matrix IDs or source marker already exist: {existing}")
        if connection.execute("SELECT count(*) FROM sources WHERE source_id=20").fetchone()[0] != 1:
            raise RuntimeError("Hildebrand source 20 is missing")
        duplicate = connection.execute(
            f"SELECT matrix_id FROM matrices WHERE matrix IN ({','.join('?' for _ in rows)})",
            tuple(row[2] for row in rows),
        ).fetchall()
        if duplicate:
            raise RuntimeError(f"generated matrices duplicate existing corpus rows: {duplicate}")

        print("matrix_id order max_digits bytes")
        for matrix_id, order, packed, _source in rows:
            maximum_digits = max(len(token.lstrip("-")) for token in packed.split(","))
            print(matrix_id, order, maximum_digits, len(packed))
        if not arguments.apply:
            print("dry_run=1")
            return

        connection.execute("BEGIN IMMEDIATE")
        connection.executemany(
            """
            INSERT INTO matrices(
                matrix_id,dimension,matrix,is_strictly_copositive,is_copositive,source,source_id,family,
                smoke_set,core_and_stress_test,preprocessing_solved,additional_source_ids,references_solved,references_unsolved,
                fastest_elapsed_ns,fastest_result_ref
            ) VALUES (?, ?, ?, 0, 1, ?, 20, ?, 0, 0, 0, '[]', '[]', '[]', NULL, NULL)
            """,
            ((matrix_id, order, packed, source, FAMILY) for matrix_id, order, packed, source in rows),
        )
        connection.execute(
            "UPDATE sources SET comment=? WHERE source_id=20",
            ("Primary source for the circulant support-n-2 family sampled at orders 7-30 in the corpus.",),
        )
        if connection.execute("PRAGMA integrity_check").fetchone()[0] != "ok":
            raise RuntimeError("corpus integrity check failed")
        if list(connection.execute("PRAGMA foreign_key_check")):
            raise RuntimeError("corpus foreign-key check failed")
        connection.commit()
        print(f"inserted={len(rows)} first_id={expected_ids[0]} last_id={expected_ids[-1]}")
    except BaseException:
        connection.rollback()
        raise
    finally:
        connection.close()


if __name__ == "__main__":
    main()
