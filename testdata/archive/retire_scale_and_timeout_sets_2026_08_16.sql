-- Remove the retired Scale and Timeout 5s Strict benchmark flags.

BEGIN IMMEDIATE;

CREATE TEMP TABLE retired_set_guard (
    matrix_rows INTEGER NOT NULL CHECK(matrix_rows = 3519),
    scale_rows INTEGER NOT NULL CHECK(scale_rows = 184),
    timeout_rows INTEGER NOT NULL CHECK(timeout_rows = 105)
);
INSERT INTO retired_set_guard
SELECT count(*),
       count(*) FILTER (WHERE scale_set),
       count(*) FILTER (WHERE timeout_5s_strict_set)
FROM matrices;

ALTER TABLE matrices DROP COLUMN scale_set;
ALTER TABLE matrices DROP COLUMN timeout_5s_strict_set;

COMMIT;
