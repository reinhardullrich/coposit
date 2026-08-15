#!/usr/bin/env python3
"""Record literature papers that report solving each identified matrix."""

from __future__ import annotations

import argparse
import json
import math
import sqlite3
from collections import defaultdict
from pathlib import Path


PROJECT = Path(__file__).resolve().parents[2]
DATABASE = PROJECT / "testdata/copos_testdata.sqlite3"
EXPECTED_MATRICES = 3490
EXPECTED_RETAINED = 3157
EXPECTED_SOURCES = 94

GLOBAL_COMMENT = "equivalent global standard-quadratic-program solve"
TOLERANCE_COMMENT = "reported under the paper's numerical copositivity tolerance"


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
        connection.execute("PRAGMA foreign_keys=ON")
        columns = {row[1] for row in connection.execute("PRAGMA table_info(matrices)")}
        has_column = "references_solved" in columns
        selected = "references_solved" if has_column else "'[]'"
        rows = list(connection.execute(
            f"SELECT matrix_id, dimension, matrix, file_sha256, source_id, additional_source_ids, source, {selected} "
            "FROM matrices ORDER BY matrix_id"
        ))
        sources = {row[0]: row[1] for row in connection.execute("SELECT source_id, publication_year FROM sources")}
        if len(rows) not in {EXPECTED_MATRICES, EXPECTED_RETAINED} or len(sources) != EXPECTED_SOURCES:
            raise ValueError(f"expected {EXPECTED_MATRICES} or {EXPECTED_RETAINED} matrices and {EXPECTED_SOURCES} sources")
        deduplicated = len(rows) == EXPECTED_RETAINED

        by_id = {row[0]: row for row in rows}
        solved: dict[int, dict[int, str | None]] = defaultdict(dict)
        for row in rows:
            solved[row[0]] = {item["source_id"]: item.get("comment") for item in json.loads(row[7])}

        def add(source_id: int, matrix_ids, comment: str | None = None) -> None:
            if source_id not in sources:
                raise ValueError(f"unknown source {source_id}")
            for matrix_id in matrix_ids:
                if matrix_id not in by_id:
                    if deduplicated:
                        continue
                    raise ValueError(f"source {source_id} references absent matrix {matrix_id}")
                previous = solved[matrix_id].get(source_id)
                if previous is not None and comment is not None and previous != comment:
                    raise ValueError(f"conflicting comments for matrix {matrix_id}, source {source_id}")
                solved[matrix_id][source_id] = previous or comment

        def ids_where(predicate) -> list[int]:
            return [row[0] for row in rows if predicate(row)]

        # Exact relaxation, direct copositivity, and global-StQP claims identified in the audited papers' tables.
        add(23, (10706, 10708, 10714), "exact first-order relaxation reported")
        add(26, (10706, 10707, 10708, 10711, 10712, 10713, 10714, 10715, 10767, 10769, 10772, 10882, 9162, 12510, 9613, 9628),
            GLOBAL_COMMENT)
        add(27, (10706, 10707, 10708, 10711, 10712, 10713, 10714, 9222, 11194, 9223, 11197, 9224, 11200,
                 9218, 11179, 9219, 11182, 10852, 11185, 9220, 11188, 9221, 11191, 9162, 12510, 10715, 10767,
                 10769, 10772), GLOBAL_COMMENT)
        add(30, (9162, 12510, 9165, 10878, 10886, 10965, 10879, 10935, 10880, 10904, 10990, 10881, 10953,
                 10883, 10715, 10767, 10768, 10769, 10770, 10771, 10772, 10773))
        add(31, (10695, 9159, 9158, 10696, 9161, 10697, 9162, 12510, 9163, 10876, 10877, 9196, 11193,
                 9192, 11178, 9193, 11181, 10853, 11184, 9194, 11187, 9218, 11179, 9241, 11251, 9242, 11254))
        add(32, ids_where(lambda row: row[4] == 32), TOLERANCE_COMMENT)
        add(32, (10703, 10715, 10716, 10767, 9162, 12510), TOLERANCE_COMMENT)

        tanaka_2015 = ids_where(lambda row: "catalog_instance_id=tanaka_yoshise_2015:" in (row[6] or "") and
                               "paper_locator=Table" in (row[6] or "") and
                               ":G8_B_3.0;" not in (row[6] or "") and ":G12_B_4.0;" not in (row[6] or ""))
        add(33, tanaka_2015, TOLERANCE_COMMENT)
        tanaka_2018 = ids_where(lambda row: "catalog_instance_id=tanaka_yoshise_2018:" in (row[6] or "") and
                               ":G8_B_3.0;" not in (row[6] or "") and ":G12_B_4.0;" not in (row[6] or ""))
        add(34, tanaka_2018, TOLERANCE_COMMENT)

        add(35, (9157, 9158, 9159, 9160, 9161, 9162, 12510, 9163, 9575, 9578, 9581, 9584, 9611, 9614, 9617,
                 9620, 9629, 9632, 9635, 9638, 9641, 9648, 9651, 10870, 10872, 10874, 9196, 11193, 9193, 11181,
                 10853, 11184, 9194, 11187, 9212, 9213, 9214, 9215, 9216, 11241, 11244, 11247, 11250, 11253))
        add(36, (9162, 12510, 9163, 10995, 10996), "reported with the paper's numerical roundoff threshold")

        liuzzi = ids_where(lambda row: 39 in json.loads(row[5]) and row[1] <= 500)
        add(39, liuzzi, GLOBAL_COMMENT)
        add(40, (10686, 9162, 12510, 10687, 10058, 10688, 10062, 10689, 10070, 10690, 10086),
            "numerical Horn variational-index screening; not an exact certificate")
        add(43, (9162, 12510, 9163, 10995, 10769, 10770, 10882, 10883, 10871, 10873, 10875, 9610, 9613,
                 9628, 9631, 9634, 9640, 9647, 10870, 10872, 10874, 9611, 9614, 9629, 9632, 9635, 9641, 9648),
            TOLERANCE_COMMENT)
        add(44, ids_where(lambda row: row[4] == 59 or 44 in json.loads(row[5])), GLOBAL_COMMENT)
        add(44, range(10783, 10791), GLOBAL_COMMENT)
        add(46, (9162, 12510, 10775, 10776, 9629, 9628, 10777, 10778, 10779, 10780, 10781, 10782), GLOBAL_COMMENT)
        add(47, ids_where(lambda row: row[4] == 47 or 47 in json.loads(row[5])))
        add(48, (9157, 11177, 11178, 11180, 11181, 11183, 11184, 11189, 11190, 11252, 11253),
            "1000 random starts; negative witnesses decide, while positive reports use the paper's heuristic")
        add(49, (9157, 9158, 9159, 9160, 9161, 9162, 12510, 9163, 9196, 11193, 9193, 11181, 10853, 11184,
                 9194, 11187, 9212, 9213, 9214, 9215, 9216, 11241, 11244, 11247, 11250, 11253, 9222, 11194,
                 9219, 11182, 10852, 11185, 9220, 11188, 9238, 9239, 9240, 9241, 9242, 11242, 11245, 11248,
                 11251, 11254, 9575, 9584, 10870, 10872, 10874, 9611, 9614, 9617, 9620, 9629, 9632, 9635,
                 9638, 9641, 9648, 9651, 10871, 10873, 10875, 9610, 9613, 9616, 9619, 9628, 9631, 9647), GLOBAL_COMMENT)
        add(55, (10064,),
            "equivalent exact second-order moment/SOS relaxation of the global simplex minimum")
        add(60, range(11685, 11698), GLOBAL_COMMENT)
        add(67, (10058,))
        add(72, tuple(range(11685, 11698)) + tuple(range(11807, 11815)), GLOBAL_COMMENT)
        add(76, (10706, 10707, 10708, 10711, 10712, 10713, 10714), GLOBAL_COMMENT)

        # A positive scalar multiple is the same copositivity decision. Do not propagate through permutations: papers report
        # dramatically permutation-dependent runtimes, and a family-level mention is not an exact coefficient-array claim.
        groups: dict[tuple[str, int, str], list[int]] = defaultdict(list)
        for matrix_id, n, storage, digest, *_ in rows:
            groups[projective_key(n, storage, digest)].append(matrix_id)
        for matrix_ids in groups.values():
            merged: dict[int, str | None] = {}
            for matrix_id in matrix_ids:
                for source_id, comment in solved[matrix_id].items():
                    merged[source_id] = merged.get(source_id) or comment
            for matrix_id in matrix_ids:
                solved[matrix_id].update(merged)

        assignments = []
        links = 0
        for row in rows:
            matrix_id = row[0]
            objects = []
            for source_id in sorted(solved[matrix_id], key=lambda value: (sources[value], value)):
                item: dict[str, int | str] = {"source_id": source_id}
                comment = solved[matrix_id][source_id]
                if comment is not None:
                    item["comment"] = comment
                objects.append(item)
            encoded = json.dumps(objects, separators=(",", ":"))
            assignments.append((encoded, matrix_id))
            links += len(objects)

        nonempty = sum(value != "[]" for value, _ in assignments)
        pending = [(value, matrix_id) for value, matrix_id in assignments if value != by_id[matrix_id][7]]
        print(f"solved_reference_rows={nonempty} solved_reference_links={links} pending={len(pending)}")
        if arguments.dry_run or not pending:
            return

        connection.execute("BEGIN IMMEDIATE")
        if not has_column:
            connection.execute("""
                ALTER TABLE matrices ADD COLUMN references_solved TEXT NOT NULL DEFAULT '[]'
                    CHECK(json_valid(references_solved) AND json_type(references_solved) = 'array')
            """)
        connection.executemany("UPDATE matrices SET references_solved=? WHERE matrix_id=?", pending)

        invalid = list(connection.execute("""
            SELECT m.matrix_id
            FROM matrices AS m, json_each(m.references_solved) AS item
            LEFT JOIN sources AS s ON s.source_id = json_extract(item.value, '$.source_id')
            WHERE json_type(item.value) <> 'object'
               OR json_type(item.value, '$.source_id') <> 'integer'
               OR s.source_id IS NULL
               OR (json_type(item.value, '$.comment') IS NOT NULL AND json_type(item.value, '$.comment') <> 'text')
        """))
        if invalid:
            raise ValueError(f"invalid solved-reference objects: {invalid[:5]}")
        if list(connection.execute("PRAGMA foreign_key_check")):
            raise ValueError("foreign-key check failed")
        connection.commit()


if __name__ == "__main__":
    main()
