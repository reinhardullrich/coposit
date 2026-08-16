#!/usr/bin/env python3
"""Remove partial or heuristic positive results from literature-reported solves."""

from __future__ import annotations

import json
import sqlite3
from pathlib import Path


DATABASE = Path(__file__).resolve().parents[1] / "copos_testdata.sqlite3"
PARTIAL_BRAS_IDS = {
    9161, 9162, 9163, 9193, 9212, 9213, 9575, 9578, 9581, 9584, 9614, 9629, 9635, 9638, 9648, 9651, 10853,
}
POSITIVE_EXPECTED = {40: 10, 48: 5, 97: 17}
UNSOLVED_COMMENTS = {
    48: "1000 randomized starts remained nonnegative; the paper says this supports only a guess, not a copositivity decision",
    97: "100 randomized DCA/BDCA starts reached critical points; the paper defines this outcome as undecidable",
}


def source_id(item: dict[str, object]) -> int:
    return int(item["source_id"])


def main() -> None:
    with sqlite3.connect(DATABASE) as connection:
        connection.execute("PRAGMA foreign_keys=ON")
        rows = list(connection.execute(
            "SELECT matrix_id,is_copositive,references_solved,references_unsolved FROM matrices ORDER BY matrix_id"
        ))
        years = dict(connection.execute("SELECT source_id,publication_year FROM sources"))

        positive_counts = {key: 0 for key in POSITIVE_EXPECTED}
        partial_bras_count = 0
        for matrix_id, copositive, solved_text, _ in rows:
            solved_ids = {source_id(item) for item in json.loads(solved_text)}
            if copositive == 1:
                for key in positive_counts:
                    positive_counts[key] += key in solved_ids
            partial_bras_count += matrix_id in PARTIAL_BRAS_IDS and 35 in solved_ids

        if all(count == 0 for count in positive_counts.values()) and partial_bras_count == 0:
            print("already_applied=1")
            return
        if positive_counts != POSITIVE_EXPECTED or partial_bras_count != len(PARTIAL_BRAS_IDS):
            raise ValueError(f"unexpected claims: positive={positive_counts}, partial_bras={partial_bras_count}")

        updates: list[tuple[str, str, int]] = []
        for matrix_id, copositive, solved_text, unsolved_text in rows:
            solved = json.loads(solved_text)
            unsolved = json.loads(unsolved_text)
            removed = {
                source_id(item)
                for item in solved
                if (copositive == 1 and source_id(item) in POSITIVE_EXPECTED)
                or (matrix_id in PARTIAL_BRAS_IDS and source_id(item) == 35)
            }
            if not removed:
                continue

            solved = [item for item in solved if source_id(item) not in removed]
            by_source = {source_id(item): item for item in unsolved}
            for key in removed & UNSOLVED_COMMENTS.keys():
                by_source[key] = {"source_id": key, "comment": UNSOLVED_COMMENTS[key]}
            unsolved = [by_source[key] for key in sorted(by_source, key=lambda value: (years[value], value))]
            updates.append((
                json.dumps(solved, separators=(",", ":")),
                json.dumps(unsolved, separators=(",", ":")),
                matrix_id,
            ))

        connection.execute("BEGIN IMMEDIATE")
        connection.executemany(
            "UPDATE matrices SET references_solved=?,references_unsolved=? WHERE matrix_id=?",
            updates,
        )
        if list(connection.execute("PRAGMA foreign_key_check")):
            raise ValueError("foreign-key check failed")
        if connection.execute("PRAGMA integrity_check").fetchone()[0] != "ok":
            raise ValueError("integrity check failed")
        connection.commit()

        solved_links, solved_rows, unsolved_links, unsolved_rows = connection.execute("""
            SELECT SUM(json_array_length(references_solved)),
                   COUNT(*) FILTER (WHERE json_array_length(references_solved) > 0),
                   SUM(json_array_length(references_unsolved)),
                   COUNT(*) FILTER (WHERE json_array_length(references_unsolved) > 0)
            FROM matrices
        """).fetchone()
        print(
            f"updated={len(updates)} solved_rows={solved_rows} solved_links={solved_links} "
            f"unsolved_rows={unsolved_rows} unsolved_links={unsolved_links}"
        )


if __name__ == "__main__":
    main()
