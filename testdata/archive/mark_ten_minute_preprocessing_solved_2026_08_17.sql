-- Mark the 22 Motzkin--Straus matrices completely classified by the
-- depth-2 ten-minute preprocessing continuation. The other 23 inputs timed
-- out and deliberately remain unmarked.

BEGIN IMMEDIATE;

CREATE TEMP TABLE ten_minute_preprocessing_solved (
    matrix_id INTEGER PRIMARY KEY,
    is_copositive INTEGER NOT NULL CHECK(is_copositive IN (0, 1)),
    is_strictly_copositive INTEGER NOT NULL CHECK(is_strictly_copositive IN (0, 1))
) WITHOUT ROWID;

INSERT INTO ten_minute_preprocessing_solved VALUES
    (9585, 1, 1),
    (9586, 1, 0),
    (9587, 0, 0),
    (9588, 1, 1),
    (9589, 1, 0),
    (9591, 1, 1),
    (9592, 1, 0),
    (9593, 0, 0),
    (9594, 1, 1),
    (9595, 1, 0),
    (9596, 0, 0),
    (9608, 0, 0),
    (9621, 1, 1),
    (9622, 1, 0),
    (9623, 0, 0),
    (9644, 0, 0),
    (9652, 1, 1),
    (9653, 1, 0),
    (9654, 0, 0),
    (10825, 0, 0),
    (10831, 0, 0),
    (10850, 0, 0);

CREATE TEMP TABLE ten_minute_preprocessing_initial_guard (
    matrix_count INTEGER NOT NULL CHECK(matrix_count = 3513),
    evidence_count INTEGER NOT NULL CHECK(evidence_count = 22),
    missing_or_conflicting INTEGER NOT NULL CHECK(missing_or_conflicting = 0),
    preprocessing_solved_count INTEGER NOT NULL CHECK(preprocessing_solved_count IN (2708, 2730))
);

INSERT INTO ten_minute_preprocessing_initial_guard
SELECT (SELECT count(*) FROM matrices),
       (SELECT count(*) FROM ten_minute_preprocessing_solved),
       (SELECT count(*)
        FROM ten_minute_preprocessing_solved AS e
        LEFT JOIN matrices AS m USING(matrix_id)
        WHERE m.matrix_id IS NULL
           OR m.is_copositive IS NOT e.is_copositive
           OR m.is_strictly_copositive IS NOT e.is_strictly_copositive),
       (SELECT count(*) FROM matrices WHERE preprocessing_solved);

UPDATE matrices
SET preprocessing_solved = 1
WHERE matrix_id IN (SELECT matrix_id FROM ten_minute_preprocessing_solved);

CREATE TEMP TABLE ten_minute_preprocessing_final_guard (
    marked_evidence INTEGER NOT NULL CHECK(marked_evidence = 22),
    preprocessing_solved_count INTEGER NOT NULL CHECK(preprocessing_solved_count = 2730),
    pending_evidence INTEGER NOT NULL CHECK(pending_evidence = 0)
);

INSERT INTO ten_minute_preprocessing_final_guard
SELECT count(*) FILTER (WHERE m.preprocessing_solved),
       (SELECT count(*) FROM matrices WHERE preprocessing_solved),
       count(*) FILTER (WHERE NOT m.preprocessing_solved)
FROM ten_minute_preprocessing_solved AS e
JOIN matrices AS m USING(matrix_id);

COMMIT;
