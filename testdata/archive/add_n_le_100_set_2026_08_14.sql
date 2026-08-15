BEGIN IMMEDIATE;

CREATE TEMP TABLE n_le_100_corpus_guard (
    matrix_count INTEGER CHECK(matrix_count = 3157)
);
INSERT INTO n_le_100_corpus_guard SELECT count(*) FROM matrices;

ALTER TABLE matrices
ADD COLUMN n_le_100 INTEGER GENERATED ALWAYS AS (dimension <= 100) VIRTUAL;

CREATE TEMP TABLE n_le_100_assignment_guard (
    selected_count INTEGER CHECK(selected_count = 2619),
    mismatch_count INTEGER CHECK(mismatch_count = 0)
);
INSERT INTO n_le_100_assignment_guard
SELECT count(*), count(*) FILTER (WHERE n_le_100 <> (dimension <= 100))
FROM matrices
WHERE n_le_100 = 1;

COMMIT;
