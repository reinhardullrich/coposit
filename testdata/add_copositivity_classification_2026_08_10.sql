.bail on

-- Restore the nullable ordinary-copositivity field from the immutable FracESSA
-- source snapshot and the exact literature/generated classifications added later.
PRAGMA foreign_keys = OFF;
BEGIN IMMEDIATE;

CREATE TEMP TABLE old_boundary (matrix_id INTEGER PRIMARY KEY);
INSERT INTO old_boundary VALUES
    (811), (813), (9161), (9218), (9219), (9220), (9221), (9222), (9223), (9224), (9225), (9226), (9227), (9228),
    (9229), (9230), (9231), (9232), (9233), (9234), (9235), (9236), (9237), (9238), (9239), (9240), (9241), (9242),
    (9243), (9574), (9577), (9580), (9583), (9586), (9589), (9592), (9595), (9598), (9601), (9604), (9607), (9610),
    (9613), (9616), (9619), (9622), (9625), (9628), (9631), (9634), (9637), (9640), (9643), (9647), (9650), (9653);

CREATE TEMP TABLE old_not_copositive (matrix_id INTEGER PRIMARY KEY);
INSERT INTO old_not_copositive VALUES
    (9192), (9193), (9194), (9195), (9196), (9197), (9198), (9199), (9200), (9201), (9202), (9203), (9204), (9205),
    (9206), (9207), (9208), (9209), (9210), (9211), (9212), (9213), (9214), (9215), (9216), (9217), (9575), (9578),
    (9581), (9584), (9587), (9590), (9593), (9596), (9599), (9602), (9605), (9608), (9611), (9614), (9617), (9620),
    (9623), (9626), (9629), (9632), (9635), (9638), (9641), (9644), (9645), (9648), (9651), (9654), (9655);

CREATE TABLE matrices_rebuilt (
    matrix_id INTEGER PRIMARY KEY,
    dimension INTEGER NOT NULL CHECK(dimension > 0),
    matrix TEXT NOT NULL CHECK(length(matrix) > 0),
    is_strictly_copositive INTEGER NOT NULL CHECK(is_strictly_copositive IN (0, 1)),
    is_copositive INTEGER CHECK(is_copositive IS NULL OR is_copositive IN (0, 1)),
    source TEXT,
    family TEXT,
    smoke_set INTEGER NOT NULL DEFAULT 0 CHECK(smoke_set IN (0, 1)),
    representative_core INTEGER NOT NULL DEFAULT 0 CHECK(representative_core IN (0, 1)),
    stress_test INTEGER NOT NULL DEFAULT 0 CHECK(stress_test IN (0, 1)),
    scale_set INTEGER NOT NULL DEFAULT 0 CHECK(scale_set IN (0, 1)),
    CHECK(is_strictly_copositive = 0 OR is_copositive IS 1)
) STRICT;

INSERT INTO matrices_rebuilt
SELECT
    m.matrix_id,
    m.dimension,
    m.matrix,
    m.is_strictly_copositive,
    CASE
        WHEN m.is_strictly_copositive = 1 THEN 1
        WHEN EXISTS (SELECT 1 FROM old_not_copositive AS n WHERE n.matrix_id = m.matrix_id)
            OR m.family LIKE '%/ not copositive' THEN 0
        WHEN EXISTS (SELECT 1 FROM old_boundary AS b WHERE b.matrix_id = m.matrix_id)
            OR m.family LIKE '%boundary%'
            OR m.family = 'Väliaho almost-strict equality' THEN 1
        ELSE NULL
    END,
    m.source,
    m.family,
    m.smoke_set,
    m.representative_core,
    m.stress_test,
    m.scale_set
FROM matrices AS m;

CREATE TEMP TABLE classification_guard (
    matrix_count INTEGER CHECK(matrix_count = 2442),
    strict_count INTEGER CHECK(strict_count = 674),
    boundary_count INTEGER CHECK(boundary_count = 764),
    not_copositive_count INTEGER CHECK(not_copositive_count = 115),
    unknown_count INTEGER CHECK(unknown_count = 889),
    unknown_named_family_count INTEGER CHECK(unknown_named_family_count = 0)
);
INSERT INTO classification_guard
SELECT
    count(*),
    sum(is_strictly_copositive = 1),
    sum(is_strictly_copositive = 0 AND is_copositive = 1),
    sum(is_copositive = 0),
    sum(is_copositive IS NULL),
    sum(is_copositive IS NULL AND family IS NOT NULL)
FROM matrices_rebuilt;

DROP TABLE matrices;
ALTER TABLE matrices_rebuilt RENAME TO matrices;
COMMIT;
PRAGMA foreign_keys = ON;

SELECT is_strictly_copositive, is_copositive, count(*)
FROM matrices
GROUP BY is_strictly_copositive, is_copositive
ORDER BY is_strictly_copositive, is_copositive;
