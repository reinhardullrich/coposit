-- Refill the curated Smoke and Core sets after the current depth-2
-- preprocessing refresh. N <= 100 remains generated and comprehensive, so
-- its membership naturally falls to the 754 matrices that reach a model.

BEGIN IMMEDIATE;

CREATE TEMP TABLE benchmark_refresh_initial_guard (
    matrix_count INTEGER NOT NULL CHECK(matrix_count = 3513),
    preprocessing_solved_count INTEGER NOT NULL CHECK(preprocessing_solved_count = 2708),
    smoke_count INTEGER NOT NULL CHECK(smoke_count = 49),
    core_count INTEGER NOT NULL CHECK(core_count = 512),
    smoke_preprocessed INTEGER NOT NULL CHECK(smoke_preprocessed = 21),
    core_preprocessed INTEGER NOT NULL CHECK(core_preprocessed = 226),
    n_le_100_count INTEGER NOT NULL CHECK(n_le_100_count = 754),
    n_gt_100_solved_count INTEGER NOT NULL CHECK(n_gt_100_solved_count = 0)
);
INSERT INTO benchmark_refresh_initial_guard
SELECT count(*),
       count(*) FILTER (WHERE preprocessing_solved),
       count(*) FILTER (WHERE smoke_set),
       count(*) FILTER (WHERE core_and_stress_test),
       count(*) FILTER (WHERE smoke_set AND preprocessing_solved),
       count(*) FILTER (WHERE core_and_stress_test AND preprocessing_solved),
       count(*) FILTER (WHERE n_le_100),
       count(*) FILTER (WHERE n_gt_100_solved)
FROM matrices;

CREATE TEMP TABLE smoke_replacements(matrix_id INTEGER PRIMARY KEY) WITHOUT ROWID;
INSERT INTO smoke_replacements VALUES
    (9471), (9491), (9674), (9678), (9681), (9716), (9717),
    (9727), (9728), (9729), (9730), (9755), (9893), (9928),
    (9959), (9971), (10245), (10266), (10271), (10272), (10274);

CREATE TEMP TABLE core_only_replacements(matrix_id INTEGER PRIMARY KEY) WITHOUT ROWID;
INSERT INTO core_only_replacements VALUES
    (8069), (9289), (9290), (9291), (9292), (9293), (9295), (9296),
    (9297), (9298), (9299), (9301), (9302), (9305), (9307), (9309),
    (9311), (9312), (9313), (9315), (9316), (9317), (9327), (9475),
    (9480), (9483), (9484), (9486), (9493), (9494), (9495), (9496),
    (9497), (9532), (9539), (9540), (9543), (9544), (9545), (9546),
    (9547), (9548), (9549), (9550), (9552), (9553), (9555), (9556),
    (9586), (9589), (9592), (9595), (9601), (9603), (9604), (9606),
    (9607), (9621), (9625), (9643), (9653), (9657), (9658), (9659),
    (9660), (9661), (9662), (9663), (9664), (9665), (9667), (9670),
    (9671), (9673), (9676), (9679), (9718), (9721), (9722), (9724),
    (9725), (9726), (9731), (9732), (9733), (9734), (9735), (9736),
    (9737), (9743), (9745), (9746), (9747), (9749), (9750), (9751),
    (9752), (9753), (9754), (9756), (9774), (9800), (9829), (9830),
    (9832), (9833), (9835), (9836), (9838), (9839), (9840), (9841),
    (9842), (9844), (9847), (9851), (9857), (9859), (9862), (9863),
    (9864), (9867), (9868), (9869), (9870), (9871), (9873), (9875),
    (9876), (9877), (9881), (9883), (9884), (9885), (9886), (9888),
    (9889), (9890), (9892), (9894), (9896), (9897), (9898), (9899),
    (9900), (9901), (9902), (9904), (9905), (9906), (9907), (9910),
    (9914), (9917), (9918), (9919), (9920), (9921), (9923), (9925),
    (9931), (9932), (9934), (9935), (9936), (9937), (9938), (9939),
    (9940), (9941), (9943), (9944), (9945), (9947), (9949), (9950),
    (9951), (9955), (9963), (9964), (9966), (9967), (9968), (9969),
    (9970), (9972), (9973), (9974), (9975), (9978), (9979), (9980),
    (9981), (9984), (10259), (10260), (10261), (10262), (10263),
    (10264), (10265), (10267), (10269), (10757), (10760);

CREATE TEMP TABLE benchmark_refresh_replacement_guard (
    smoke_count INTEGER NOT NULL CHECK(smoke_count = 21),
    core_only_count INTEGER NOT NULL CHECK(core_only_count = 205),
    invalid_count INTEGER NOT NULL CHECK(invalid_count = 0)
);
INSERT INTO benchmark_refresh_replacement_guard
SELECT (SELECT count(*) FROM smoke_replacements),
       (SELECT count(*) FROM core_only_replacements),
       (SELECT count(*)
        FROM matrices AS m
        WHERE m.matrix_id IN (
            SELECT matrix_id FROM smoke_replacements
            UNION ALL
            SELECT matrix_id FROM core_only_replacements
        )
          AND (
              m.preprocessing_solved
              OR m.core_and_stress_test
              OR m.is_copositive IS NULL
              OR (m.matrix_id IN (SELECT matrix_id FROM smoke_replacements)
                  AND (m.dimension > 20 OR m.fastest_elapsed_ns IS NULL OR m.fastest_elapsed_ns > 50000000))
          ));

UPDATE matrices
SET smoke_set = 0,
    core_and_stress_test = 0
WHERE preprocessing_solved;

UPDATE matrices
SET smoke_set = 1,
    core_and_stress_test = 1
WHERE matrix_id IN (SELECT matrix_id FROM smoke_replacements);

UPDATE matrices
SET core_and_stress_test = 1
WHERE matrix_id IN (SELECT matrix_id FROM core_only_replacements);

CREATE TEMP TABLE benchmark_refresh_final_guard (
    matrix_count INTEGER NOT NULL CHECK(matrix_count = 3513),
    preprocessing_solved_count INTEGER NOT NULL CHECK(preprocessing_solved_count = 2708),
    smoke_count INTEGER NOT NULL CHECK(smoke_count = 49),
    core_count INTEGER NOT NULL CHECK(core_count = 512),
    smoke_preprocessed INTEGER NOT NULL CHECK(smoke_preprocessed = 0),
    core_preprocessed INTEGER NOT NULL CHECK(core_preprocessed = 0),
    smoke_outside_core INTEGER NOT NULL CHECK(smoke_outside_core = 0),
    n_le_100_count INTEGER NOT NULL CHECK(n_le_100_count = 754),
    n_gt_100_solved_count INTEGER NOT NULL CHECK(n_gt_100_solved_count = 0),
    smoke_strict INTEGER NOT NULL CHECK(smoke_strict = 23),
    smoke_boundary INTEGER NOT NULL CHECK(smoke_boundary = 26),
    smoke_not_copositive INTEGER NOT NULL CHECK(smoke_not_copositive = 0),
    core_strict INTEGER NOT NULL CHECK(core_strict = 242),
    core_boundary INTEGER NOT NULL CHECK(core_boundary = 251),
    core_not_copositive INTEGER NOT NULL CHECK(core_not_copositive = 19)
);
INSERT INTO benchmark_refresh_final_guard
SELECT count(*),
       count(*) FILTER (WHERE preprocessing_solved),
       count(*) FILTER (WHERE smoke_set),
       count(*) FILTER (WHERE core_and_stress_test),
       count(*) FILTER (WHERE smoke_set AND preprocessing_solved),
       count(*) FILTER (WHERE core_and_stress_test AND preprocessing_solved),
       count(*) FILTER (WHERE smoke_set AND NOT core_and_stress_test),
       count(*) FILTER (WHERE n_le_100),
       count(*) FILTER (WHERE n_gt_100_solved),
       count(*) FILTER (WHERE smoke_set AND is_strictly_copositive = 1),
       count(*) FILTER (WHERE smoke_set AND is_copositive = 1 AND is_strictly_copositive = 0),
       count(*) FILTER (WHERE smoke_set AND is_copositive = 0),
       count(*) FILTER (WHERE core_and_stress_test AND is_strictly_copositive = 1),
       count(*) FILTER (WHERE core_and_stress_test AND is_copositive = 1 AND is_strictly_copositive = 0),
       count(*) FILTER (WHERE core_and_stress_test AND is_copositive = 0)
FROM matrices;

COMMIT;
