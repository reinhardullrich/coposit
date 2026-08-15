#!/usr/bin/env python3
"""Import the eligible materializable matrix occurrences from the frozen literature catalog, without deduplication."""

from __future__ import annotations

import argparse
import csv
import gzip
import hashlib
import io
import math
import os
import re
import sqlite3
import zipfile
from collections import Counter
from fractions import Fraction
from functools import reduce
from pathlib import Path

import numpy as np
from scipy.io import loadmat


PROJECT = Path(__file__).resolve().parents[2]
DATABASE = PROJECT / "testdata/copos_testdata.sqlite3"
CATALOG = PROJECT / "research/data/copositive_optimization_matrix_extraction_2026-08-14"
MANIFEST = CATALOG / "instances.csv"
SOURCES = CATALOG / "sources.csv"
QUADPROG_ARCHIVE = CATALOG / "upstream/quadprogIP/QuadProgBB_instances.zip"
EXPECTED_EXISTING = 2442
EXPECTED_MATERIALIZABLE = 1838
EXPECTED_EXCLUDED = 790
EXPECTED_IMPORTED = EXPECTED_MATERIALIZABLE - EXPECTED_EXCLUDED
FIRST_IMPORTED_ID = 10685
EXTERNAL_THRESHOLD = 500_000
RAW_QP_SOURCE_KEYS = {
    "bomze_locatelli_tardella_2008",
    "vandenbussche_nemhauser_2005",
    "chen_burer_2012",
}

DIRECT_REPRESENTATIONS = {
    "compact_integer_upper",
    "exact_integer_matrix",
    "exact_decimal_matrix",
    "dimacs_binary_graph",
    "dimacs_binary_graph_transform",
    "graph_file",
    "matrix_market_file",
    "matrix_market_graph",
    "matlab_v5_archive_member_matrix",
    "matlab_v5_graph_matrix",
    "hex_encoded_matlab_v5_qp",
    "mps_quadratic_matrix",
    "quadratic_program_file",
    "box_qp_file",
    "quadratic_program_archive_member",
}


def fraction(token: object, *, binary_float: bool = False) -> Fraction:
    if isinstance(token, (int, np.integer)):
        return Fraction(int(token))
    if isinstance(token, (float, np.floating)):
        value = float(token)
        if not math.isfinite(value):
            raise ValueError(f"non-finite matrix entry: {value}")
        return Fraction.from_float(value) if binary_float else Fraction(str(value))
    return Fraction(str(token).replace("D", "E").replace("d", "e"))


def packed_index(i: int, j: int, n: int) -> int:
    if i > j:
        i, j = j, i
    return i * n - i * (i - 1) // 2 + j - i


def primitive(values: list[Fraction | int]) -> tuple[int, ...]:
    denominator = 1
    for value in values:
        denominator = math.lcm(denominator, value.denominator if isinstance(value, Fraction) else 1)
    integers = [
        value.numerator * (denominator // value.denominator) if isinstance(value, Fraction) else value * denominator
        for value in values
    ]
    divisor = reduce(math.gcd, map(abs, integers)) or 1
    return tuple(value // divisor for value in integers)


def pack_rows(rows: list[list[Fraction | int]]) -> tuple[int, tuple[int, ...]]:
    n = len(rows)
    if not n or any(len(row) != n for row in rows):
        raise ValueError("matrix is not nonempty and square")
    if any(rows[i][j] != rows[j][i] for i in range(n) for j in range(i + 1, n)):
        raise ValueError("matrix is not symmetric")
    return n, primitive([rows[i][j] for i in range(n) for j in range(i, n)])


def pack_numpy(array: object) -> tuple[int, tuple[int, ...]]:
    if hasattr(array, "toarray"):
        array = array.toarray()
    values = np.asarray(array)
    if values.ndim != 2 or values.shape[0] == 0 or values.shape[0] != values.shape[1]:
        raise ValueError(f"array is not a nonempty square matrix: {values.shape}")
    n = int(values.shape[0])
    if any(values[i, j] != values[j, i] for i in range(n) for j in range(i + 1, n)):
        raise ValueError("stored array is not exactly symmetric")
    return n, primitive([fraction(values[i, j], binary_float=np.issubdtype(values.dtype, np.floating))
                         for i in range(n) for j in range(i, n)])


def compact_matrix(path: Path) -> tuple[int, tuple[int, ...]]:
    dimension, separator, payload = path.read_text().strip().partition("#")
    if separator != "#":
        raise ValueError(f"invalid compact matrix: {path}")
    n = int(dimension)
    values = tuple(map(int, payload.split(",")))
    if len(values) != n * (n + 1) // 2:
        raise ValueError(f"wrong packed value count: {path}")
    return n, values


def dense_text_matrix(path: Path) -> tuple[int, tuple[int, ...]]:
    rows = [
        [fraction(token) for token in re.split(r"[\s,]+", line.strip()) if token]
        for line in path.read_text().splitlines() if line.strip()
    ]
    return pack_rows(rows)


def dimacs_binary_matrix(path: Path, parameter: int | None = None) -> tuple[int, tuple[int, ...]]:
    with path.open("rb") as stream:
        prefix = bytearray()
        while not prefix.endswith(b"\n"):
            byte = stream.read(1)
            if not byte:
                raise ValueError(f"missing DIMACS preamble length: {path}")
            prefix.extend(byte)
        preamble = stream.read(int(prefix))
        match = re.search(rb"^p\s+(?:edge|col)\s+(\d+)\s+(\d+)", preamble, re.MULTILINE)
        if not match:
            raise ValueError(f"missing DIMACS graph size: {path}")
        n, expected_edges = map(int, match.groups())
        adjacency = [[0] * n for _ in range(n)]
        edges = 0
        for i in range(n):
            row = stream.read((i + 8) // 8)
            if len(row) != (i + 8) // 8:
                raise ValueError(f"short DIMACS row {i}: {path}")
            for j in range(i):
                if row[j >> 3] & (1 << (7 - (j & 7))):
                    adjacency[i][j] = adjacency[j][i] = 1
                    edges += 1
        # DSJC500.5 and DSJC1000.5 report twice the undirected edge count in their retained preambles.
        if edges != expected_edges and 2 * edges != expected_edges:
            raise ValueError(f"wrong DIMACS edge count: {path}")
    return n, primitive([
        adjacency[i][j] if parameter is None else parameter * (1 - adjacency[i][j]) - 1
        for i in range(n) for j in range(i, n)
    ])


def matrix_market(path: Path) -> tuple[int, tuple[int, ...]]:
    with path.open() as stream:
        header = stream.readline().lower().split()
        if len(header) != 5 or header[:3] != ["%%matrixmarket", "matrix", "coordinate"]:
            raise ValueError(f"unsupported Matrix Market header: {path}")
        field, symmetry = header[3:]
        shape = next(line for line in stream if line.strip() and not line.lstrip().startswith("%"))
        n, columns, count = map(int, shape.split())
        if n != columns:
            raise ValueError(f"Matrix Market input is rectangular: {path}")
        entries: dict[tuple[int, int], Fraction] = {}
        read_count = 0
        for line in stream:
            tokens = line.split()
            if not tokens:
                continue
            read_count += 1
            i, j = int(tokens[0]) - 1, int(tokens[1]) - 1
            value = Fraction(1) if field == "pattern" else fraction(tokens[2])
            entries[(i, j)] = entries.get((i, j), Fraction(0)) + value
        if read_count != count:
            raise ValueError(f"wrong Matrix Market entry count: {path}")
    values = [Fraction(0)] * (n * (n + 1) // 2)
    if symmetry == "symmetric":
        for (i, j), value in entries.items():
            if i != j and (j, i) in entries:
                raise ValueError(f"symmetric Matrix Market input stores both triangles: {path}")
            values[packed_index(i, j, n)] += value
    elif symmetry == "general":
        for i in range(n):
            for j in range(i, n):
                if entries.get((i, j), Fraction(0)) != entries.get((j, i), Fraction(0)):
                    raise ValueError(f"general Matrix Market input is not symmetric: {path}")
                values[packed_index(i, j, n)] = entries.get((i, j), Fraction(0))
    else:
        raise ValueError(f"unsupported Matrix Market symmetry: {path}")
    return n, primitive(values)


def mps_quadratic_matrix(path: Path) -> tuple[int, tuple[int, ...]]:
    opener = gzip.open if path.suffix == ".gz" else open
    columns: dict[str, int] = {}
    quadratic: dict[tuple[str, str], Fraction] = {}
    section = ""
    with opener(path, "rt", errors="strict") as stream:
        for line in stream:
            tokens = line.split()
            if not tokens or line.lstrip().startswith("*"):
                continue
            marker = tokens[0].upper()
            if marker in {"NAME", "ROWS", "COLUMNS", "RHS", "RANGES", "BOUNDS", "QMATRIX", "ENDATA"}:
                section = marker
                continue
            if section == "COLUMNS" and "'MARKER'" not in line:
                columns.setdefault(tokens[0], len(columns))
            elif section == "QMATRIX":
                left, right, token = tokens[:3]
                columns.setdefault(left, len(columns))
                columns.setdefault(right, len(columns))
                key = tuple(sorted((left, right)))
                value = fraction(token)
                if key in quadratic and quadratic[key] != value:
                    raise ValueError(f"conflicting symmetric MPS entries in {path}: {key}")
                quadratic[key] = value
    n = len(columns)
    values = [Fraction(0)] * (n * (n + 1) // 2)
    for (left, right), value in quadratic.items():
        values[packed_index(columns[left], columns[right], n)] = value
    return n, primitive(values)


def qplib_matrix(path: Path) -> tuple[int, tuple[int, ...]]:
    lines = [line.split("#", 1)[0].strip() for line in path.read_text().splitlines()]
    lines = [line for line in lines if line]
    n = int(lines[3])
    term_count = int(lines[5])
    values = [Fraction(0)] * (n * (n + 1) // 2)
    for line in lines[6:6 + term_count]:
        i, j, token = line.split()[:3]
        values[packed_index(int(i) - 1, int(j) - 1, n)] += fraction(token)
    return n, primitive(values)


def box_qp_matrix(path: Path) -> tuple[int, tuple[int, ...]]:
    lines = [line.strip() for line in path.read_text().splitlines() if line.strip()]
    n = int(lines[0])
    rows = [[fraction(token) for token in line.split()] for line in lines[1:1 + n]]
    if len(rows) != n or any(len(row) != n for row in rows):
        raise ValueError(f"BoxQP objective matrix is not {n}-by-{n}: {path}")
    return n, primitive([(rows[i][j] + rows[j][i]) / 2 for i in range(n) for j in range(i, n)])


def matlab_matrix(data: bytes, variable: str) -> tuple[int, tuple[int, ...]]:
    variables = loadmat(io.BytesIO(data))
    if variable not in variables:
        raise ValueError(f"MATLAB variable {variable!r} not found")
    return pack_numpy(variables[variable])


class QuadProgMatrices:
    def __init__(self, archive: Path) -> None:
        self.archive = zipfile.ZipFile(archive)
        self.by_stem: dict[str, str] = {}
        self.cache: dict[str, tuple[int, tuple[int, ...]]] = {}
        for name in self.archive.namelist():
            if not name.endswith(".mat"):
                continue
            variables = loadmat(io.BytesIO(self.archive.read(name)))
            matrix = variables.get("H")
            if matrix is not None and getattr(matrix, "shape", (0, 0))[0] > 0:
                self.by_stem[Path(name).stem] = name

    def matrix(self, member: str) -> tuple[int, tuple[int, ...]] | None:
        stem = Path(member).stem.removesuffix("_tr")
        matrix_member = self.by_stem.get(stem)
        if matrix_member is None:
            return None
        if stem not in self.cache:
            self.cache[stem] = matlab_matrix(self.archive.read(matrix_member), "H")
        return self.cache[stem]

    def close(self) -> None:
        self.archive.close()


def materialize(row: dict[str, str], quadprog: QuadProgMatrices) -> tuple[int, tuple[int, ...]] | None:
    representation = row["representation"]
    path = CATALOG / row["path"]
    if representation == "compact_integer_upper":
        return compact_matrix(path)
    if representation in {"exact_integer_matrix", "exact_decimal_matrix", "graph_file"}:
        return dense_text_matrix(path)
    if representation == "dimacs_binary_graph":
        return dimacs_binary_matrix(path)
    if representation == "dimacs_binary_graph_transform":
        parameter = int(re.search(r"A=(\d+)\*", row["note"]).group(1))
        return dimacs_binary_matrix(path, parameter)
    if representation in {"matrix_market_file", "matrix_market_graph"}:
        return matrix_market(path)
    if representation == "matlab_v5_archive_member_matrix":
        if "symmetric=no" in row["note"]:
            return None
        member = re.search(r"internal_member=([^;]+)", row["note"]).group(1)
        with zipfile.ZipFile(path) as archive:
            return matlab_matrix(archive.read(member), "Q")
    if representation == "matlab_v5_graph_matrix":
        return matlab_matrix(path.read_bytes(), "A")
    if representation == "hex_encoded_matlab_v5_qp":
        return matlab_matrix(bytes.fromhex("".join(path.read_text().split())), "H")
    if representation == "mps_quadratic_matrix":
        return mps_quadratic_matrix(path)
    if representation == "quadratic_program_file":
        return qplib_matrix(path)
    if representation == "box_qp_file":
        return box_qp_matrix(path)
    if representation == "quadratic_program_archive_member":
        member = re.search(r"internal_member=([^;]+)", row["note"]).group(1)
        return quadprog.matrix(member)
    raise ValueError(f"no materializer for {representation}")


def source_text(row: dict[str, str]) -> str:
    fields = [
        f"catalog_instance_id={row['instance_id']}",
        f"paper_locator={row['paper_locator']}",
        f"paper_usage={row['paper_usage']}",
        f"retained_path={row['path']}",
    ]
    if row["note"]:
        fields.append(f"note={row['note']}")
    if row["representation"] == "box_qp_file":
        fields.append("stored_matrix=exact symmetric part (Q+Q^T)/2 of the quadratic objective")
    return "; ".join(field.replace("\n", " ") for field in fields)


def write_storage(matrix_id: int, n: int, values: tuple[int, ...]) -> tuple[str, str | None, Path | None]:
    value_strings = tuple(map(str, values))
    if sum(map(len, value_strings)) + len(value_strings) - 1 <= EXTERNAL_THRESHOLD:
        return ",".join(value_strings), None, None

    matrix_directory = DATABASE.parent / "matrices"
    matrix_directory.mkdir(exist_ok=True)
    path = matrix_directory / f"{matrix_id}.mtx"
    if path.exists():
        raise FileExistsError(path)
    temporary = path.with_suffix(".mtx.tmp")
    nonzero_count = sum(value != 0 for value in values)
    array_bytes = sum(len(value) + 1 for value in value_strings)
    coordinate_bytes = sum(
        len(str(j + 1)) + len(str(i + 1)) + len(str(value)) + 3
        for i in range(n) for j in range(i, n)
        if (value := values[packed_index(i, j, n)]) != 0
    )
    try:
        with temporary.open("w", encoding="ascii", newline="\n") as output:
            if coordinate_bytes < array_bytes:
                output.write("%%MatrixMarket matrix coordinate integer symmetric\n")
                output.write(f"{n} {n} {nonzero_count}\n")
                for i in range(n):
                    for j in range(i, n):
                        value = values[packed_index(i, j, n)]
                        if value:
                            output.write(f"{j + 1} {i + 1} {value}\n")
            else:
                output.write("%%MatrixMarket matrix array integer symmetric\n")
                output.write(f"{n} {n}\n")
                output.write("\n".join(value_strings))
                output.write("\n")
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)
    with path.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    return f"file:matrices/{matrix_id}.mtx", digest, path


def rebuild_matrices_for_unknown_truth(connection: sqlite3.Connection) -> None:
    table_info = list(connection.execute("PRAGMA table_info(matrices)"))
    strict_info = next(row for row in table_info if row[1] == "is_strictly_copositive")
    if not strict_info[3]:
        return
    has_additional_sources = any(row[1] == "additional_source_ids" for row in table_info)
    has_solved_references = any(row[1] == "references_solved" for row in table_info)
    connection.execute("""
        CREATE TABLE matrices_with_unknown_truth (
            matrix_id INTEGER PRIMARY KEY,
            dimension INTEGER NOT NULL CHECK(dimension > 0),
            matrix TEXT NOT NULL CHECK(length(matrix) > 0),
            file_sha256 TEXT CHECK(
                (matrix NOT LIKE 'file:%' AND file_sha256 IS NULL)
                OR (matrix LIKE 'file:%' AND file_sha256 IS NOT NULL
                    AND length(file_sha256) = 64 AND file_sha256 NOT GLOB '*[^0-9a-f]*')
            ),
            is_strictly_copositive INTEGER CHECK(is_strictly_copositive IS NULL OR is_strictly_copositive IN (0, 1)),
            is_copositive INTEGER CHECK(is_copositive IS NULL OR is_copositive IN (0, 1)),
            source TEXT,
            source_id INTEGER REFERENCES sources(source_id),
            family TEXT,
            smoke_set INTEGER NOT NULL DEFAULT 0 CHECK(smoke_set IN (0, 1)),
            representative_core INTEGER NOT NULL DEFAULT 0 CHECK(representative_core IN (0, 1)),
            stress_test INTEGER NOT NULL DEFAULT 0 CHECK(stress_test IN (0, 1)),
            scale_set INTEGER NOT NULL DEFAULT 0 CHECK(scale_set IN (0, 1)),
            timeout_5s_strict_set INTEGER NOT NULL DEFAULT 0 CHECK(timeout_5s_strict_set IN (0, 1)),
            additional_source_ids TEXT NOT NULL DEFAULT '[]'
                CHECK(json_valid(additional_source_ids) AND json_type(additional_source_ids) = 'array'),
            references_solved TEXT NOT NULL DEFAULT '[]'
                CHECK(json_valid(references_solved) AND json_type(references_solved) = 'array'),
            CHECK(is_strictly_copositive IS NULL OR is_strictly_copositive = 0 OR is_copositive IS 1)
        ) STRICT
    """)
    columns = """matrix_id, dimension, matrix, file_sha256, is_strictly_copositive, is_copositive, source, source_id, family,
                 smoke_set, representative_core, stress_test, scale_set, timeout_5s_strict_set, additional_source_ids,
                 references_solved"""
    selected_columns = columns
    if not has_additional_sources:
        selected_columns = selected_columns.replace("additional_source_ids", "'[]'")
    if not has_solved_references:
        selected_columns = selected_columns.replace("references_solved", "'[]'")
    connection.execute(f"INSERT INTO matrices_with_unknown_truth ({columns}) SELECT {selected_columns} FROM matrices")
    connection.execute("DROP TABLE matrices")
    connection.execute("ALTER TABLE matrices_with_unknown_truth RENAME TO matrices")


def selected_rows(rows: list[dict[str, str]], quadprog: QuadProgMatrices):
    for row in rows:
        preimport_match = any(int(matrix_id) < FIRST_IMPORTED_ID for matrix_id in re.findall(r"\d+", row["corpus_matrix_ids"]))
        if preimport_match or row["representation"] not in DIRECT_REPRESENTATIONS:
            continue
        matrix = materialize(row, quadprog)
        if matrix is not None:
            yield row, matrix


def excluded_raw_qp(row: dict[str, str], n: int, values: tuple[int, ...]) -> bool:
    """Keep raw QP objectives in the literature catalog, but not trivial negative-diagonal cases in the solver corpus."""
    return row["source_key"] in RAW_QP_SOURCE_KEYS and any(values[packed_index(i, i, n)] < 0 for i in range(n))


def validate_sources(connection: sqlite3.Connection) -> dict[str, int]:
    source_rows = list(csv.DictReader(SOURCES.open()))
    if len(source_rows) != 78:
        raise ValueError("expected 78 catalog sources")
    database_sources = {
        source_id: (authors, title, reference)
        for source_id, authors, title, reference in connection.execute(
            "SELECT source_id, authors, title, reference FROM sources WHERE source_id <= 78"
        )
    }
    source_ids = {}
    for source_id, row in enumerate(source_rows, 1):
        if database_sources.get(source_id) != (row["authors"], row["title"], row["reference"]):
            raise ValueError(f"catalog source {source_id} does not match the database")
        source_ids[row["source_key"]] = source_id
    return source_ids


def dry_run(rows: list[dict[str, str]], quadprog: QuadProgMatrices) -> None:
    counts = Counter()
    total_values = 0
    imported = 0
    materializable = 0
    excluded = 0
    for row, (n, values) in selected_rows(rows, quadprog):
        materializable += 1
        if len(values) != n * (n + 1) // 2:
            raise ValueError(f"wrong packed value count for {row['instance_id']}")
        if excluded_raw_qp(row, n, values):
            excluded += 1
            continue
        counts[row["representation"]] += 1
        total_values += len(values)
        imported += 1
        if imported % 100 == 0:
            print(f"validated {imported}/{EXPECTED_IMPORTED}", flush=True)
    if (materializable, excluded, imported) != (EXPECTED_MATERIALIZABLE, EXPECTED_EXCLUDED, EXPECTED_IMPORTED):
        raise ValueError(f"expected {EXPECTED_MATERIALIZABLE} materializable, {EXPECTED_EXCLUDED} excluded, and "
                         f"{EXPECTED_IMPORTED} imported rows; found {materializable}, {excluded}, and {imported}")
    print(f"dry_run=ok matrices={imported} excluded_raw_qp={excluded} packed_values={total_values}")
    for representation, count in sorted(counts.items()):
        print(f"{representation}={count}")


def import_rows(connection: sqlite3.Connection, rows: list[dict[str, str]], source_ids: dict[str, int],
                quadprog: QuadProgMatrices) -> list[Path]:
    created_files: list[Path] = []
    first_id = connection.execute("SELECT max(matrix_id) + 1 FROM matrices").fetchone()[0]
    if first_id != FIRST_IMPORTED_ID:
        raise ValueError(f"expected first imported matrix ID {FIRST_IMPORTED_ID}, found {first_id}")
    imported = 0
    excluded = 0
    materializable = 0
    for materializable, (row, (n, values)) in enumerate(selected_rows(rows, quadprog), 1):
        matrix_id = first_id + materializable - 1
        if excluded_raw_qp(row, n, values):
            excluded += 1
            continue
        storage, digest, path = write_storage(matrix_id, n, values)
        if path is not None:
            created_files.append(path)
        connection.execute(
            """INSERT INTO matrices(
                   matrix_id, dimension, matrix, file_sha256, is_strictly_copositive, is_copositive, source, source_id, family
               ) VALUES (?, ?, ?, ?, NULL, NULL, ?, ?, NULL)""",
            (matrix_id, n, storage, digest, source_text(row), source_ids[row["source_key"]]),
        )
        imported += 1
        if imported % 100 == 0:
            print(f"inserted {imported}/{EXPECTED_IMPORTED}", flush=True)
    if (materializable, excluded, imported) != (EXPECTED_MATERIALIZABLE, EXPECTED_EXCLUDED, EXPECTED_IMPORTED):
        raise ValueError(f"expected {EXPECTED_MATERIALIZABLE} materializable, {EXPECTED_EXCLUDED} excluded, and "
                         f"{EXPECTED_IMPORTED} imported rows; found {materializable}, {excluded}, and {imported}")
    return created_files


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dry-run", action="store_true")
    arguments = parser.parse_args()
    rows = list(csv.DictReader(MANIFEST.open()))
    if len(rows) != 3290:
        raise ValueError(f"expected 3290 catalog records, found {len(rows)}")

    quadprog = QuadProgMatrices(QUADPROG_ARCHIVE)
    try:
        with sqlite3.connect(DATABASE) as connection:
            source_ids = validate_sources(connection)
            if arguments.dry_run:
                dry_run(rows, quadprog)
                return

            existing_instances = {
                instance
                for (source,) in connection.execute("SELECT source FROM matrices WHERE source LIKE '%catalog_instance_id=%'")
                for instance in re.findall(r"catalog_instance_id=([^;|]+)", source or "")
            }
            if existing_instances:
                if len(existing_instances) != EXPECTED_IMPORTED:
                    raise ValueError(f"partial catalog import found: {len(existing_instances)}/{EXPECTED_IMPORTED} occurrences")
                print(f"all {EXPECTED_IMPORTED} eligible catalog matrix occurrences are already represented")
                return
            if connection.execute("SELECT count(*) FROM matrices").fetchone()[0] != EXPECTED_EXISTING:
                raise ValueError("unexpected pre-import matrix count")

            connection.execute("PRAGMA foreign_keys=OFF")
            connection.execute("BEGIN IMMEDIATE")
            created_files: list[Path] = []
            try:
                rebuild_matrices_for_unknown_truth(connection)
                created_files = import_rows(connection, rows, source_ids, quadprog)
                if connection.execute("SELECT count(*) FROM matrices").fetchone()[0] != EXPECTED_EXISTING + EXPECTED_IMPORTED:
                    raise ValueError("wrong post-import matrix count")
                if list(connection.execute("PRAGMA foreign_key_check")):
                    raise ValueError("foreign-key check failed after import")
                connection.commit()
            except Exception:
                connection.rollback()
                for path in created_files:
                    path.unlink(missing_ok=True)
                raise
            finally:
                connection.execute("PRAGMA foreign_keys=ON")

        print(f"imported={EXPECTED_IMPORTED} excluded_raw_qp={EXPECTED_EXCLUDED} "
              f"matrix_id_slots={FIRST_IMPORTED_ID}..{FIRST_IMPORTED_ID + EXPECTED_MATERIALIZABLE - 1}")
    finally:
        quadprog.close()


if __name__ == "__main__":
    main()
