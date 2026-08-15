-- Run from the repository root with:
-- sqlite3 testdata/copos_testdata.sqlite3 < testdata/archive/restore_fastest_result_cache_from_diagnostics_2026_08_15.sql

ATTACH DATABASE 'experiments/diagnostics.sqlite3' AS diagnostics;
BEGIN IMMEDIATE;

ALTER TABLE matrices
ADD COLUMN fastest_elapsed_ns INTEGER CHECK(fastest_elapsed_ns IS NULL OR fastest_elapsed_ns >= 0);

ALTER TABLE matrices
ADD COLUMN fastest_result_ref TEXT CHECK(CASE
    WHEN fastest_elapsed_ns IS NULL THEN fastest_result_ref IS NULL
    WHEN fastest_result_ref IS NULL OR NOT json_valid(fastest_result_ref) THEN 0
    ELSE json_type(fastest_result_ref) = 'object'
        AND json_type(fastest_result_ref, '$.model_id') = 'text'
        AND length(json_extract(fastest_result_ref, '$.model_id')) > 0
        AND json_extract(fastest_result_ref, '$.model_id') = lower(json_extract(fastest_result_ref, '$.model_id'))
        AND json_extract(fastest_result_ref, '$.mode') IN ('copositive', 'strictly_copositive', 'both')
        AND json_extract(fastest_result_ref, '$.preprocessing') IN ('none', 'connected_components', 'pre_checks', 'both')
        AND json_type(fastest_result_ref, '$.binary_sha256') = 'text'
        AND (
            (json_extract(fastest_result_ref, '$.model_id') = 'hadeler_1983'
             AND json_extract(fastest_result_ref, '$.binary_sha256') = '')
            OR (length(json_extract(fastest_result_ref, '$.binary_sha256')) = 64
                AND json_extract(fastest_result_ref, '$.binary_sha256') NOT GLOB '*[^0-9a-f]*')
        )
    END);

CREATE TEMP VIEW fastest_result_candidates AS
SELECT r.matrix_id, r.model_id, r.mode, r.preprocessing, r.binary_sha256, r.elapsed_ns
FROM diagnostics.results AS r
JOIN main.matrices AS m USING(matrix_id)
WHERE r.status = 'ok'
  AND (r.is_copositive IS NULL OR m.is_copositive IS NULL OR r.is_copositive = m.is_copositive)
  AND (r.is_strictly_copositive IS NULL OR m.is_strictly_copositive IS NULL
       OR r.is_strictly_copositive = m.is_strictly_copositive);

UPDATE matrices AS m
SET (fastest_elapsed_ns, fastest_result_ref) = (
    SELECT elapsed_ns, json_object(
        'model_id', model_id, 'mode', mode, 'preprocessing', preprocessing, 'binary_sha256', binary_sha256
    )
    FROM fastest_result_candidates
    WHERE matrix_id = m.matrix_id
    ORDER BY elapsed_ns, model_id, mode, preprocessing, binary_sha256
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
          ORDER BY elapsed_ns, model_id, mode, preprocessing, binary_sha256
          LIMIT 1
      )
   OR m.fastest_result_ref IS NOT (
          SELECT json_object(
              'model_id', model_id, 'mode', mode, 'preprocessing', preprocessing, 'binary_sha256', binary_sha256
          )
          FROM fastest_result_candidates
          WHERE matrix_id = m.matrix_id
          ORDER BY elapsed_ns, model_id, mode, preprocessing, binary_sha256
          LIMIT 1
      );

COMMIT;
DETACH DATABASE diagnostics;
