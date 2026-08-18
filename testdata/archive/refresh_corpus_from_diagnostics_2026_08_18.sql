-- Applied from the repository root with:
-- sqlite3 testdata/copos_testdata.sqlite3 < testdata/archive/refresh_corpus_from_diagnostics_2026_08_18.sql

.bail on
PRAGMA foreign_keys = ON;
ATTACH DATABASE 'experiments/diagnostics.sqlite3' AS diagnostics;
BEGIN IMMEDIATE;

CREATE TEMP TABLE completed_truth AS
SELECT matrix_id,
       min(is_copositive) AS is_copositive,
       max(is_copositive) AS maximum_copositive,
       min(is_strictly_copositive) AS is_strictly_copositive,
       max(is_strictly_copositive) AS maximum_strictly_copositive
FROM diagnostics.results
WHERE status = 'ok' AND mode = 'both'
GROUP BY matrix_id;

CREATE TEMP TABLE truth_guard (
    candidate_count INTEGER NOT NULL CHECK(candidate_count = 17),
    conflict_count INTEGER NOT NULL CHECK(conflict_count = 0),
    stored_contradiction_count INTEGER NOT NULL CHECK(stored_contradiction_count = 0)
);
INSERT INTO truth_guard
SELECT count(*),
       count(*) FILTER (
           WHERE t.is_copositive <> t.maximum_copositive
              OR t.is_strictly_copositive <> t.maximum_strictly_copositive
       ),
       count(*) FILTER (
           WHERE (m.is_copositive IS NOT NULL AND m.is_copositive <> t.is_copositive)
              OR (m.is_strictly_copositive IS NOT NULL
                  AND m.is_strictly_copositive <> t.is_strictly_copositive)
       )
FROM completed_truth AS t
JOIN matrices AS m USING(matrix_id)
WHERE m.is_copositive IS NULL OR m.is_strictly_copositive IS NULL;

UPDATE matrices AS m
SET is_copositive = coalesce(m.is_copositive, (
        SELECT t.is_copositive FROM completed_truth AS t WHERE t.matrix_id = m.matrix_id
    )),
    is_strictly_copositive = coalesce(m.is_strictly_copositive, (
        SELECT t.is_strictly_copositive FROM completed_truth AS t WHERE t.matrix_id = m.matrix_id
    ))
WHERE (m.is_copositive IS NULL OR m.is_strictly_copositive IS NULL)
  AND EXISTS (
      SELECT 1
      FROM completed_truth AS t
      WHERE t.matrix_id = m.matrix_id
        AND t.is_copositive = t.maximum_copositive
        AND t.is_strictly_copositive = t.maximum_strictly_copositive
  );

CREATE TEMP TABLE new_preprocessing_evidence AS
SELECT r.matrix_id, min(r.is_copositive) AS is_copositive,
       max(r.is_copositive) AS maximum_copositive,
       min(r.is_strictly_copositive) AS is_strictly_copositive,
       max(r.is_strictly_copositive) AS maximum_strictly_copositive
FROM diagnostics.results AS r
JOIN matrices AS m USING(matrix_id)
WHERE r.status = 'ok'
  AND r.mode = 'both'
  AND r.preprocessing = 'both'
  AND instr(r.diagnostics, 'preprocessing_outcome=resolved') > 0
  AND instr(r.diagnostics, 'model_delegations=0') > 0
  AND m.preprocessing_solved = 0
GROUP BY r.matrix_id;

CREATE TEMP TABLE preprocessing_guard (
    addition_count INTEGER NOT NULL CHECK(addition_count = 33),
    conflict_count INTEGER NOT NULL CHECK(conflict_count = 0),
    truth_mismatch_count INTEGER NOT NULL CHECK(truth_mismatch_count = 0),
    existing_flag_count INTEGER NOT NULL CHECK(existing_flag_count = 2732)
);
INSERT INTO preprocessing_guard
SELECT count(*),
       count(*) FILTER (
           WHERE e.is_copositive <> e.maximum_copositive
              OR e.is_strictly_copositive <> e.maximum_strictly_copositive
       ),
       count(*) FILTER (
           WHERE e.is_copositive <> m.is_copositive
              OR e.is_strictly_copositive <> m.is_strictly_copositive
       ),
       (SELECT count(*) FROM matrices WHERE preprocessing_solved)
FROM new_preprocessing_evidence AS e
JOIN matrices AS m USING(matrix_id);

-- Additive by design: the earlier long-timeout preprocessing evidence remains flagged.
UPDATE matrices
SET preprocessing_solved = 1
WHERE preprocessing_solved = 0
  AND matrix_id IN (SELECT matrix_id FROM new_preprocessing_evidence);

CREATE TEMP TABLE fastest_evidence AS
WITH ranked AS (
    SELECT r.matrix_id, r.elapsed_ns,
           json_object(
               'model_id', r.model_id,
               'mode', r.mode,
               'preprocessing', r.preprocessing,
               'binary_sha256', r.binary_sha256
           ) AS result_ref,
           row_number() OVER (
               PARTITION BY r.matrix_id
               ORDER BY r.elapsed_ns, r.model_id, r.mode, r.preprocessing, r.binary_sha256
           ) AS position
    FROM diagnostics.results AS r
    JOIN matrices AS m USING(matrix_id)
    WHERE r.status = 'ok'
      AND r.mode = 'both'
      AND r.is_copositive = m.is_copositive
      AND r.is_strictly_copositive = m.is_strictly_copositive
)
SELECT matrix_id, elapsed_ns, result_ref
FROM ranked
WHERE position = 1;

UPDATE matrices AS m
SET (fastest_elapsed_ns, fastest_result_ref) = (
    SELECT e.elapsed_ns, e.result_ref
    FROM fastest_evidence AS e
    WHERE e.matrix_id = m.matrix_id
)
WHERE EXISTS (SELECT 1 FROM fastest_evidence AS e WHERE e.matrix_id = m.matrix_id);

CREATE TEMP TABLE final_guard (
    truth_remaining_count INTEGER NOT NULL CHECK(truth_remaining_count = 105),
    preprocessing_solved_count INTEGER NOT NULL CHECK(preprocessing_solved_count = 2765),
    fastest_evidence_count INTEGER NOT NULL CHECK(fastest_evidence_count = 3349),
    fastest_mismatch_count INTEGER NOT NULL CHECK(fastest_mismatch_count = 0),
    integrity_result TEXT NOT NULL CHECK(integrity_result = 'ok'),
    foreign_key_error_count INTEGER NOT NULL CHECK(foreign_key_error_count = 0)
);
INSERT INTO final_guard
SELECT count(*) FILTER (WHERE is_copositive IS NULL OR is_strictly_copositive IS NULL),
       count(*) FILTER (WHERE preprocessing_solved),
       (SELECT count(*) FROM fastest_evidence),
       count(*) FILTER (
           WHERE m.fastest_elapsed_ns IS NOT e.elapsed_ns
              OR m.fastest_result_ref IS NOT e.result_ref
       ),
       (SELECT integrity_check FROM pragma_integrity_check LIMIT 1),
       (SELECT count(*) FROM pragma_foreign_key_check)
FROM matrices AS m
LEFT JOIN fastest_evidence AS e USING(matrix_id);

COMMIT;
DETACH DATABASE diagnostics;
