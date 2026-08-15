#!/usr/bin/env python3
"""Replace three catalog rows reconstructed from the wrong portfolio matrix."""

from __future__ import annotations

import argparse
import sqlite3
from pathlib import Path


DATABASE = Path(__file__).parents[1] / "copos_testdata.sqlite3"
COMMENT = ("catalog_correction=the initial reconstruction used an unrelated portfolio matrix; restored the printed Q4 from "
           "Bomze-de Klerk Example 5.4 and the later exact reproductions")
MATRICES = {
    10709: "9044,1054,5140,3322,0,8715,7385,5866,9751,6936,5368,8086,5633,7478,12932",
    10772: "4244,-3746,340,-1478,-4800,3915,2585,1066,4951,2136,568,3286,833,2678,8132",
    10773: "4044,-3946,140,-1678,-5000,3715,2385,866,4751,1936,368,3086,633,2478,7932",
}
OLD_MATRICES = {
    10709: "820,-230,155,-13,-314,484,346,197,592,298,143,419,172,362,916",
    10772: "340,-710,-325,-493,-794,4,-134,-283,112,-182,-337,-61,-308,-118,436",
    10773: "320,-730,-345,-513,-814,-16,-154,-303,92,-202,-357,-81,-328,-138,416",
}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dry-run", action="store_true")
    arguments = parser.parse_args()

    with sqlite3.connect(DATABASE) as connection:
        rows = {
            matrix_id: (dimension, matrix, file_hash, source)
            for matrix_id, dimension, matrix, file_hash, source in connection.execute(
                "SELECT matrix_id, dimension, matrix, file_sha256, source FROM matrices WHERE matrix_id IN (10709, 10772, 10773)"
            )
        }
        if set(rows) != set(MATRICES):
            raise ValueError(f"expected rows {sorted(MATRICES)}, found {sorted(rows)}")
        pending = []
        for matrix_id, expected in MATRICES.items():
            dimension, matrix, file_hash, source = rows[matrix_id]
            if dimension != 5 or file_hash is not None:
                raise ValueError(f"matrix {matrix_id}: unexpected storage")
            if matrix == expected and f"; {COMMENT}" in source:
                continue
            if COMMENT in source:
                raise ValueError(f"matrix {matrix_id}: partial portfolio correction")
            if matrix != OLD_MATRICES[matrix_id]:
                raise ValueError(f"matrix {matrix_id}: unexpected pre-correction payload")
            pending.append((expected, source + f"; {COMMENT}", matrix_id))

        print(f"pending={len(pending)} already_applied={len(MATRICES) - len(pending)}")
        if arguments.dry_run or not pending:
            return
        if len(pending) != len(MATRICES):
            raise ValueError("partial portfolio correction found")

        connection.execute("BEGIN IMMEDIATE")
        connection.executemany("UPDATE matrices SET matrix=?, source=? WHERE matrix_id=?", pending)
        connection.commit()
        if connection.execute("PRAGMA integrity_check").fetchone()[0] != "ok":
            raise ValueError("integrity check failed")

    print(f"applied={len(pending)}")


if __name__ == "__main__":
    main()
