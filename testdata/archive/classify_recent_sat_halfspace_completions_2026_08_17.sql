-- Fill previously unknown corpus truth from the exact combined classifications
-- completed by sat_halfspace_dickinson on 2026-08-17. Matrix 12580 already
-- carried the same truth and is deliberately not rewritten here.

BEGIN IMMEDIATE;

CREATE TEMP TABLE sat_halfspace_truth (
    matrix_id INTEGER PRIMARY KEY,
    is_copositive INTEGER NOT NULL CHECK(is_copositive IN (0, 1)),
    is_strictly_copositive INTEGER NOT NULL CHECK(is_strictly_copositive IN (0, 1))
) WITHOUT ROWID;

INSERT INTO sat_halfspace_truth VALUES
    (12090, 1, 1),
    (12091, 1, 1),
    (12624, 0, 0),
    (12626, 0, 0),
    (12627, 0, 0),
    (12628, 1, 1),
    (12629, 0, 0),
    (12630, 0, 0),
    (12631, 1, 1),
    (12632, 0, 0),
    (12633, 0, 0),
    (12634, 1, 1),
    (12635, 0, 0),
    (12636, 0, 0),
    (12637, 1, 1),
    (12638, 1, 1),
    (12639, 1, 1),
    (12640, 1, 1),
    (12641, 1, 1),
    (12642, 0, 0),
    (12643, 1, 1),
    (12644, 1, 1),
    (12645, 1, 1),
    (12646, 0, 0),
    (12647, 0, 0),
    (12648, 1, 1),
    (12753, 0, 0),
    (12774, 0, 0),
    (12776, 0, 0),
    (12777, 0, 0),
    (12778, 0, 0),
    (12780, 0, 0),
    (12784, 0, 0),
    (12785, 0, 0),
    (12786, 0, 0),
    (12787, 0, 0),
    (12791, 0, 0),
    (12792, 0, 0),
    (12793, 0, 0),
    (12794, 0, 0),
    (12798, 0, 0),
    (12803, 0, 0),
    (12804, 0, 0),
    (12805, 0, 0),
    (12806, 0, 0),
    (12807, 0, 0),
    (12809, 0, 0),
    (12811, 0, 0),
    (12812, 0, 0),
    (12814, 0, 0),
    (12817, 0, 0),
    (12818, 0, 0),
    (12819, 0, 0);

CREATE TEMP TABLE sat_halfspace_truth_guard (
    evidence_count INTEGER NOT NULL CHECK(evidence_count = 53),
    missing_count INTEGER NOT NULL CHECK(missing_count = 0),
    nonnull_count INTEGER NOT NULL CHECK(nonnull_count = 0)
);

INSERT INTO sat_halfspace_truth_guard
SELECT (SELECT count(*) FROM sat_halfspace_truth),
       count(*) FILTER (WHERE m.matrix_id IS NULL),
       count(*) FILTER (WHERE m.is_copositive IS NOT NULL OR m.is_strictly_copositive IS NOT NULL)
FROM sat_halfspace_truth AS e
LEFT JOIN matrices AS m USING(matrix_id);

UPDATE matrices
SET is_copositive = (SELECT e.is_copositive FROM sat_halfspace_truth AS e WHERE e.matrix_id = matrices.matrix_id),
    is_strictly_copositive = (
        SELECT e.is_strictly_copositive FROM sat_halfspace_truth AS e WHERE e.matrix_id = matrices.matrix_id
    )
WHERE matrix_id IN (SELECT matrix_id FROM sat_halfspace_truth);

CREATE TEMP TABLE sat_halfspace_truth_final_guard (
    matching_count INTEGER NOT NULL CHECK(matching_count = 53),
    conflict_count INTEGER NOT NULL CHECK(conflict_count = 0)
);

INSERT INTO sat_halfspace_truth_final_guard
SELECT count(*) FILTER (
           WHERE m.is_copositive IS e.is_copositive
             AND m.is_strictly_copositive IS e.is_strictly_copositive
       ),
       count(*) FILTER (
           WHERE m.is_copositive IS NOT e.is_copositive
              OR m.is_strictly_copositive IS NOT e.is_strictly_copositive
       )
FROM sat_halfspace_truth AS e
JOIN matrices AS m USING(matrix_id);

COMMIT;
