PRAGMA auto_vacuum = FULL;

CREATE TABLE sources (
    source_id INTEGER PRIMARY KEY,
    authors TEXT NOT NULL CHECK(length(authors) > 0),
    title TEXT NOT NULL CHECK(length(title) > 0),
    publication_year INTEGER NOT NULL CHECK(publication_year BETWEEN 1000 AND 9999),
    reference TEXT NOT NULL CHECK(length(reference) > 0),
    comment TEXT
) STRICT;

CREATE TABLE matrices (
    matrix_id INTEGER PRIMARY KEY,
    dimension INTEGER NOT NULL CHECK(dimension > 0),
    matrix TEXT NOT NULL CHECK(length(matrix) > 0),
    file_sha256 TEXT CHECK(
        (matrix NOT LIKE 'file:%' AND file_sha256 IS NULL)
        OR (matrix LIKE 'file:%' AND file_sha256 IS NOT NULL
            AND length(file_sha256) = 64 AND file_sha256 NOT GLOB '*[^0-9a-f]*')
    ),
    is_strictly_copositive INTEGER CHECK(is_strictly_copositive IS NULL OR is_strictly_copositive IN (0, 1)),
    is_copositive INTEGER CHECK(is_copositive IS NULL OR is_copositive IN (0, 1)),
    source TEXT,
    source_id INTEGER REFERENCES sources(source_id),
    family TEXT,
    smoke_set INTEGER NOT NULL DEFAULT 0 CHECK(smoke_set IN (0, 1)),
    representative_core INTEGER NOT NULL DEFAULT 0 CHECK(representative_core IN (0, 1)),
    stress_test INTEGER NOT NULL DEFAULT 0 CHECK(stress_test IN (0, 1)),
    scale_set INTEGER NOT NULL DEFAULT 0 CHECK(scale_set IN (0, 1)),
    timeout_5s_strict_set INTEGER NOT NULL DEFAULT 0 CHECK(timeout_5s_strict_set IN (0, 1)),
    n_le_100 INTEGER GENERATED ALWAYS AS (dimension <= 100) VIRTUAL,
    additional_source_ids TEXT NOT NULL DEFAULT '[]'
        CHECK(json_valid(additional_source_ids) AND json_type(additional_source_ids) = 'array'),
    references_solved TEXT NOT NULL DEFAULT '[]'
        CHECK(json_valid(references_solved) AND json_type(references_solved) = 'array'),
    references_unsolved TEXT NOT NULL DEFAULT '[]'
        CHECK(json_valid(references_unsolved) AND json_type(references_unsolved) = 'array'),
    n_gt_100_solved INTEGER GENERATED ALWAYS AS (
        dimension > 100 AND json_array_length(references_solved) > 0
    ) VIRTUAL,
    fastest_elapsed_ns INTEGER CHECK(fastest_elapsed_ns IS NULL OR fastest_elapsed_ns >= 0),
    fastest_result_ref TEXT CHECK(CASE
        WHEN fastest_elapsed_ns IS NULL THEN fastest_result_ref IS NULL
        WHEN fastest_result_ref IS NULL OR NOT json_valid(fastest_result_ref) THEN 0
        ELSE json_type(fastest_result_ref) = 'object'
            AND json_type(fastest_result_ref, '$.model_id') = 'text'
            AND length(json_extract(fastest_result_ref, '$.model_id')) > 0
            AND json_extract(fastest_result_ref, '$.model_id') = lower(json_extract(fastest_result_ref, '$.model_id'))
            AND json_extract(fastest_result_ref, '$.mode') IN ('copositive', 'strictly_copositive', 'both')
            AND json_extract(fastest_result_ref, '$.preprocessing') IN ('none', 'connected_components', 'pre_checks', 'both')
            AND json_type(fastest_result_ref, '$.binary_sha256') = 'text'
            AND (
                (json_extract(fastest_result_ref, '$.model_id') = 'hadeler_1983'
                 AND json_extract(fastest_result_ref, '$.binary_sha256') = '')
                OR (length(json_extract(fastest_result_ref, '$.binary_sha256')) = 64
                    AND json_extract(fastest_result_ref, '$.binary_sha256') NOT GLOB '*[^0-9a-f]*')
            )
        END),
    CHECK(is_strictly_copositive IS NULL OR is_strictly_copositive = 0 OR is_copositive IS 1)
) STRICT;
