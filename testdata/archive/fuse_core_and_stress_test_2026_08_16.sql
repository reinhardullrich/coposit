-- Replace the separate Representative Core and Stress flags with their union,
-- excluding the controlled sparse/dense matrices that belong only to Scale.

BEGIN IMMEDIATE;

ALTER TABLE matrices
ADD COLUMN core_and_stress_test INTEGER NOT NULL DEFAULT 0 CHECK(core_and_stress_test IN (0, 1));

UPDATE matrices
SET core_and_stress_test = (representative_core OR stress_test)
                           AND (source_id IS NULL OR source_id NOT IN (93, 94));

CREATE TEMP TABLE core_and_stress_guard (
    rows INTEGER NOT NULL CHECK(rows = 512),
    generated_rows INTEGER NOT NULL CHECK(generated_rows = 0)
);
INSERT INTO core_and_stress_guard
SELECT count(*) FILTER (WHERE core_and_stress_test),
       count(*) FILTER (WHERE source_id IN (93, 94) AND core_and_stress_test)
FROM matrices;

ALTER TABLE matrices DROP COLUMN representative_core;
ALTER TABLE matrices DROP COLUMN stress_test;

COMMIT;
