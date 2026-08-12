.bail on

-- Preserve every strict-only reference row while making the result identity and payload mode-aware.
BEGIN IMMEDIATE;

CREATE TEMP TABLE migration_guard (
    existing_mode_columns INTEGER CHECK(existing_mode_columns = 0)
);
INSERT INTO migration_guard
SELECT count(*) FROM pragma_table_info('results') WHERE name = 'mode';

ALTER TABLE results RENAME TO strict_only_results;

CREATE TABLE results (
    matrix_id INTEGER NOT NULL REFERENCES matrices(matrix_id) ON DELETE CASCADE,
    model_id TEXT NOT NULL CHECK(length(model_id) > 0 AND model_id = lower(model_id)),
    mode TEXT NOT NULL CHECK(mode IN ('copositive', 'strictly_copositive', 'both')),
    parameters TEXT NOT NULL DEFAULT '',
    binary_sha256 TEXT NOT NULL CHECK(
        (model_id = 'hadeler_1983' AND binary_sha256 = '')
        OR (model_id <> 'hadeler_1983' AND length(binary_sha256) = 64 AND binary_sha256 NOT GLOB '*[^0-9a-f]*')
    ),
    status TEXT NOT NULL CHECK(status IN ('ok', 'timeout', 'node_limit', 'error')),
    is_copositive INTEGER CHECK(is_copositive IN (0, 1)),
    is_strictly_copositive INTEGER CHECK(is_strictly_copositive IN (0, 1)),
    elapsed_ns INTEGER CHECK(elapsed_ns IS NULL OR elapsed_ns >= 0),
    timeout_ns INTEGER NOT NULL CHECK(timeout_ns > 0),
    recorded_at TEXT NOT NULL CHECK(length(recorded_at) > 0),
    message TEXT,
    PRIMARY KEY (matrix_id, model_id, mode, parameters, binary_sha256),
    CHECK (
        (status = 'ok' AND elapsed_ns IS NOT NULL AND (
            (mode = 'copositive' AND is_copositive IS NOT NULL AND is_strictly_copositive IS NULL)
            OR (mode = 'strictly_copositive' AND is_copositive IS NULL AND is_strictly_copositive IS NOT NULL)
            OR (mode = 'both' AND is_copositive IS NOT NULL AND is_strictly_copositive IS NOT NULL)
        ))
        OR (status IN ('timeout', 'node_limit', 'error')
            AND is_copositive IS NULL AND is_strictly_copositive IS NULL AND elapsed_ns IS NULL)
    )
) STRICT;

INSERT INTO results (
    matrix_id,
    model_id,
    mode,
    parameters,
    binary_sha256,
    status,
    is_copositive,
    is_strictly_copositive,
    elapsed_ns,
    timeout_ns,
    recorded_at,
    message
)
SELECT
    matrix_id,
    model_id,
    'strictly_copositive',
    parameters,
    binary_sha256,
    status,
    NULL,
    is_strictly_copositive,
    elapsed_ns,
    timeout_ns,
    recorded_at,
    message
FROM strict_only_results;

DROP TABLE strict_only_results;

CREATE TEMP TABLE migrated_result_guard (
    result_count INTEGER CHECK(result_count = 87097),
    non_strict_modes INTEGER CHECK(non_strict_modes = 0),
    invalid_payloads INTEGER CHECK(invalid_payloads = 0)
);
INSERT INTO migrated_result_guard
SELECT
    count(*),
    sum(mode <> 'strictly_copositive'),
    sum(is_copositive IS NOT NULL)
FROM results;

COMMIT;

PRAGMA foreign_key_check;
PRAGMA integrity_check;
