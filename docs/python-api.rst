Python API
##########

The ``pycoposit`` package launches the same exact command-line implementation used by standalone releases. Inputs are small
dataclasses and every result is an ordinary dictionary.

Public and research interfaces
******************************

Use :func:`pycoposit.check` in an application. It selects the current incumbent and the complete preprocessing pipeline. The other
execution functions require an explicit model name and are intended for controlled research comparisons.

.. autofunction:: pycoposit.check

Result dictionary
*****************

Every execution result contains these fields:

.. list-table::
   :header-rows: 1
   :widths: 34 66

   * - Field
     - Meaning
   * - ``algorithm``
     - Model identifier used for the run.
   * - ``mode``
     - Requested classification: ``copositive``, ``strictly_copositive``, or ``both``.
   * - ``preprocessing``
     - ``both`` for the complete preprocessing pipeline or ``none`` for an explicit research run.
   * - ``model_parameter``
     - Model-specific research parameter, otherwise ``None``.
   * - ``matrix_id``
     - Optional signed 64-bit identifier copied from :class:`pycoposit.Matrix`.
   * - ``status``
     - Integer :class:`pycoposit.StatusCode`. Only ``0`` means a completed classification.
   * - ``is_copositive``
     - Exact Boolean answer when requested and completed; otherwise ``None``.
   * - ``is_strictly_copositive``
     - Exact Boolean answer when requested and completed; otherwise ``None``.
   * - ``elapsed_ns``
     - Native execution duration in nanoseconds measured with a monotonic clock.
   * - ``error_message``
     - Empty on success; otherwise a parser, execution, worker, or resource diagnostic.
   * - ``diagnostics``
     - Captured textual diagnostics when requested.
   * - ``certificate_joint_distribution``
     - Model-specific certificate counters used for analysis and visualization.

Core types
**********

.. autoclass:: pycoposit.StatusCode
   :members:

.. autoclass:: pycoposit.Matrix
   :members:

.. autoclass:: pycoposit.MPConfig
   :members:

Sequential research execution
*****************************

``compute_matrix()`` is the single-matrix primitive. ``run()`` returns one dictionary for one :class:`pycoposit.Matrix`, or a lazy
input-ordered iterator for an iterable.

.. autofunction:: pycoposit.compute_matrix

.. autofunction:: pycoposit.run

Leaving ``mode`` unset selects ``both`` only for models that implement combined classification in one traversal. A model that does
not support that contract requires an explicit single-property mode. ``preprocessing="none"`` is for algorithm experiments; normal
use should keep ``preprocessing="both"``.

Multiprocessing
***************

.. autofunction:: pycoposit.run_multiprocessing

One worker analyzes one matrix at a time, and results are yielded in completion order. Submission is bounded by
``min(queue_maxsize, workers * prefetch_per_worker)`` so a lazy input iterable does not become an unbounded in-memory queue. Closing
an unfinished iterator terminates its workers. Scripts using the default ``spawn`` method must protect their entry point:

.. code-block:: python

   if __name__ == "__main__":
       main()

Input contract
**************

:class:`pycoposit.Matrix` stores compact matrix text, inline Matrix Market text, or a file path. Compact and Matrix Market formats
are described in :doc:`getting-started`. ``matrix_id`` is correlation metadata only and does not affect the calculation.

The package locates ``coposit`` beside itself, in the standard source-build directories, or on ``PATH``. Set the ``COPOSIT``
environment variable to an explicit launcher path when embedding a custom research build.
