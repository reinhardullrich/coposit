from contextlib import closing
import hashlib
import json
import os
from pathlib import Path
import re
import sqlite3
import subprocess
import sys
import tempfile
import unittest

from pycoposit import (
    ALGORITHMS,
    COMBINED_CLASSIFICATION_ALGORITHMS,
    COPOSITIVITY_MODES,
    PREPROCESSING_MODES,
    MPConfig,
    Matrix,
    StatusCode,
    compute_matrix,
    run,
    run_multiprocessing,
)
from pycoposit.core import coposit_path, matrix_parser_source, model_companion_path
from pycoposit.mp import _max_pending_matrices
from pycoposit.types import (
    PARAMETERIZED_ALGORITHMS,
    _resolve_model_parameter,
    _validate_algorithm,
    _validate_mode,
    _validate_preprocessing,
)
from run_results import _resolve_mode, _result_model_id


COPOSITIVE_BASELINES = (
    "dutour_2018",
    "danninger_1990",
    "copomatrix_2011",
    "hadeler_1983",
    "dickinson_2019",
    "safi_2021",
    "bundfuss_2008",
    "sponsel_2012",
)
COPOSITIVE_MODE_ALGORITHMS = tuple(dict.fromkeys(COPOSITIVE_BASELINES + (
    "adaptive_dutour_danninger",
    "adaptive_dutour_copomatrix",
    "adaptive_sponsel_copomatrix",
    "adaptive_zischg_sponsel_copomatrix",
    "frank_wolfe_sponsel",
    *COMBINED_CLASSIFICATION_ALGORITHMS,
)))
STRICT_ONLY_ALGORITHMS = tuple(algorithm for algorithm in ALGORITHMS if algorithm not in COPOSITIVE_MODE_ALGORITHMS)


def initialize_runner_database(connection: sqlite3.Connection, root: Path) -> None:
    connection.executescript((root / "testdata" / "schema.sql").read_text())
    connection.executescript((root / "testdata" / "diagnostics_schema.sql").read_text())


class WrapperTests(unittest.TestCase):
    def test_corpus_and_diagnostics_schemas_are_separate(self):
        root = Path(__file__).resolve().parents[2]
        with closing(sqlite3.connect(":memory:")) as corpus:
            corpus.executescript((root / "testdata" / "schema.sql").read_text())
            self.assertEqual(
                {row[0] for row in corpus.execute("SELECT name FROM sqlite_schema WHERE type = 'table'")},
                {"matrices", "sources"},
            )

        with closing(sqlite3.connect(":memory:")) as diagnostics:
            diagnostics.executescript((root / "testdata" / "diagnostics_schema.sql").read_text())
            diagnostics.execute(
                """INSERT INTO results (
                       matrix_id, model_id, mode, preprocessing, binary_sha256, status, elapsed_ns,
                       timeout_ns, recorded_at, diagnostics, certificate_joint_distribution
                   ) VALUES (1, 'cbdd_dickinson', 'strictly_copositive', 'none', ?, 'timeout',
                             60000000000, 60000000000, 'now', 'last diagnostics line', '[[2,28,492]]')""",
                ("1" * 64,),
            )
            self.assertEqual(
                diagnostics.execute(
                    "SELECT elapsed_ns, diagnostics, certificate_joint_distribution FROM results"
                ).fetchone(),
                (60_000_000_000, "last diagnostics line", "[[2,28,492]]"),
            )

    def test_schema_keeps_strict_and_copositive_truth_consistent(self):
        root = Path(__file__).resolve().parents[2]
        with closing(sqlite3.connect(":memory:")) as connection:
            connection.executescript((root / "testdata" / "schema.sql").read_text())
            connection.execute(
                "INSERT INTO matrices(matrix_id, dimension, matrix, is_strictly_copositive, is_copositive) "
                "VALUES (1, 1, '0', 0, NULL)"
            )
            with self.assertRaises(sqlite3.IntegrityError):
                connection.execute(
                    "INSERT INTO matrices(matrix_id, dimension, matrix, is_strictly_copositive, is_copositive) "
                    "VALUES (2, 1, '1', 1, NULL)"
                )

    def test_schema_binds_external_files_to_sha256(self):
        root = Path(__file__).resolve().parents[2]
        with closing(sqlite3.connect(":memory:")) as connection:
            connection.executescript((root / "testdata" / "schema.sql").read_text())
            with self.assertRaises(sqlite3.IntegrityError):
                connection.execute(
                    "INSERT INTO matrices(matrix_id, dimension, matrix, is_strictly_copositive, is_copositive) "
                    "VALUES (1, 1, 'file:matrices/1.mtx', 1, 1)"
                )
            connection.execute(
                "INSERT INTO matrices(matrix_id, dimension, matrix, file_sha256, is_strictly_copositive, is_copositive) "
                "VALUES (1, 1, 'file:matrices/1.mtx', ?, 1, 1)",
                ("0" * 64,),
            )
            with self.assertRaises(sqlite3.IntegrityError):
                connection.execute(
                    "INSERT INTO matrices(matrix_id, dimension, matrix, file_sha256, is_strictly_copositive, is_copositive) "
                    "VALUES (2, 1, '1', ?, 1, 1)",
                    ("0" * 64,),
                )

    def test_public_validation(self):
        self.assertIn("hadeler_1983", ALGORITHMS)
        self.assertEqual(COPOSITIVITY_MODES, ("copositive", "strictly_copositive", "both"))
        self.assertEqual(PREPROCESSING_MODES, ("none", "both"))
        expected_combined = tuple(
            algorithm
            for algorithm in ALGORITHMS
            if algorithm == "danninger_1990"
            or algorithm == "dickinson_2019"
            or algorithm.endswith("_dickinson")
            or algorithm == "cbdd_dickinson_improved_1"
            or algorithm == "sat_b1"
            or algorithm == "sat_b2"
            or algorithm == "sat_b3"
            or algorithm == "clasp_b3"
            or algorithm == "bdd_b3"
            or algorithm == "sat_b4"
            or algorithm == "sat_b5"
            or algorithm == "nbc_b6"
            or algorithm == "nbc_b7"
            or algorithm == "improved_nbc_b7"
            or algorithm == "dual_frontier_nbc"
            or algorithm == "dual_frontier_nbc_two"
            or algorithm == "dual_frontier_nbc_three"
            or algorithm == "dual_frontier_nbc_four"
            or algorithm == "improved_nbc_b8"
            or algorithm == "improved_nbc_b9"
            or algorithm == "improved_nbc_g2"
            or algorithm == "sat_c1"
            or algorithm == "sat_c2"
            or algorithm == "sat_c3"
            or algorithm == "sat_c4"
            or algorithm == "f1"
            or algorithm == "f2"
            or algorithm == "g1"
            or algorithm == "sat_a1"
            or algorithm == "sat_a2"
            or algorithm == "sat_a3"
            or algorithm == "sat_a4"
            or algorithm == "sat_a5"
            or algorithm == "xxx"
            or algorithm == "xxx_two"
            or algorithm == "hadeler_1983"
            or algorithm.endswith("_hadeler")
            or algorithm == "fracessa"
            or algorithm.startswith("fracessa_")
            or algorithm.endswith("_fracessa")
        )
        self.assertEqual(COMBINED_CLASSIFICATION_ALGORITHMS, expected_combined)
        self.assertEqual(StatusCode.NODE_LIMIT, 6)
        with self.assertRaises(TypeError):
            _validate_algorithm(1)
        with self.assertRaises(ValueError):
            _validate_algorithm("unknown")
        with self.assertRaises(TypeError):
            _validate_mode(1)
        with self.assertRaises(ValueError):
            _validate_mode("unknown")
        with self.assertRaises(TypeError):
            _validate_preprocessing(1)
        with self.assertRaises(ValueError):
            _validate_preprocessing("unknown")
        with self.assertRaises(ValueError):
            Matrix("1#1", matrix_id=1 << 63)
        with self.assertRaises(TypeError):
            run("hadeler_1983", Matrix("1#1"), diagnostics="yes")
        with self.assertRaises(TypeError):
            compute_matrix("cbdd_dickinson", Matrix("1#1"), collect_certificate_joint_distribution="yes")
        self.assertIsNone(Matrix("1#1").matrix_id)
        unlabeled_result = run("hadeler_1983", Matrix("1#1"))
        self.assertEqual(unlabeled_result["status"], StatusCode.OK)
        self.assertIsNone(unlabeled_result["matrix_id"])
        with self.assertRaises(ValueError):
            MPConfig(workers=0)
        self.assertEqual(_max_pending_matrices(MPConfig(workers=2, prefetch_per_worker=3, queue_maxsize=4)), 4)

    def test_reference_runner_defaults_only_to_full_classification(self):
        for algorithm in COMBINED_CLASSIFICATION_ALGORITHMS:
            self.assertEqual(_resolve_mode(algorithm, None), "both")
        self.assertEqual(_resolve_mode("bundfuss_2008", "strictly_copositive"), "strictly_copositive")
        with self.assertRaisesRegex(ValueError, "run it once with --mode copositive"):
            _resolve_mode("bundfuss_2008", None)

    def test_wide_certificate_runtime_percentage(self):
        self.assertNotIn("wide_75_certificate_cbdd_dickinson", ALGORITHMS)
        for model in (model for model in PARAMETERIZED_ALGORITHMS if model != "xxx_two"):
            with self.subTest(model=model):
                with self.assertRaises(ValueError):
                    _resolve_model_parameter(model, None)
                self.assertEqual(_resolve_model_parameter(model, "95"), "95")
                self.assertEqual(_result_model_id(model, "95"), f"{model}@95")
                with self.assertRaises(ValueError):
                    _resolve_model_parameter(model, "101")

                result = run(model, Matrix("1#1"), model_parameter="75")
                self.assertEqual(result["status"], StatusCode.OK)
                self.assertEqual(result["model_parameter"], "75")
                self.assertEqual((result["is_copositive"], result["is_strictly_copositive"]), (True, True))
        with self.assertRaises(ValueError):
            _resolve_model_parameter("xxx_two", None)
        self.assertEqual(_resolve_model_parameter("xxx_two", "alternating"), "alternating")
        self.assertEqual(_resolve_model_parameter("xxx_two", "ascending"), "ascending")
        with self.assertRaises(ValueError):
            _resolve_model_parameter("xxx_two", "50")
        with self.assertRaises(ValueError):
            _resolve_model_parameter("hadeler_1983", "75")

    def test_serial_dickinson_experiments_collect_sparse_certificate_joint_distributions(self):
        for model in (
            "cbdd_dickinson",
            "cbdd_halfspace_dickinson",
            "upper_endpoint_cbdd_dickinson",
            "czdd_dickinson",
            "sat_dickinson",
            "sat_b1",
            "sat_a1",
            "sat_a2",
            "sat_a3",
            "sat_a4",
            "sat_a5",
            "sat_halfspace_rays_lookahead_dickinson",
            "sat_halfspace_rays_wide_dickinson",
            "wide_certificate_sat_dickinson",
            "clingo_dickinson",
            "clingo_halfspace_dickinson",
        ):
            with self.subTest(model=model):
                parameter = "alternating" if model == "xxx_two" else "50" if model in PARAMETERIZED_ALGORITHMS else None
                result = compute_matrix(
                    model,
                    Matrix("2#1,0,1"),
                    preprocessing="none",
                    collect_certificate_joint_distribution=True,
                    model_parameter=parameter,
                )
                self.assertEqual(result["status"], StatusCode.OK)
                expected = [[1, 1, 2, 2]]
                if model == "upper_endpoint_cbdd_dickinson": expected.append([2, 0, 2, 1])
                self.assertEqual(result["certificate_joint_distribution"], expected)

    def test_matrix_market_file_path(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            matrix_directory = Path(temporary_directory) / "matrices"
            matrix_directory.mkdir()
            (matrix_directory / "7.mtx").write_text(
                "%%MatrixMarket matrix array real symmetric\n% exact 2x2 example\n2 2\n5e-1\n-1\n5e-1\n",
                encoding="ascii",
            )
            matrix = Matrix(str(matrix_directory / "7.mtx"), matrix_id=7)
            self.assertEqual(matrix_parser_source(matrix), (str(matrix_directory / "7.mtx"), True))
            result = run("hadeler_1983", matrix, "copositive")
            self.assertEqual(result["status"], StatusCode.OK)
            self.assertIs(result["is_copositive"], False)

            (matrix_directory / "8.mtx").write_text(
                "%%MatrixMarket matrix coordinate integer symmetric\n3 3 4\n1 1 1\n3 1 -2\n2 2 1\n3 3 1\n",
                encoding="ascii",
            )
            sparse = Matrix(str(matrix_directory / "8.mtx"), matrix_id=8)
            sparse_result = run("hadeler_1983", sparse, "copositive")
            self.assertEqual(sparse_result["status"], StatusCode.OK)
            self.assertIs(sparse_result["is_copositive"], False)

    def test_direct_matrix_file_path(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            path = Path(temporary_directory) / "matrix with spaces.mtx"
            matrix_market = "%%MatrixMarket matrix array integer symmetric\n2 2\n1\n-1\n1\n"
            path.write_text(matrix_market, encoding="ascii")
            matrix = Matrix(str(path), matrix_id=10)
            self.assertEqual(matrix_parser_source(matrix), (str(path), True))
            result = run("hadeler_1983", matrix, "copositive")
            self.assertEqual(result["status"], StatusCode.OK)
            self.assertIs(result["is_copositive"], True)

            relative_path = os.path.relpath(path, Path.cwd())
            self.assertEqual(run("hadeler_1983", Matrix(relative_path), "copositive")["status"], StatusCode.OK)

            inline = Matrix(matrix_market, matrix_id=11)
            self.assertEqual(matrix_parser_source(inline), (matrix_market, False))
            self.assertEqual(run("hadeler_1983", inline, "copositive")["status"], StatusCode.OK)

    def test_file_parse_errors_match_sequential_and_multiprocessing(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            directory = Path(temporary_directory)
            malformed = directory / "malformed.mtx"
            malformed.write_text("%%MatrixMarket matrix array integer symmetric\n2 2\n1\n", encoding="ascii")
            matrices = [Matrix(str(malformed), matrix_id=1), Matrix(str(directory / "missing.mtx"), matrix_id=2)]

            sequential = [run("hadeler_1983", matrix) for matrix in matrices]
            multiprocessing = sorted(run_multiprocessing("hadeler_1983", matrices, MPConfig(workers=1)),
                                         key=lambda result: result["matrix_id"])

            self.assertEqual([result["status"] for result in sequential], [StatusCode.PARSE_ERROR, StatusCode.PARSE_ERROR])
            self.assertEqual([result["status"] for result in multiprocessing], [StatusCode.PARSE_ERROR, StatusCode.PARSE_ERROR])
            self.assertTrue(all(result["is_strictly_copositive"] is None for result in sequential + multiprocessing))

    def test_fracessa_fraction_scientific_and_circular_input(self):
        fraction = run("hadeler_1983", Matrix("2#5e-1,-1/4,+1/2", matrix_id=1))
        circular = run("hadeler_1983", Matrix("5#0,1/2,-2.5e-1", matrix_id=2))
        invalid_sign = run("hadeler_1983", Matrix("2#1,1/-2,1", matrix_id=3))

        self.assertEqual(fraction["status"], StatusCode.OK)
        self.assertEqual(fraction["preprocessing"], "both")
        self.assertIs(fraction["is_strictly_copositive"], True)
        self.assertEqual(circular["status"], StatusCode.OK)
        self.assertIs(circular["is_strictly_copositive"], False)
        self.assertEqual(invalid_sign["status"], StatusCode.PARSE_ERROR)

    def test_fracessa_input_rejects_whitespace(self):
        result = run("hadeler_1983", Matrix("2#1, -1,1", matrix_id=1))
        self.assertEqual(result["status"], StatusCode.PARSE_ERROR)
        self.assertIn("must not contain whitespace", result["error_message"])

    def test_every_model_uses_coposit(self):
        root = Path(__file__).resolve().parents[2]
        source_models = {
            source.parent.name
            for category in ("baselines", "experiments", "hadeler-based")
            for source in (root / "models" / category).glob("*/solver.cpp")
        }
        self.assertEqual(set(ALGORITHMS), source_models)
        help_output = subprocess.run([coposit_path(), "--help"], check=True, capture_output=True, text=True).stdout
        self.assertEqual(set(help_output.partition("Models:\n")[2].split()), source_models)
        for algorithm in ALGORITHMS:
            with self.subTest(algorithm=algorithm):
                parameter = "alternating" if algorithm == "xxx_two" else "50" if algorithm in PARAMETERIZED_ALGORITHMS else None
                positive = run(
                    algorithm, Matrix("2#1,0,1", matrix_id=1), "strictly_copositive", model_parameter=parameter
                )
                boundary = run(
                    algorithm, Matrix("1#0", matrix_id=2), "strictly_copositive", model_parameter=parameter
                )
                self.assertEqual(positive["status"], StatusCode.OK)
                self.assertEqual(positive["mode"], "strictly_copositive")
                self.assertIsNone(positive["is_copositive"])
                self.assertIs(positive["is_strictly_copositive"], True)
                self.assertIs(boundary["is_strictly_copositive"], False)
                self.assertTrue(model_companion_path(algorithm).is_file())

    def test_every_supported_model_decides_non_strict_copositivity_before_and_after_preprocessing(self):
        for algorithm in COPOSITIVE_MODE_ALGORITHMS:
            for preprocessing in ("none", "both"):
                with self.subTest(algorithm=algorithm, preprocessing=preprocessing):
                    parameter = "alternating" if algorithm == "xxx_two" else "50" if algorithm in PARAMETERIZED_ALGORITHMS else None
                    boundary = run(
                        algorithm, Matrix("2#1,-1,1", matrix_id=1), "copositive", preprocessing,
                        model_parameter=parameter,
                    )
                    negative = run(
                        algorithm, Matrix("2#1,-2,1", matrix_id=2), "copositive", preprocessing,
                        model_parameter=parameter,
                    )
                    self.assertEqual(boundary["status"], StatusCode.OK)
                    self.assertEqual(boundary["mode"], "copositive")
                    self.assertIs(boundary["is_copositive"], True)
                    self.assertIsNone(boundary["is_strictly_copositive"])
                    self.assertEqual(negative["status"], StatusCode.OK)
                    self.assertIs(negative["is_copositive"], False)

    def test_every_strict_only_model_rejects_copositive_mode_before_preprocessing(self):
        for algorithm in STRICT_ONLY_ALGORITHMS:
            for preprocessing in PREPROCESSING_MODES:
                with self.subTest(algorithm=algorithm, preprocessing=preprocessing):
                    result = run(algorithm, Matrix("1#1", matrix_id=1), "copositive", preprocessing)
                    self.assertEqual(result["status"], StatusCode.EXEC_ERROR)
                    self.assertIsNone(result["is_copositive"])
                    self.assertIn("supports only strict copositivity", result["error_message"])

    def test_preprocessing_modes_preserve_exact_classification(self):
        matrix = Matrix("4#1,-1,0,0,1,0,0,1,-1,1", matrix_id=1)
        for preprocessing in PREPROCESSING_MODES:
            with self.subTest(preprocessing=preprocessing):
                result = run("dickinson_2019", matrix, "copositive", preprocessing)
                self.assertEqual(result["status"], StatusCode.OK)
                self.assertEqual(result["preprocessing"], preprocessing)
                self.assertIs(result["is_copositive"], True)

    def test_supported_models_classify_both_predicates(self):
        cases = (
            ("4#5,-1,-1,-1,5,-1,-1,5,-1,5", True, True),
            ("4#3,-1,-1,-1,3,-1,-1,3,-1,3", True, False),
            ("4#2,-1,-1,-1,2,-1,-1,2,-1,2", False, False),
        )
        for algorithm in COMBINED_CLASSIFICATION_ALGORITHMS:
            for matrix_id, (matrix, expected_copositive, expected_strict) in enumerate(cases, 1):
                with self.subTest(algorithm=algorithm, matrix_id=matrix_id):
                    parameter = "alternating" if algorithm == "xxx_two" else "50" if algorithm in PARAMETERIZED_ALGORITHMS else None
                    result = run(algorithm, Matrix(matrix, matrix_id=matrix_id), model_parameter=parameter)
                    self.assertEqual(result["status"], StatusCode.OK)
                    self.assertEqual(result["mode"], "both")
                    self.assertIs(result["is_copositive"], expected_copositive)
                    self.assertIs(result["is_strictly_copositive"], expected_strict)

    def test_unsupported_model_rejects_combined_classification(self):
        result = run("dutour_2018", Matrix("1#1", matrix_id=1), "both")
        self.assertEqual(result["status"], StatusCode.EXEC_ERROR)
        self.assertIsNone(result["is_copositive"])
        self.assertIsNone(result["is_strictly_copositive"])
        self.assertIn("does not support combined classification", result["error_message"])

    def test_dutour_open_node_limit_is_unresolved(self):
        root = Path(__file__).resolve().parents[2]
        with closing(sqlite3.connect(root / "testdata" / "copos_testdata.sqlite3")) as connection:
            dimension, values = connection.execute(
                "SELECT dimension, matrix FROM matrices WHERE matrix_id = 9660"
            ).fetchone()

        result = run(
            "dutour_2018", Matrix(f"{dimension}#{values}", matrix_id=9660), "strictly_copositive", preprocessing="none"
        )
        self.assertEqual(result["status"], StatusCode.NODE_LIMIT)
        self.assertIsNone(result["is_strictly_copositive"])
        self.assertIn("50000 open nodes", result["error_message"])

    def test_sequential_iterable_and_parser_status(self):
        results = list(run("hadeler_1983", [Matrix("1#1", matrix_id=4), Matrix("1#bad", matrix_id=5)]))
        self.assertEqual([result["matrix_id"] for result in results], [4, 5])
        self.assertEqual(results[0]["status"], StatusCode.OK)
        self.assertEqual(results[1]["status"], StatusCode.PARSE_ERROR)
        self.assertIsNone(results[1]["is_strictly_copositive"])

    def test_multiprocessing_hadeler(self):
        matrices = [Matrix("2#1,0,1", matrix_id=10), Matrix("2#1,-2,1", matrix_id=11)]
        results = list(run_multiprocessing("hadeler_1983", matrices, MPConfig(workers=2)))
        self.assertCountEqual([result["matrix_id"] for result in results], [10, 11])
        self.assertTrue(all(result["status"] == StatusCode.OK for result in results))
        self.assertCountEqual([result["is_strictly_copositive"] for result in results], [True, False])

    def test_multiprocessing_copositive_hadeler(self):
        matrices = [Matrix("2#1,-1,1", matrix_id=12), Matrix("2#1,-2,1", matrix_id=13)]
        results = list(run_multiprocessing("hadeler_1983", matrices, MPConfig(workers=2), "copositive"))
        self.assertCountEqual([result["matrix_id"] for result in results], [12, 13])
        self.assertCountEqual([result["is_copositive"] for result in results], [True, False])

    def test_multiprocessing_combined_hadeler(self):
        matrices = [Matrix("2#1,-1,1", matrix_id=14), Matrix("2#1,-2,1", matrix_id=15)]
        results = list(run_multiprocessing("hadeler_1983", matrices, MPConfig(workers=2), "both"))
        self.assertCountEqual(
            [(result["is_copositive"], result["is_strictly_copositive"]) for result in results],
            [(True, False), (False, False)],
        )

@unittest.skipUnless(hasattr(os, "sched_getaffinity"), "CPU affinity requires Linux")
class ResultsRunnerTests(unittest.TestCase):
    def setUp(self):
        available_cpus = sorted(os.sched_getaffinity(0))
        if len(available_cpus) < 2:
            self.skipTest("results runner needs distinct parent and worker CPUs")
        self.parent_cpu, self.worker_cpu = available_cpus[:2]

    def test_runner_refreshes_fastest_classification_cache_from_separate_diagnostics_database(self):
        root = Path(__file__).resolve().parents[2]
        with tempfile.TemporaryDirectory() as temporary_directory:
            directory = Path(temporary_directory)
            corpus = directory / "corpus.sqlite3"
            diagnostics = directory / "diagnostics.sqlite3"
            with closing(sqlite3.connect(corpus)) as connection:
                connection.executescript((root / "testdata" / "schema.sql").read_text())
                connection.executemany(
                    "INSERT INTO matrices(matrix_id, dimension, matrix, is_strictly_copositive, is_copositive) "
                    "VALUES (?, 1, ?, ?, ?)",
                    ((1, "1", 1, 1), (2, "0", 0, 1), (3, "-1", 0, 0)),
                )
                connection.commit()
            with closing(sqlite3.connect(diagnostics)) as connection:
                connection.executescript((root / "testdata" / "diagnostics_schema.sql").read_text())
                connection.executemany(
                    """INSERT INTO results (
                           matrix_id, model_id, mode, preprocessing, binary_sha256, status,
                           is_copositive, is_strictly_copositive, elapsed_ns, timeout_ns, recorded_at
                       ) VALUES (?, ?, ?, ?, ?, 'ok', ?, ?, ?, 1, 'now')""",
                    (
                        (1, "legacy_strict", "strictly_copositive", "both", "0" * 64, None, 1, 0),
                        (1, "full_positive", "both", "both", "0" * 64, 1, 1, 1),
                        (2, "full_no_preprocessing", "both", "none", "0" * 64, 1, 0, 0),
                        (2, "full_boundary", "both", "both", "0" * 64, 1, 0, 1),
                        (3, "legacy_cp", "copositive", "both", "0" * 64, 0, None, 0),
                        (3, "full_negative", "both", "both", "0" * 64, 0, 0, 1),
                    ),
                )
                connection.commit()

            command = [
                sys.executable,
                str(root / "python" / "run_results.py"),
                "hadeler_1983",
                "--timeout-seconds",
                "5",
                "--parent-cpu",
                str(self.parent_cpu),
                "--cpus",
                str(self.worker_cpu),
                "--database",
                str(corpus),
                "--results-database",
                str(diagnostics),
                "--without-diagnostics",
            ]
            environment = os.environ.copy()
            environment["PYTHONPATH"] = str(root / "python")
            subprocess.run(command, cwd=root, env=environment, check=True, capture_output=True, text=True)

            with closing(sqlite3.connect(diagnostics)) as connection:
                self.assertEqual(
                    connection.execute(
                        "SELECT diagnostics, certificate_joint_distribution FROM results WHERE model_id = 'hadeler_1983'"
                    ).fetchall(),
                    [(None, None), (None, None), (None, None)],
                )

            with closing(sqlite3.connect(corpus)) as connection:
                cached = connection.execute(
                    "SELECT matrix_id, fastest_elapsed_ns, fastest_result_ref FROM matrices ORDER BY matrix_id"
                ).fetchall()
            self.assertEqual(
                [(matrix_id, elapsed_ns, json.loads(reference)["model_id"], json.loads(reference)["mode"])
                 for matrix_id, elapsed_ns, reference in cached],
                [(1, 1, "full_positive", "both"),
                 (2, 0, "full_no_preprocessing", "both"),
                 (3, 1, "full_negative", "both")],
            )

    def test_large_runner_throttles_diagnostics_output(self):
        root = Path(__file__).resolve().parents[2]
        with tempfile.TemporaryDirectory() as temporary_directory:
            database = Path(temporary_directory) / "results.sqlite3"
            with closing(sqlite3.connect(database)) as connection:
                initialize_runner_database(connection, root)
                connection.executemany(
                    "INSERT INTO matrices(matrix_id, dimension, matrix, is_strictly_copositive, is_copositive, source, family) "
                    "VALUES (?, 1, '1', 1, 1, NULL, NULL)",
                    ((matrix_id,) for matrix_id in range(1, 102)),
                )
                connection.commit()

            command = [
                sys.executable,
                str(root / "python" / "run_results.py"),
                "hadeler_1983",
                "--timeout-seconds",
                "5",
                "--dimension-from",
                "1",
                "--dimension-to",
                "1",
                "--parent-cpu",
                str(self.parent_cpu),
                "--cpus",
                str(self.worker_cpu),
                "--database",
                str(database),
            ]
            environment = os.environ.copy()
            environment["PYTHONPATH"] = str(root / "python")
            completed = subprocess.run(command, cwd=root, env=environment, check=True, capture_output=True, text=True)

            diagnostics_lines = re.findall(r"^\[\d+/101\].*$", completed.stdout, flags=re.MULTILINE)
            self.assertLess(len(diagnostics_lines), 101)
            self.assertTrue(diagnostics_lines[-1].startswith("[101/101]"))
            with closing(sqlite3.connect(database)) as connection:
                self.assertEqual(connection.execute("SELECT COUNT(*) FROM results").fetchone()[0], 101)

    def test_runner_selects_the_union_of_matrix_set_flags(self):
        root = Path(__file__).resolve().parents[2]
        with tempfile.TemporaryDirectory() as temporary_directory:
            database = Path(temporary_directory) / "results.sqlite3"
            with closing(sqlite3.connect(database)) as connection:
                initialize_runner_database(connection, root)
                connection.executemany(
                    "INSERT INTO matrices(matrix_id, dimension, matrix, is_strictly_copositive, is_copositive, "
                    "core_and_stress_test, bpqy_quick_test, preprocessing_solved, references_unsolved) "
                    "VALUES (?, ?, '1', ?, ?, ?, ?, ?, ?)",
                    ((1, 1, 1, 1, 1, 0, 0, "[]"), (2, 1, 1, 1, 1, 0, 1, "[]"),
                     (3, 1, None, None, 0, 0, 0, "[]"),
                     (4, 101, 1, 1, 0, 0, 0, '[{"source_id":1,"comment":"timeout"}]'),
                     (5, 1, 1, 1, 1, 0, 0, "[]"), (6, 1, 1, 1, 0, 1, 0, "[]")),
                )
                connection.execute(
                    """INSERT INTO results (
                           matrix_id, model_id, mode, preprocessing, binary_sha256, status, is_copositive,
                           is_strictly_copositive, elapsed_ns, timeout_ns, recorded_at, message
                       ) VALUES (5, 'prior_model', 'strictly_copositive', 'none', ?, 'ok', NULL, 1, 1, 1, 'now', NULL)""",
                    ("0" * 64,),
                )
                connection.commit()

            command = [
                sys.executable,
                str(root / "python" / "run_results.py"),
                "hadeler_1983",
                "--timeout-seconds",
                "5",
                "--matrix-set",
                "core_and_stress_test",
                "n_le_100",
                "bpqy_benchmark",
                "bpqy_quick_test",
                "references_unsolved",
                "--matrix-ids",
                "1",
                "2",
                "3",
                "4",
                "5",
                "6",
                "--without-results",
                "--parent-cpu",
                str(self.parent_cpu),
                "--cpus",
                str(self.worker_cpu),
                "--database",
                str(database),
            ]
            environment = os.environ.copy()
            environment["PYTHONPATH"] = str(root / "python")
            completed = subprocess.run(command, cwd=root, env=environment, check=True, capture_output=True, text=True)

            with closing(sqlite3.connect(database)) as connection:
                rows = connection.execute(
                    "SELECT matrix_id, preprocessing FROM results WHERE model_id = 'hadeler_1983' ORDER BY matrix_id"
                ).fetchall()
            self.assertEqual(rows, [(1, "both"), (3, "both"), (4, "both"), (6, "both")])
            self.assertIn(
                "matrix_sets=core_and_stress_test,n_le_100,bpqy_benchmark,bpqy_quick_test,references_unsolved",
                completed.stdout,
            )
            self.assertIn("preprocessing=both", completed.stdout)
            self.assertIn("without_results=yes", completed.stdout)
            self.assertIn("comparison=unverified", completed.stdout)

    def test_bpqy_benchmark_is_generated_from_construction_truth_and_preprocessing(self):
        root = Path(__file__).resolve().parents[2]
        with tempfile.TemporaryDirectory() as temporary_directory:
            database = Path(temporary_directory) / "results.sqlite3"
            with closing(sqlite3.connect(database)) as connection:
                initialize_runner_database(connection, root)
                connection.execute(
                    "INSERT INTO sources(source_id,authors,title,publication_year,reference) VALUES (51,'A','T',2026,'R')"
                )
                connection.executemany(
                    """INSERT INTO matrices(
                           matrix_id,dimension,matrix,is_strictly_copositive,is_copositive,source_id,family,preprocessing_solved
                       ) VALUES (?,1,'1',?,?,51,?,?)""",
                    (
                        (1, 1, 1, "BPQY COP intended copositive-boundary generator", 0),
                        (2, None, None, "BPQY COP intended copositive-boundary extension", 0),
                        (3, 0, 0, "BPQY COP intended copositive-boundary generator", 0),
                        (4, 0, 1, "BPQY COP intended copositive-boundary generator", 0),
                        (5, 1, 1, "BPQY PSD intended copositive-boundary generator", 0),
                        (6, 1, 1, "BPQY COP intended copositive-boundary generator", 1),
                    ),
                )
                rows = connection.execute("SELECT matrix_id,bpqy_benchmark FROM matrices ORDER BY matrix_id").fetchall()
            self.assertEqual(rows, [(1, 1), (2, 1), (3, 0), (4, 0), (5, 0), (6, 0)])

    def test_runner_stores_file_parse_error_and_reuses_worker(self):
        root = Path(__file__).resolve().parents[2]
        with tempfile.TemporaryDirectory() as temporary_directory:
            directory = Path(temporary_directory)
            matrix_directory = directory / "matrices"
            matrix_directory.mkdir()
            malformed = matrix_directory / "1.mtx"
            malformed.write_text("%%MatrixMarket matrix array integer symmetric\n2 2\n1\n", encoding="ascii")
            database = directory / "results.sqlite3"
            with closing(sqlite3.connect(database)) as connection:
                initialize_runner_database(connection, root)
                connection.execute(
                    "INSERT INTO matrices(matrix_id, dimension, matrix, file_sha256, is_strictly_copositive, is_copositive) "
                    "VALUES (1, 2, 'file:matrices/1.mtx', ?, 1, 1)",
                    (hashlib.sha256(malformed.read_bytes()).hexdigest(),),
                )
                connection.execute(
                    "INSERT INTO matrices(matrix_id, dimension, matrix, is_strictly_copositive, is_copositive) "
                    "VALUES (2, 1, '1', 1, 1)"
                )
                connection.commit()

            command = [
                sys.executable,
                str(root / "python" / "run_results.py"),
                "hadeler_1983",
                "--timeout-seconds",
                "5",
                "--parent-cpu",
                str(self.parent_cpu),
                "--cpus",
                str(self.worker_cpu),
                "--database",
                str(database),
            ]
            environment = os.environ.copy()
            environment["PYTHONPATH"] = str(root / "python")
            completed = subprocess.run(command, cwd=root, env=environment, check=True, capture_output=True, text=True)

            with closing(sqlite3.connect(database)) as connection:
                rows = connection.execute(
                    "SELECT matrix_id, status, is_strictly_copositive, elapsed_ns, message FROM results ORDER BY matrix_id"
                ).fetchall()
            self.assertEqual(rows[0][0:4], (1, "parse_error", None, None))
            self.assertTrue(rows[0][4])
            self.assertEqual(rows[1][0:3], (2, "ok", 1))
            self.assertGreaterEqual(rows[1][3], 0)
            worker_pids = re.findall(r"\bpid=(\d+)", completed.stdout)
            self.assertEqual(len(worker_pids), 2)
            self.assertEqual(len(set(worker_pids)), 1)

    def test_runner_reuses_external_result_without_reading_or_hashing_the_file(self):
        root = Path(__file__).resolve().parents[2]
        with tempfile.TemporaryDirectory() as temporary_directory:
            directory = Path(temporary_directory)
            matrix_directory = directory / "matrices"
            matrix_directory.mkdir()
            matrix_path = matrix_directory / "1.mtx"
            matrix_path.write_text("%%MatrixMarket matrix array integer symmetric\n1 1\n1\n", encoding="ascii")
            database = directory / "results.sqlite3"
            with closing(sqlite3.connect(database)) as connection:
                initialize_runner_database(connection, root)
                connection.execute(
                    "INSERT INTO matrices(matrix_id, dimension, matrix, file_sha256, is_strictly_copositive, is_copositive) "
                    "VALUES (1, 1, 'file:matrices/1.mtx', ?, 1, 1)",
                    (hashlib.sha256(matrix_path.read_bytes()).hexdigest(),),
                )
                connection.commit()

            command = [
                sys.executable,
                str(root / "python" / "run_results.py"),
                "hadeler_1983",
                "--timeout-seconds",
                "5",
                "--parent-cpu",
                str(self.parent_cpu),
                "--cpus",
                str(self.worker_cpu),
                "--database",
                str(database),
            ]
            environment = os.environ.copy()
            environment["PYTHONPATH"] = str(root / "python")
            subprocess.run(command, cwd=root, env=environment, check=True, capture_output=True, text=True)
            matrix_path.unlink()
            reused = subprocess.run(command, cwd=root, env=environment, check=True, capture_output=True, text=True)

            self.assertIn("matrices=0", reused.stdout)
            with closing(sqlite3.connect(database)) as connection:
                self.assertEqual(connection.execute("SELECT COUNT(*) FROM results").fetchone()[0], 1)

    def test_runner_stores_and_resumes_hadeler_by_binary_hash(self):
        root = Path(__file__).resolve().parents[2]
        with tempfile.TemporaryDirectory() as temporary_directory:
            database = Path(temporary_directory) / "results.sqlite3"
            with closing(sqlite3.connect(database)) as connection:
                initialize_runner_database(connection, root)
                connection.execute(
                    "INSERT INTO matrices(matrix_id, dimension, matrix, is_strictly_copositive, is_copositive, source, family) "
                    "VALUES (1, 2, '1,0,1', 1, 1, NULL, NULL)"
                )
                connection.execute(
                    "INSERT INTO matrices(matrix_id, dimension, matrix, is_strictly_copositive, is_copositive, source, family) "
                    "VALUES (2, 2, '1,-2,1', 0, 0, NULL, NULL)"
                )
                connection.commit()

            command = [
                sys.executable,
                str(root / "python" / "run_results.py"),
                "hadeler_1983",
                "--timeout-seconds",
                "1000000",
                "--preprocessing",
                "none",
                "--dimension-from",
                "2",
                "--dimension-to",
                "2",
                "--parent-cpu",
                str(self.parent_cpu),
                "--cpus",
                str(self.worker_cpu),
                "--database",
                str(database),
            ]
            environment = os.environ.copy()
            environment["PYTHONPATH"] = str(root / "python")
            first = subprocess.run(command, cwd=root, env=environment, check=True, capture_output=True, text=True)
            second = subprocess.run(command, cwd=root, env=environment, check=True, capture_output=True, text=True)
            alternative_command = command.copy()
            alternative_command.extend(("--preprocessing", "both", "--matrix-id-from", "2", "--matrix-id-to", "2"))
            third = subprocess.run(alternative_command, cwd=root, env=environment, check=True, capture_output=True, text=True)
            copositive_command = command.copy()
            copositive_command[3:3] = ("--mode", "copositive")
            copositive_run = subprocess.run(copositive_command, cwd=root, env=environment, check=True, capture_output=True, text=True)

            with closing(sqlite3.connect(database)) as connection:
                rows = connection.execute(
                    """SELECT model_id, mode, preprocessing, length(binary_sha256), status, is_copositive,
                              is_strictly_copositive, elapsed_ns, timeout_ns, message
                       FROM results ORDER BY mode, matrix_id, preprocessing"""
                ).fetchall()
                with self.assertRaises(sqlite3.IntegrityError):
                    connection.execute(
                        """INSERT INTO results (
                               matrix_id, model_id, mode, preprocessing, binary_sha256, status, is_copositive,
                               is_strictly_copositive, elapsed_ns, timeout_ns, recorded_at, message
                           ) VALUES (1, 'hadeler_1983', 'strictly_copositive', 'none', ?,
                                     'timeout', NULL, NULL, NULL, 1, 'now', NULL)""",
                        ("0" * 63,),
                    )
                with self.assertRaises(sqlite3.IntegrityError):
                    connection.execute(
                        """INSERT INTO results (
                               matrix_id, model_id, mode, preprocessing, binary_sha256, status, is_copositive,
                               is_strictly_copositive, elapsed_ns, timeout_ns, recorded_at, message
                           ) VALUES (1, 'invalid_preprocessing', 'strictly_copositive', 'invalid', ?,
                                     'ok', NULL, 1, 0, 1, 'now', NULL)""",
                        ("0" * 64,),
                    )
                with self.assertRaises(sqlite3.IntegrityError):
                    connection.execute(
                        """INSERT INTO results (
                               matrix_id, model_id, mode, preprocessing, binary_sha256, status, is_copositive,
                               is_strictly_copositive, elapsed_ns, timeout_ns, recorded_at, message
                           ) VALUES (1, 'invalid_combined', 'both', 'none', ?,
                                     'ok', 0, 1, 0, 1, 'now', NULL)""",
                        ("0" * 64,),
                    )
            self.assertEqual(len(rows), 5)
            self.assertTrue(all(row[0] == "hadeler_1983" and row[3:5] == (64, "ok") for row in rows))
            self.assertEqual({row[2] for row in rows}, {"none", "both"})
            combined_rows = [row for row in rows if row[1] == "both"]
            copositive_rows = [row for row in rows if row[1] == "copositive"]
            self.assertEqual([(row[5], row[6]) for row in combined_rows], [(1, 1), (0, 0), (0, 0)])
            self.assertEqual([(row[5], row[6]) for row in copositive_rows], [(1, None), (0, None)])
            self.assertTrue(all(row[7] >= 0 for row in rows))
            self.assertTrue(all(row[8:] == (1_000_000_000_000_000, None) for row in rows))
            worker_pids = re.findall(r"\bpid=(\d+)", first.stdout)
            self.assertEqual(len(worker_pids), 2)
            self.assertEqual(len(set(worker_pids)), 1)
            self.assertIn("matrices=0", second.stdout)
            self.assertIn("matrices=1", third.stdout)
            self.assertIn("matrices=2", copositive_run.stdout)

    def test_runner_reuses_worker_after_cooperative_timeout(self):
        root = Path(__file__).resolve().parents[2]
        with closing(sqlite3.connect(root / "testdata" / "copos_testdata.sqlite3")) as source:
            dimension, hard_matrix, expected, expected_copositive = source.execute(
                "SELECT dimension, matrix, is_strictly_copositive, is_copositive FROM matrices WHERE matrix_id = 9656"
            ).fetchone()

        with tempfile.TemporaryDirectory() as temporary_directory:
            database = Path(temporary_directory) / "results.sqlite3"
            with closing(sqlite3.connect(database)) as connection:
                initialize_runner_database(connection, root)
                connection.execute(
                    "INSERT INTO matrices(matrix_id, dimension, matrix, is_strictly_copositive, is_copositive, source, family) "
                    "VALUES (1, ?, ?, ?, ?, NULL, NULL)",
                    (dimension, hard_matrix, expected, expected_copositive),
                )
                zero_matrix = ",".join("0" for _ in range(dimension * (dimension + 1) // 2))
                connection.execute(
                    "INSERT INTO matrices(matrix_id, dimension, matrix, is_strictly_copositive, is_copositive, source, family) "
                    "VALUES (2, ?, ?, 0, 1, NULL, NULL)",
                    (dimension, zero_matrix),
                )
                connection.execute(
                    "INSERT INTO matrices(matrix_id, dimension, matrix, is_strictly_copositive, is_copositive, source, family) "
                    "VALUES (3, ?, ?, 0, 1, NULL, NULL)",
                    (dimension, zero_matrix),
                )
                connection.commit()

            command = [
                sys.executable,
                str(root / "python" / "run_results.py"),
                "dutour_2018",
                "--mode",
                "strictly_copositive",
                "--timeout-seconds",
                "0.1",
                "--dimension-from",
                str(dimension),
                "--dimension-to",
                str(dimension),
                "--parent-cpu",
                str(self.parent_cpu),
                "--cpus",
                str(self.worker_cpu),
                "--preprocessing",
                "none",
                "--database",
                str(database),
            ]
            environment = os.environ.copy()
            environment["PYTHONPATH"] = str(root / "python")
            completed = subprocess.run(command, cwd=root, env=environment, check=True, capture_output=True, text=True)

            with closing(sqlite3.connect(database)) as connection:
                rows = connection.execute(
                    "SELECT matrix_id, status, is_strictly_copositive, elapsed_ns, length(binary_sha256) FROM results ORDER BY matrix_id"
                ).fetchall()
            self.assertEqual(rows[0][:3], (1, "timeout", None))
            self.assertGreater(rows[0][3], 0)
            self.assertEqual(rows[0][4], 64)
            self.assertEqual([row[:3] for row in rows[1:]], [(2, "ok", 0), (3, "ok", 0)])
            self.assertTrue(all(row[3] >= 0 for row in rows[1:]))
            self.assertTrue(all(row[4] == 64 for row in rows[1:]))
            worker_pids = re.findall(r"\bpid=(\d+)", completed.stdout)
            self.assertEqual(len(worker_pids), 3)
            self.assertEqual(len(set(worker_pids)), 1)

            retry_command = command.copy()
            retry_command[retry_command.index("0.1")] = "0.2"
            retry_command.append("--retry-timeouts")
            retried = subprocess.run(retry_command, cwd=root, env=environment, check=True, capture_output=True, text=True)
            with closing(sqlite3.connect(database)) as connection:
                timeout_values = connection.execute("SELECT matrix_id, timeout_ns FROM results ORDER BY matrix_id").fetchall()
            self.assertEqual(timeout_values, [(1, 200_000_000), (2, 100_000_000), (3, 100_000_000)])
            self.assertIn("matrices=1", retried.stdout)

    def test_runner_automatically_stores_last_cbdd_diagnostics_on_timeout(self):
        root = Path(__file__).resolve().parents[2]
        with closing(sqlite3.connect(root / "testdata" / "copos_testdata.sqlite3")) as source:
            dimension, hard_matrix = source.execute(
                "SELECT dimension, matrix FROM matrices WHERE matrix_id = 9630"
            ).fetchone()

        with tempfile.TemporaryDirectory() as temporary_directory:
            database = Path(temporary_directory) / "results.sqlite3"
            with closing(sqlite3.connect(database)) as connection:
                initialize_runner_database(connection, root)
                connection.execute(
                    "INSERT INTO matrices(matrix_id, dimension, matrix, is_strictly_copositive, is_copositive) "
                    "VALUES (1, 2, '1,0,1', 1, 1)"
                )
                connection.execute(
                    "INSERT INTO matrices(matrix_id, dimension, matrix, is_strictly_copositive, is_copositive) "
                    "VALUES (2, ?, ?, 1, 1)",
                    (dimension, hard_matrix),
                )
                connection.commit()

            command = [
                sys.executable,
                str(root / "python" / "run_results.py"),
                "cbdd_dickinson",
                "--timeout-seconds",
                "0.1",
                "--parent-cpu",
                str(self.parent_cpu),
                "--cpus",
                str(self.worker_cpu),
                "--database",
                str(database),
                "--preprocessing",
                "none",
            ]
            environment = os.environ.copy()
            environment["PYTHONPATH"] = str(root / "python")
            subprocess.run(command, cwd=root, env=environment, check=True, capture_output=True, text=True)

            with closing(sqlite3.connect(database)) as connection:
                rows = connection.execute(
                    "SELECT matrix_id, status, elapsed_ns, diagnostics, certificate_joint_distribution "
                    "FROM results ORDER BY matrix_id"
                ).fetchall()
            self.assertEqual(rows[0][:2], (1, "ok"))
            self.assertGreaterEqual(rows[0][2], 0)
            self.assertIn("metric=decision-diagram", rows[0][3])
            self.assertEqual(json.loads(rows[0][4]), [[1, 1, 2, 2]])
            self.assertEqual(rows[1][1], "timeout")
            self.assertGreater(rows[1][2], 0)
            self.assertIn("metric=decision-diagram", rows[1][3])
            self.assertTrue(json.loads(rows[1][4]))


if __name__ == "__main__":
    unittest.main()
