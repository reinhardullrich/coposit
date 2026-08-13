"""Run pycoposit matrices in processes with bounded shared queues."""

from __future__ import annotations

from collections.abc import Iterable, Iterator
import multiprocessing as mp
from multiprocessing.reduction import ForkingPickler
import pickle
from queue import Empty
import time

from .core import compute_matrix
from .types import (
    Algorithm,
    CopositivityMode,
    MPConfig,
    Matrix,
    Preprocessing,
    StatusCode,
    _validate_algorithm,
    _validate_mode,
    _validate_preprocessing,
)

_SENTINEL = None


def _safe_compute(algorithm: Algorithm, matrix: Matrix, mode: CopositivityMode, preprocessing: Preprocessing) -> dict:
    try:
        return compute_matrix(algorithm, matrix, mode, preprocessing)
    except Exception as error:  # defensive: keep one bad job from breaking the worker protocol
        return {
            "algorithm": algorithm,
            "mode": mode,
            "preprocessing": preprocessing,
            "matrix_id": matrix.matrix_id if matrix.matrix_id is None or type(matrix.matrix_id) is int else None,
            "status": int(StatusCode.INTERNAL_ERROR),
            "is_copositive": None,
            "is_strictly_copositive": None,
            "elapsed_ns": 0,
            "error_message": f"Worker exception: {error}",
        }


def _queue_worker(input_queue, output_queue, algorithm: Algorithm, mode: CopositivityMode, preprocessing: Preprocessing) -> None:
    while True:
        payload = input_queue.get()
        if payload is _SENTINEL:
            return
        matrix = pickle.loads(payload)
        output_queue.put(bytes(ForkingPickler.dumps(_safe_compute(algorithm, matrix, mode, preprocessing))))


def _max_pending_matrices(config: MPConfig) -> int:
    return max(1, min(config.queue_maxsize, config.workers * config.prefetch_per_worker))


class _QueueRunner:
    def __init__(self, algorithm: Algorithm, mode: CopositivityMode, preprocessing: Preprocessing, config: MPConfig):
        self.algorithm = algorithm
        self.config = config
        self._context = mp.get_context(config.start_method)
        self._input_queue = self._context.Queue()
        self._output_queue = self._context.Queue(maxsize=config.queue_maxsize)
        self._workers: list[mp.Process] = []
        self._input_closed = False

        try:
            for worker_index in range(config.workers):
                process = self._context.Process(
                    target=_queue_worker,
                    name=f"coposit-worker-{worker_index}",
                    args=(self._input_queue, self._output_queue, algorithm, mode, preprocessing),
                    daemon=True,
                )
                process.start()
                self._workers.append(process)
        except BaseException:
            self.shutdown(cancel=True)
            raise

    def submit(self, matrix: Matrix) -> None:
        if self._input_closed:
            raise RuntimeError("Cannot submit after close_input()")
        self._input_queue.put(bytes(ForkingPickler.dumps(matrix)))

    def close_input(self) -> None:
        if self._input_closed:
            return
        for _ in self._workers:
            self._input_queue.put(_SENTINEL)
        self._input_closed = True

    def get_result(self) -> dict:
        while True:
            try:
                return pickle.loads(self._output_queue.get(timeout=0.1))
            except Empty:
                failed = [process for process in self._workers if process.exitcode not in (None, 0)]
                if failed:
                    details = ", ".join(f"{process.name}={process.exitcode}" for process in failed)
                    raise RuntimeError(f"coposit worker exited without a result: {details}")
                if all(process.exitcode is not None for process in self._workers):
                    raise RuntimeError("All coposit workers exited before producing the expected result")

    def shutdown(self, *, cancel: bool, join_timeout_s: float = 5.0) -> None:
        if cancel:
            self._input_queue.cancel_join_thread()
            for process in self._workers:
                if process.is_alive():
                    process.terminate()
        elif not self._input_closed:
            self.close_input()

        deadline = time.monotonic() + join_timeout_s
        for process in self._workers:
            process.join(timeout=max(0.0, deadline - time.monotonic()))
            if process.is_alive():
                process.terminate()
                process.join(timeout=1.0)

        self._input_queue.close()
        self._output_queue.close()


def _run_matrices_multiprocessing(
    algorithm: Algorithm,
    matrices: Iterable[Matrix],
    mode: CopositivityMode,
    preprocessing: Preprocessing,
    config: MPConfig,
) -> Iterator[dict]:
    runner = _QueueRunner(algorithm, mode, preprocessing, config)
    completed = False
    try:
        matrices_iter = iter(matrices)
        max_pending = _max_pending_matrices(config)
        submitted = 0
        yielded = 0
        input_exhausted = False

        def submit_until_window() -> None:
            nonlocal submitted, input_exhausted
            while not input_exhausted and submitted - yielded < max_pending:
                try:
                    matrix = next(matrices_iter)
                except StopIteration:
                    input_exhausted = True
                    runner.close_input()
                    return
                runner.submit(matrix)
                submitted += 1

        submit_until_window()
        while yielded < submitted or not input_exhausted:
            if yielded >= submitted:
                submit_until_window()
                if yielded >= submitted and input_exhausted:
                    break
            yield runner.get_result()
            yielded += 1
            submit_until_window()
        completed = True
    finally:
        runner.shutdown(cancel=not completed)


def run_multiprocessing(
    algorithm: Algorithm,
    matrices: Matrix | Iterable[Matrix],
    mp_config: MPConfig | None = None,
    mode: CopositivityMode = "strictly_copositive",
    preprocessing: Preprocessing = "none",
) -> dict | Iterator[dict]:
    """Run one matrix or an iterable across worker processes in completion order."""

    _validate_algorithm(algorithm)
    _validate_mode(mode)
    _validate_preprocessing(preprocessing)
    config = mp_config if mp_config is not None else MPConfig()
    is_single = isinstance(matrices, Matrix)
    results = _run_matrices_multiprocessing(algorithm, (matrices,) if is_single else matrices, mode, preprocessing, config)
    if is_single:
        return list(results)[0]
    return results
