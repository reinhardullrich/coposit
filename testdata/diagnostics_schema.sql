PRAGMA auto_vacuum = FULL;

CREATE TABLE results (
    matrix_id INTEGER NOT NULL CHECK(matrix_id > 0),
    model_id TEXT NOT NULL CHECK(length(model_id) > 0 AND model_id = lower(model_id)),
    mode TEXT NOT NULL CHECK(mode IN ('copositive', 'strictly_copositive', 'both')),
    preprocessing TEXT NOT NULL CHECK(preprocessing IN ('none', 'connected_components', 'pre_checks', 'both')),
    binary_sha256 TEXT NOT NULL CHECK(
        (model_id = 'hadeler_1983' AND binary_sha256 = '')
        OR (length(binary_sha256) = 64 AND binary_sha256 NOT GLOB '*[^0-9a-f]*')
    ),
    status TEXT NOT NULL CHECK(status IN ('running', 'ok', 'parse_error', 'timeout', 'node_limit', 'error')),
    is_copositive INTEGER CHECK(is_copositive IN (0, 1)),
    is_strictly_copositive INTEGER CHECK(is_strictly_copositive IN (0, 1)),
    elapsed_ns INTEGER CHECK(elapsed_ns IS NULL OR elapsed_ns >= 0),
    timeout_ns INTEGER NOT NULL CHECK(timeout_ns > 0),
    recorded_at TEXT NOT NULL CHECK(length(recorded_at) > 0),
    diagnostics TEXT,
    certificate_joint_distribution TEXT CHECK(
        certificate_joint_distribution IS NULL
        OR (json_valid(certificate_joint_distribution) AND json_type(certificate_joint_distribution) = 'array')
    ),
    message TEXT,
    PRIMARY KEY (matrix_id, model_id, mode, preprocessing, binary_sha256),
    CHECK (
        (status = 'ok' AND elapsed_ns IS NOT NULL AND (
            (mode = 'copositive' AND is_copositive IS NOT NULL AND is_strictly_copositive IS NULL)
            OR (mode = 'strictly_copositive' AND is_copositive IS NULL AND is_strictly_copositive IS NOT NULL)
            OR (mode = 'both' AND is_copositive IS NOT NULL AND is_strictly_copositive IS NOT NULL)
        ))
        OR (status = 'running' AND elapsed_ns IS NOT NULL
            AND is_copositive IS NULL AND is_strictly_copositive IS NULL)
        OR (status IN ('parse_error', 'timeout', 'node_limit', 'error')
            AND is_copositive IS NULL AND is_strictly_copositive IS NULL)
    ),
    CHECK(status <> 'ok' OR mode <> 'both' OR is_strictly_copositive = 0 OR is_copositive = 1)
) STRICT;

CREATE TABLE preprocessing_results (
    run_id TEXT NOT NULL CHECK(length(run_id) > 0),
    matrix_id INTEGER NOT NULL CHECK(matrix_id > 0),
    mode TEXT NOT NULL CHECK(mode IN ('copositive', 'strictly_copositive')),
    preprocessing TEXT NOT NULL CHECK(preprocessing IN ('connected_components', 'pre_checks', 'both')),
    status TEXT NOT NULL CHECK(status IN ('ok', 'timeout', 'hard_timeout', 'error')),
    elapsed_ns INTEGER CHECK(elapsed_ns IS NULL OR elapsed_ns >= 0),
    delegate_calls INTEGER CHECK(delegate_calls IS NULL OR delegate_calls >= 0),
    outcome TEXT CHECK(outcome IS NULL OR outcome IN ('positive', 'negative', 'unresolved')),
    message TEXT,
    PRIMARY KEY (run_id, matrix_id, mode, preprocessing),
    CHECK (
        (status = 'ok' AND elapsed_ns IS NOT NULL AND delegate_calls IS NOT NULL AND outcome IS NOT NULL)
        OR (status <> 'ok' AND outcome IS NULL)
    )
) STRICT;
