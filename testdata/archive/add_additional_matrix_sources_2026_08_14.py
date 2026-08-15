#!/usr/bin/env python3
"""Store each matrix's earliest known source and its other explicit literature references."""

from __future__ import annotations

import argparse
import csv
import json
import math
import re
import sqlite3
from collections import defaultdict
from pathlib import Path


PROJECT = Path(__file__).resolve().parents[2]
DATABASE = PROJECT / "testdata/copos_testdata.sqlite3"
CATALOG = PROJECT / "research/data/copositive_optimization_matrix_extraction_2026-08-14"
EXPECTED_MATRICES = 3490
EXPECTED_RETAINED = 3157
EXPECTED_SOURCES = 94
EXPECTED_PRIMARY_CHANGES = 138
EXPECTED_ADDITIONAL_ROWS = 641
EXPECTED_ADDITIONAL_LINKS = 1076

CURATED_MENTIONS = (
    ((9162,), ("burer_anstreicher_duer_2009", "nie_yang_zhang_2018", "anstreicher_2021", "ferreira_gao_nemeth_rigo_2024")),
    ((9163,), ("bras_eichfelder_judice_2016", "nie_yang_zhang_2018", "anstreicher_2021", "peng_2022",
                "ferreira_gao_nemeth_rigo_2024", "hildebrand_2018_support_two")),
    (range(9757, 9956), ("peng_2022", "hildebrand_2018_support_two")),
    ((9157,), ("bras_eichfelder_judice_2016", "ferreira_gao_nemeth_rigo_2024", "judice_sessa_fukushima_2024")),
    (range(9158, 9162), ("bras_eichfelder_judice_2016", "judice_sessa_fukushima_2024")),
    ((10696,), ("bomze_eichfelder_2013",)),
    (range(10706, 10710), ("bundfuss_duer_2009", "sponsel_bundfuss_duer_2012")),
    ((10707,), ("anstreicher_2021",)),
    (range(10767, 10774), ("sponsel_bundfuss_duer_2012",)),
    ((11794,), ("bundfuss_duer_2009", "sponsel_bundfuss_duer_2012", "anstreicher_2021")),
    (range(10997, 11007), ("badenbroek_deklerk_2021", "nishijima_poirion_takeda_2025")),
    (range(11007, 11027), ("gondzio_yildirim_2021",)),
    (range(11050, 11058), ("vargas_vera_dickinson_2025",)),
)


def projective_key(n: int, storage: str, file_sha256: str | None) -> tuple[str, int, str]:
    if storage.startswith("file:"):
        if file_sha256 is None:
            raise ValueError("external matrix has no file hash")
        return "file", n, file_sha256
    values = list(map(int, storage.split(",")))
    divisor = 0
    for value in values:
        divisor = math.gcd(divisor, value)
    divisor = divisor or 1
    return "inline", n, ",".join(str(value // divisor) for value in values)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dry-run", action="store_true")
    arguments = parser.parse_args()

    with sqlite3.connect(DATABASE) as connection:
        columns = {row[1] for row in connection.execute("PRAGMA table_info(matrices)")}
        has_column = "additional_source_ids" in columns
        select_additional = "additional_source_ids" if has_column else "'[]'"
        rows = list(connection.execute(
            f"SELECT matrix_id, dimension, matrix, file_sha256, source, source_id, {select_additional} FROM matrices ORDER BY matrix_id"
        ))
        source_rows = list(connection.execute(
            "SELECT source_id, authors, title, publication_year, reference FROM sources ORDER BY source_id"
        ))
        if len(rows) not in {EXPECTED_MATRICES, EXPECTED_RETAINED} or len(source_rows) != EXPECTED_SOURCES:
            raise ValueError(f"expected {EXPECTED_MATRICES} or {EXPECTED_RETAINED} matrices and {EXPECTED_SOURCES} sources")
        deduplicated = len(rows) == EXPECTED_RETAINED
        if any(row[5] is None for row in rows):
            raise ValueError("every matrix must have a primary source before adding references")

        catalog_sources = list(csv.DictReader((CATALOG / "sources.csv").open()))
        if len(catalog_sources) != 78:
            raise ValueError("expected 78 literature catalog sources")
        source_by_key = {}
        database_sources = {source_id: (authors, title, reference) for source_id, authors, title, _, reference in source_rows}
        for source_id, row in enumerate(catalog_sources, 1):
            if database_sources.get(source_id) != (row["authors"], row["title"], row["reference"]):
                raise ValueError(f"catalog source {source_id} does not match the database")
            source_by_key[row["source_key"]] = source_id

        current_ids = {row[0] for row in rows}
        known = {matrix_id: {source_id, *json.loads(additional)} for matrix_id, _, _, _, _, source_id, additional in rows}
        instance_source = {}
        occurrences = defaultdict(list)
        instances = list(csv.DictReader((CATALOG / "instances.csv").open()))
        if len(instances) != 3290:
            raise ValueError("expected 3290 literature catalog records")
        for row in instances:
            source_id = source_by_key[row["source_key"]]
            instance_source[row["instance_id"]] = source_id
            for matrix_id in {int(value) for value in re.findall(r"\d+", row["corpus_matrix_ids"])} & current_ids:
                known[matrix_id].add(source_id)

        for matrix_id, _, _, _, source, _, _ in rows:
            for instance in re.findall(r"catalog_instance_id=([^;|]+)", source or ""):
                source_id = instance_source[instance]
                known[matrix_id].add(source_id)
                occurrences[source_id].append(matrix_id)

        def add_mentions(matrix_ids, source_keys) -> None:
            source_ids = {source_by_key[source_key] for source_key in source_keys}
            for matrix_id in matrix_ids:
                if matrix_id not in current_ids:
                    if deduplicated:
                        continue
                    raise ValueError(f"curated mention references absent matrix {matrix_id}")
                known[matrix_id].update(source_ids)

        for matrix_ids, source_keys in CURATED_MENTIONS:
            add_mentions(matrix_ids, source_keys)
        for occurrence_source in ("bomze_locatelli_tardella_2008", "pena_vera_zuluaga_2007", "liuzzi_locatelli_piccialli_2019"):
            add_mentions(occurrences[source_by_key[occurrence_source]], ("gondzio_yildirim_2021",))

        groups = defaultdict(list)
        for matrix_id, n, storage, digest, _, _, _ in rows:
            groups[projective_key(n, storage, digest)].append(matrix_id)
        for matrix_ids in groups.values():
            if len(matrix_ids) > 1:
                group_sources = set().union(*(known[matrix_id] for matrix_id in matrix_ids))
                for matrix_id in matrix_ids:
                    known[matrix_id].update(group_sources)

        years = {source_id: year for source_id, _, _, year, _ in source_rows}
        assignments = []
        primary_changes = 0
        additional_rows = 0
        additional_links = 0
        for matrix_id, _, _, _, _, old_source_id, _ in rows:
            ordered = sorted(known[matrix_id], key=lambda source_id: (years[source_id], source_id))
            primary, additional = ordered[0], ordered[1:]
            encoded = json.dumps(additional, separators=(",", ":"))
            assignments.append((primary, encoded, matrix_id))
            primary_changes += primary != old_source_id
            additional_rows += bool(additional)
            additional_links += len(additional)

        if primary_changes not in ({0, EXPECTED_PRIMARY_CHANGES} if has_column else {EXPECTED_PRIMARY_CHANGES}):
            raise ValueError(f"unexpected partial primary-source migration: {primary_changes} changes remain")
        if not deduplicated and (additional_rows, additional_links) != (EXPECTED_ADDITIONAL_ROWS, EXPECTED_ADDITIONAL_LINKS):
            raise ValueError(f"expected {EXPECTED_ADDITIONAL_ROWS} additional rows and {EXPECTED_ADDITIONAL_LINKS} links, "
                             f"found {additional_rows} and {additional_links}")

        if has_column:
            stored = {row[0]: (row[5], row[6]) for row in rows}
            pending = []
            for primary, encoded, matrix_id in assignments:
                stored_primary, stored_json = stored[matrix_id]
                expected_additional = json.loads(encoded)
                stored_additional = json.loads(stored_json)
                if not set(stored_additional) <= set(expected_additional):
                    raise ValueError(f"inconsistent source references on matrix {matrix_id}")
                if stored_primary != primary or stored_additional != expected_additional:
                    pending.append((primary, encoded, matrix_id))
            if not pending:
                print(f"all {additional_rows} matrix reference lists are already populated")
                return
        else:
            pending = assignments

        print(f"primary_changes={primary_changes} additional_rows={additional_rows} additional_links={additional_links} "
              f"pending={len(pending)}")
        if arguments.dry_run:
            return

        connection.execute("BEGIN IMMEDIATE")
        if not has_column:
            connection.execute("""
                ALTER TABLE matrices ADD COLUMN additional_source_ids TEXT NOT NULL DEFAULT '[]'
                    CHECK(json_valid(additional_source_ids) AND json_type(additional_source_ids) = 'array')
            """)
        connection.executemany(
            "UPDATE matrices SET source_id=?, additional_source_ids=? WHERE matrix_id=?",
            pending,
        )
        if list(connection.execute("PRAGMA foreign_key_check")):
            raise ValueError("foreign-key check failed")
        connection.commit()
        if connection.execute("PRAGMA integrity_check").fetchone()[0] != "ok":
            raise ValueError("integrity check failed")

    print(f"populated={additional_rows} updated={len(pending)}")


if __name__ == "__main__":
    main()
