-- Run from the repository root with:
-- sqlite3 testdata/copos_testdata.sqlite3 < testdata/archive/reset_fastest_result_cache_to_both_2026_08_16.sql

ATTACH DATABASE 'experiments/diagnostics.sqlite3' AS diagnostics;
BEGIN IMMEDIATE;

CREATE TEMP VIEW fastest_result_candidates AS
SELECT r.matrix_id, r.model_id, r.mode, r.preprocessing, r.binary_sha256, r.elapsed_ns
FROM diagnostics.results AS r
JOIN main.matrices AS m USING(matrix_id)
WHERE r.status = 'ok'
  AND r.mode = 'both'
  AND (m.is_copositive IS NULL OR r.is_copositive = m.is_copositive)
  AND (m.is_strictly_copositive IS NULL OR r.is_strictly_copositive = m.is_strictly_copositive);

UPDATE matrices AS m
SET (fastest_elapsed_ns, fastest_result_ref) = (
    SELECT elapsed_ns, json_object(
        'model_id', model_id, 'mode', mode, 'preprocessing', preprocessing, 'binary_sha256', binary_sha256
    )
    FROM fastest_result_candidates
    WHERE matrix_id = m.matrix_id
    ORDER BY elapsed_ns, model_id, binary_sha256
    LIMIT 1
);

CREATE TEMP TABLE fastest_result_cache_guard (
    mismatch_count INTEGER CHECK(mismatch_count = 0)
);
INSERT INTO fastest_result_cache_guard
SELECT count(*)
FROM matrices AS m
WHERE m.fastest_elapsed_ns IS NOT (
          SELECT elapsed_ns
          FROM fastest_result_candidates
          WHERE matrix_id = m.matrix_id
          ORDER BY elapsed_ns, model_id, binary_sha256
          LIMIT 1
      )
   OR m.fastest_result_ref IS NOT (
          SELECT json_object(
              'model_id', model_id, 'mode', mode, 'preprocessing', preprocessing, 'binary_sha256', binary_sha256
          )
          FROM fastest_result_candidates
          WHERE matrix_id = m.matrix_id
          ORDER BY elapsed_ns, model_id, binary_sha256
          LIMIT 1
      );

COMMIT;
DETACH DATABASE diagnostics;
