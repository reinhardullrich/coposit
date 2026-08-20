#!/usr/bin/env python3
"""Import the 750 extended BPQY Julia-1.8.5 numerical matrix materializations."""

from __future__ import annotations

import argparse
import hashlib
import math
import sqlite3
from pathlib import Path

from deduplicate_literature_import_2026_08_14 import load_values


PROJECT = Path(__file__).resolve().parents[2]
DATABASE = PROJECT / "testdata/copos_testdata.sqlite3"
DATA = Path(__file__).with_name("bpqy_julia185_extension_2026_08_19.tsv")
SOURCE_TITLE = "Tighter yet more tractable relaxations and nontrivial instance generation for sparse standard quadratic optimization"
MARKER = "import_batch=bpqy-julia185-float-extension-2026-08-19;"
DIMENSIONS = (20, 30, 35, 40, 45)
SUPPORTS = {
    20: (5, 10, 15),
    30: (8, 15, 22),
    35: (9, 18, 26),
    40: (10, 20, 30),
    45: (11, 22, 34),
}


def read_records():
    lines = DATA.read_text().splitlines()
    if not lines or lines[0] != "class\tdimension\trho0\tseed\tprimitive_upper":
        raise ValueError("wrong extended BPQY artifact header")
    records = []
    keys = set()
    for line in lines[1:]:
        matrix_class, n_text, rho0_text, seed_text, values_text = line.split("\t")
        n, rho0, seed = int(n_text), int(rho0_text), int(seed_text)
        values = tuple(map(int, values_text.split(",")))
        key = matrix_class, n, rho0, seed
        if key in keys or matrix_class not in {"COP", "PSD"}:
            raise ValueError(f"duplicate or invalid extended BPQY key: {key}")
        if len(values) != n * (n + 1) // 2 or math.gcd(*map(abs, values)) != 1:
            raise ValueError(f"invalid primitive upper triangle: {key}")
        keys.add(key)
        records.append((key, values))
    expected = {
        (matrix_class, n, rho0, seed)
        for matrix_class in ("COP", "PSD")
        for n in DIMENSIONS
        for rho0 in SUPPORTS[n]
        for seed in range(1, 26)
    }
    if keys != expected:
        raise ValueError(f"wrong extended BPQY key set: got {len(keys)}, expected {len(expected)}")
    if len({values for _, values in records}) != 750:
        raise ValueError("duplicate exact extended BPQY materializations")
    return records


def assert_no_direct_scaling_duplicates(connection: sqlite3.Connection, records) -> None:
    existing = {
        n: {load_values(n, storage) for (storage,) in connection.execute("SELECT matrix FROM matrices WHERE dimension=?", (n,))}
        for n in DIMENSIONS
    }
    duplicate = next((key for key, values in records if values in existing[key[1]]), None)
    if duplicate:
        raise ValueError(f"extended BPQY {duplicate} duplicates an existing matrix by direct positive scaling")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apply", action="store_true")
    arguments = parser.parse_args()
    records = read_records()
    with sqlite3.connect(DATABASE) as connection:
        connection.execute("PRAGMA foreign_keys=ON")
        source = connection.execute("SELECT source_id FROM sources WHERE title=?", (SOURCE_TITLE,)).fetchone()
        if not source:
            raise ValueError(f"missing source row: {SOURCE_TITLE}")
        existing = connection.execute("SELECT count(*) FROM matrices WHERE source LIKE ?", (MARKER + "%",)).fetchone()[0]
        if existing not in (0, 750):
            raise ValueError(f"partial extended BPQY import: {existing}/750")
        if arguments.apply and existing == 0:
            connection.execute("BEGIN IMMEDIATE")
            assert_no_direct_scaling_duplicates(connection, records)
            first_id = connection.execute("SELECT max(matrix_id)+1 FROM matrices").fetchone()[0]
            for offset, ((matrix_class, n, rho0, seed), values) in enumerate(records):
                evidence = (
                    "Unmodified BPQY Julia 1.8.5 Float64 recipe extended to a new order; three paper-style support sizes and 25 seeds; "
                    "upper Float64 triangle lifted exactly to a primitive dyadic integer representative. The intended construction "
                    "is copositive with a boundary zero, but the exact integer materialization is deliberately left unclassified."
                )
                connection.execute(
                    """INSERT INTO matrices(matrix_id,dimension,matrix,is_strictly_copositive,is_copositive,source,source_id,family)
                       VALUES (?,?,?,?,?,?,?,?)""",
                    (
                        first_id + offset,
                        n,
                        ",".join(map(str, values)),
                        None,
                        None,
                        f"{MARKER} instance={matrix_class}-n{n}-rho0-{rho0}-seed-{seed}; {evidence}",
                        source[0],
                        f"BPQY {matrix_class} intended copositive-boundary extension",
                    ),
                )
        stored = connection.execute("SELECT count(*) FROM matrices WHERE source LIKE ?", (MARKER + "%",)).fetchone()[0]
        integrity = connection.execute("PRAGMA integrity_check").fetchone()[0]
        if integrity != "ok":
            raise ValueError(f"SQLite integrity check failed: {integrity}")
    digest = hashlib.sha256(DATA.read_bytes()).hexdigest()
    print(f"rows={len(records)} stored={stored} apply={int(arguments.apply)} artifact_sha256={digest} integrity={integrity}")


if __name__ == "__main__":
    main()
