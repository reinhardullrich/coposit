#!/usr/bin/env python3
"""Move large inline corpus matrices to their smallest exact Matrix Market form."""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import sqlite3

ARRAY_HEADER = "%%MatrixMarket matrix array integer symmetric\n"
COORDINATE_HEADER = "%%MatrixMarket matrix coordinate integer symmetric\n"


def _sha256(path: Path) -> str:
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def _array_entries(path: Path, dimension: int):
    with path.open("r", encoding="ascii") as source:
        if next(source, "").strip().lower() != ARRAY_HEADER.strip().lower():
            raise ValueError(f"expected symmetric integer array: {path}")
        data = (line.strip() for line in source if line.strip() and not line.lstrip().startswith("%"))
        if next(data, "").split() != [str(dimension), str(dimension)]:
            raise ValueError(f"wrong Matrix Market size: {path}")
        for column in range(dimension):
            for row in range(column, dimension):
                token = next(data, None)
                if token is None or len(token.split()) != 1:
                    raise ValueError(f"wrong Matrix Market value count: {path}")
                yield row, column, token
        if next(data, None) is not None:
            raise ValueError(f"wrong Matrix Market value count: {path}")


def _is_zero(token: str) -> bool:
    digits = token[1:] if token[:1] in ("+", "-") else token
    return bool(digits) and not digits.strip("0")


def _optimize_matrix_market(path: Path, dimension: int) -> str:
    with path.open("r", encoding="ascii") as source:
        header = next(source, "").strip().lower().split()
    if header == COORDINATE_HEADER.strip().lower().split():
        return "coordinate"
    if header != ARRAY_HEADER.strip().lower().split():
        raise ValueError(f"unsupported Matrix Market file: {path}")

    array_data_bytes = 0
    coordinate_data_bytes = 0
    nonzero_entries = 0
    for row, column, token in _array_entries(path, dimension):
        array_data_bytes += len(token) + 1
        if not _is_zero(token):
            nonzero_entries += 1
            coordinate_data_bytes += len(str(row + 1)) + len(str(column + 1)) + len(token) + 3

    array_bytes = len(ARRAY_HEADER) + len(f"{dimension} {dimension}\n") + array_data_bytes
    coordinate_bytes = len(COORDINATE_HEADER) + len(f"{dimension} {dimension} {nonzero_entries}\n") + coordinate_data_bytes
    selected = "coordinate" if coordinate_bytes < array_bytes else "array"
    temporary = path.with_suffix(".mtx.tmp")
    with temporary.open("w", encoding="ascii", newline="\n") as output:
        if selected == "coordinate":
            output.write(COORDINATE_HEADER)
            output.write(f"{dimension} {dimension} {nonzero_entries}\n")
            for row, column, token in _array_entries(path, dimension):
                if not _is_zero(token):
                    output.write(f"{row + 1} {column + 1} {token}\n")
        else:
            output.write(ARRAY_HEADER)
            output.write(f"{dimension} {dimension}\n")
            for _row, _column, token in _array_entries(path, dimension):
                output.write(f"{token}\n")
    os.replace(temporary, path)
    return selected


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--database",
        type=Path,
        default=Path(__file__).resolve().parents[1] / "copos_testdata.sqlite3",
    )
    parser.add_argument("--threshold-bytes", type=int, default=500_000)
    arguments = parser.parse_args()
    if arguments.threshold_bytes < 1:
        parser.error("--threshold-bytes must be positive")

    database = arguments.database.resolve()
    matrix_directory = database.parent / "matrices"
    matrix_directory.mkdir(exist_ok=True)
    connection = sqlite3.connect(database)
    connection.execute("PRAGMA foreign_keys = ON")
    try:
        rows = connection.execute(
            "SELECT matrix_id, dimension, matrix FROM matrices "
            "WHERE matrix NOT LIKE 'file:%' AND length(matrix) > ? ORDER BY matrix_id",
            (arguments.threshold_bytes,),
        ).fetchall()
        for matrix_id, dimension, values in rows:
            expected_values = dimension * (dimension + 1) // 2
            if values.count(",") + 1 != expected_values:
                raise ValueError(f"matrix {matrix_id} has the wrong packed value count")

            relative_path = Path("matrices") / f"{matrix_id}.mtx"
            target = database.parent / relative_path
            temporary = target.with_suffix(".mtx.tmp")
            with temporary.open("w", encoding="ascii", newline="\n") as output:
                output.write(ARRAY_HEADER)
                output.write(f"{dimension} {dimension}\n")
                output.write(values.replace(",", "\n"))
                output.write("\n")
            os.replace(temporary, target)
            connection.execute(
                "UPDATE matrices SET matrix = ?, file_sha256 = ? WHERE matrix_id = ?",
                (f"file:{relative_path.as_posix()}", _sha256(target), matrix_id),
            )

        formats = {"array": 0, "coordinate": 0}
        references = connection.execute(
            "SELECT matrix_id, dimension, matrix FROM matrices WHERE matrix LIKE 'file:%' ORDER BY matrix_id"
        ).fetchall()
        for matrix_id, dimension, reference in references:
            path = database.parent / reference.removeprefix("file:")
            selected_format = _optimize_matrix_market(path, dimension)
            formats[selected_format] += 1
            connection.execute("UPDATE matrices SET file_sha256 = ? WHERE matrix_id = ?", (_sha256(path), matrix_id))
        connection.commit()
        if rows:
            connection.execute("VACUUM")
        print(
            f"externalized={len(rows)} array={formats['array']} coordinate={formats['coordinate']} "
            f"threshold_bytes={arguments.threshold_bytes} directory={matrix_directory}"
        )
    finally:
        connection.close()


if __name__ == "__main__":
    main()
