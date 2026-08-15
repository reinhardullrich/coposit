#!/usr/bin/env python3
"""Import the 450 BPQY Julia-1.8.5 numerical matrix materializations."""

from __future__ import annotations

import hashlib
import math
import sqlite3
from pathlib import Path

from deduplicate_literature_import_2026_08_14 import fingerprints, load_values, permutation_equivalent


PROJECT = Path(__file__).resolve().parents[2]
DATABASE = PROJECT / "testdata/copos_testdata.sqlite3"
DATA = Path(__file__).with_name("bpqy_julia185_matrices.tsv")
SOURCE_TITLE = "Tighter yet more tractable relaxations and nontrivial instance generation for sparse standard quadratic optimization"
MARKER = "import_batch=bpqy-julia185-float-materialization-2026-08-14;"


def read_records():
    lines = DATA.read_text().splitlines()
    if not lines or lines[0] != "class\tdimension\trho0\tseed\tprimitive_upper":
        raise ValueError("wrong BPQY artifact header")
    records = []
    keys = set()
    for line in lines[1:]:
        matrix_class, n_text, rho0_text, seed_text, values_text = line.split("\t")
        n, rho0, seed = int(n_text), int(rho0_text), int(seed_text)
        values = tuple(map(int, values_text.split(",")))
        key = matrix_class, n, rho0, seed
        if key in keys or matrix_class not in {"COP", "PSD", "SPN"}:
            raise ValueError(f"duplicate or invalid BPQY key: {key}")
        if len(values) != n * (n + 1) // 2 or math.gcd(*map(abs, values)) != 1:
            raise ValueError(f"invalid primitive upper triangle: {key}")
        keys.add(key)
        records.append((key, values))
    expected = {
        (matrix_class, n, rho0, seed)
        for matrix_class in ("COP", "PSD", "SPN")
        for n in (25, 50)
        for rho0 in ({25: (6, 12, 19), 50: (12, 25, 38)}[n])
        for seed in range(25)
    }
    if keys != expected:
        raise ValueError(f"wrong BPQY key set: got {len(keys)}, expected {len(expected)}")
    if len({hashlib.sha256(str(values).encode()).digest() for _, values in records}) != 450:
        raise ValueError("duplicate exact BPQY materializations")
    return records


def assert_not_existing(connection: sqlite3.Connection, records) -> None:
    by_dimension = {}
    for n in (25, 50):
        by_dimension[n] = [
            (matrix_id, values, fingerprints(n, values)[1])
            for matrix_id, storage in connection.execute("SELECT matrix_id,matrix FROM matrices WHERE dimension=?", (n,))
            for values in (load_values(n, storage),)
        ]
    for key, values in records:
        n = key[1]
        invariant = fingerprints(n, values)[1]
        for matrix_id, other, other_invariant in by_dimension[n]:
            if invariant == other_invariant and permutation_equivalent(n, values, other):
                raise ValueError(f"BPQY {key} duplicates existing matrix {matrix_id} under scaling/permutation")


def main() -> None:
    records = read_records()
    with sqlite3.connect(DATABASE) as connection:
        connection.execute("PRAGMA foreign_keys=ON")
        connection.execute("BEGIN IMMEDIATE")
        source = connection.execute("SELECT source_id FROM sources WHERE title=?", (SOURCE_TITLE,)).fetchone()
        if not source:
            raise ValueError(f"missing source row: {SOURCE_TITLE}")
        existing = connection.execute("SELECT count(*) FROM matrices WHERE source LIKE ?", (MARKER + "%",)).fetchone()[0]
        if existing not in (0, 450):
            raise ValueError(f"partial BPQY import: {existing}/450")
        if existing == 0:
            assert_not_existing(connection, records)
            first_id = connection.execute("SELECT max(matrix_id)+1 FROM matrices").fetchone()[0]
            for offset, ((matrix_class, n, rho0, seed), values) in enumerate(records):
                evidence = (
                    "Julia 1.8.5 seeded generator; solver-only packages removed without changing random calls; "
                    "upper Float64 triangle lifted exactly to a primitive dyadic integer representative. "
                    "The paper's intended construction is copositive with a designated boundary zero, but this rounded numerical "
                    "materialization is deliberately left unclassified."
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
                        f"BPQY {matrix_class} intended copositive-boundary generator",
                    ),
                )
        stored = connection.execute("SELECT count(*) FROM matrices WHERE source LIKE ?", (MARKER + "%",)).fetchone()[0]
        if stored != 450:
            raise ValueError(f"wrong stored BPQY count: {stored}")
        integrity = connection.execute("PRAGMA integrity_check").fetchone()[0]
        if integrity != "ok":
            raise ValueError(f"SQLite integrity check failed: {integrity}")
    print(f"rows=450 artifact_sha256={hashlib.sha256(DATA.read_bytes()).hexdigest()} integrity={integrity}")


if __name__ == "__main__":
    main()
