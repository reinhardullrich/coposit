#!/usr/bin/env python3
"""Reproduce the second 2026-08-07 literature-matrix corpus import."""

from __future__ import annotations

import argparse
import itertools
import sqlite3
from fractions import Fraction
from pathlib import Path

from import_exceptional_matrices_2026_08_07 import decode_graph6, primitive_upper_text


def signed_cosine_sum(parameters: list[Fraction], indices: tuple[int, ...], negative_angles: bool = False) -> Fraction:
    real, imaginary = Fraction(1), Fraction(0)
    for index in indices:
        tangent = parameters[index]
        denominator = 1 + tangent * tangent
        cosine = (1 - tangent * tangent) / denominator
        sine = 2 * tangent / denominator
        if negative_angles:
            sine = -sine
        real, imaginary = real * cosine - imaginary * sine, real * sine + imaginary * cosine
    return real


def hildebrand_matrix(parameters: tuple[Fraction, ...]) -> list[list[Fraction]]:
    def cosine(*indices: int) -> Fraction:
        return signed_cosine_sum(list(parameters), indices, negative_angles=True)

    def sine(index: int) -> Fraction:
        tangent = parameters[index]
        return -2 * tangent / (1 + tangent * tangent)

    matrix = [[Fraction(0) for _ in range(5)] for _ in range(5)]
    for i in range(5):
        matrix[i][i] = 1
    entries = {
        (0, 1): sine(3),
        (0, 2): -cosine(3, 4),
        (0, 3): -cosine(1, 2),
        (0, 4): sine(2),
        (1, 2): sine(4),
        (1, 3): -cosine(4, 0),
        (1, 4): -cosine(2, 3),
        (2, 3): sine(0),
        (2, 4): -cosine(0, 1),
        (3, 4): sine(1),
    }
    for (i, j), value in entries.items():
        matrix[i][j] = matrix[j][i] = value
    return matrix


def baston_base(p: int) -> list[list[int]]:
    matrix = [[1] * (3 * p) for _ in range(3 * p)]

    def negative(i: int, j: int) -> None:
        matrix[i - 1][j - 1] = matrix[j - 1][i - 1] = -1

    for j in range(2, p + 3):
        negative(1, j)
    for i in range(2, p + 1):
        negative(i, p + 2 * i - 1)
        negative(i, p + 2 * i)
    for i in range(1, p):
        for r in range(1, p - i + 1):
            negative(p + 2 * i - 1, p + 2 * i + 2 * r)
            negative(p + 2 * i, p + 2 * i + 2 * r - 1)
    return matrix


def baston_all_orders(order: int) -> list[list[int]]:
    if order < 9:
        raise ValueError("Baston's Theorem 4.1 starts at order 9")
    p = (order + 2) // 3
    matrix = baston_base(p)
    removed = {p + 1} if order % 3 == 2 else ({p, p + 1} if order % 3 == 1 else set())
    kept = [i for i in range(3 * p) if i not in removed]
    return [[matrix[i][j] for j in kept] for i in kept]


def baston_cyclic(m: int) -> list[list[int]]:
    order = 3 * m + 2
    negative_offsets = {1 + 3 * k for k in range(m + 1)}
    return [
        [1 if i == j or (j - i) % order not in negative_offsets else -1 for j in range(order)]
        for i in range(order)
    ]


def generalized_horn(order: int) -> list[list[int]]:
    if order < 5 or order % 2 == 0:
        raise ValueError("the Johnson-Reams construction requires odd order at least 5")
    return [
        [1 if i == j or (i - j) % order not in (1, order - 1) else -1 for j in range(order)]
        for i in range(order)
    ]


def additional_johnson_reams_orders() -> tuple[int, ...]:
    base_gaps = (8, 12, 10, 14, 6, 10, 16, 4, 12, 8)
    gaps = [gap for shift in range(8) for gap in base_gaps[shift:] + base_gaps[:shift]]
    orders = [163, 175, 181, 199]
    for gap in gaps:
        orders.append(orders[-1] + gap)
    return tuple(orders)


def dickinson_matrix(graph6: str, stability: int) -> list[list[int]]:
    adjacency = decode_graph6(graph6.encode("ascii"))
    return [
        [stability - 1 if i == j or adjacency[i] >> j & 1 else -1 for j in range(len(adjacency))]
        for i in range(len(adjacency))
    ]


DICKINSON_GRAPHS = (
    ("FCp`_", 3),
    ("GCQb`o", 3),
    ("GCp`dO", 3),
    ("H?bB@_W", 4),
    ("HCQb`or", 3),
    ("HCQb`rK", 3),
    ("HCQe`pc", 3),
    ("HCQe`qX", 3),
    ("HCQe`q[", 3),
    ("HCRbdO{", 3),
    ("I?`D@`WH_", 4),
    ("I?`D@`WJ?", 4),
    ("I?`DA_wJ?", 4),
    ("I?bB@_Wc_", 4),
    ("I?bB@_Wh?", 4),
    ("ICQbQpeJO", 3),
    ("ICQbQpqMO", 3),
    ("ICQbRbKJ_", 3),
    ("ICQb`rKs_", 3),
    ("ICQeR_{hO", 3),
    ("ICQeR_{h_", 3),
    ("ICQeR`[Mg", 3),
    ("ICQeR`[e_", 3),
    ("ICQe`pc[g", 3),
    ("ICQe`pcj_", 3),
    ("ICQe`pcq_", 3),
    ("ICQe`phU_", 3),
    ("ICQe`q[eW", 3),
    ("ICQe`rKX_", 3),
    ("ICRbcqhN?", 3),
    ("ICpdUg{[_", 3),
)


def build_rows() -> list[tuple[int, str, int, str, str]]:
    rows = []
    hildebrand_source = (
        "Hildebrand, The extreme rays of the 5x5 copositive cone, Theorem 3.1 "
        "<https://doi.org/10.1016/j.laa.2012.05.006>"
    )
    choices = (Fraction(3, 5), Fraction(2, 3), Fraction(5, 7), Fraction(3, 4), Fraction(4, 5),
               Fraction(5, 6), Fraction(6, 7), Fraction(7, 8), Fraction(8, 9), Fraction(9, 10))
    for parameters in itertools.islice(itertools.combinations(choices, 5), 24):
        parameter_text = ",".join(map(str, parameters))
        rows.append((5, primitive_upper_text(hildebrand_matrix(parameters)), 0,
                     f"{hildebrand_source}; t=({parameter_text}), phi=-2 atan(t)",
                     "exceptional boundary / Hildebrand COP5 rational family"))

    baston_source = "Baston 1969, Extreme copositive quadratic forms, Theorem 4.1 <https://eudml.org/doc/204902>"
    for order in range(11, 65):
        rows.append((order, primitive_upper_text(baston_all_orders(order)), 0, f"{baston_source}; q_{order}",
                     "exceptional boundary / Baston all-orders family"))

    cyclic_source = "Baston 1969, Extreme copositive quadratic forms, Theorem 4.4 <https://eudml.org/doc/204902>"
    for m in range(3, 21):
        order = 3 * m + 2
        rows.append((order, primitive_upper_text(baston_cyclic(m)), 0, f"{cyclic_source}; m={m}, q_{order}",
                     "exceptional boundary / Baston cyclic family"))

    johnson_source = (
        "Johnson-Reams 2008, Constructing copositive matrices from interior matrices, Section 4 "
        "<https://doi.org/10.13001/1081-3810.1245>"
    )
    for order in range(7, 152, 2):
        rows.append((order, primitive_upper_text(generalized_horn(order)), 0, f"{johnson_source}; odd order n={order}",
                     "exceptional boundary / Johnson-Reams generalized Horn"))

    dickinson_source = (
        "Dickinson-de Zeeuw 2021, Generating irreducible copositive matrices using the stable set problem, Table 2 "
        "<https://doi.org/10.1016/j.dam.2020.04.013>"
    )
    for graph6, stability in DICKINSON_GRAPHS:
        matrix = dickinson_matrix(graph6, stability)
        rows.append((len(matrix), primitive_upper_text(matrix), 0,
                     f"{dickinson_source}; graph6={graph6}, alpha={stability}",
                     "exceptional boundary / Dickinson-de Zeeuw cop-irreducible graph"))

    for order in additional_johnson_reams_orders():
        rows.append((order, primitive_upper_text(generalized_horn(order)), 0, f"{johnson_source}; odd order n={order}",
                     "exceptional boundary / Johnson-Reams generalized Horn"))

    assert len(rows) == 284
    assert len({(dimension, matrix) for dimension, matrix, *_ in rows}) == 284
    return rows


def self_check() -> None:
    assert primitive_upper_text(baston_cyclic(1)) == "1,-1,1,1,-1,1,-1,1,1,1,-1,1,1,-1,1"
    assert len(baston_all_orders(9)) == 9
    assert all(abs(value) == 1 for row in baston_all_orders(9) for value in row)
    base12 = baston_base(4)
    assert baston_all_orders(11) == [[base12[i][j] for j in range(12) if j != 5] for i in range(12) if i != 5]
    assert baston_all_orders(10) == [[base12[i][j] for j in range(12) if j not in (4, 5)] for i in range(12) if i not in (4, 5)]
    assert len(generalized_horn(7)) == 7
    assert all(generalized_horn(7)[i][(i + 1) % 7] == -1 for i in range(7))
    orders = additional_johnson_reams_orders()
    assert len(orders) == 84 and orders[:4] == (163, 175, 181, 199) and orders[-1] == 999
    assert all(order % 2 == 1 for order in orders)
    gaps = [orders[0] - 151, *(b - a for a, b in zip(orders, orders[1:]))]
    assert min(gaps) == 4 and max(gaps) == 18
    assert all(len(decode_graph6(graph6.encode("ascii"))) in (7, 8, 9, 10) for graph6, _ in DICKINSON_GRAPHS)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--database", type=Path, default=Path(__file__).resolve().parents[1] / "copos_testdata.sqlite3")
    parser.add_argument("--import", dest="do_import", action="store_true", help="insert the checked rows in one transaction")
    args = parser.parse_args()

    self_check()
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
