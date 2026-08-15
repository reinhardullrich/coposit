BEGIN IMMEDIATE;

CREATE TEMP TABLE fastest_result_corpus_guard (
    matrix_count INTEGER CHECK(matrix_count = 5095),
    result_count INTEGER CHECK(result_count = 188608)
);
INSERT INTO fastest_result_corpus_guard SELECT (SELECT count(*) FROM matrices), (SELECT count(*) FROM results);

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

CREATE VIEW fastest_result_candidates AS
SELECT r.matrix_id, r.model_id, r.mode, r.preprocessing, r.binary_sha256, r.elapsed_ns
FROM results AS r
JOIN matrices AS m USING(matrix_id)
WHERE r.status = 'ok'
  AND (r.is_copositive IS NULL OR m.is_copositive IS NULL OR r.is_copositive = m.is_copositive)
  AND (r.is_strictly_copositive IS NULL OR m.is_strictly_copositive IS NULL
       OR r.is_strictly_copositive = m.is_strictly_copositive);

CREATE TRIGGER results_fastest_cache_after_insert AFTER INSERT ON results BEGIN
    UPDATE matrices
    SET (fastest_elapsed_ns, fastest_result_ref) = (
        SELECT elapsed_ns, json_object(
            'model_id', model_id, 'mode', mode, 'preprocessing', preprocessing, 'binary_sha256', binary_sha256
        )
        FROM fastest_result_candidates
        WHERE matrix_id = NEW.matrix_id
        ORDER BY elapsed_ns, model_id, mode, preprocessing, binary_sha256
        LIMIT 1
    )
    WHERE matrix_id = NEW.matrix_id;
END;

CREATE TRIGGER results_fastest_cache_after_update AFTER UPDATE ON results BEGIN
    UPDATE matrices
    SET (fastest_elapsed_ns, fastest_result_ref) = (
        SELECT elapsed_ns, json_object(
            'model_id', model_id, 'mode', mode, 'preprocessing', preprocessing, 'binary_sha256', binary_sha256
        )
        FROM fastest_result_candidates
        WHERE matrix_id = OLD.matrix_id
        ORDER BY elapsed_ns, model_id, mode, preprocessing, binary_sha256
        LIMIT 1
    )
    WHERE matrix_id = OLD.matrix_id;
    UPDATE matrices
    SET (fastest_elapsed_ns, fastest_result_ref) = (
        SELECT elapsed_ns, json_object(
            'model_id', model_id, 'mode', mode, 'preprocessing', preprocessing, 'binary_sha256', binary_sha256
        )
        FROM fastest_result_candidates
        WHERE matrix_id = NEW.matrix_id
        ORDER BY elapsed_ns, model_id, mode, preprocessing, binary_sha256
        LIMIT 1
    )
    WHERE matrix_id = NEW.matrix_id AND NEW.matrix_id <> OLD.matrix_id;
END;

CREATE TRIGGER results_fastest_cache_after_delete AFTER DELETE ON results BEGIN
    UPDATE matrices
    SET (fastest_elapsed_ns, fastest_result_ref) = (
        SELECT elapsed_ns, json_object(
            'model_id', model_id, 'mode', mode, 'preprocessing', preprocessing, 'binary_sha256', binary_sha256
        )
        FROM fastest_result_candidates
        WHERE matrix_id = OLD.matrix_id
        ORDER BY elapsed_ns, model_id, mode, preprocessing, binary_sha256
        LIMIT 1
    )
    WHERE matrix_id = OLD.matrix_id;
END;

CREATE TRIGGER matrices_fastest_cache_after_truth_update
AFTER UPDATE OF is_copositive, is_strictly_copositive ON matrices BEGIN
    UPDATE matrices
    SET (fastest_elapsed_ns, fastest_result_ref) = (
        SELECT elapsed_ns, json_object(
            'model_id', model_id, 'mode', mode, 'preprocessing', preprocessing, 'binary_sha256', binary_sha256
        )
        FROM fastest_result_candidates
        WHERE matrix_id = NEW.matrix_id
        ORDER BY elapsed_ns, model_id, mode, preprocessing, binary_sha256
        LIMIT 1
    )
    WHERE matrix_id = NEW.matrix_id;
END;

UPDATE matrices
SET (fastest_elapsed_ns, fastest_result_ref) = (
    SELECT elapsed_ns, json_object(
        'model_id', model_id, 'mode', mode, 'preprocessing', preprocessing, 'binary_sha256', binary_sha256
    )
    FROM fastest_result_candidates
    WHERE matrix_id = matrices.matrix_id
    ORDER BY elapsed_ns, model_id, mode, preprocessing, binary_sha256
    LIMIT 1
);

CREATE TEMP TABLE fastest_result_assignment_guard (
    populated_count INTEGER CHECK(populated_count = 3271),
    pair_mismatch_count INTEGER CHECK(pair_mismatch_count = 0),
    reference_mismatch_count INTEGER CHECK(reference_mismatch_count = 0)
);
INSERT INTO fastest_result_assignment_guard
SELECT
    count(*) FILTER (WHERE m.fastest_elapsed_ns IS NOT NULL),
    count(*) FILTER (WHERE (m.fastest_elapsed_ns IS NULL) <> (m.fastest_result_ref IS NULL)),
    count(*) FILTER (WHERE m.fastest_result_ref IS NOT NULL AND NOT EXISTS (
        SELECT 1
        FROM fastest_result_candidates AS candidate
        WHERE candidate.matrix_id = m.matrix_id
          AND candidate.elapsed_ns = m.fastest_elapsed_ns
          AND candidate.model_id = json_extract(m.fastest_result_ref, '$.model_id')
          AND candidate.mode = json_extract(m.fastest_result_ref, '$.mode')
          AND candidate.preprocessing = json_extract(m.fastest_result_ref, '$.preprocessing')
          AND candidate.binary_sha256 = json_extract(m.fastest_result_ref, '$.binary_sha256')
    ))
FROM matrices AS m;

COMMIT;
