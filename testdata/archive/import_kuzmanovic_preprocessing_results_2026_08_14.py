#!/usr/bin/env python3
"""Stage Kuzmanovic's 100,000 published preprocessing outputs without treating their labels as truth."""

from __future__ import annotations

import hashlib
import re
import sqlite3
from pathlib import Path


PROJECT = Path(__file__).resolve().parents[2]
DATABASE = PROJECT / "testdata/copos_testdata.sqlite3"
RESULTS = PROJECT / "research/kuzmanovic-results"
SOURCE_TITLE = "Copositivity detection - a preprocessing study"
FILES = {
    "copositiveResults.txt": ("copositive", "6875c1a0b0e33c05a3d6565167698197a208e607057d1aa86410656aa4b21288"),
    "notCopositiveResults.txt": ("not_copositive", "ae18a72a46e4baf607c4121bb8532729432b4de07cffd9c275825c090825b54b"),
    "NoAnswerResults.txt": ("no_answer", "cc17d662ddc7342b66ec42d3c0d6a9980e90be615cba1e2c34d8614104fd13ba"),
}
EXPECTED_LABEL_COUNTS = {"copositive": 5988, "not_copositive": 91162, "no_answer": 2850}
HEADER = re.compile(rb"(?m)^Example : (\d+)\r?$")
RESULT = re.compile(r"The result is\s*:\s*([^\r\n]+)")


def dimension(example: int) -> int:
    if example <= 10000:
        return 5
    return min(20, 6 + (example - 10001) // 5000)


def parse_file(path: Path, label: str):
    payload = path.read_bytes()
    matches = list(HEADER.finditer(payload))
    for record_number, match in enumerate(matches):
        end = matches[record_number + 1].start() if record_number + 1 < len(matches) else len(payload)
        raw = payload[match.start():end]
        lines = raw.decode("utf-8-sig").splitlines()
        example = int(match.group(1))
        n = dimension(example)
        rows: list[list[str]] = []
        for line in lines[1:]:
            if not line.strip():
                if rows:
                    break
                continue
            tokens = line.split()
            if not all(re.fullmatch(r"-?\d+(?:\.\d+)?", token) for token in tokens):
                raise ValueError(f"non-numeric matrix row in {path.name}, example {example}")
            rows.append(tokens)
        if len(rows) != n or any(len(row) not in (n, n + 1) for row in rows):
            raise ValueError(f"wrong matrix shape in {path.name}, example {example}")
        widths = {len(row) for row in rows}
        if len(widths) != 1:
            raise ValueError(f"inconsistent published vector in {path.name}, example {example}")

        matrix_rows = [[int(token) for token in row[:n]] for row in rows]
        if any(matrix_rows[i][j] != matrix_rows[j][i] for i in range(n) for j in range(n)):
            raise ValueError(f"nonsymmetric matrix in {path.name}, example {example}")
        packed = ",".join(str(matrix_rows[i][j]) for i in range(n) for j in range(i, n))
        vector = ",".join(row[n] for row in rows) if widths == {n + 1} else None
        result = RESULT.search(raw.decode("utf-8-sig"))
        yield (
            example,
            n,
            packed,
            label,
            vector,
            result.group(1).strip() if result else None,
            path.name,
            hashlib.sha256(raw).hexdigest(),
        )


def main() -> None:
    records = []
    observed = {label: 0 for label in EXPECTED_LABEL_COUNTS}
    for filename, (label, expected_sha256) in FILES.items():
        path = RESULTS / filename
        if hashlib.sha256(path.read_bytes()).hexdigest() != expected_sha256:
            raise ValueError(f"source hash mismatch: {path}")
        current = list(parse_file(path, label))
        observed[label] = len(current)
        records.extend(current)
    if observed != EXPECTED_LABEL_COUNTS or len(records) != 100000:
        raise ValueError(f"wrong label counts: {observed}")
    if sorted(record[0] for record in records) != list(range(1, 100001)):
        raise ValueError("published example numbers are not exactly 1 through 100000")

    with sqlite3.connect(DATABASE) as connection:
        connection.execute("PRAGMA foreign_keys=ON")
        source = connection.execute("SELECT source_id FROM sources WHERE title=?", (SOURCE_TITLE,)).fetchone()
        if not source:
            raise ValueError(f"missing source row: {SOURCE_TITLE}")
        connection.execute(
            """CREATE TABLE IF NOT EXISTS kuzmanovic_test_matrices (
                   published_example INTEGER PRIMARY KEY CHECK(published_example BETWEEN 1 AND 100000),
                   dimension INTEGER NOT NULL CHECK(dimension BETWEEN 5 AND 20),
                   matrix TEXT NOT NULL CHECK(length(matrix) > 0),
                   published_label TEXT NOT NULL CHECK(published_label IN ('copositive','not_copositive','no_answer')),
                   published_vector TEXT,
                   published_quadratic_value TEXT,
                   source_file TEXT NOT NULL CHECK(length(source_file) > 0),
                   source_record_sha256 TEXT NOT NULL CHECK(
                       length(source_record_sha256)=64 AND source_record_sha256 NOT GLOB '*[^0-9a-f]*'
                   ),
                   source_id INTEGER NOT NULL REFERENCES sources(source_id),
                   cbdd_zed_status TEXT CHECK(
                       cbdd_zed_status IS NULL OR cbdd_zed_status IN ('ok','parse_error','timeout','node_limit','error')
                   ),
                   cbdd_zed_is_copositive INTEGER CHECK(cbdd_zed_is_copositive IS NULL OR cbdd_zed_is_copositive IN (0,1)),
                   cbdd_zed_matches_published_label INTEGER
                       CHECK(cbdd_zed_matches_published_label IS NULL OR cbdd_zed_matches_published_label IN (0,1)),
                   cbdd_zed_elapsed_ns INTEGER CHECK(cbdd_zed_elapsed_ns IS NULL OR cbdd_zed_elapsed_ns >= 0),
                   cbdd_zed_timeout_ns INTEGER CHECK(cbdd_zed_timeout_ns IS NULL OR cbdd_zed_timeout_ns > 0),
                   cbdd_zed_binary_sha256 TEXT CHECK(cbdd_zed_binary_sha256 IS NULL OR (
                       length(cbdd_zed_binary_sha256)=64 AND cbdd_zed_binary_sha256 NOT GLOB '*[^0-9a-f]*'
                   )),
                   cbdd_zed_recorded_at TEXT,
                   cbdd_zed_message TEXT
               ) STRICT"""
        )
        existing = connection.execute("SELECT count(*) FROM kuzmanovic_test_matrices").fetchone()[0]
        if existing not in (0, 100000):
            raise ValueError(f"partial Kuzmanovic staging import: {existing}/100000")
        if existing == 0:
            connection.executemany(
                """INSERT INTO kuzmanovic_test_matrices(
                       published_example,dimension,matrix,published_label,published_vector,published_quadratic_value,
                       source_file,source_record_sha256,source_id
                   ) VALUES (?,?,?,?,?,?,?,?,?)""",
                (record + (source[0],) for record in records),
            )
            connection.execute(
                """UPDATE sources
                   SET comment=replace(comment, 'the unseeded 100000-matrix result archive remains excluded',
                                               'the exact unseeded 100000-matrix result archive is staged separately with published labels')
                   WHERE source_id=?""",
                source,
            )

        stored = dict(connection.execute(
            "SELECT published_label,count(*) FROM kuzmanovic_test_matrices GROUP BY published_label"
        ))
        if stored != EXPECTED_LABEL_COUNTS:
            raise ValueError(f"stored label counts differ: {stored}")
        integrity = connection.execute("PRAGMA integrity_check").fetchone()[0]
        if integrity != "ok":
            raise ValueError(f"SQLite integrity check failed: {integrity}")
    print(f"rows=100000 labels={observed} integrity={integrity}")


if __name__ == "__main__":
    main()
