#!/usr/bin/env python3
"""Reproduce the 2026-08-07 exceptional-matrix corpus import."""

from __future__ import annotations

import argparse
import gzip
import sqlite3
from fractions import Fraction
from functools import reduce
from math import gcd, lcm
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
GRAPH_DIR = ROOT / "research/data/mckay_connected_graphs"


def decode_graph6(line: bytes) -> list[int]:
    data = line.strip()
    if not data or data[0] == ord("~") or data[0] < 63:
        raise ValueError("only standard graph6 records with n <= 62 are supported")
    n = data[0] - 63
    bits = [(byte - 63) >> shift & 1 for byte in data[1:] for shift in range(5, -1, -1)]
    adjacency = [0] * n
    k = 0
    for j in range(1, n):
        for i in range(j):
            if bits[k]:
                adjacency[i] |= 1 << j
                adjacency[j] |= 1 << i
            k += 1
    return adjacency


def is_nontrivial_extreme(adjacency: list[int]) -> bool:
    n = len(adjacency)
    if any(adjacency[i] & adjacency[j] for i in range(n) for j in range(i + 1, n) if adjacency[i] >> j & 1):
        return False

    zero_pairs = [
        (i, j)
        for j in range(1, n)
        for i in range(j)
        if not (adjacency[i] >> j & 1) and not (adjacency[i] & adjacency[j])
    ]
    line_adjacency = [0] * len(zero_pairs)
    for a, (i, j) in enumerate(zero_pairs):
        for b in range(a):
            k, ell = zero_pairs[b]
            common = {i, j} & {k, ell}
            if len(common) != 1:
                continue
            common_vertex = next(iter(common))
            other_a = j if i == common_vertex else i
            other_b = ell if k == common_vertex else k
            if adjacency[other_a] >> other_b & 1:
                line_adjacency[a] |= 1 << b
                line_adjacency[b] |= 1 << a
    if any(mask == 0 for mask in line_adjacency):
        return False

    unseen = (1 << len(zero_pairs)) - 1
    while unseen:
        start = (unseen & -unseen).bit_length() - 1
        colors = {start: 0}
        stack = [start]
        unseen &= ~(1 << start)
        bipartite = True
        while stack:
            vertex = stack.pop()
            neighbors = line_adjacency[vertex]
            while neighbors:
                neighbor_bit = neighbors & -neighbors
                neighbor = neighbor_bit.bit_length() - 1
                neighbors ^= neighbor_bit
                if neighbor not in colors:
                    colors[neighbor] = 1 - colors[vertex]
                    stack.append(neighbor)
                    unseen &= ~neighbor_bit
                elif colors[neighbor] == colors[vertex]:
                    bipartite = False
        if bipartite:
            return False

    if zero_pairs:
        return True

    colors = {0: 0}
    stack = [0]
    while stack:
        vertex = stack.pop()
        neighbors = adjacency[vertex]
        while neighbors:
            neighbor_bit = neighbors & -neighbors
            neighbor = neighbor_bit.bit_length() - 1
            neighbors ^= neighbor_bit
            if neighbor not in colors:
                colors[neighbor] = 1 - colors[vertex]
                stack.append(neighbor)
            elif colors[neighbor] == colors[vertex]:
                return True
    return False


def graph_matrix_text(adjacency: list[int]) -> str:
    values = []
    for i in range(len(adjacency)):
        for j in range(i, len(adjacency)):
            values.append(1 if i == j or adjacency[i] & adjacency[j] else -(adjacency[i] >> j & 1))
    return ",".join(map(str, values))


def graph_records(path: Path):
    opener = gzip.open if path.suffix == ".gz" else open
    with opener(path, "rb") as stream:
        yield from enumerate(stream, 1)


def hoffman_pereira_rows() -> list[tuple[int, str, int, str, str]]:
    rows = []
    expected_counts = {5: 0, 6: 1, 7: 3, 8: 10, 9: 55, 10: 130}
    for n, expected in expected_counts.items():
        name = f"graph{n}c.g6" + (".gz" if n == 10 else "")
        selected = 0
        for record_number, line in graph_records(GRAPH_DIR / name):
            adjacency = decode_graph6(line)
            if not is_nontrivial_extreme(adjacency):
                continue
            if n in (5, 7) and all(neighbors.bit_count() == 2 for neighbors in adjacency):
                continue  # Existing Horn and order-7 Hoffman--Pereira cycle classes.
            graph6 = line.strip().decode("ascii")
            source = (
                "Hoffman-Pereira extreme {-1,0,1} construction; "
                f"McKay {name} record {record_number}, graph6={graph6}; "
                "Peng 2022 Section 3.1 <https://doi.org/10.1016/j.ejco.2022.100037>; "
                "catalog <https://users.cecs.anu.edu.au/~bdm/data/graphs.html>"
            )
            rows.append((n, graph_matrix_text(adjacency), 0, source, "exceptional boundary / Hoffman-Pereira graph enumeration"))
            selected += 1
            if n == 10 and selected == 130:
                break
        assert selected == expected, (n, selected, expected)
    return rows


def extend(matrix: list[list[int]], vector: list[int]) -> list[list[int]]:
    product = [sum(matrix[i][j] * vector[j] for j in range(len(vector))) for i in range(len(vector))]
    quadratic = sum(vector[i] * product[i] for i in range(len(vector)))
    return [row + [product[i]] for i, row in enumerate(matrix)] + [product + [quadratic]]


def primitive_upper_text(matrix: list[list[int | Fraction]]) -> str:
    upper = [matrix[i][j] for i in range(len(matrix)) for j in range(i, len(matrix))]
    denominator = lcm(*(value.denominator if isinstance(value, Fraction) else 1 for value in upper))
    integers = [int(value * denominator) for value in upper]
    divisor = reduce(gcd, map(abs, integers))
    return ",".join(str(value // divisor) for value in integers)


def explicit_rows() -> list[tuple[int, str, int, str, str]]:
    a0 = [
        [1, -1, -1, -1, 1, 1],
        [-1, 1, 1, 1, -1, 1],
        [-1, 1, 1, 1, 1, -1],
        [-1, 1, 1, 1, -1, 1],
        [1, -1, 1, -1, 1, -1],
        [1, 1, -1, 1, -1, 1],
    ]
    a8 = [
        [2, 2, -1, -1, -1, -1, 2, -1],
        [2, 2, 2, -1, -1, -1, -1, -1],
        [-1, 2, 2, 2, -1, -1, -1, 2],
        [-1, -1, 2, 2, 2, -1, -1, -1],
        [-1, -1, -1, 2, 2, 2, -1, -1],
        [-1, -1, -1, -1, 2, 2, 2, 2],
        [2, -1, -1, -1, -1, 2, 2, 2],
        [-1, -1, 2, -1, -1, 2, 2, 2],
    ]
    c5 = [
        [Fraction(17), Fraction(-91, 5), Fraction(33, 2), Fraction(38, 3), Fraction(-36, 5)],
        [Fraction(-91, 5), Fraction(59, 3), Fraction(-53, 4), Fraction(8), Fraction(33, 4)],
        [Fraction(33, 2), Fraction(-53, 4), Fraction(39, 4), Fraction(-13, 2), Fraction(8)],
        [Fraction(38, 3), Fraction(8), Fraction(-13, 2), Fraction(16, 3), Fraction(-13, 3)],
        [Fraction(-36, 5), Fraction(33, 4), Fraction(8), Fraction(-13, 3), Fraction(1373628701, 353935575)],
    ]
    kostyukova = "Kostyukova-Tchemisova 2026 <https://doi.org/10.3390/axioms15060414>"
    strekelj = "Strekelj-Zalar, Construction of exceptional copositive matrices <https://arxiv.org/abs/2502.20133>"
    return [
        (
            7,
            primitive_upper_text(extend(a0, [1, 1, 0, 1, 0, 0])),
            0,
            f"{kostyukova}, Example 1, B(A0,a), a=(1,1,0,1,0,0)",
            "exceptional boundary / Kostyukova extension",
        ),
        (8, primitive_upper_text(a8), 0, f"{kostyukova}, Example 5, A", "exceptional boundary / Kostyukova base"),
        (
            9,
            primitive_upper_text(extend(a8, [1, 0, 0, 0, 1, 0, 0, 0])),
            0,
            f"{kostyukova}, Example 5, B(A,a), a=(1,0,0,0,1,0,0,0)",
            "exceptional boundary / Kostyukova extension",
        ),
        (
            9,
            primitive_upper_text(extend(a8, [1, 0, 0, 0, 2, 0, 0, 0])),
            0,
            f"{kostyukova}, Example 5, B(A,b), b=(1,0,0,0,2,0,0,0)",
            "exceptional boundary / Kostyukova extension",
        ),
        (5, primitive_upper_text(c5), 1, f"{strekelj}, matrix C", "exceptional strict / Strekelj-Zalar"),
    ]


def build_rows() -> list[tuple[int, str, int, str, str]]:
    rows = hoffman_pereira_rows() + explicit_rows()
    assert len(rows) == 204
    assert len({matrix for _, matrix, _, _, _ in rows}) == 204
    assert sum(strict for _, _, strict, _, _ in rows) == 1
    return rows


def self_check() -> None:
    cycle5 = decode_graph6(b"Dhc\n")
    assert cycle5 == [0b10010, 0b00101, 0b01010, 0b10100, 0b01001]
    assert is_nontrivial_extreme(cycle5)
    assert graph_matrix_text(cycle5) == "1,-1,1,1,-1,1,-1,1,1,1,-1,1,1,-1,1"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--database", type=Path, default=Path(__file__).resolve().parents[1] / "copos_testdata.sqlite3")
    parser.add_argument("--import", dest="do_import", action="store_true", help="insert the checked rows in one transaction")
    args = parser.parse_args()

    self_check()
    rows = build_rows()
    if not args.do_import:
        print(f"checked {len(rows)} rows; pass --import to write them")
        return

    connection = sqlite3.connect(args.database)
    try:
        connection.execute("PRAGMA foreign_keys = ON")
        existing = {matrix for (matrix,) in connection.execute("SELECT matrix FROM matrices")}
        overlaps = [matrix for _, matrix, _, _, _ in rows if matrix in existing]
        if overlaps:
            raise RuntimeError(f"refusing import: {len(overlaps)} exact matrix rows already exist")
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
