#!/usr/bin/env python3
"""Reproduce the third 2026-08-07 hard literature-matrix import."""

from __future__ import annotations

import argparse
import itertools
import sqlite3
from fractions import Fraction
from pathlib import Path

from import_exceptional_matrices_2026_08_07 import primitive_upper_text
from import_literature_extremes_2026_08_07 import generalized_horn


def angle_components(parameters: tuple[Fraction, ...], indices: tuple[int, ...]) -> tuple[Fraction, Fraction]:
    real, imaginary = Fraction(1), Fraction(0)
    for index in indices:
        tangent = parameters[index]
        denominator = 1 + tangent * tangent
        cosine = (1 - tangent * tangent) / denominator
        sine = 2 * tangent / denominator
        real, imaginary = real * cosine - imaginary * sine, real * sine + imaginary * cosine
    return real, imaginary


def angle_lt_pi(parameters: tuple[Fraction, ...], indices: tuple[int, ...]) -> bool:
    # Every selected angle is below pi/2 and at most four are summed, so a positive sine means the sum is below pi.
    return all(0 < parameters[index] < 1 for index in indices) and angle_components(parameters, indices)[1] > 0


def matrix_vector(matrix: list[list[Fraction]], vector: list[Fraction]) -> list[Fraction]:
    return [sum(row[j] * vector[j] for j in range(len(vector))) for row in matrix]


def is_zero(matrix: list[list[Fraction]], vector: list[Fraction]) -> bool:
    product = matrix_vector(matrix, vector)
    return all(value >= 0 for value in product) and sum(vector[i] * product[i] for i in range(len(vector))) == 0


def determinant(matrix: list[list[Fraction]]) -> Fraction:
    work = [row[:] for row in matrix]
    result = Fraction(1)
    for column in range(len(work)):
        pivot = next((row for row in range(column, len(work)) if work[row][column]), None)
        if pivot is None:
            return Fraction(0)
        if pivot != column:
            work[column], work[pivot] = work[pivot], work[column]
            result = -result
        pivot_value = work[column][column]
        result *= pivot_value
        for row in range(column + 1, len(work)):
            factor = work[row][column] / pivot_value
            for inner in range(column + 1, len(work)):
                work[row][inner] -= factor * work[column][inner]
    return result


def dickinson_matrix(parameters: tuple[Fraction, ...]) -> list[list[Fraction]]:
    def cosine(*indices: int) -> Fraction:
        return angle_components(parameters, indices)[0]

    matrix = [[Fraction(0) for _ in range(6)] for _ in range(6)]
    for i in range(6):
        matrix[i][i] = 1
    entries = {
        (0, 1): -cosine(0),
        (0, 2): cosine(0, 1),
        (0, 3): cosine(3),
        (0, 4): Fraction(-1),
        (0, 5): cosine(0),
        (1, 2): -cosine(1),
        (1, 3): cosine(1, 2),
        (1, 4): cosine(0),
        (1, 5): Fraction(-1),
        (2, 3): -cosine(2),
        (2, 4): cosine(2, 3),
        (2, 5): cosine(1),
        (3, 4): -cosine(3),
        (3, 5): cosine(3, 4),
        (4, 5): -cosine(4),
    }
    for (i, j), value in entries.items():
        matrix[i][j] = matrix[j][i] = value
    return matrix


def dickinson_zeros(parameters: tuple[Fraction, ...]) -> list[list[Fraction]]:
    zeros = []
    for i in range(4):
        vector = [Fraction(0)] * 6
        vector[i] = angle_components(parameters, (i + 1,))[1]
        vector[i + 1] = angle_components(parameters, (i, i + 1))[1]
        vector[i + 2] = angle_components(parameters, (i,))[1]
        zeros.append(vector)
    zeros.extend(
        ([Fraction(1), 0, 0, 0, Fraction(1), 0], [0, Fraction(1), 0, 0, 0, Fraction(1)])
    )
    return zeros


def dickinson_parameters() -> list[tuple[Fraction, ...]]:
    choices = tuple(Fraction(n, 40) for n in (4, 5, 6, 7, 8, 9, 10, 11, 12))
    return list(itertools.islice(itertools.combinations(choices, 5), 3))


def afonin_matrix(parameters: tuple[Fraction, ...]) -> list[list[Fraction]]:
    def cosine(*indices: int) -> Fraction:
        return angle_components(parameters, indices)[0]

    matrix = [[Fraction(0) for _ in range(6)] for _ in range(6)]
    for i in range(6):
        matrix[i][i] = 1
    entries = {
        (0, 1): -cosine(0),
        (0, 2): cosine(0, 1),
        (0, 3): -cosine(0, 1, 2),
        (0, 4): cosine(4, 5),
        (0, 5): -cosine(5),
        (1, 2): -cosine(1),
        (1, 3): cosine(1, 2),
        (1, 4): -cosine(1, 2, 3),
        (1, 5): cosine(0, 5),
        (2, 3): -cosine(2),
        (2, 4): cosine(2, 3),
        (2, 5): -cosine(2, 3, 4),
        (3, 4): -cosine(3),
        (3, 5): cosine(3, 4),
        (4, 5): -cosine(4),
    }
    for (i, j), value in entries.items():
        matrix[i][j] = matrix[j][i] = value
    return matrix


def afonin_zeros(parameters: tuple[Fraction, ...]) -> list[list[Fraction]]:
    def sine(*indices: int) -> Fraction:
        return angle_components(parameters, indices)[1]

    return [
        [sine(1), sine(0, 1), sine(0), 0, 0, 0],
        [0, sine(2), sine(1, 2), sine(1), 0, 0],
        [0, 0, sine(3), sine(2, 3), sine(2), 0],
        [0, 0, 0, sine(4), sine(3, 4), sine(3)],
        [sine(4), 0, 0, 0, sine(5), sine(4, 5)],
        [sine(0, 5), sine(5), 0, 0, 0, sine(0)],
    ]


def afonin_certificate_determinant(parameters: tuple[Fraction, ...]) -> Fraction:
    coefficients = []
    for i in range(6):
        cosine, sine = angle_components(parameters, (i,))
        successor = (i + 1) % 6
        positive = [Fraction(0)] * 12
        positive[i], positive[successor], positive[6 + i] = 1, cosine, sine
        coefficients.append(positive)
    for i in range(6):
        cosine, sine = angle_components(parameters, (i,))
        successor = (i + 1) % 6
        negative = [Fraction(0)] * 12
        negative[i], negative[successor], negative[6 + i] = -cosine, -1, sine
        coefficients.append(negative)
    return determinant(coefficients)


def afonin_parameters() -> list[tuple[Fraction, ...]]:
    base = (Fraction(103, 317), Fraction(412, 841), Fraction(107, 210), Fraction(280, 741), Fraction(19, 201), Fraction(28, 891))
    variants = []
    for index, sign in ((0, -1), (0, 1), (1, -1)):
        changed = list(base)
        changed[index] += sign * Fraction(1, 10000)
        variants.append(tuple(changed))
    return variants


def validate_afonin(parameters: tuple[Fraction, ...], matrix: list[list[Fraction]]) -> None:
    for i in range(5):
        assert angle_lt_pi(parameters, (i, i + 1))
    assert angle_lt_pi(parameters, (0, 5))
    for indices in ((0, 1, 2), (3, 4, 5), (2, 3, 4), (0, 1, 5), (1, 2, 3), (0, 4, 5)):
        assert angle_lt_pi(parameters, indices)
    assert angle_components(parameters, (0, 1, 2))[0] < angle_components(parameters, (3, 4, 5))[0]
    assert angle_components(parameters, (2, 3, 4))[0] < angle_components(parameters, (0, 1, 5))[0]
    assert angle_components(parameters, (1, 2, 3))[0] < angle_components(parameters, (0, 4, 5))[0]
    assert angle_components(parameters, tuple(range(6)))[1] != 0

    def cosine(*indices: int) -> Fraction:
        return angle_components(parameters, indices)[0]

    m136 = (
        cosine(0, 1, 5) - cosine(2, 3, 4) + cosine(0, 1) - cosine(2, 3, 4, 5)
        - cosine(5) + cosine(0, 1, 2, 3, 4)
    )
    assert m136 < 0
    assert afonin_certificate_determinant(parameters) != 0
    assert all(is_zero(matrix, zero) for zero in afonin_zeros(parameters))


def laurent_direct_sum(m: int) -> list[list[Fraction]]:
    horn = generalized_horn(5)
    if m == 1:
        matrix = [[Fraction(value) for value in row] + [Fraction(0)] for row in horn]
        return matrix + [[Fraction(0)] * 6]
    order = 5 + m
    matrix = [[Fraction(0) for _ in range(order)] for _ in range(order)]
    for i in range(5):
        for j in range(5):
            matrix[i][j] = (m - 1) * horn[i][j]
    for i in range(m):
        for j in range(m):
            matrix[5 + i][5 + j] = m - 1 if i == j else -1
    return matrix


HILDEBRAND_CIRCULANT_PARAMETERS = (
    (7, Fraction(5, 9), Fraction(1, 5)),
    (7, Fraction(7, 13), Fraction(1, 5)),
    (7, Fraction(6, 11), Fraction(1, 5)),
    (7, Fraction(8, 15), Fraction(1, 5)),
    (8, Fraction(1, 2), Fraction(1, 5)),
    (8, Fraction(5, 9), Fraction(1, 5)),
    (8, Fraction(7, 13), Fraction(1, 5)),
    (8, Fraction(6, 11), Fraction(1, 5)),
    (9, Fraction(2, 5), Fraction(1, 6)),
    (9, Fraction(5, 12), Fraction(1, 6)),
    (9, Fraction(12, 29), Fraction(3, 19)),
    (9, Fraction(5, 12), Fraction(2, 13)),
    (9, Fraction(7, 17), Fraction(3, 19)),
    (10, Fraction(2, 5), Fraction(1, 6)),
    (10, Fraction(5, 12), Fraction(1, 6)),
    (10, Fraction(7, 17), Fraction(2, 13)),
    (10, Fraction(12, 29), Fraction(3, 19)),
    (11, Fraction(1, 3), Fraction(2, 15)),
    (11, Fraction(11, 32), Fraction(3, 23)),
    (11, Fraction(13, 38), Fraction(5, 38)),
    (12, Fraction(1, 3), Fraction(1, 8)),
    (12, Fraction(1, 3), Fraction(2, 15)),
    (12, Fraction(1, 3), Fraction(3, 23)),
    (12, Fraction(18, 53), Fraction(5, 38)),
    (13, Fraction(2, 7), Fraction(1, 8)),
    (13, Fraction(9, 31), Fraction(1, 9)),
    (14, Fraction(2, 7), Fraction(1, 9)),
    (14, Fraction(2, 7), Fraction(1, 8)),
    (14, Fraction(9, 31), Fraction(1, 9)),
    (14, Fraction(11, 38), Fraction(1, 9)),
    (15, Fraction(1, 4), Fraction(1, 10)),
    (16, Fraction(1, 4), Fraction(1, 10)),
    (16, Fraction(1, 4), Fraction(6, 61)),
    (16, Fraction(1, 4), Fraction(5, 51)),
    (16, Fraction(1, 4), Fraction(4, 41)),
    (17, Fraction(2, 9), Fraction(1, 11)),
    (18, Fraction(2, 9), Fraction(1, 11)),
    (18, Fraction(2, 9), Fraction(2, 23)),
    (18, Fraction(2, 9), Fraction(5, 57)),
    (19, Fraction(1, 5), Fraction(1, 12)),
    (20, Fraction(1, 5), Fraction(1, 13)),
    (20, Fraction(1, 5), Fraction(1, 12)),
    (21, Fraction(2, 11), Fraction(1, 14)),
    (22, Fraction(2, 11), Fraction(1, 14)),
    (23, Fraction(1, 6), Fraction(1, 15)),
    (24, Fraction(1, 6), Fraction(1, 15)),
    (25, Fraction(2, 13), Fraction(1, 16)),
)


def complex_power(real: Fraction, imaginary: Fraction, exponent: int) -> tuple[Fraction, Fraction]:
    result = (Fraction(1), Fraction(0))
    for _ in range(exponent):
        result = (result[0] * real - result[1] * imaginary, result[0] * imaginary + result[1] * real)
    return result


def hildebrand_circulant(
    order: int, theta_quarter_tangent: Fraction, alpha_quarter_tangent: Fraction
) -> tuple[list[list[Fraction]], list[Fraction]]:
    m = (order - 3) // 2
    assert 0 < theta_quarter_tangent < 1 and 0 < alpha_quarter_tangent < 1
    def atan_upper(tangent: Fraction) -> Fraction:
        return tangent - tangent**3 / 3 + tangent**5 / 5

    # The alternating series bounds atan(t) above for 0 < t < 1, and 333/106 is a strict lower bound for pi.
    assert 2 * (atan_upper(theta_quarter_tangent) + (m - 1) * atan_upper(alpha_quarter_tangent)) < Fraction(333, 212)
    half_angles = []
    for j in range(m):
        half_angles.append(angle_components((theta_quarter_tangent, alpha_quarter_tangent), (0,) + (1,) * j))
    cosines = [cosine * cosine - sine * sine for cosine, sine in half_angles]
    assert all(cosines[j] > cosines[j + 1] for j in range(m - 1))
    assert half_angles[-1][0] > 0

    polynomial = [Fraction(1)]
    for cosine in cosines:
        factor = (Fraction(1), -2 * cosine, Fraction(1))
        polynomial = [
            sum((polynomial[i - j] * factor[j] for j in range(3) if 0 <= i - j < len(polynomial)), Fraction(0))
            for i in range(len(polynomial) + 2)
        ]
    if order % 2 == 0:
        polynomial = [
            sum((polynomial[i - j] for j in range(2) if 0 <= i - j < len(polynomial)), Fraction(0))
            for i in range(len(polynomial) + 1)
        ]
    assert len(polynomial) == order - 2 and all(coefficient > 0 for coefficient in polynomial)

    first_half = []
    for k in range(order // 2 + 1):
        value = Fraction(0)
        for j, ((half_cosine, half_sine), cosine) in enumerate(zip(half_angles, cosines)):
            numerator = complex_power(cosine, 2 * half_cosine * half_sine, k)[0]
            sine_multiple = complex_power(half_cosine, half_sine, order)[1]
            assert sine_multiple > 0 if j % 2 == 0 else sine_multiple < 0
            product = Fraction(1)
            for ell, other_cosine in enumerate(cosines):
                if j != ell:
                    product *= cosine - other_cosine
            first_sine = half_sine if order % 2 else 2 * half_sine * half_cosine
            value += numerator / (first_sine * sine_multiple * product)
        first_half.append(value)

    matrix = [[Fraction(0) for _ in range(order)] for _ in range(order)]
    for i in range(order):
        for j in range(order):
            distance = min((j - i) % order, (i - j) % order)
            matrix[i][j] = first_half[distance]
    zero = polynomial + [Fraction(0), Fraction(0)]
    assert is_zero(matrix, zero)
    return matrix, zero


def build_rows() -> list[tuple[int, str, int, str, str]]:
    rows = []
    dickinson_source = "Dickinson 2019, A new certificate for copositivity, Section 7 <https://doi.org/10.1016/j.laa.2018.12.025>"
    for parameters in dickinson_parameters():
        assert parameters[0] < parameters[4]
        assert angle_lt_pi(parameters, (0, 1, 2, 3)) and angle_lt_pi(parameters, (1, 2, 3, 4))
        matrix = dickinson_matrix(parameters)
        assert all(is_zero(matrix, zero) for zero in dickinson_zeros(parameters))
        parameter_text = ",".join(map(str, parameters))
        rows.append((6, primitive_upper_text(matrix), 0, f"{dickinson_source}; case 9, t=({parameter_text})",
                     "exceptional boundary / Dickinson COP6 case 9"))

    afonin_source = (
        "Hildebrand-Afonin 2024, On the structure of the 6x6 copositive cone, Theorem 4.3 "
        "<https://arxiv.org/abs/2209.08039>"
    )
    for parameters in afonin_parameters():
        matrix = afonin_matrix(parameters)
        validate_afonin(parameters, matrix)
        parameter_text = ",".join(map(str, parameters))
        rows.append((6, primitive_upper_text(matrix), 0, f"{afonin_source}; exact nearby case 13.1, t=({parameter_text})",
                     "exceptional boundary outside K6^(1) / Hildebrand-Afonin"))

    laurent_source = (
        "Laurent-Vargas 2023, Exactness of Parrilo's conic approximations, Theorem 3.2 and Example 3.4 "
        "<https://ir.cwi.nl/pub/33322/33322.pdf>"
    )
    for m in (1, 2, 3, 5, 10, 20, 40):
        matrix = laurent_direct_sum(m)
        zero = [Fraction(0)] * 5 + [Fraction(1)] * m
        assert is_zero(matrix, zero)
        rows.append((5 + m, primitive_upper_text(matrix), 0, f"{laurent_source}; m={m}",
                     "boundary outside every Parrilo K^(r) / Laurent-Vargas direct sum"))

    for order, theta_tangent, alpha_tangent in HILDEBRAND_CIRCULANT_PARAMETERS:
        matrix, _ = hildebrand_circulant(order, theta_tangent, alpha_tangent)
        theorem = "Theorem 8 and Lemma 25" if order % 2 == 0 else "Theorem 9 and Lemma 26"
        hildebrand_source = (
            f"Hildebrand 2017, Copositive matrices with circulant zero support set, {theorem} "
            "<https://arxiv.org/abs/1603.05111>"
        )
        rows.append((order, primitive_upper_text(matrix), 0,
                     f"{hildebrand_source}; n={order}, tan(theta/4)={theta_tangent}, tan(alpha/4)={alpha_tangent}",
                     "exceptional boundary / Hildebrand circulant support n-2"))

    assert len(rows) == 60
    assert len({(dimension, matrix) for dimension, matrix, *_ in rows}) == len(rows)
    return rows


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--database", type=Path, default=Path(__file__).resolve().parents[1] / "copos_testdata.sqlite3")
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
