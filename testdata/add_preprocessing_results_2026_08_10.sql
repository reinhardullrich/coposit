PRAGMA foreign_keys = ON;

CREATE TABLE preprocessing_results (
    run_id TEXT NOT NULL CHECK(length(run_id) > 0),
    matrix_id INTEGER NOT NULL REFERENCES matrices(matrix_id) ON DELETE CASCADE,
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
