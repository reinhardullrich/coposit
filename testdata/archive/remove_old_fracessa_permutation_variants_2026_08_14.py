#!/usr/bin/env python3
"""Remove the restored old FracESSA reduced-B coordinate orderings."""

import sqlite3
from pathlib import Path


DATABASE = Path(__file__).resolve().parents[1] / "copos_testdata.sqlite3"
OLD_FRACESSA_IDS = (
    1092, 1360, 1915, 2091, 2095, 2505, 2507, 2545, 2546, 2559,
    2572, 2789, 5539, 6351, 7447, 7628, 8016, 8022, 8071, 8113,
    8115, 8122, 8169, 8177, 8238, 8249, 8252, 8254, 8303, 8334,
    8354, 8355, 8358, 8363, 8415, 8760, 8843, 8845, 8891, 8897,
)
RETAINED_LITERATURE_IDS = (
    10132, 10700, 10720, 10727, 10746, 10747, 10753, 10852, 10853,
    10858, 11050, 11058, 11068, 11069, 11070, 11071, 11072, 11073,
    11074, 11075, 11076, 11077, 11078, 11079, 11080, 11081, 11082,
    11083, 11084, 11085, 11086, 11087, 11088, 11089, 11090, 11091,
    11183, 11807, 11927, 12117, 12120,
)


def placeholders(values: tuple[int, ...]) -> str:
    return ",".join("?" for _ in values)


def main() -> None:
    with sqlite3.connect(DATABASE) as connection:
        connection.execute("PRAGMA foreign_keys=ON")
        marked_ids = {
            row[0] for row in connection.execute(
                "SELECT matrix_id FROM matrices "
                "WHERE source LIKE 'FracESSA:%restored permutation-sensitive reduced-B occurrence%'"
            )
        }
        original_ids = {
            row[0] for row in connection.execute(
                f"SELECT matrix_id FROM matrices WHERE matrix_id IN ({placeholders(OLD_FRACESSA_IDS)}) "
                "AND source_id=91 AND source GLOB 'FracESSA:[0-9]*' AND instr(source, ';')=0",
                OLD_FRACESSA_IDS,
            )
        }
        target_ids = marked_ids | original_ids
        if target_ids and (len(marked_ids), len(original_ids), len(target_ids)) != (1356, 40, 1396):
            raise RuntimeError(
                f"unexpected removal set: marked={len(marked_ids)}, original={len(original_ids)}, total={len(target_ids)}"
            )

        retained = connection.execute(
            f"SELECT COUNT(*) FROM matrices WHERE matrix_id IN ({placeholders(RETAINED_LITERATURE_IDS)})",
            RETAINED_LITERATURE_IDS,
        ).fetchone()[0]
        if retained != len(RETAINED_LITERATURE_IDS):
            raise RuntimeError(f"expected 41 retained literature matrices, found {retained}")

        result_count = 0
        if target_ids:
            ordered_ids = tuple(sorted(target_ids))
            result_count = connection.execute(
                f"SELECT COUNT(*) FROM results WHERE matrix_id IN ({placeholders(ordered_ids)})",
                ordered_ids,
            ).fetchone()[0]
            connection.execute("BEGIN IMMEDIATE")
            connection.execute(
                f"DELETE FROM matrices WHERE matrix_id IN ({placeholders(ordered_ids)})",
                ordered_ids,
            )

        if list(connection.execute("PRAGMA foreign_key_check")):
            raise RuntimeError("foreign-key check failed")
        if connection.execute("PRAGMA integrity_check").fetchone()[0] != "ok":
            raise RuntimeError("integrity check failed")
        connection.commit()
        print(f"removed_matrices={len(target_ids)} removed_results={result_count} retained_literature={retained}")


if __name__ == "__main__":
    main()
