-- Applied from the repository root with:
-- sqlite3 testdata/copos_testdata.sqlite3 < testdata/archive/exclude_preprocessing_solved_from_bpqy_benchmark_2026_08_23.sql

.bail on
PRAGMA foreign_keys = ON;
BEGIN IMMEDIATE;

ALTER TABLE matrices DROP COLUMN bpqy_benchmark;
ALTER TABLE matrices ADD COLUMN bpqy_benchmark INTEGER GENERATED ALWAYS AS (
    CASE
        WHEN source_id = 51
         AND family GLOB 'BPQY COP *'
         AND preprocessing_solved = 0
         AND (
             is_copositive = 1 AND is_strictly_copositive = 1
             OR is_copositive IS NULL AND is_strictly_copositive IS NULL
         )
        THEN 1
        ELSE 0
    END
) VIRTUAL;

CREATE TEMP TABLE bpqy_benchmark_guard (
    members INTEGER NOT NULL CHECK(members = 404),
    preprocessing_members INTEGER NOT NULL CHECK(preprocessing_members = 0),
    strict_members INTEGER NOT NULL CHECK(strict_members = 298),
    unknown_members INTEGER NOT NULL CHECK(unknown_members = 106),
    integrity_result TEXT NOT NULL CHECK(integrity_result = 'ok'),
    foreign_key_errors INTEGER NOT NULL CHECK(foreign_key_errors = 0)
);
INSERT INTO bpqy_benchmark_guard
SELECT count(*) FILTER (WHERE bpqy_benchmark),
       count(*) FILTER (WHERE bpqy_benchmark AND preprocessing_solved),
       count(*) FILTER (WHERE bpqy_benchmark AND is_copositive = 1 AND is_strictly_copositive = 1),
       count(*) FILTER (WHERE bpqy_benchmark AND is_copositive IS NULL AND is_strictly_copositive IS NULL),
       (SELECT integrity_check FROM pragma_integrity_check LIMIT 1),
       (SELECT count(*) FROM pragma_foreign_key_check)
FROM matrices;

COMMIT;
