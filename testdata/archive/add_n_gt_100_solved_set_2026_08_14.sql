BEGIN IMMEDIATE;

CREATE TEMP TABLE n_gt_100_solved_corpus_guard (
    matrix_count INTEGER CHECK(matrix_count = 3157)
);
INSERT INTO n_gt_100_solved_corpus_guard SELECT count(*) FROM matrices;

ALTER TABLE matrices
ADD COLUMN n_gt_100_solved INTEGER GENERATED ALWAYS AS (
    dimension > 100 AND json_array_length(references_solved) > 0
) VIRTUAL;

CREATE TEMP TABLE n_gt_100_solved_assignment_guard (
    selected_count INTEGER CHECK(selected_count = 58),
    mismatch_count INTEGER CHECK(mismatch_count = 0)
);
INSERT INTO n_gt_100_solved_assignment_guard
SELECT count(*), count(*) FILTER (
    WHERE n_gt_100_solved <> (dimension > 100 AND json_array_length(references_solved) > 0)
)
FROM matrices
WHERE n_gt_100_solved = 1;

COMMIT;
