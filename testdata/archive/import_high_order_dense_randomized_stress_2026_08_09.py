#!/usr/bin/env python3
"""Reproduce the 2026-08-09 dense randomized high-order stress matrices."""

from __future__ import annotations

import argparse
import hashlib
import io
import sqlite3
from pathlib import Path
from typing import Iterator

import numpy as np

from import_high_order_small_integer_stress_2026_08_09 import DIMENSIONS, MASK64, splitmix64


RANK = 8
CODE_MASK = (1 << (2 * RANK)) - 1
BASE_SEED = 0xD3E5E_20260809
COORDINATES = np.array((-2, -1, 1, 2), dtype=np.int64)
KINDS = ("boundary", "strict", "not_copositive")
SALTS = {"boundary": 0xB0A0D, "strict": 0x57A1C7, "not_copositive": 0xBAD}
FAMILIES = {
    "boundary": "generated dense randomized PSD / copositive boundary",
    "strict": "generated dense randomized positive definite / strict copositive",
    "not_copositive": "generated dense randomized two-coordinate witness / not copositive",
}


def unique_codes(dimension: int, seed: int, opposite_pair: bool) -> tuple[list[int], int]:
    state = seed
    codes: list[int] = []
    used: set[int] = set()
    if opposite_pair:
        state, word = splitmix64(state)
        first = word & CODE_MASK
        codes.extend((first, first ^ CODE_MASK))
        used.update(codes)
    while len(codes) < dimension:
        state, word = splitmix64(state)
        code = word & CODE_MASK
        if code not in used:
            used.add(code)
            codes.append(code)
    return codes, state


def columns_from_codes(codes: list[int]) -> np.ndarray:
    encoded = np.asarray(codes, dtype=np.uint16)
    shifts = (2 * np.arange(RANK, dtype=np.uint16))[:, np.newaxis]
    digits = (encoded[np.newaxis, :] >> shifts) & 3
    return COORDINATES[digits]


def build_matrix(dimension: int, kind: str) -> tuple[np.ndarray, str]:
    seed = (BASE_SEED ^ (dimension * 0x9E3779B97F4A7C15) ^ SALTS[kind]) & MASK64
    codes, state = unique_codes(dimension, seed, kind == "boundary")
    columns = columns_from_codes(codes)
    matrix = columns.T @ columns

    if kind == "boundary":
        matrix[0, 0] += 1
        matrix[1, 1] += 1
        matrix[0, 1] -= 1
        matrix[1, 0] -= 1
        indices = np.arange(2, dimension)
        matrix[indices, indices] += 1
        value = int(matrix[0, 0] + matrix[1, 1] + 2 * matrix[0, 1])
        assert value == 0
        proof = "PSD Gram plus rank-(n-1) regularizer; nonnegative zero witness e1+e2"
    else:
        matrix.flat[:: dimension + 1] += 1
        if kind == "strict":
            proof = "positive definite I plus a dense randomized Gram matrix"
        else:
            state, word = splitmix64(state)
            first = word % dimension
            state, word = splitmix64(state)
            second = word % dimension
            if second == first:
                second = (second + 1) % dimension
            matrix[first, second] = matrix[second, first] = -(matrix[first, first] + matrix[second, second])
            value = int(matrix[first, first] + matrix[second, second] + 2 * matrix[first, second])
            assert value < 0
            proof = f"positive diagonal; x=e{first + 1}+e{second + 1} gives x^T A x={value}"

    assert np.array_equal(matrix, matrix.T)
    assert int(np.max(np.abs(matrix))) <= 100
    assert int(matrix.min()) < 0 < int(matrix.max())
    return matrix, f"seed=0x{seed:016x}; rank={RANK}; {proof}"


def upper_text(matrix: np.ndarray) -> tuple[str, int, float]:
    output = io.StringIO()
    nonzero = 0
    entries = 0
    for row_index in range(matrix.shape[0]):
        row = matrix[row_index, row_index:]
        nonzero += int(np.count_nonzero(row))
        entries += row.size
        if row_index:
            output.write(",")
        output.write(",".join(row.astype(str).tolist()))
    return output.getvalue(), int(np.max(np.abs(matrix))), nonzero / entries


def generated_rows() -> Iterator[tuple[int, str, int, str, str, int, float]]:
    prefix = "Coposit deterministic dense randomized high-order stress generator 2026-08-09"
    for dimension in DIMENSIONS:
        seen: set[str] = set()
        for kind in KINDS:
            matrix, proof = build_matrix(dimension, kind)
            text, maximum, density = upper_text(matrix)
            digest = hashlib.sha256(text.encode("ascii")).hexdigest()
            assert digest not in seen
            assert density >= 0.75
            seen.add(digest)
            strict = int(kind == "strict")
            source = f"{prefix}; dimension={dimension}; class={kind}; {proof}"
            yield dimension, text, strict, source, FAMILIES[kind], maximum, density


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--database", type=Path, default=Path(__file__).resolve().parents[1] / "copos_testdata.sqlite3")
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument("--import", dest="do_import", action="store_true", help="insert all checked rows in one transaction")
    modes.add_argument("--verify-existing", action="store_true", help="regenerate and compare the already imported rows exactly")
    args = parser.parse_args()

    connection = sqlite3.connect(args.database)
    connection.execute("PRAGMA foreign_keys = ON")
    next_id = connection.execute("SELECT COALESCE(MAX(matrix_id), 0) + 1 FROM matrices").fetchone()[0]
    count = strict_count = text_bytes = maximum = 0
    minimum_density = 1.0
    ids: list[int] = []

    try:
        with connection:
            for offset, (dimension, matrix, strict, source, family, row_maximum, density) in enumerate(generated_rows()):
                copositive = int(not family.endswith("/ not copositive"))
                count += 1
                strict_count += strict
                text_bytes += len(matrix)
                maximum = max(maximum, row_maximum)
                minimum_density = min(minimum_density, density)
                if args.verify_existing:
                    existing = connection.execute(
                        "SELECT matrix_id, dimension, matrix, is_strictly_copositive, is_copositive, family "
                        "FROM matrices WHERE source = ?",
                        (source,),
                    ).fetchall()
                    if len(existing) != 1 or existing[0][1:] != (dimension, matrix, strict, copositive, family):
                        raise RuntimeError(f"stored row does not exactly match regenerated {source}")
                    ids.append(existing[0][0])
                    continue
                overlap = connection.execute(
                    "SELECT matrix_id FROM matrices WHERE dimension = ? AND matrix = ?", (dimension, matrix)
                ).fetchone()
                if overlap:
                    raise RuntimeError(f"refusing import: generated matrix duplicates ID {overlap[0]}")
                if args.do_import:
                    matrix_id = next_id + offset
                    connection.execute(
                        "INSERT INTO matrices(matrix_id, dimension, matrix, is_strictly_copositive, is_copositive, source, family) "
                        "VALUES (?, ?, ?, ?, ?, ?, ?)",
                        (matrix_id, dimension, matrix, strict, copositive, source, family),
                    )
                    ids.append(matrix_id)
        assert count == len(DIMENSIONS) * 3 == 90
        assert strict_count == len(DIMENSIONS)
        action = "verified" if args.verify_existing else "inserted" if args.do_import else "checked"
        id_text = f" as IDs {min(ids)}-{max(ids)}" if ids else ""
        print(
            f"{action} {count} rows{id_text}; text_bytes={text_bytes}; max_abs_entry={maximum}; "
            f"minimum_density={minimum_density:.3%}"
        )
        if not args.do_import and not args.verify_existing:
            print("pass --import to write them")
    finally:
        connection.close()


if __name__ == "__main__":
    main()
