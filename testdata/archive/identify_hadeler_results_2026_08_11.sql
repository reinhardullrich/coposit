.bail on

PRAGMA foreign_keys = OFF;
BEGIN IMMEDIATE;

CREATE TEMP TABLE hadeler_migration_guard (
    row_count INTEGER NOT NULL CHECK(row_count = 2096)
);

INSERT INTO hadeler_migration_guard
SELECT COUNT(*)
FROM results AS r
JOIN matrices AS m USING(matrix_id)
WHERE r.model_id = 'hadeler_1983'
  AND r.binary_sha256 = ''
  AND r.recorded_at BETWEEN '2026-08-11T03:46:49+00:00' AND '2026-08-11T04:07:30+00:00'
  AND (m.representative_core = 1 OR m.stress_test = 1)
  AND r.mode IN ('copositive', 'strictly_copositive')
  AND r.parameters IN ('', 'preprocessing=both');

ALTER TABLE results RENAME TO results_before_hadeler_hashes;

CREATE TABLE results (
    matrix_id INTEGER NOT NULL REFERENCES matrices(matrix_id) ON DELETE CASCADE,
    model_id TEXT NOT NULL CHECK(length(model_id) > 0 AND model_id = lower(model_id)),
    mode TEXT NOT NULL CHECK(mode IN ('copositive', 'strictly_copositive', 'both')),
    parameters TEXT NOT NULL DEFAULT '',
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

INSERT INTO results
SELECT * FROM results_before_hadeler_hashes;

UPDATE results
SET binary_sha256 = '1ecd99e49df73f3955c8a2684f84c5db7e7f3420d785c452bbbf7ed5a16b2c24'
WHERE rowid IN (
    SELECT r.rowid
    FROM results AS r
    JOIN matrices AS m USING(matrix_id)
    WHERE r.model_id = 'hadeler_1983'
      AND r.binary_sha256 = ''
      AND r.recorded_at BETWEEN '2026-08-11T03:46:49+00:00' AND '2026-08-11T04:07:30+00:00'
      AND (m.representative_core = 1 OR m.stress_test = 1)
      AND r.mode IN ('copositive', 'strictly_copositive')
      AND r.parameters IN ('', 'preprocessing=both')
);

DROP TABLE results_before_hadeler_hashes;
DROP TABLE hadeler_migration_guard;
COMMIT;
PRAGMA foreign_keys = ON;
