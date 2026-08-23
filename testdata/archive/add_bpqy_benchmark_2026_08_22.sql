-- Applied from the repository root with:
-- sqlite3 testdata/copos_testdata.sqlite3 < testdata/archive/add_bpqy_benchmark_2026_08_22.sql

.bail on
PRAGMA foreign_keys = ON;
ATTACH DATABASE 'experiments/diagnostics.sqlite3' AS diagnostics;
BEGIN IMMEDIATE;

CREATE TEMP TABLE bpqy_completed_truth AS
SELECT r.matrix_id,
       min(r.is_copositive) AS min_copositive,
       max(r.is_copositive) AS max_copositive,
       min(r.is_strictly_copositive) AS min_strict,
       max(r.is_strictly_copositive) AS max_strict
FROM diagnostics.results AS r
JOIN matrices AS m USING(matrix_id)
WHERE m.source_id = 51
  AND m.family GLOB 'BPQY COP *'
  AND m.is_copositive IS NULL
  AND m.is_strictly_copositive IS NULL
  AND r.status = 'ok'
  AND r.mode = 'both'
  AND r.is_copositive IS NOT NULL
  AND r.is_strictly_copositive IS NOT NULL
GROUP BY r.matrix_id;

CREATE TEMP TABLE bpqy_truth_guard (
    candidate_rows INTEGER NOT NULL CHECK(candidate_rows = 57),
    conflict_rows INTEGER NOT NULL CHECK(conflict_rows = 0),
    strict_rows INTEGER NOT NULL CHECK(strict_rows = 57)
);
INSERT INTO bpqy_truth_guard
SELECT count(*),
       count(*) FILTER (WHERE min_copositive <> max_copositive OR min_strict <> max_strict),
       count(*) FILTER (WHERE min_copositive = 1 AND min_strict = 1)
FROM bpqy_completed_truth;

UPDATE matrices AS m
SET is_copositive=(SELECT min_copositive FROM bpqy_completed_truth AS t WHERE t.matrix_id=m.matrix_id),
    is_strictly_copositive=(SELECT min_strict FROM bpqy_completed_truth AS t WHERE t.matrix_id=m.matrix_id)
WHERE m.matrix_id IN (SELECT matrix_id FROM bpqy_completed_truth);

ALTER TABLE matrices
ADD COLUMN bpqy_benchmark INTEGER GENERATED ALWAYS AS (
    CASE
        WHEN source_id = 51
         AND family GLOB 'BPQY COP *'
         AND (
             is_copositive = 1 AND is_strictly_copositive = 1
             OR is_copositive IS NULL AND is_strictly_copositive IS NULL
         )
        THEN 1
        ELSE 0
    END
) VIRTUAL;

CREATE TEMP TABLE bpqy_benchmark_final_guard (
    bpqy_cop_rows INTEGER NOT NULL CHECK(bpqy_cop_rows = 825),
    strict_rows INTEGER NOT NULL CHECK(strict_rows = 307),
    unknown_rows INTEGER NOT NULL CHECK(unknown_rows = 106),
    excluded_rows INTEGER NOT NULL CHECK(excluded_rows = 412),
    flagged_rows INTEGER NOT NULL CHECK(flagged_rows = 413),
    effective_rows INTEGER NOT NULL CHECK(effective_rows = 404),
    cache_mismatches INTEGER NOT NULL CHECK(cache_mismatches = 0),
    integrity_result TEXT NOT NULL CHECK(integrity_result = 'ok'),
    foreign_key_errors INTEGER NOT NULL CHECK(foreign_key_errors = 0)
);
INSERT INTO bpqy_benchmark_final_guard
SELECT count(*),
       count(*) FILTER (WHERE is_copositive = 1 AND is_strictly_copositive = 1),
       count(*) FILTER (WHERE is_copositive IS NULL AND is_strictly_copositive IS NULL),
       count(*) FILTER (WHERE NOT bpqy_benchmark),
       count(*) FILTER (WHERE bpqy_benchmark),
       count(*) FILTER (WHERE bpqy_benchmark AND NOT preprocessing_solved),
       count(*) FILTER (
           WHERE bpqy_benchmark AND fastest_elapsed_ns IS NOT NULL AND NOT EXISTS (
               SELECT 1
               FROM diagnostics.results AS r
               WHERE r.matrix_id=m.matrix_id
                 AND r.status='ok'
                 AND r.mode='both'
                 AND r.elapsed_ns=m.fastest_elapsed_ns
                 AND r.model_id=json_extract(m.fastest_result_ref,'$.model_id')
                 AND r.preprocessing=json_extract(m.fastest_result_ref,'$.preprocessing')
                 AND r.binary_sha256=json_extract(m.fastest_result_ref,'$.binary_sha256')
                 AND r.is_copositive=m.is_copositive
                 AND r.is_strictly_copositive=m.is_strictly_copositive
           )
       ),
       (SELECT integrity_check FROM pragma_integrity_check LIMIT 1),
       (SELECT count(*) FROM pragma_foreign_key_check)
FROM matrices AS m
WHERE source_id = 51 AND family GLOB 'BPQY COP *';

COMMIT;
DETACH DATABASE diagnostics;
