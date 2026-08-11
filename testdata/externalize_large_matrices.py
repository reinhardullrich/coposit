#!/usr/bin/env python3
"""Move large inline corpus matrices to exact Matrix Market files."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import sqlite3


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--database",
        type=Path,
        default=Path(__file__).with_name("Copos_testdata.sqlite3"),
    )
    parser.add_argument("--threshold-bytes", type=int, default=500_000)
    arguments = parser.parse_args()
    if arguments.threshold_bytes < 1:
        parser.error("--threshold-bytes must be positive")

    database = arguments.database.resolve()
    matrix_directory = database.parent / "matrices"
    matrix_directory.mkdir(exist_ok=True)
    connection = sqlite3.connect(database)
    connection.execute("PRAGMA foreign_keys = ON")
    try:
        rows = connection.execute(
            "SELECT matrix_id, dimension, matrix FROM matrices "
            "WHERE matrix NOT LIKE 'file:%' AND length(matrix) > ? ORDER BY matrix_id",
            (arguments.threshold_bytes,),
        ).fetchall()
        for matrix_id, dimension, values in rows:
            expected_values = dimension * (dimension + 1) // 2
            if values.count(",") + 1 != expected_values:
                raise ValueError(f"matrix {matrix_id} has the wrong packed value count")

            relative_path = Path("matrices") / f"{matrix_id}.mtx"
            target = database.parent / relative_path
            temporary = target.with_suffix(".mtx.tmp")
            with temporary.open("w", encoding="ascii", newline="\n") as output:
                output.write("%%MatrixMarket matrix array integer symmetric\n")
                output.write(f"% Coposit corpus matrix {matrix_id}\n")
                output.write(f"{dimension} {dimension}\n")
                output.write(values.replace(",", "\n"))
                output.write("\n")
            os.replace(temporary, target)
            connection.execute(
                "UPDATE matrices SET matrix = ? WHERE matrix_id = ?",
                (f"file:{relative_path.as_posix()}", matrix_id),
            )
        connection.commit()
        connection.execute("VACUUM")
        print(f"externalized={len(rows)} threshold_bytes={arguments.threshold_bytes} directory={matrix_directory}")
    finally:
        connection.close()


if __name__ == "__main__":
    main()
