#!/usr/bin/env python3
"""Import missing negated FracESSA game matrices up to positive scaling."""

from __future__ import annotations

import argparse
import hashlib
import math
import os
import sqlite3
import sys
from collections import defaultdict
from datetime import datetime, timezone
from fractions import Fraction
from pathlib import Path

from deduplicate_literature_import_2026_08_14 import load_values


PROJECT = Path(__file__).resolve().parents[2]
DATABASE = PROJECT / "testdata/copos_testdata.sqlite3"
FRACESSA_DATABASE = PROJECT.parent / "fracessa/testdata/fracessa_testdata.sqlite3"
SOURCE_TITLE = "FracESSA exact test and regression corpus"
MARKER = "import_batch=negated-fracessa-games-2026-08-20;"
EXTERNAL_THRESHOLD = 500_000
EXPECTED_SOURCE_ROWS = 1_411
EXPECTED_SOURCE_SHA256 = "30cc6483e4b737948764f10819ea29139aca98ea87a3ebf785358a3ca48999b6"

sys.set_int_max_str_digits(0)


def primitive(values: tuple[Fraction, ...]) -> tuple[int, ...]:
    denominator = math.lcm(*(value.denominator for value in values))
    integers = tuple(value.numerator * (denominator // value.denominator) for value in values)
    divisor = math.gcd(*(abs(value) for value in integers)) or 1
    return tuple(value // divisor for value in integers)


def fracessa_upper(dimension: int, is_circular: int, text: str) -> tuple[Fraction, ...]:
    values = tuple(Fraction(token) for token in text.split(","))
    packed_size = dimension * (dimension + 1) // 2
    if is_circular:
        expected = dimension // 2 + 1
        if len(values) != expected:
            raise ValueError(f"wrong compact circular length for order {dimension}: {len(values)} != {expected}")
        return tuple(
            values[min(column - row, dimension - (column - row))]
            for row in range(dimension)
            for column in range(row, dimension)
        )
    if len(values) == packed_size:
        return values
    if len(values) == dimension * dimension:
        if any(values[row * dimension + column] != values[column * dimension + row]
               for row in range(dimension) for column in range(row + 1, dimension)):
            raise ValueError(f"non-symmetric full FracESSA matrix of order {dimension}")
        return tuple(values[row * dimension + column]
                     for row in range(dimension) for column in range(row, dimension))
    raise ValueError(f"wrong FracESSA matrix length for order {dimension}: {len(values)}")


def source_groups(path: Path) -> tuple[dict[tuple[int, tuple[int, ...]], list[int]], str]:
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    groups: dict[tuple[int, tuple[int, ...]], list[int]] = defaultdict(list)
    with sqlite3.connect(f"file:{path}?mode=ro", uri=True) as connection:
        for matrix_id, dimension, is_circular, text in connection.execute(
            "SELECT matrix_id, dimension, is_cs, matrix FROM matrices ORDER BY matrix_id"
        ):
            negated = tuple(-value for value in fracessa_upper(dimension, is_circular, text))
            groups[(dimension, primitive(negated))].append(matrix_id)
    return groups, digest


def existing_keys(connection: sqlite3.Connection) -> dict[tuple[int, tuple[int, ...]], list[int]]:
    keys: dict[tuple[int, tuple[int, ...]], list[int]] = defaultdict(list)
    for matrix_id, dimension, storage in connection.execute(
        "SELECT matrix_id, dimension, matrix FROM matrices ORDER BY matrix_id"
    ):
        keys[(dimension, load_values(dimension, storage))].append(matrix_id)
    return keys


def matrix_market_payload(dimension: int, values: tuple[int, ...]) -> bytes:
    array = "%%MatrixMarket matrix array integer symmetric\n" + f"{dimension} {dimension}\n"
    array += "\n".join(map(str, values)) + "\n"
    coordinate_values = []
    offset = 0
    for row in range(dimension):
        for column in range(row, dimension):
            value = values[offset]
            offset += 1
            if value:
                coordinate_values.append(f"{column + 1} {row + 1} {value}")
    coordinate = "%%MatrixMarket matrix coordinate integer symmetric\n"
    coordinate += f"{dimension} {dimension} {len(coordinate_values)}\n"
    coordinate += "\n".join(coordinate_values) + ("\n" if coordinate_values else "")
    return min((array.encode("ascii"), coordinate.encode("ascii")), key=len)


def provenance(matrix_ids: list[int]) -> str:
    references = " | ".join(f"FracESSA:{matrix_id}" for matrix_id in matrix_ids)
    return f"{MARKER} {references} | exact negated game matrix reduced to primitive positive scale"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apply", action="store_true")
    parser.add_argument("--database", type=Path, default=DATABASE)
    parser.add_argument("--fracessa-database", type=Path, default=FRACESSA_DATABASE)
    arguments = parser.parse_args()

    database = arguments.database.resolve()
    fracessa_database = arguments.fracessa_database.resolve()
    groups, source_sha256 = source_groups(fracessa_database)
    source_rows = sum(map(len, groups.values()))
    if source_rows != EXPECTED_SOURCE_ROWS or source_sha256 != EXPECTED_SOURCE_SHA256:
        raise ValueError(f"unexpected FracESSA source snapshot: rows={source_rows} sha256={source_sha256}")
    with sqlite3.connect(database) as connection:
        connection.execute("PRAGMA foreign_keys=ON")
        source = connection.execute("SELECT source_id FROM sources WHERE title=?", (SOURCE_TITLE,)).fetchone()
        if not source:
            raise ValueError(f"missing source row: {SOURCE_TITLE}")
        existing = existing_keys(connection)
        existing_duplicate_groups = sum(len(ids) > 1 for ids in existing.values())
        planned = [(key, ids) for key, ids in groups.items() if key not in existing]
        matched_source_rows = sum(len(ids) for key, ids in groups.items() if key in existing)
        internal_merges = sum(len(ids) - 1 for _key, ids in planned)
        external_planned = sum(
            len(",".join(map(str, values))) > EXTERNAL_THRESHOLD for (dimension, values), _ids in planned
        )

        backup = None
        created_files: list[Path] = []
        if arguments.apply and planned:
            backup_directory = PROJECT / ".local/database-backups"
            backup_directory.mkdir(parents=True, exist_ok=True)
            timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
            backup = backup_directory / f"copos_testdata.before_negated_fracessa_{timestamp}.sqlite3"
            with sqlite3.connect(backup) as backup_connection:
                connection.backup(backup_connection)

            try:
                connection.execute("BEGIN IMMEDIATE")
                first_id = connection.execute("SELECT max(matrix_id)+1 FROM matrices").fetchone()[0]
                matrix_directory = database.parent / "matrices"
                matrix_directory.mkdir(exist_ok=True)
                for offset, ((dimension, values), matrix_ids) in enumerate(planned):
                    matrix_id = first_id + offset
                    packed = ",".join(map(str, values))
                    storage = packed
                    file_sha256 = None
                    if len(packed) > EXTERNAL_THRESHOLD:
                        payload = matrix_market_payload(dimension, values)
                        relative = Path("matrices") / f"{matrix_id}.mtx"
                        target = database.parent / relative
                        temporary = target.with_suffix(".mtx.tmp")
                        temporary.write_bytes(payload)
                        os.replace(temporary, target)
                        created_files.append(target)
                        storage = f"file:{relative.as_posix()}"
                        file_sha256 = hashlib.sha256(payload).hexdigest()
                    connection.execute(
                        """INSERT INTO matrices(
                               matrix_id, dimension, matrix, file_sha256,
                               is_strictly_copositive, is_copositive, source, source_id, family
                           ) VALUES (?, ?, ?, ?, NULL, NULL, ?, ?, ?)""",
                        (
                            matrix_id,
                            dimension,
                            storage,
                            file_sha256,
                            provenance(matrix_ids),
                            source[0],
                            "negated FracESSA game matrix",
                        ),
                    )
                connection.commit()
            except BaseException:
                connection.rollback()
                for path in created_files:
                    path.unlink(missing_ok=True)
                raise

        integrity = connection.execute("PRAGMA integrity_check").fetchone()[0]
        foreign_keys = connection.execute("PRAGMA foreign_key_check").fetchall()
        stored = connection.execute("SELECT count(*) FROM matrices WHERE source LIKE ?", (MARKER + "%",)).fetchone()[0]

    print(
        f"source_rows={source_rows} source_unique={len(groups)} "
        f"already_covered={matched_source_rows} insert_rows={len(planned)} internal_merges={internal_merges} "
        f"external_rows={external_planned} existing_duplicate_groups={existing_duplicate_groups} "
        f"stored_batch_rows={stored} apply={int(arguments.apply)} source_sha256={source_sha256} "
        f"integrity={integrity} foreign_key_errors={len(foreign_keys)} backup={backup}"
    )


if __name__ == "__main__":
    main()
