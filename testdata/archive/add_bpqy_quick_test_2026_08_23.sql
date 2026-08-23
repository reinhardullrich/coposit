-- Applied from the repository root with:
-- sqlite3 testdata/copos_testdata.sqlite3 < testdata/archive/add_bpqy_quick_test_2026_08_23.sql

.bail on
PRAGMA foreign_keys = ON;
BEGIN IMMEDIATE;

CREATE TEMP TABLE bpqy_quick_selection (
    matrix_id INTEGER PRIMARY KEY,
    expected_dimension INTEGER NOT NULL,
    sat_b3_elapsed_ns INTEGER NOT NULL,
    CHECK(sat_b3_elapsed_ns BETWEEN 15000000000 AND 100000000000)
) STRICT;

INSERT INTO bpqy_quick_selection VALUES
    (13173, 30, 17671501086),
    (15436, 60, 20072890289),
    (13226, 35, 32793475712),
    (13318, 40, 45078830912),
    (13377, 45, 87197968201),
    (13387, 45, 96797863195);

ALTER TABLE matrices
ADD COLUMN bpqy_quick_test INTEGER NOT NULL DEFAULT 0 CHECK(bpqy_quick_test IN (0, 1));

UPDATE matrices
SET bpqy_quick_test = matrix_id IN (SELECT matrix_id FROM bpqy_quick_selection);

CREATE TEMP TABLE bpqy_quick_guard (
    selected_rows INTEGER NOT NULL CHECK(selected_rows = 6),
    mismatched_rows INTEGER NOT NULL CHECK(mismatched_rows = 0),
    distinct_dimensions INTEGER NOT NULL CHECK(distinct_dimensions = 5),
    minimum_dimension INTEGER NOT NULL CHECK(minimum_dimension = 30),
    maximum_dimension INTEGER NOT NULL CHECK(maximum_dimension = 60),
    minimum_elapsed_ns INTEGER NOT NULL CHECK(minimum_elapsed_ns = 17671501086),
    maximum_elapsed_ns INTEGER NOT NULL CHECK(maximum_elapsed_ns = 96797863195),
    integrity_result TEXT NOT NULL CHECK(integrity_result = 'ok'),
    foreign_key_errors INTEGER NOT NULL CHECK(foreign_key_errors = 0)
) STRICT;

INSERT INTO bpqy_quick_guard
SELECT count(*) FILTER (WHERE m.bpqy_quick_test),
       count(*) FILTER (
           WHERE m.bpqy_quick_test <> (s.matrix_id IS NOT NULL)
              OR s.matrix_id IS NOT NULL AND (
                  m.dimension <> s.expected_dimension
                  OR NOT m.bpqy_benchmark
                  OR m.preprocessing_solved
                  OR m.is_copositive IS NOT 1
                  OR m.is_strictly_copositive IS NOT 1
              )
       ),
       count(DISTINCT m.dimension) FILTER (WHERE m.bpqy_quick_test),
       min(m.dimension) FILTER (WHERE m.bpqy_quick_test),
       max(m.dimension) FILTER (WHERE m.bpqy_quick_test),
       (SELECT min(sat_b3_elapsed_ns) FROM bpqy_quick_selection),
       (SELECT max(sat_b3_elapsed_ns) FROM bpqy_quick_selection),
       (SELECT integrity_check FROM pragma_integrity_check LIMIT 1),
       (SELECT count(*) FROM pragma_foreign_key_check)
FROM matrices AS m
LEFT JOIN bpqy_quick_selection AS s USING(matrix_id);

COMMIT;
