#!/usr/bin/env python3
"""Exclude preprocessing-complete matrices and refill the curated benchmark sets."""

from __future__ import annotations

import argparse
from collections import Counter
import json
import math
import sqlite3
from pathlib import Path


DATABASE = Path(__file__).resolve().parents[1] / "copos_testdata.sqlite3"
INITIAL_COUNTS = (3519, 2115, 49, 512, 29, 201, 3119, 1951, 75, 58)
FINAL_COUNTS = (3519, 2115, 49, 512, 0, 0, 1168, 0, 17, 0)
SMOKE_MAX_NS = 50_000_000


def classification(row: sqlite3.Row) -> str:
    if row["is_strictly_copositive"] == 1:
        return "strict"
    if row["is_copositive"] == 1:
        return "boundary"
    if row["is_copositive"] == 0:
        return "not_copositive"
    return "unknown"


def counts(connection: sqlite3.Connection) -> tuple[int, ...]:
    return tuple(connection.execute("""
        SELECT count(*),
               count(*) FILTER (WHERE preprocessing_solved),
               count(*) FILTER (WHERE smoke_set),
               count(*) FILTER (WHERE core_and_stress_test),
               count(*) FILTER (WHERE smoke_set AND preprocessing_solved),
               count(*) FILTER (WHERE core_and_stress_test AND preprocessing_solved),
               count(*) FILTER (WHERE n_le_100),
               count(*) FILTER (WHERE n_le_100 AND preprocessing_solved),
               count(*) FILTER (WHERE n_gt_100_solved),
               count(*) FILTER (WHERE n_gt_100_solved AND preprocessing_solved)
        FROM matrices
    """).fetchone())


def choose(
    targets: list[sqlite3.Row],
    candidates: list[sqlite3.Row],
    family_use: Counter[str],
    smoke: bool,
) -> tuple[list[sqlite3.Row], list[sqlite3.Row]]:
    available = {row["matrix_id"]: row for row in candidates}
    selected: list[sqlite3.Row] = []
    unmatched: list[sqlite3.Row] = []
    for target in sorted(targets, key=lambda row: (row["dimension"], row["matrix_id"])):
        same_class = [row for row in available.values() if classification(row) == classification(target)]
        if not same_class:
            unmatched.append(target)
            continue

        def score(candidate: sqlite3.Row) -> tuple[object, ...]:
            dimension_gap = abs(math.log2((candidate["dimension"] + 1) / (target["dimension"] + 1)))
            family_match = not (target["family"] and candidate["family"] == target["family"])
            source_match = candidate["source_id"] != target["source_id"]
            failure_mismatch = bool(json.loads(candidate["references_unsolved"])) != bool(json.loads(target["references_unsolved"]))
            elapsed = candidate["fastest_elapsed_ns"]
            timing = (elapsed is None, elapsed or 0) if smoke else (elapsed is not None, -(elapsed or 0))
            return (
                dimension_gap,
                family_match,
                source_match,
                failure_mismatch,
                family_use[candidate["family"] or ""],
                timing,
                candidate["matrix_id"],
            )

        candidate = min(same_class, key=score)
        selected.append(candidate)
        family_use[candidate["family"] or ""] += 1
        del available[candidate["matrix_id"]]
    return selected, unmatched


def summarize(label: str, rows: list[sqlite3.Row]) -> None:
    outcomes = Counter(classification(row) for row in rows)
    dimensions = [row["dimension"] for row in rows]
    print(
        f"{label}={len(rows)} dimensions={min(dimensions)}..{max(dimensions)} "
        f"strict={outcomes['strict']} boundary={outcomes['boundary']} not_copositive={outcomes['not_copositive']}"
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apply", action="store_true")
    arguments = parser.parse_args()

    with sqlite3.connect(DATABASE) as connection:
        connection.row_factory = sqlite3.Row
        current_counts = counts(connection)
        if current_counts == FINAL_COUNTS:
            print("already_applied=1")
            return
        if current_counts != INITIAL_COUNTS:
            raise ValueError(f"unexpected benchmark state: {current_counts}")

        rows = list(connection.execute("SELECT * FROM matrices ORDER BY matrix_id"))
        removed_smoke = [row for row in rows if row["smoke_set"] and row["preprocessing_solved"]]
        removed_core_only = [
            row for row in rows
            if row["core_and_stress_test"] and row["preprocessing_solved"] and not row["smoke_set"]
        ]
        candidates = [
            row for row in rows
            if not row["preprocessing_solved"] and not row["core_and_stress_test"] and classification(row) != "unknown"
        ]
        smoke_candidates = [
            row for row in candidates
            if row["dimension"] <= 20
            and row["fastest_elapsed_ns"] is not None
            and row["fastest_elapsed_ns"] <= SMOKE_MAX_NS
        ]

        family_use: Counter[str] = Counter()
        smoke_selected, smoke_unmatched = choose(removed_smoke, smoke_candidates, family_use, smoke=True)
        if smoke_unmatched:
            raise ValueError(f"no Smoke replacements for {[row['matrix_id'] for row in smoke_unmatched]}")

        selected_ids = {row["matrix_id"] for row in smoke_selected}
        core_candidates = [row for row in candidates if row["matrix_id"] not in selected_ids]
        core_selected, core_unmatched = choose(removed_core_only, core_candidates, family_use, smoke=False)
        if len(core_unmatched) != 1 or classification(core_unmatched[0]) != "not_copositive":
            raise ValueError(f"unexpected unmatched Core targets: {[row['matrix_id'] for row in core_unmatched]}")

        remaining_ids = selected_ids | {row["matrix_id"] for row in core_selected}
        fallback_candidates = [
            row for row in core_candidates
            if row["matrix_id"] not in remaining_ids and classification(row) == "boundary"
        ]
        if not fallback_candidates:
            raise ValueError("no boundary fallback for the single unavailable non-copositive replacement")
        target = core_unmatched[0]
        fallback = [min(
            fallback_candidates,
            key=lambda row: (
                abs(math.log2((row["dimension"] + 1) / (target["dimension"] + 1))),
                family_use[row["family"] or ""],
                row["fastest_elapsed_ns"] is not None,
                -(row["fastest_elapsed_ns"] or 0),
                row["matrix_id"],
            ),
        )]
        core_selected.extend(fallback)

        if len(smoke_selected) != 29 or len(core_selected) != 172:
            raise ValueError("wrong replacement counts")
        summarize("smoke_replacements", smoke_selected)
        summarize("core_only_replacements", core_selected)
        print(f"outcome_substitution=1 not_copositive target replaced by boundary matrix {fallback[0]['matrix_id']}")
        if not arguments.apply:
            print("dry_run=1")
            return

        connection.execute("BEGIN IMMEDIATE")
        connection.execute(
            "UPDATE matrices SET smoke_set=0,core_and_stress_test=0 WHERE preprocessing_solved=1"
        )
        connection.executemany(
            "UPDATE matrices SET smoke_set=1,core_and_stress_test=1 WHERE matrix_id=?",
            ((row["matrix_id"],) for row in smoke_selected),
        )
        connection.executemany(
            "UPDATE matrices SET core_and_stress_test=1 WHERE matrix_id=?",
            ((row["matrix_id"],) for row in core_selected),
        )
        connection.execute("ALTER TABLE matrices DROP COLUMN n_le_100")
        connection.execute("ALTER TABLE matrices DROP COLUMN n_gt_100_solved")
        connection.execute("""
            ALTER TABLE matrices ADD COLUMN n_le_100 INTEGER
                GENERATED ALWAYS AS (dimension <= 100 AND preprocessing_solved = 0) VIRTUAL
        """)
        connection.execute("""
            ALTER TABLE matrices ADD COLUMN n_gt_100_solved INTEGER GENERATED ALWAYS AS (
                dimension > 100 AND json_array_length(references_solved) > 0 AND preprocessing_solved = 0
            ) VIRTUAL
        """)
        if counts(connection) != FINAL_COUNTS:
            raise ValueError(f"wrong final benchmark state: {counts(connection)}")
        if connection.execute(
            "SELECT count(*) FROM matrices WHERE smoke_set AND NOT core_and_stress_test"
        ).fetchone()[0]:
            raise ValueError("Smoke is not a subset of Core and Stress")
        if connection.execute("PRAGMA integrity_check").fetchone()[0] != "ok":
            raise ValueError("integrity check failed")
        if list(connection.execute("PRAGMA foreign_key_check")):
            raise ValueError("foreign-key check failed")
        connection.commit()
        print("applied=1")


if __name__ == "__main__":
    main()
