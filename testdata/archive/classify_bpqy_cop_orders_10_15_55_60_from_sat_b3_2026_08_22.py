#!/usr/bin/env python3
"""Merge the completed SAT-B3 run for the 2026-08-22 BPQY COP extension."""

from __future__ import annotations

import sqlite3
from pathlib import Path


PROJECT = Path(__file__).resolve().parents[2]
CORPUS = PROJECT / "testdata/copos_testdata.sqlite3"
DIAGNOSTICS = PROJECT / "experiments/diagnostics.sqlite3"
MATRIX_ID_RANGE = (15206, 15505)
MODEL = "sat_b3"
BINARY = "ba65cf630189bdc632bf5c5fea990b37e1c292458ad5b8096964470a22f42439"


def main() -> None:
    with sqlite3.connect(CORPUS) as connection:
        connection.execute("PRAGMA foreign_keys=ON")
        connection.execute("ATTACH DATABASE ? AS diagnostics", (str(DIAGNOSTICS),))
        result_filter = """r.matrix_id BETWEEN ? AND ? AND r.model_id=? AND r.mode='both'
                           AND r.preprocessing='both' AND r.binary_sha256=?"""
        key = (*MATRIX_ID_RANGE, MODEL, BINARY)
        groups = connection.execute(
            f"""SELECT r.status, r.is_copositive, r.is_strictly_copositive, count(*)
                FROM diagnostics.results AS r WHERE {result_filter}
                GROUP BY r.status, r.is_copositive, r.is_strictly_copositive""",
            key,
        ).fetchall()
        expected_groups = {("ok", 0, 0, 151), ("ok", 1, 1, 101), ("timeout", None, None, 48)}
        if set(groups) != expected_groups:
            raise ValueError(f"unexpected SAT-B3 result coverage: {groups}")

        conflicts = connection.execute(
            f"""SELECT count(*)
                FROM diagnostics.results AS r JOIN matrices AS m USING(matrix_id)
                WHERE {result_filter} AND r.status='ok'
                  AND ((m.is_copositive IS NOT NULL AND m.is_copositive<>r.is_copositive)
                    OR (m.is_strictly_copositive IS NOT NULL AND m.is_strictly_copositive<>r.is_strictly_copositive))""",
            key,
        ).fetchone()[0]
        if conflicts:
            raise ValueError(f"{conflicts} completed classifications conflict with corpus truth")

        connection.execute("BEGIN IMMEDIATE")
        connection.execute(
            f"""UPDATE matrices AS m
                SET is_copositive=(
                        SELECT r.is_copositive FROM diagnostics.results AS r
                        WHERE r.matrix_id=m.matrix_id AND {result_filter} AND r.status='ok'
                    ),
                    is_strictly_copositive=(
                        SELECT r.is_strictly_copositive FROM diagnostics.results AS r
                        WHERE r.matrix_id=m.matrix_id AND {result_filter} AND r.status='ok'
                    )
                WHERE m.matrix_id BETWEEN ? AND ?
                  AND m.is_copositive IS NULL AND m.is_strictly_copositive IS NULL
                  AND EXISTS (
                      SELECT 1 FROM diagnostics.results AS r
                      WHERE r.matrix_id=m.matrix_id AND {result_filter} AND r.status='ok'
                  )""",
            (*key, *key, *MATRIX_ID_RANGE, *key),
        )
        truth_changes = connection.execute("SELECT changes()").fetchone()[0]

        connection.execute(
            f"""UPDATE matrices AS m SET preprocessing_solved=1
                WHERE m.matrix_id BETWEEN ? AND ? AND m.preprocessing_solved=0
                  AND EXISTS (
                      SELECT 1 FROM diagnostics.results AS r
                      WHERE r.matrix_id=m.matrix_id AND {result_filter} AND r.status='ok'
                        AND instr(r.diagnostics,'preprocessing_outcome=resolved')>0
                        AND instr(r.diagnostics,'model_delegations=0')>0
                  )""",
            (*MATRIX_ID_RANGE, *key),
        )
        preprocessing_changes = connection.execute("SELECT changes()").fetchone()[0]

        connection.execute(
            """WITH ranked AS (
                    SELECT r.matrix_id, r.elapsed_ns,
                           json_object('model_id',r.model_id,'mode',r.mode,'preprocessing',r.preprocessing,
                                       'binary_sha256',r.binary_sha256) AS result_ref,
                           row_number() OVER (
                               PARTITION BY r.matrix_id
                               ORDER BY r.elapsed_ns,r.model_id,r.mode,r.preprocessing,r.binary_sha256
                           ) AS position
                    FROM diagnostics.results AS r JOIN matrices AS m USING(matrix_id)
                    WHERE r.matrix_id BETWEEN ? AND ? AND r.status='ok' AND r.mode='both'
                      AND r.is_copositive=m.is_copositive
                      AND r.is_strictly_copositive=m.is_strictly_copositive
                )
                UPDATE matrices AS m
                SET (fastest_elapsed_ns,fastest_result_ref)=(
                    SELECT elapsed_ns,result_ref FROM ranked
                    WHERE ranked.matrix_id=m.matrix_id AND ranked.position=1
                )
                WHERE m.matrix_id BETWEEN ? AND ?
                  AND EXISTS (SELECT 1 FROM ranked WHERE ranked.matrix_id=m.matrix_id AND ranked.position=1)""",
            (*MATRIX_ID_RANGE, *MATRIX_ID_RANGE),
        )

        classified, unknown, preprocessing, timed = connection.execute(
            """SELECT count(*) FILTER (WHERE is_copositive IS NOT NULL AND is_strictly_copositive IS NOT NULL),
                      count(*) FILTER (WHERE is_copositive IS NULL AND is_strictly_copositive IS NULL),
                      count(*) FILTER (WHERE preprocessing_solved),
                      count(*) FILTER (WHERE fastest_elapsed_ns IS NOT NULL)
               FROM matrices WHERE matrix_id BETWEEN ? AND ?""",
            MATRIX_ID_RANGE,
        ).fetchone()
        integrity = connection.execute("PRAGMA integrity_check").fetchone()[0]
        foreign_keys = connection.execute("SELECT count(*) FROM pragma_foreign_key_check").fetchone()[0]
        if (classified, unknown, preprocessing, timed, integrity, foreign_keys) != (252, 48, 160, 252, "ok", 0):
            raise ValueError(
                "post-merge verification failed: "
                f"classified={classified} unknown={unknown} preprocessing={preprocessing} timed={timed} "
                f"integrity={integrity} foreign_keys={foreign_keys}"
            )

    print(
        f"truth_changes={truth_changes} preprocessing_changes={preprocessing_changes} "
        f"classified={classified} unknown={unknown} preprocessing={preprocessing} timed={timed} integrity={integrity}"
    )


if __name__ == "__main__":
    main()
