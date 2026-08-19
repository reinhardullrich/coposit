-- Applied from the repository root with:
-- sqlite3 experiments/diagnostics.sqlite3 < testdata/archive/record_preprocessing_times_2026_08_18.sql
--
-- The two CSV files contain exact complete shared-preprocessing outcomes that
-- predate the central combined-result capture.  This records their existing
-- native times so every preprocessing_solved matrix has a referencable result.

.bail on
PRAGMA foreign_keys = ON;
ATTACH DATABASE 'testdata/copos_testdata.sqlite3' AS corpus;
BEGIN IMMEDIATE;

CREATE TEMP TABLE raw_preprocessing (
    matrix_id INTEGER,
    dimension INTEGER,
    maximum_depth INTEGER,
    status TEXT,
    elapsed_ns INTEGER,
    copositive_known INTEGER,
    is_copositive INTEGER,
    strict_known INTEGER,
    is_strictly_copositive INTEGER,
    message TEXT
);

.mode csv
.import --skip 1 experiments/preprocessing_depth_2026-08-15/results/depth_2_current_60s_merged.csv raw_preprocessing

INSERT INTO results (
    matrix_id, model_id, mode, preprocessing, binary_sha256, status,
    is_copositive, is_strictly_copositive, elapsed_ns, timeout_ns, recorded_at, diagnostics, message
)
SELECT raw.matrix_id, 'preprocessing_depth_2', 'both', 'both',
       '01ad89756071502e0608710ff27600c535103d0c424b9112b58c12ae5e7725b4', 'ok',
       raw.is_copositive, raw.is_strictly_copositive, raw.elapsed_ns, 60000000000, '2026-08-18T00:00:00+00:00',
       'preprocessing_outcome=resolved' || char(10) || 'model_delegations=0' || char(10) ||
       'source_csv=experiments/preprocessing_depth_2026-08-15/results/depth_2_current_60s_merged.csv',
       'Imported exact complete shared-preprocessing result from the retained 60-second continuation.'
FROM raw_preprocessing AS raw
JOIN corpus.matrices AS m USING(matrix_id)
WHERE raw.status = 'ok'
  AND raw.copositive_known = 1
  AND raw.strict_known = 1
  AND m.preprocessing_solved
  AND NOT EXISTS (
      SELECT 1
      FROM results AS r
      WHERE r.matrix_id = raw.matrix_id
        AND r.status = 'ok'
        AND r.mode = 'both'
        AND r.preprocessing = 'both'
        AND instr(r.diagnostics, 'preprocessing_outcome=resolved') > 0
        AND instr(r.diagnostics, 'model_delegations=0') > 0
  )
ON CONFLICT(matrix_id, model_id, mode, preprocessing, binary_sha256) DO UPDATE SET
    status = excluded.status,
    is_copositive = excluded.is_copositive,
    is_strictly_copositive = excluded.is_strictly_copositive,
    elapsed_ns = excluded.elapsed_ns,
    timeout_ns = excluded.timeout_ns,
    diagnostics = excluded.diagnostics,
    message = excluded.message;

DELETE FROM raw_preprocessing;
.import --skip 1 experiments/preprocessing_depth_2026-08-15/results/depth_2_motzkin_straus_timeouts_600s_2026-08-17.csv raw_preprocessing

INSERT INTO results (
    matrix_id, model_id, mode, preprocessing, binary_sha256, status,
    is_copositive, is_strictly_copositive, elapsed_ns, timeout_ns, recorded_at, diagnostics, message
)
SELECT raw.matrix_id, 'preprocessing_depth_2', 'both', 'both',
       '01ad89756071502e0608710ff27600c535103d0c424b9112b58c12ae5e7725b4', 'ok',
       raw.is_copositive, raw.is_strictly_copositive, raw.elapsed_ns, 600000000000, '2026-08-18T00:00:00+00:00',
       'preprocessing_outcome=resolved' || char(10) || 'model_delegations=0' || char(10) ||
       'source_csv=experiments/preprocessing_depth_2026-08-15/results/depth_2_motzkin_straus_timeouts_600s_2026-08-17.csv',
       'Imported exact complete shared-preprocessing result from the retained 600-second continuation.'
FROM raw_preprocessing AS raw
JOIN corpus.matrices AS m USING(matrix_id)
WHERE raw.status = 'ok'
  AND raw.copositive_known = 1
  AND raw.strict_known = 1
  AND m.preprocessing_solved
  AND NOT EXISTS (
      SELECT 1
      FROM results AS r
      WHERE r.matrix_id = raw.matrix_id
        AND r.status = 'ok'
        AND r.mode = 'both'
        AND r.preprocessing = 'both'
        AND instr(r.diagnostics, 'preprocessing_outcome=resolved') > 0
        AND instr(r.diagnostics, 'model_delegations=0') > 0
  )
ON CONFLICT(matrix_id, model_id, mode, preprocessing, binary_sha256) DO UPDATE SET
    status = excluded.status,
    is_copositive = excluded.is_copositive,
    is_strictly_copositive = excluded.is_strictly_copositive,
    elapsed_ns = excluded.elapsed_ns,
    timeout_ns = excluded.timeout_ns,
    diagnostics = excluded.diagnostics,
    message = excluded.message;

CREATE TEMP TABLE preprocessing_timing AS
WITH ranked AS (
    SELECT r.matrix_id,
           r.elapsed_ns,
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
    FROM results AS r
    JOIN corpus.matrices AS m USING(matrix_id)
    WHERE m.preprocessing_solved
      AND r.status = 'ok'
      AND r.mode = 'both'
      AND r.preprocessing = 'both'
      AND r.is_copositive = m.is_copositive
      AND r.is_strictly_copositive = m.is_strictly_copositive
      AND instr(r.diagnostics, 'preprocessing_outcome=resolved') > 0
      AND instr(r.diagnostics, 'model_delegations=0') > 0
)
SELECT matrix_id, elapsed_ns, result_ref
FROM ranked
WHERE position = 1;

CREATE TEMP TABLE preprocessing_timing_guard (
    imported_row_count INTEGER NOT NULL CHECK(imported_row_count = 36),
    timing_count INTEGER NOT NULL CHECK(timing_count = 2765),
    timing_mismatch_count INTEGER NOT NULL CHECK(timing_mismatch_count = 0)
);

UPDATE corpus.matrices AS m
SET (fastest_elapsed_ns, fastest_result_ref) = (
    SELECT t.elapsed_ns, t.result_ref
    FROM preprocessing_timing AS t
    WHERE t.matrix_id = m.matrix_id
)
WHERE m.preprocessing_solved;

INSERT INTO preprocessing_timing_guard
SELECT (SELECT count(*) FROM results
        WHERE model_id = 'preprocessing_depth_2' AND mode = 'both' AND preprocessing = 'both' AND status = 'ok'),
       (SELECT count(*) FROM preprocessing_timing),
       count(*)
FROM corpus.matrices AS m
JOIN preprocessing_timing AS t USING(matrix_id)
WHERE m.fastest_elapsed_ns IS NOT t.elapsed_ns
   OR m.fastest_result_ref IS NOT t.result_ref;

COMMIT;
DETACH DATABASE corpus;
