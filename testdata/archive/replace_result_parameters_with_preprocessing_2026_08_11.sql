PRAGMA foreign_keys = OFF;
BEGIN IMMEDIATE;

ALTER TABLE results RENAME TO results_with_free_text_parameters;

CREATE TABLE results (
    matrix_id INTEGER NOT NULL REFERENCES matrices(matrix_id) ON DELETE CASCADE,
    model_id TEXT NOT NULL CHECK(length(model_id) > 0 AND model_id = lower(model_id)),
    mode TEXT NOT NULL CHECK(mode IN ('copositive', 'strictly_copositive', 'both')),
    preprocessing TEXT NOT NULL CHECK(preprocessing IN ('none', 'connected_components', 'pre_checks', 'both')),
    binary_sha256 TEXT NOT NULL CHECK(
        (model_id = 'hadeler_1983' AND binary_sha256 = '')
        OR (length(binary_sha256) = 64 AND binary_sha256 NOT GLOB '*[^0-9a-f]*')
    ),
    status TEXT NOT NULL CHECK(status IN ('ok', 'timeout', 'node_limit', 'error')),
    is_copositive INTEGER CHECK(is_copositive IN (0, 1)),
    is_strictly_copositive INTEGER CHECK(is_strictly_copositive IN (0, 1)),
    elapsed_ns INTEGER CHECK(elapsed_ns IS NULL OR elapsed_ns >= 0),
    timeout_ns INTEGER NOT NULL CHECK(timeout_ns > 0),
    recorded_at TEXT NOT NULL CHECK(length(recorded_at) > 0),
    message TEXT,
    PRIMARY KEY (matrix_id, model_id, mode, preprocessing, binary_sha256),
    CHECK (
        (status = 'ok' AND elapsed_ns IS NOT NULL AND (
            (mode = 'copositive' AND is_copositive IS NOT NULL AND is_strictly_copositive IS NULL)
            OR (mode = 'strictly_copositive' AND is_copositive IS NULL AND is_strictly_copositive IS NOT NULL)
            OR (mode = 'both' AND is_copositive IS NOT NULL AND is_strictly_copositive IS NOT NULL)
        ))
        OR (status IN ('timeout', 'node_limit', 'error')
            AND is_copositive IS NULL AND is_strictly_copositive IS NULL AND elapsed_ns IS NULL)
    ),
    CHECK(status <> 'ok' OR mode <> 'both' OR is_strictly_copositive = 0 OR is_copositive = 1)
) STRICT;

WITH normalized AS (
    SELECT
        matrix_id,
        model_id,
        mode,
        CASE parameters
            WHEN 'preprocessing=connected_components' THEN 'connected_components'
            WHEN 'preprocessing=pre_checks' THEN 'pre_checks'
            WHEN 'preprocessing=both' THEN 'both'
            ELSE 'none'
        END AS preprocessing,
        binary_sha256,
        status,
        is_copositive,
        is_strictly_copositive,
        elapsed_ns,
        timeout_ns,
        recorded_at,
        message,
        rowid AS original_rowid
    FROM results_with_free_text_parameters
), ranked AS (
    SELECT *, row_number() OVER (
        PARTITION BY matrix_id, model_id, mode, preprocessing, binary_sha256
        ORDER BY (status = 'ok') DESC, timeout_ns DESC, recorded_at DESC, original_rowid DESC
    ) AS preference
    FROM normalized
)
INSERT INTO results (
    matrix_id, model_id, mode, preprocessing, binary_sha256, status, is_copositive,
    is_strictly_copositive, elapsed_ns, timeout_ns, recorded_at, message
)
SELECT
    matrix_id, model_id, mode, preprocessing, binary_sha256, status, is_copositive,
    is_strictly_copositive, elapsed_ns, timeout_ns, recorded_at, message
FROM ranked
WHERE preference = 1;

DROP TABLE results_with_free_text_parameters;

COMMIT;
PRAGMA foreign_keys = ON;
