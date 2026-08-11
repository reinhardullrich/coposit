from contextlib import closing
import os
from pathlib import Path
import re
import signal
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
    run,
    run_multiprocessing,
)
from pycoposit.core import load_native_module
from pycoposit.mp import _max_pending_matrices
from pycoposit.types import _validate_algorithm, _validate_mode, _validate_preprocessing


ORDINARY_BASELINES = (
    "dutour_2018",
    "danninger_1990",
    "copomatrix_2011",
    "hadeler_1983",
    "dickinson_2019",
    "safi_2021",
    "bundfuss_2008",
    "sponsel_2012",
)


class WrapperTests(unittest.TestCase):
    def test_schema_keeps_strict_and_ordinary_truth_consistent(self):
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

    def test_public_validation(self):
        self.assertIn("hadeler_1983", ALGORITHMS)
        self.assertEqual(COPOSITIVITY_MODES, ("copositive", "strictly_copositive", "both"))
        self.assertEqual(PREPROCESSING_MODES, ("none", "connected_components", "pre_checks", "both"))
        self.assertEqual(COMBINED_CLASSIFICATION_ALGORITHMS, ("danninger_1990", "hadeler_1983", "dickinson_2019"))
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
            Matrix(1 << 63, "1#1")
        with self.assertRaises(ValueError):
            MPConfig(workers=0)
        self.assertEqual(_max_pending_matrices(MPConfig(workers=2, prefetch_per_worker=3, queue_maxsize=4)), 4)

    def test_matrix_market_file_reference(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            matrix_directory = Path(temporary_directory) / "matrices"
            matrix_directory.mkdir()
            (matrix_directory / "7.mtx").write_text(
                "%%MatrixMarket matrix array integer symmetric\n% exact 2x2 example\n2 2\n1\n-2\n1\n",
                encoding="ascii",
            )
            matrix = Matrix(
                7,
                "file:matrices/7.mtx",
                {"dimension": 2, "base_directory": temporary_directory},
            )
            result = run("hadeler_1983", matrix, "copositive")
            self.assertEqual(result["status"], StatusCode.OK)
            self.assertIs(result["is_copositive"], False)

    def test_every_native_model(self):
        for algorithm in ALGORITHMS:
            with self.subTest(algorithm=algorithm):
                positive = run(algorithm, Matrix(1, "2#1,0,1"))
                boundary = run(algorithm, Matrix(2, "1#0"))
                self.assertEqual(positive["status"], StatusCode.OK)
                self.assertEqual(positive["mode"], "strictly_copositive")
                self.assertIsNone(positive["is_copositive"])
                self.assertIs(positive["is_strictly_copositive"], True)
                self.assertIs(boundary["is_strictly_copositive"], False)
                native_module = load_native_module(algorithm)
                self.assertEqual(native_module.STATUS_NODE_LIMIT, StatusCode.NODE_LIMIT)

    def test_every_baseline_decides_ordinary_copositivity(self):
        for algorithm in ORDINARY_BASELINES:
            with self.subTest(algorithm=algorithm):
                boundary = run(algorithm, Matrix(1, "2#1,-1,1"), "copositive")
                negative = run(algorithm, Matrix(2, "2#1,-2,1"), "copositive")
                self.assertEqual(boundary["status"], StatusCode.OK)
                self.assertEqual(boundary["mode"], "copositive")
                self.assertIs(boundary["is_copositive"], True)
                self.assertIsNone(boundary["is_strictly_copositive"])
                self.assertEqual(negative["status"], StatusCode.OK)
                self.assertIs(negative["is_copositive"], False)

    def test_created_models_reject_ordinary_mode(self):
        result = run("fracessa", Matrix(1, "1#1"), "copositive")
        self.assertEqual(result["status"], StatusCode.EXEC_ERROR)
        self.assertIsNone(result["is_copositive"])
        self.assertIn("supports only strict copositivity", result["error_message"])

    def test_adaptive_sponsel_copomatrix_decides_ordinary_copositivity(self):
        boundary = run("adaptive_sponsel_copomatrix", Matrix(1, "2#1,-1,1"), "copositive")
        negative = run("adaptive_sponsel_copomatrix", Matrix(2, "2#1,-2,1"), "copositive")
        self.assertEqual(boundary["status"], StatusCode.OK)
        self.assertIs(boundary["is_copositive"], True)
        self.assertIsNone(boundary["is_strictly_copositive"])
        self.assertEqual(negative["status"], StatusCode.OK)
        self.assertIs(negative["is_copositive"], False)

    def test_preprocessing_modes_preserve_exact_classification(self):
        matrix = Matrix(1, "4#1,-1,0,0,1,0,0,1,-1,1")
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
                    result = run(algorithm, Matrix(matrix_id, matrix), "both")
                    self.assertEqual(result["status"], StatusCode.OK)
                    self.assertEqual(result["mode"], "both")
                    self.assertIs(result["is_copositive"], expected_copositive)
                    self.assertIs(result["is_strictly_copositive"], expected_strict)

    def test_unsupported_model_rejects_combined_classification(self):
        result = run("dutour_2018", Matrix(1, "1#1"), "both")
        self.assertEqual(result["status"], StatusCode.EXEC_ERROR)
        self.assertIsNone(result["is_copositive"])
        self.assertIsNone(result["is_strictly_copositive"])
        self.assertIn("does not support combined classification", result["error_message"])

    def test_dutour_open_node_limit_is_unresolved(self):
        root = Path(__file__).resolve().parents[2]
        with closing(sqlite3.connect(root / "testdata" / "Copos_testdata.sqlite3")) as connection:
            dimension, values = connection.execute(
                "SELECT dimension, matrix FROM matrices WHERE matrix_id = 9660"
            ).fetchone()

        result = run("dutour_2018", Matrix(9660, f"{dimension}#{values}"))
        self.assertEqual(result["status"], StatusCode.NODE_LIMIT)
        self.assertIsNone(result["is_strictly_copositive"])
        self.assertIn("50000 open nodes", result["error_message"])

    def test_sequential_iterable_and_parser_status(self):
        results = list(run("hadeler_1983", [Matrix(4, "1", {"dimension": 1}), Matrix(5, "1#bad")]))
        self.assertEqual([result["matrix_id"] for result in results], [4, 5])
        self.assertEqual(results[0]["status"], StatusCode.OK)
        self.assertEqual(results[1]["status"], StatusCode.PARSE_ERROR)
        self.assertIsNone(results[1]["is_strictly_copositive"])

    def test_multiprocessing_hadeler(self):
        matrices = [Matrix(10, "2#1,0,1"), Matrix(11, "2#1,-2,1")]
        results = list(run_multiprocessing("hadeler_1983", matrices, MPConfig(workers=2)))
        self.assertCountEqual([result["matrix_id"] for result in results], [10, 11])
        self.assertTrue(all(result["status"] == StatusCode.OK for result in results))
        self.assertCountEqual([result["is_strictly_copositive"] for result in results], [True, False])

    def test_multiprocessing_ordinary_hadeler(self):
        matrices = [Matrix(12, "2#1,-1,1"), Matrix(13, "2#1,-2,1")]
        results = list(run_multiprocessing("hadeler_1983", matrices, MPConfig(workers=2), "copositive"))
        self.assertCountEqual([result["matrix_id"] for result in results], [12, 13])
        self.assertCountEqual([result["is_copositive"] for result in results], [True, False])

    def test_multiprocessing_combined_hadeler(self):
        matrices = [Matrix(14, "2#1,-1,1"), Matrix(15, "2#1,-2,1")]
        results = list(run_multiprocessing("hadeler_1983", matrices, MPConfig(workers=2), "both"))
        self.assertCountEqual(
            [(result["is_copositive"], result["is_strictly_copositive"]) for result in results],
            [(True, False), (False, False)],
        )

    @unittest.skipUnless(hasattr(signal, "SIGUSR1"), "native timeout signalling requires POSIX SIGUSR1")
    def test_every_native_model_observes_the_timeout_signal(self):
        for algorithm in ALGORITHMS:
            with self.subTest(algorithm=algorithm):
                native_module = load_native_module(algorithm)
                native_module._install_timeout_handler(signal.SIGUSR1)
                native_module._reset_timeout()
                os.kill(os.getpid(), signal.SIGUSR1)
                result = native_module.compute_matrix("2#1,0,1")
                self.assertEqual(result["status"], StatusCode.TIMEOUT)
                self.assertIsNone(result["is_strictly_copositive"])


@unittest.skipUnless(hasattr(os, "sched_getaffinity"), "CPU affinity requires Linux")
class ResultsRunnerTests(unittest.TestCase):
    def setUp(self):
        available_cpus = sorted(os.sched_getaffinity(0))
        if len(available_cpus) < 2:
            self.skipTest("results runner needs distinct parent and worker CPUs")
        self.parent_cpu, self.worker_cpu = available_cpus[:2]

    def test_large_runner_throttles_progress_output(self):
        root = Path(__file__).resolve().parents[2]
        with tempfile.TemporaryDirectory() as temporary_directory:
            database = Path(temporary_directory) / "results.sqlite3"
            with closing(sqlite3.connect(database)) as connection:
                connection.executescript((root / "testdata" / "schema.sql").read_text())
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

            progress_lines = re.findall(r"^\[\d+/101\].*$", completed.stdout, flags=re.MULTILINE)
            self.assertLess(len(progress_lines), 101)
            self.assertTrue(progress_lines[-1].startswith("[101/101]"))
            with closing(sqlite3.connect(database)) as connection:
                self.assertEqual(connection.execute("SELECT COUNT(*) FROM results").fetchone()[0], 101)

    def test_runner_selects_the_union_of_matrix_set_flags(self):
        root = Path(__file__).resolve().parents[2]
        with tempfile.TemporaryDirectory() as temporary_directory:
            database = Path(temporary_directory) / "results.sqlite3"
            with closing(sqlite3.connect(database)) as connection:
                connection.executescript((root / "testdata" / "schema.sql").read_text())
                connection.executemany(
                    "INSERT INTO matrices(matrix_id, dimension, matrix, is_strictly_copositive, is_copositive, "
                    "representative_core, stress_test) VALUES (?, 1, '1', 1, 1, ?, ?)",
                    ((1, 1, 0), (2, 0, 1), (3, 0, 0)),
                )
                connection.commit()

            command = [
                sys.executable,
                str(root / "python" / "run_results.py"),
                "hadeler_1983",
                "--timeout-seconds",
                "5",
                "--matrix-set",
                "representative_core",
                "stress_test",
                "--parent-cpu",
                str(self.parent_cpu),
                "--cpus",
                str(self.worker_cpu),
                "--database",
                str(database),
                "--preprocessing",
                "both",
            ]
            environment = os.environ.copy()
            environment["PYTHONPATH"] = str(root / "python")
            completed = subprocess.run(command, cwd=root, env=environment, check=True, capture_output=True, text=True)

            with closing(sqlite3.connect(database)) as connection:
                rows = connection.execute("SELECT matrix_id, parameters FROM results ORDER BY matrix_id").fetchall()
            self.assertEqual(rows, [(1, "preprocessing=both"), (2, "preprocessing=both")])
            self.assertIn("matrix_sets=representative_core,stress_test", completed.stdout)
            self.assertIn("preprocessing=both", completed.stdout)

    def test_runner_stores_and_resumes_one_hadeler_baseline_result(self):
        root = Path(__file__).resolve().parents[2]
        with tempfile.TemporaryDirectory() as temporary_directory:
            database = Path(temporary_directory) / "results.sqlite3"
            with closing(sqlite3.connect(database)) as connection:
                connection.executescript((root / "testdata" / "schema.sql").read_text())
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
                "--parameters",
                "baseline defaults",
            ]
            environment = os.environ.copy()
            environment["PYTHONPATH"] = str(root / "python")
            first = subprocess.run(command, cwd=root, env=environment, check=True, capture_output=True, text=True)
            second = subprocess.run(command, cwd=root, env=environment, check=True, capture_output=True, text=True)
            alternative_command = command.copy()
            alternative_command[-1] = "alternate parameters"
            alternative_command.extend(("--matrix-id-from", "2", "--matrix-id-to", "2"))
            third = subprocess.run(alternative_command, cwd=root, env=environment, check=True, capture_output=True, text=True)
            ordinary_command = command.copy()
            ordinary_command[3:3] = ("--mode", "copositive")
            ordinary = subprocess.run(ordinary_command, cwd=root, env=environment, check=True, capture_output=True, text=True)

            with closing(sqlite3.connect(database)) as connection:
                rows = connection.execute(
                    """SELECT model_id, mode, parameters, length(binary_sha256), status, is_copositive,
                              is_strictly_copositive, elapsed_ns, timeout_ns, message
                       FROM results ORDER BY mode, matrix_id, parameters"""
                ).fetchall()
                with self.assertRaises(sqlite3.IntegrityError):
                    connection.execute(
                        """INSERT INTO results (
                               matrix_id, model_id, mode, parameters, binary_sha256, status, is_copositive,
                               is_strictly_copositive, elapsed_ns, timeout_ns, recorded_at, message
                           ) VALUES (1, 'hadeler_1983', 'strictly_copositive', 'invalid hash', ?,
                                     'timeout', NULL, NULL, NULL, 1, 'now', NULL)""",
                        ("0" * 64,),
                    )
            self.assertEqual(len(rows), 5)
            self.assertTrue(all(row[0] == "hadeler_1983" and row[3:5] == (0, "ok") for row in rows))
            self.assertEqual({row[2] for row in rows}, {"baseline defaults", "alternate parameters"})
            ordinary_rows = [row for row in rows if row[1] == "copositive"]
            strict_rows = [row for row in rows if row[1] == "strictly_copositive"]
            self.assertEqual([(row[5], row[6]) for row in ordinary_rows], [(1, None), (0, None)])
            self.assertEqual([(row[5], row[6]) for row in strict_rows], [(None, 1), (None, 0), (None, 0)])
            self.assertTrue(all(row[7] >= 0 for row in rows))
            self.assertTrue(all(row[8:] == (1_000_000_000_000_000, None) for row in rows))
            worker_pids = re.findall(r"\bpid=(\d+)", first.stdout)
            self.assertEqual(len(worker_pids), 2)
            self.assertEqual(len(set(worker_pids)), 1)
            self.assertIn("matrices=0", second.stdout)
            self.assertIn("matrices=1", third.stdout)
            self.assertIn("matrices=2", ordinary.stdout)

    def test_runner_reuses_worker_after_cooperative_timeout(self):
        root = Path(__file__).resolve().parents[2]
        with closing(sqlite3.connect(root / "testdata" / "Copos_testdata.sqlite3")) as source:
            dimension, hard_matrix, expected, expected_copositive = source.execute(
                "SELECT dimension, matrix, is_strictly_copositive, is_copositive FROM matrices WHERE matrix_id = 9656"
            ).fetchone()

        with tempfile.TemporaryDirectory() as temporary_directory:
            database = Path(temporary_directory) / "results.sqlite3"
            with closing(sqlite3.connect(database)) as connection:
                connection.executescript((root / "testdata" / "schema.sql").read_text())
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
                "--timeout-seconds",
                "0.01",
                "--dimension-from",
                str(dimension),
                "--dimension-to",
                str(dimension),
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
                    "SELECT matrix_id, status, is_strictly_copositive, elapsed_ns, length(binary_sha256) FROM results ORDER BY matrix_id"
                ).fetchall()
            self.assertEqual(rows[0], (1, "timeout", None, None, 64))
            self.assertEqual([row[:3] for row in rows[1:]], [(2, "ok", 0), (3, "ok", 0)])
            self.assertTrue(all(row[3] >= 0 for row in rows[1:]))
            self.assertTrue(all(row[4] == 64 for row in rows[1:]))
            worker_pids = re.findall(r"\bpid=(\d+)", completed.stdout)
            self.assertEqual(len(worker_pids), 3)
            self.assertEqual(len(set(worker_pids)), 1)

            retry_command = command.copy()
            retry_command[retry_command.index("0.01")] = "0.02"
            retry_command.append("--retry-timeouts")
            retried = subprocess.run(retry_command, cwd=root, env=environment, check=True, capture_output=True, text=True)
            with closing(sqlite3.connect(database)) as connection:
                timeout_values = connection.execute("SELECT matrix_id, timeout_ns FROM results ORDER BY matrix_id").fetchall()
            self.assertEqual(timeout_values, [(1, 20_000_000), (2, 10_000_000), (3, 10_000_000)])
            self.assertIn("matrices=1", retried.stdout)


if __name__ == "__main__":
    unittest.main()
