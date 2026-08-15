#!/usr/bin/env python3
"""Apply only source-backed truth labels to the 2026-08-14 literature-catalog import."""

from __future__ import annotations

import argparse
import re
import sqlite3
from collections import Counter
from fractions import Fraction
from pathlib import Path


DATABASE = Path(__file__).parents[1] / "copos_testdata.sqlite3"
FIRST_IMPORTED_ID = 10685
LAST_IMPORTED_ID = 12522
EXPECTED_IMPORTED = 1048
EXPECTED_COUNTS = {
    (1, 1): 91,
    (0, 1): 89,
    (0, 0): 90,
    (None, 1): 48,
}


def fields(source: str) -> tuple[str, str, str, str]:
    instance = re.search(r"^catalog_instance_id=(.*?); paper_locator=", source)
    usage = re.search(r"; paper_usage=(.*?); retained_path=", source)
    path = re.search(r"; retained_path=(.*?)(?:; note=|; stored_matrix=|$)", source)
    if not instance or not usage or not path:
        raise ValueError(f"invalid catalog provenance: {source}")
    instance_id = instance.group(1)
    source_key = instance_id.split(":", 2)[1] if instance_id.startswith("archive:") else instance_id.split(":", 1)[0]
    return source_key, instance_id.split(":", 1)[1], usage.group(1), path.group(1)


def graph_label(name: str, orders: dict[str, Fraction]) -> tuple[int, int, str] | None:
    match = re.fullmatch(r"G(" + "|".join(orders) + r")_B_?([0-9]+(?:\.[0-9]+)?)", name)
    if not match:
        return None
    gamma = Fraction(match.group(2))
    threshold = orders[match.group(1)]
    if gamma < threshold:
        return 0, 0, "graph parameter below the paper's exact clique threshold"
    if gamma == threshold:
        return 0, 1, "graph parameter equals the paper's exact clique threshold"
    return 1, 1, "graph parameter exceeds the paper's exact clique threshold"


def classify(source: str) -> tuple[int | None, int, str] | None:
    source_key, name, usage, path = fields(source)
    lower_usage = usage.lower()

    if source_key == "ferreira_gao_nemeth_rigo_2024":
        if "_In_Interior.txt" in path:
            return 1, 1, "repository interior label"
        if "_On_Boundary.txt" in path:
            return 0, 1, "repository boundary label"
        if "_Not_Cop.txt" in path:
            return 0, 0, "repository non-copositive label"

    if source_key in {"tanaka_yoshise_2015", "tanaka_yoshise_2018"}:
        if label := graph_label(name, {"8": Fraction(3), "12": Fraction(4)}):
            return label
        if name in {"Example_3.3", "Example_3.5"}:
            return None, 1, "paper places the matrix in a copositive inner cone"

    if source_key == "safi_nabavi_caron_2021":
        if label := graph_label(name, {"8": Fraction(3), "10": Fraction(3), "12": Fraction(4)}):
            return label

    if "strictly copositive" in lower_usage or "strict copositive" in lower_usage:
        return 1, 1, "explicit strict-copositivity statement"
    if "non-copositive" in lower_usage or "noncopositive" in lower_usage or "not copositive" in lower_usage:
        return 0, 0, "explicit non-copositivity statement"
    if ("boundary-copositive" in lower_usage or "boundary copositive" in lower_usage
            or ("cop-irreducible" in lower_usage and "boundary" in lower_usage)):
        return 0, 1, "explicit copositive-boundary statement"
    if "extreme copositive" in lower_usage:
        return 0, 1, "explicit nonzero extreme copositive ray"

    if source_key == "sponsel_bundfuss_duer_2012":
        if name.endswith("NotCopos"):
            return 0, 0, "source test-set label"
        if name.endswith("Copos"):
            return 0, 1, "source graph test at the exact clique threshold"
    if source_key == "bomze_deklerk_2002":
        if name.endswith("NotCopos"):
            return 0, 0, "source test-set label"
        if name.endswith("Copos"):
            return None, 1, "source test-set label"
    if source_key == "badenbroek_deklerk_2019":
        return None, 1, "paper's doubly-nonnegative construction"
    if source_key == "dobre_vera_2015":
        return 0, 1, "paper's graph matrix at the exact stability-number threshold"
    if source_key == "vargas_vera_dickinson_2025":
        return 0, 1, "paper's copositive-boundary graph construction"
    if source_key == "bomze_schachinger_ullrich_2014" and name in {"S7", "S9", "S11"}:
        return 0, 1, "copositive support matrix with the paper's exact zero generators"
    if source_key == "bras_eichfelder_judice_2016" and "lambda_" in name:
        return 0, 0, "graph parameter is one below the exact clique number"
    if source_key == "anstreicher_2021":
        return 0, 1, "graph parameter equals the exact clique number"
    if source_key == "zilinskas_2011_programming" and name == "Johnson6-4-4_t_3":
        return 0, 1, "deterministic graph matrix at the paper's exact clique number"

    copositive_classes = (
        "copositive matrix", "copositive example", "copositive factor", "copositive inverse",
        "hard copositive graph", "graph-derived copositive", "doubly-nonnegative", "completely-positive",
        "positive-semidefinite", "nonnegative matrix", "nonnegative summand", "spn matrix", "psd matrix",
        "psd summand", "dnn hardness", "cp-rank",
    )
    if any(label in lower_usage for label in copositive_classes):
        return None, 1, "explicit membership in a cone contained in the copositive cone"
    return None


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dry-run", action="store_true")
    arguments = parser.parse_args()

    with sqlite3.connect(DATABASE) as connection:
        rows = list(connection.execute(
            """SELECT matrix_id, source, is_strictly_copositive, is_copositive
               FROM matrices WHERE matrix_id BETWEEN ? AND ? ORDER BY matrix_id""",
            (FIRST_IMPORTED_ID, LAST_IMPORTED_ID),
        ))
        if len(rows) != EXPECTED_IMPORTED:
            raise ValueError(f"expected {EXPECTED_IMPORTED} retained imported rows, found {len(rows)}")

        decisions = [(matrix_id, *label) for matrix_id, source, _, _ in rows if (label := classify(source)) is not None]
        counts = Counter((strict, ordinary) for _, strict, ordinary, _ in decisions)
        if counts != Counter(EXPECTED_COUNTS):
            raise ValueError(f"unexpected classification counts: {counts}")

        current = {matrix_id: (strict, ordinary) for matrix_id, _, strict, ordinary in rows}
        pending = []
        matching = []
        conflicts = []
        for matrix_id, proposed_strict, proposed_ordinary, evidence in decisions:
            old_strict, old_ordinary = current[matrix_id]
            if ((proposed_strict is not None and old_strict is not None and proposed_strict != old_strict)
                    or (proposed_ordinary is not None and old_ordinary is not None and proposed_ordinary != old_ordinary)):
                conflicts.append((matrix_id, proposed_strict, proposed_ordinary, evidence))
                continue
            merged = (proposed_strict if proposed_strict is not None else old_strict,
                      proposed_ordinary if proposed_ordinary is not None else old_ordinary)
            if merged == (old_strict, old_ordinary):
                matching.append((matrix_id, proposed_strict, proposed_ordinary, evidence))
            else:
                pending.append((matrix_id, *merged, evidence))
        if conflicts:
            raise ValueError(f"existing truth conflicts with source-backed labels: {conflicts[:5]}")
        if pending and matching:
            raise ValueError(f"partial truth migration found: {len(matching)} applied, {len(pending)} pending")

        for (strict, ordinary), count in sorted(counts.items(), key=lambda item: str(item[0])):
            print(f"strict={strict} copositive={ordinary}: {count}")
        print(f"classified={len(decisions)} unchanged_unknown={len(rows) - len(decisions)}")
        if arguments.dry_run:
            print(f"dry_run=ok pending={len(pending)} already_applied={len(matching)}")
            return
        if not pending:
            print("all source-backed truth labels are already applied")
            return

        connection.execute("BEGIN IMMEDIATE")
        connection.executemany(
            "UPDATE matrices SET is_strictly_copositive=?, is_copositive=? WHERE matrix_id=?",
            [(strict, ordinary, matrix_id) for matrix_id, strict, ordinary, _ in pending],
        )
        if list(connection.execute("PRAGMA foreign_key_check")):
            raise ValueError("foreign-key check failed")
        connection.commit()

        stored = {
            matrix_id: (strict, ordinary)
            for matrix_id, strict, ordinary in connection.execute(
                """SELECT matrix_id, is_strictly_copositive, is_copositive FROM matrices
                   WHERE matrix_id BETWEEN ? AND ?""",
                (FIRST_IMPORTED_ID, LAST_IMPORTED_ID),
            )
        }
        for matrix_id, strict, ordinary, _ in decisions:
            actual = stored[matrix_id]
            if ((strict is not None and actual[0] != strict)
                    or (ordinary is not None and actual[1] != ordinary)):
                raise ValueError(f"truth verification failed for matrix {matrix_id}: {actual}")
        if connection.execute("PRAGMA integrity_check").fetchone()[0] != "ok":
            raise ValueError("integrity check failed")

    print(f"applied={len(pending)}")


if __name__ == "__main__":
    main()
