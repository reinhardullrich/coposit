#!/usr/bin/env python3
"""Record papers that explicitly report a failed solve for an identified matrix."""

from __future__ import annotations

import argparse
import json
import sqlite3
from pathlib import Path


PROJECT = Path(__file__).resolve().parents[2]
DATABASE = PROJECT / "testdata/copos_testdata.sqlite3"
EXPECTED_MATRICES = 3157
EXPECTED_SOURCES = 94


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dry-run", action="store_true")
    arguments = parser.parse_args()

    with sqlite3.connect(DATABASE) as connection:
        connection.execute("PRAGMA foreign_keys=ON")
        columns = {row[1] for row in connection.execute("PRAGMA table_info(matrices)")}
        has_column = "references_unsolved" in columns
        selected = "references_unsolved" if has_column else "'[]'"
        rows = list(connection.execute(
            f"SELECT matrix_id, dimension, source, {selected} FROM matrices ORDER BY matrix_id"
        ))
        sources = {row[0]: row[1] for row in connection.execute("SELECT source_id, publication_year FROM sources")}
        if len(rows) != EXPECTED_MATRICES or len(sources) != EXPECTED_SOURCES:
            raise ValueError(f"expected {EXPECTED_MATRICES} matrices and {EXPECTED_SOURCES} sources")

        by_id = {row[0]: row for row in rows}
        claims: dict[int, dict[int, str]] = {row[0]: {} for row in rows}

        def add(source_id: int, matrix_ids, comment: str) -> None:
            if source_id not in sources:
                raise ValueError(f"unknown source {source_id}")
            for matrix_id in matrix_ids:
                if matrix_id not in by_id:
                    continue  # A pre-deduplication occurrence may already have been merged into its retained representative.
                previous = claims[matrix_id].get(source_id)
                if previous is not None and previous != comment:
                    raise ValueError(f"conflicting comments for matrix {matrix_id}, source {source_id}")
                claims[matrix_id][source_id] = comment

        # Žilinskas Table 3: none of the named DIMACS copositive programs reached a guaranteed optimum.
        add(27, (9574, 9577, 9580, 9583, 9586, 9589, 9592, 9595, 9598, 9601, 9604, 9607,
                 9610, 9613, 9616, 9619, 9622, 9625, 9628, 9631, 9634, 9637, 9640, 9643, 9645,
                 9647, 9650, 9653, 9655),
            "Table 3 copositive-program run reached its time limit without a guaranteed optimum")

        # Sponsel Tables 2-4: failures are method-specific; other variants sometimes completed the same matrix.
        add(30, (10880,), "M=N did not terminate within 30 minutes; M=H and M=S++N completed")
        add(30, (10882,), "all three tested inner-cone variants did not terminate within 30 minutes")
        add(30, (10883,), "M=S++N did not terminate within 30 minutes; M=N and M=H completed")
        add(30, (9611,), "B31 for hamming6-2 did not terminate within one hour and the 500 MB partition limit")

        add(33, (10878, 10880, *range(10905, 10916)),
            "at least one reported LP/simplicial variant did not terminate within six hours")
        add(34, (10878, 10880, *range(10905, 10925)),
            "at least one reported tractable-subcone variant did not terminate within six hours")

        # Brás Tables 16-17: distinguish an inconclusive classification from a global-optimum timeout that still found a witness.
        add(35, (9193, 9194, *range(9212, 9217)),
            "BARON did not prove global optimality within 7200 seconds, although its negative upper bound proved non-copositivity")
        add(35, (9575, 9578, 9581, 9584, 9651),
            "one or more LCP/MIP procedures were inconclusive, and BARON did not prove global optimality within 7200 seconds")
        add(35, (10870, 10872, 10874, 9611, 9614, 9617, 9620, 9629, 9632, 9635, 9638, 9641, 9648),
            "BARON did not prove global optimality within 7200 seconds, although its negative upper bound proved non-copositivity")

        add(39, range(11304, 11310), "BBB and BDF ran out of memory; NBB and NDF completed")

        # Gondzio-Yıldırım aggregate rows are used only where every matrix in the named group shares the failure.
        add(44, range(11260, 11284), "BARON hit the 3600-second limit on every ST100 instance")
        add(44, range(11285, 11303), "CPLEX QP and BARON hit the 3600-second limit on every ST200 instance")
        add(44, range(11304, 11315), "CPLEX QP and BARON hit the 3600-second limit on every ST500 instance")
        add(44, (11258,), "all tested methods except MILP2-L1 hit the 3600-second limit on ST1000")
        add(44, range(10783, 10791), "CPLEX QP and BARON hit the 3600-second limit on every DIMACS1 instance")
        add(44, range(11012, 11027), "CPLEX QP and BARON hit the 3600-second limit on the BSU instances of orders 10 through 24")

        add(47, (10878, 10880, *range(10905, 10915)),
            "the BD algorithm failed on G8 at gamma=3 and on G12 for gamma at least 4; SNC completed")

        # Júdice Tables 13-19: the paper's proposed method, benchmark B&B, or both can fail on one retained matrix.
        add(49, (9194,), "the benchmark B&B algorithm had numerical trouble during initialization")
        add(49, (9578, 9581, 9584, 9635, 9638, 9641, 9651),
            "Algorithm 4 and/or benchmark B&B was inconclusive after 7200 seconds, out of memory, or numerical trouble")
        add(49, (9574, 9577, 9580, 9583, 9610, 9616, 9619, 9631, 9634, 9637, 9640, 9647, 9650),
            "Algorithm 4 and/or benchmark B&B failed to certify copositivity within 7200 seconds or hit numerical trouble")
        add(49, (10871, 10873, 10875),
            "benchmark B&B returned an incorrect non-copositive conclusion; Algorithm 4 completed correctly")

        add(71, (11053,), "the unreduced order-14 SDP hierarchy ran out of memory at level 4; symmetry reduction completed")
        add(71, range(11054, 11057),
            "the unreduced hierarchy ran out of memory, and a higher symmetry-reduced level also hit memory or the 8000-second limit")

        add(72, (11809,),
            "BARON timed out on stable3; the retained scaling class also contains stable8, where all three competing solvers timed out")
        add(72, range(11810, 11814), "quadprogBB, BARON, and CPLEX timed out at the maximum allowed time")
        add(72, (11320, 11266, 11272, 11688, 11291, 11691, 11304, 11258, 11696, 11697, 11694, 11695),
            "one or more of quadprogBB, BARON, and CPLEX timed out; on the order-1000 instance quadprogIP also timed out")

        assignments = []
        links = 0
        for matrix_id, _, _, existing in rows:
            objects = [
                {"source_id": source_id, "comment": claims[matrix_id][source_id]}
                for source_id in sorted(claims[matrix_id], key=lambda value: (sources[value], value))
            ]
            encoded = json.dumps(objects, separators=(",", ":"))
            assignments.append((encoded, matrix_id))
            links += len(objects)

        nonempty = sum(value != "[]" for value, _ in assignments)
        pending = [(value, matrix_id) for value, matrix_id in assignments if value != by_id[matrix_id][3]]
        print(f"unsolved_reference_rows={nonempty} unsolved_reference_links={links} pending={len(pending)}")
        if arguments.dry_run or not pending:
            return

        connection.execute("BEGIN IMMEDIATE")
        if not has_column:
            connection.execute("""
                ALTER TABLE matrices ADD COLUMN references_unsolved TEXT NOT NULL DEFAULT '[]'
                    CHECK(json_valid(references_unsolved) AND json_type(references_unsolved) = 'array')
            """)
        connection.executemany("UPDATE matrices SET references_unsolved=? WHERE matrix_id=?", pending)

        invalid = list(connection.execute("""
            SELECT m.matrix_id
            FROM matrices AS m, json_each(m.references_unsolved) AS item
            LEFT JOIN sources AS s ON s.source_id = json_extract(item.value, '$.source_id')
            WHERE json_type(item.value) <> 'object'
               OR json_type(item.value, '$.source_id') <> 'integer'
               OR s.source_id IS NULL
               OR json_type(item.value, '$.comment') <> 'text'
        """))
        if invalid:
            raise ValueError(f"invalid unsolved-reference objects: {invalid[:5]}")
        if list(connection.execute("PRAGMA foreign_key_check")):
            raise ValueError("foreign-key check failed")
        connection.commit()


if __name__ == "__main__":
    main()
