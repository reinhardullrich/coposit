.bail on

-- Recompute the four benchmark flags after the four columns in schema.sql exist.
-- The dated guards freeze this assignment to the 2026-08-10 corpus and canonical baseline rows.
BEGIN IMMEDIATE;

CREATE TEMP TABLE canonical_baseline (
    model_id TEXT NOT NULL,
    binary_sha256 TEXT NOT NULL,
    PRIMARY KEY (model_id, binary_sha256)
) WITHOUT ROWID;

INSERT INTO canonical_baseline VALUES
    ('dutour_2018', '1b8263b9d3b68b7c6919ce8a508d64cb801692977dbfe5aa45f39ccde7d95b67'),
    ('danninger_1990', 'bc67b28681faea6c5c677e1472fd429a838a5cc34f38ed1a3ee05276a6eac312'),
    ('hadeler_1983', ''),
    ('bundfuss_2008', '475a228ac7a4aca81380c8b2ef169cf29c74a603f3db0e1d031343de7922013f'),
    ('copomatrix_2011', '7b5fa023c9ff053eaf2fd03cadafae3da28121d90d179f70ea893a800b88ee47'),
    ('sponsel_2012', 'e047dec119eef94666b51646dbf434aedd6507bc97dbff340717067bfd7a1416'),
    ('dickinson_2019', 'bd3beac5785dd1ef5c2936d8a32eea73a5a9912d3e5cfa6ede7929df1050e5db'),
    ('safi_2021', '0bd6d3d0430c0d3d01f39659073eeabc1e21ae885c9b788d5ee201c07fd9b3d1');

CREATE TEMP TABLE baseline_score AS
SELECT
    r.matrix_id,
    count(*) AS result_count,
    sum(r.status <> 'ok') AS unresolved_count,
    sum(CASE WHEN r.status = 'ok' THEN r.elapsed_ns ELSE r.timeout_ns END) AS work_ns,
    max(CASE WHEN r.status = 'ok' THEN r.elapsed_ns ELSE r.timeout_ns END) AS max_ns
FROM results AS r
JOIN canonical_baseline AS c USING (model_id, binary_sha256)
WHERE r.parameters = ''
GROUP BY r.matrix_id;

CREATE TEMP TABLE raw_annotated AS
SELECT
    m.matrix_id,
    m.dimension,
    m.is_strictly_copositive,
    m.is_copositive,
    m.family,
    coalesce(m.family, 'legacy corpus') AS family_group,
    CASE
        WHEN m.dimension <= 3 THEN '01-03'
        WHEN m.dimension <= 7 THEN '04-07'
        WHEN m.dimension <= 12 THEN '08-12'
        WHEN m.dimension <= 20 THEN '13-20'
        WHEN m.dimension <= 50 THEN '21-50'
        WHEN m.dimension <= 100 THEN '51-100'
        ELSE '101+'
    END AS dimension_band,
    coalesce(s.result_count, 0) AS result_count,
    coalesce(s.unresolved_count, 0) AS unresolved_count,
    coalesce(s.work_ns, 0) AS work_ns,
    coalesce(s.max_ns, 0) AS max_ns,
    ((m.matrix_id * 1103515245 + 12345) % 2147483647) AS stable_order
FROM matrices AS m
LEFT JOIN baseline_score AS s USING (matrix_id);

CREATE TEMP TABLE annotated AS
SELECT
    r.*,
    CASE
        WHEN is_strictly_copositive = 1 THEN 'strict'
        WHEN is_copositive = 1 THEN 'boundary'
        WHEN is_copositive = 0 THEN 'not-copositive'
        ELSE 'ordinary-unknown'
    END AS class
FROM raw_annotated AS r;

-- Smoke: 49 fast rows, balanced across four small-order bands and exact/unknown mathematical classes.
-- The ten anchors retain direct diagonal/off-diagonal checks, arbitrary-precision input, Horn,
-- Hoffman-Pereira, and a strict perfect-copositive example.
CREATE TEMP TABLE smoke_anchor (matrix_id INTEGER PRIMARY KEY);
INSERT INTO smoke_anchor VALUES (1), (2), (4), (5), (17), (47), (9109), (9162), (9163), (9256);

CREATE TEMP TABLE smoke_quota (
    dimension_band TEXT NOT NULL,
    class TEXT NOT NULL,
    quota INTEGER NOT NULL,
    PRIMARY KEY (dimension_band, class)
) WITHOUT ROWID;

INSERT INTO smoke_quota VALUES
    ('01-03', 'ordinary-unknown', 6), ('01-03', 'strict', 6),
    ('04-07', 'boundary', 3), ('04-07', 'ordinary-unknown', 3), ('04-07', 'strict', 6),
    ('08-12', 'boundary', 3), ('08-12', 'ordinary-unknown', 3), ('08-12', 'strict', 6),
    ('13-20', 'boundary', 2), ('13-20', 'ordinary-unknown', 2), ('13-20', 'not-copositive', 4), ('13-20', 'strict', 5);

CREATE TEMP TABLE smoke_selected (matrix_id INTEGER PRIMARY KEY);
INSERT INTO smoke_selected
WITH candidates AS (
    SELECT
        a.*,
        EXISTS (SELECT 1 FROM smoke_anchor AS x WHERE x.matrix_id = a.matrix_id) AS anchor,
        row_number() OVER (
            PARTITION BY a.dimension_band, a.class, a.family_group
            ORDER BY a.stable_order
        ) AS family_rank
    FROM annotated AS a
    JOIN smoke_quota AS q USING (dimension_band, class)
    WHERE a.result_count = 8 AND a.unresolved_count = 0 AND a.max_ns <= 50000000
), ranked AS (
    SELECT
        c.matrix_id,
        q.quota,
        row_number() OVER (
            PARTITION BY c.dimension_band, c.class
            ORDER BY c.anchor DESC, c.family_rank, c.stable_order
        ) AS selection_rank
    FROM candidates AS c
    JOIN smoke_quota AS q USING (dimension_band, class)
)
SELECT matrix_id FROM ranked WHERE selection_rank <= quota;

-- Representative core: 384 rows through order 100. Quotas balance order and known class;
-- round-robin family ranks stop large families from dominating. Every smoke row is selected first.
CREATE TEMP TABLE core_quota (
    dimension_band TEXT NOT NULL,
    class TEXT NOT NULL,
    quota INTEGER NOT NULL,
    PRIMARY KEY (dimension_band, class)
) WITHOUT ROWID;

INSERT INTO core_quota VALUES
    ('01-03', 'ordinary-unknown', 12), ('01-03', 'strict', 12),
    ('04-07', 'boundary', 18), ('04-07', 'ordinary-unknown', 18), ('04-07', 'strict', 36),
    ('08-12', 'boundary', 18), ('08-12', 'ordinary-unknown', 18), ('08-12', 'strict', 36),
    ('13-20', 'boundary', 8), ('13-20', 'ordinary-unknown', 7), ('13-20', 'not-copositive', 21), ('13-20', 'strict', 36),
    ('21-50', 'boundary', 15), ('21-50', 'ordinary-unknown', 14), ('21-50', 'not-copositive', 7), ('21-50', 'strict', 36),
    ('51-100', 'boundary', 25), ('51-100', 'ordinary-unknown', 6), ('51-100', 'not-copositive', 5), ('51-100', 'strict', 36);

CREATE TEMP TABLE core_selected (matrix_id INTEGER PRIMARY KEY);
INSERT INTO core_selected
WITH candidates AS (
    SELECT
        a.*,
        EXISTS (SELECT 1 FROM smoke_selected AS s WHERE s.matrix_id = a.matrix_id) AS smoke,
        row_number() OVER (
            PARTITION BY a.dimension_band, a.class, a.family_group
            ORDER BY a.stable_order
        ) AS family_rank
    FROM annotated AS a
    JOIN core_quota AS q USING (dimension_band, class)
    WHERE a.dimension <= 100
), ranked AS (
    SELECT
        c.matrix_id,
        q.quota,
        row_number() OVER (
            PARTITION BY c.dimension_band, c.class
            ORDER BY c.smoke DESC, c.family_rank, c.stable_order
        ) AS selection_rank
    FROM candidates AS c
    JOIN core_quota AS q USING (dimension_band, class)
)
SELECT matrix_id FROM ranked WHERE selection_rank <= quota;

-- Stress: all rows unresolved by all eight canonical baselines, all historical bad-26 rows,
-- two hard anchors per scored literature/generated family, and maximum-order family/outcome anchors.
-- Difficulty-ranked filler reaches 240 while capping non-mandatory rows from one family at twelve.
CREATE TEMP TABLE bad26 AS
SELECT DISTINCT matrix_id
FROM results
WHERE parameters IN ('bad26_original_cone_30s_2026-08-09', 'bad26_strict_zero_5s_2026-08-09');

CREATE TEMP TABLE family_hard AS
WITH ranked AS (
    SELECT
        a.matrix_id,
        row_number() OVER (
            PARTITION BY a.family
            ORDER BY a.unresolved_count DESC, a.work_ns DESC, a.stable_order
        ) AS family_rank
    FROM annotated AS a
    WHERE a.family IS NOT NULL AND a.result_count = 8
)
SELECT matrix_id FROM ranked WHERE family_rank <= 2;

CREATE TEMP TABLE high_order_anchor AS
WITH ranked AS (
    SELECT
        a.matrix_id,
        row_number() OVER (
            PARTITION BY a.family, a.is_strictly_copositive
            ORDER BY a.dimension DESC, a.stable_order
        ) AS family_rank
    FROM annotated AS a
    WHERE a.family IS NOT NULL AND a.dimension > 100
)
SELECT matrix_id FROM ranked WHERE family_rank = 1;

CREATE TEMP TABLE stress_mandatory (matrix_id INTEGER PRIMARY KEY);
INSERT OR IGNORE INTO stress_mandatory SELECT matrix_id FROM annotated WHERE result_count = 8 AND unresolved_count = 8;
INSERT OR IGNORE INTO stress_mandatory SELECT matrix_id FROM bad26;
INSERT OR IGNORE INTO stress_mandatory SELECT matrix_id FROM family_hard;
INSERT OR IGNORE INTO stress_mandatory SELECT matrix_id FROM high_order_anchor;

CREATE TEMP TABLE stress_selected (matrix_id INTEGER PRIMARY KEY);
INSERT INTO stress_selected SELECT matrix_id FROM stress_mandatory;
INSERT OR IGNORE INTO stress_selected
WITH ranked_family AS (
    SELECT
        a.*,
        row_number() OVER (
            PARTITION BY a.family_group
            ORDER BY a.unresolved_count DESC, a.work_ns DESC, a.stable_order
        ) AS family_rank
    FROM annotated AS a
    WHERE a.result_count = 8
), filler AS (
    SELECT
        r.matrix_id,
        row_number() OVER (
            ORDER BY r.unresolved_count DESC, r.work_ns DESC, r.stable_order
        ) AS fill_rank
    FROM ranked_family AS r
    WHERE r.family_rank <= 12
      AND NOT EXISTS (SELECT 1 FROM stress_mandatory AS m WHERE m.matrix_id = r.matrix_id)
)
SELECT matrix_id FROM filler WHERE fill_rank <= 240 - (SELECT count(*) FROM stress_mandatory);

-- Scale: every matrix above order 100, plus all 180 controlled high-order generated rows
-- (including their six order-51 members).
CREATE TEMP TABLE scale_selected AS
SELECT matrix_id FROM annotated WHERE dimension > 100 OR family LIKE 'generated %';

-- Fail rather than silently changing a dated benchmark assignment when its source snapshot changes.
CREATE TEMP TABLE corpus_guard (
    matrix_count INTEGER CHECK (matrix_count = 2442),
    max_matrix_id INTEGER CHECK (max_matrix_id = 10684),
    scored_count INTEGER CHECK (scored_count = 2078)
);
INSERT INTO corpus_guard
SELECT count(*), max(matrix_id), (SELECT count(*) FROM baseline_score WHERE result_count = 8)
FROM matrices;

CREATE TEMP TABLE selection_guard (
    smoke_count INTEGER CHECK (smoke_count = 49),
    core_count INTEGER CHECK (core_count = 384),
    stress_count INTEGER CHECK (stress_count = 240),
    scale_count INTEGER CHECK (scale_count = 364),
    smoke_id_sum INTEGER CHECK (smoke_id_sum = 322236),
    core_id_sum INTEGER CHECK (core_id_sum = 3313707),
    stress_id_sum INTEGER CHECK (stress_id_sum = 2399162),
    scale_id_sum INTEGER CHECK (scale_id_sum = 3743441),
    smoke_id_square_sum INTEGER CHECK (smoke_id_square_sum = 2907588178),
    core_id_square_sum INTEGER CHECK (core_id_square_sum = 31708483335),
    stress_id_square_sum INTEGER CHECK (stress_id_square_sum = 24040116150),
    scale_id_square_sum INTEGER CHECK (scale_id_square_sum = 38548418567),
    smoke_outside_core INTEGER CHECK (smoke_outside_core = 0),
    bad26_outside_stress INTEGER CHECK (bad26_outside_stress = 0)
);
INSERT INTO selection_guard
SELECT
    (SELECT count(*) FROM smoke_selected),
    (SELECT count(*) FROM core_selected),
    (SELECT count(*) FROM stress_selected),
    (SELECT count(*) FROM scale_selected),
    (SELECT sum(matrix_id) FROM smoke_selected),
    (SELECT sum(matrix_id) FROM core_selected),
    (SELECT sum(matrix_id) FROM stress_selected),
    (SELECT sum(matrix_id) FROM scale_selected),
    (SELECT sum(matrix_id * matrix_id) FROM smoke_selected),
    (SELECT sum(matrix_id * matrix_id) FROM core_selected),
    (SELECT sum(matrix_id * matrix_id) FROM stress_selected),
    (SELECT sum(matrix_id * matrix_id) FROM scale_selected),
    (SELECT count(*) FROM smoke_selected AS s WHERE NOT EXISTS (
        SELECT 1 FROM core_selected AS c WHERE c.matrix_id = s.matrix_id
    )),
    (SELECT count(*) FROM bad26 AS b WHERE NOT EXISTS (
        SELECT 1 FROM stress_selected AS s WHERE s.matrix_id = b.matrix_id
    ));

UPDATE matrices
SET smoke_set = EXISTS (SELECT 1 FROM smoke_selected AS s WHERE s.matrix_id = matrices.matrix_id)
WHERE smoke_set <> EXISTS (SELECT 1 FROM smoke_selected AS s WHERE s.matrix_id = matrices.matrix_id);

UPDATE matrices
SET representative_core = EXISTS (SELECT 1 FROM core_selected AS s WHERE s.matrix_id = matrices.matrix_id)
WHERE representative_core <> EXISTS (SELECT 1 FROM core_selected AS s WHERE s.matrix_id = matrices.matrix_id);

UPDATE matrices
SET stress_test = EXISTS (SELECT 1 FROM stress_selected AS s WHERE s.matrix_id = matrices.matrix_id)
WHERE stress_test <> EXISTS (SELECT 1 FROM stress_selected AS s WHERE s.matrix_id = matrices.matrix_id);

UPDATE matrices
SET scale_set = EXISTS (SELECT 1 FROM scale_selected AS s WHERE s.matrix_id = matrices.matrix_id)
WHERE scale_set <> EXISTS (SELECT 1 FROM scale_selected AS s WHERE s.matrix_id = matrices.matrix_id);

COMMIT;

SELECT
    sum(smoke_set) AS smoke,
    sum(representative_core) AS representative_core,
    sum(stress_test) AS stress,
    sum(scale_set) AS scale
FROM matrices;
