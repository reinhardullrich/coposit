.bail on

-- Freeze matrices on which both pre-checked final algorithms timed out in strict mode after five seconds.
-- Reuse the research-report runs on Core/Stress and the later missing-only runs elsewhere.
BEGIN IMMEDIATE;

CREATE TEMP TABLE selected AS
WITH evidence AS (
    SELECT r.matrix_id, r.model_id
    FROM results AS r
    JOIN matrices AS m USING (matrix_id)
    WHERE (m.representative_core = 1 OR m.stress_test = 1)
      AND r.mode = 'strictly_copositive'
      AND r.preprocessing = 'both'
      AND r.timeout_ns = 5000000000
      AND r.status = 'timeout'
      AND (
          (r.model_id = 'dickinson_2019'
           AND r.binary_sha256 = 'e34d25851717f3ce018cfefb605aff189511e715992eb5642958cb68b916b726')
          OR
          (r.model_id = 'adaptive_sponsel_copomatrix'
           AND r.binary_sha256 = '249d413159ae27519472c474d68eb697a57af1ceb06b2129ab69d7f796856977')
      )

    UNION ALL

    SELECT r.matrix_id, r.model_id
    FROM results AS r
    JOIN matrices AS m USING (matrix_id)
    WHERE m.representative_core = 0 AND m.stress_test = 0
      AND r.mode = 'strictly_copositive'
      AND r.preprocessing = 'both'
      AND r.timeout_ns = 5000000000
      AND r.status = 'timeout'
      AND (
          (r.model_id = 'dickinson_final'
           AND r.binary_sha256 = 'ebaa7d4f1d25d3fd07101c3842950e2bb03c0f547c645560f980adbbeb06077d')
          OR
          (r.model_id = 'adaptive_sponsel_copomatrix'
           AND r.binary_sha256 = '30d99480338c1f47ce1b458a4ce066dea647adedc4715efe37065d76efe7b203')
      )
)
SELECT matrix_id
FROM evidence
GROUP BY matrix_id
HAVING count(DISTINCT model_id) = 2;

CREATE TEMP TABLE selection_guard (
    matrix_count INTEGER CHECK(matrix_count = 129),
    matrix_id_sum INTEGER CHECK(matrix_id_sum = 1289068),
    matrix_id_square_sum INTEGER CHECK(matrix_id_square_sum = 12907746992)
);

INSERT INTO selection_guard
SELECT count(*), sum(matrix_id), sum(matrix_id * matrix_id)
FROM selected;

UPDATE matrices SET timeout_5s_strict_set = 0;
UPDATE matrices
SET timeout_5s_strict_set = 1
WHERE matrix_id IN (SELECT matrix_id FROM selected);

COMMIT;
