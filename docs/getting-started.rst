Getting Started
###############

coposit accepts exact symmetric matrices through the command line, Python, or C++. Ordinary users do not select an algorithm: all
three public interfaces use the current incumbent and the complete exact preprocessing pipeline.

Install
*******

Python
------

Python 3.11 through 3.14 users can install the package from PyPI:

.. code-block:: console

   python -m pip install pycoposit

Command line
------------

Download the package for Linux, macOS, or Windows from the `release page
<https://github.com/reinhardullrich/coposit/releases>`_ and extract it. Keep the public launcher and its incumbent companion in the
same directory. On Linux and macOS, make both executable once:

.. code-block:: console

   chmod +x coposit coposit-improved_nbc_x6

Windows examples use ``coposit.exe`` instead of ``./coposit``.

Build from source
-----------------

A source build requires CMake 3.18 or newer, C and C++17 compilers, Python 3.11 or newer, FLINT, MPFR, and GMP. This minimal build
creates the public incumbent and command line without the retained research experiments:

.. code-block:: console

   git clone https://github.com/reinhardullrich/coposit.git
   cd coposit
   cmake -S cpp -B cpp/build-release -DCMAKE_BUILD_TYPE=Release \
     -DCOPOSIT_BUILD_EXPERIMENTS=OFF \
     -DCOPOSIT_BUILD_TESTS=OFF \
     -DCOPOSIT_BUILD_PYTHON=OFF
   cmake --build cpp/build-release --parallel

Encode a matrix
***************

Compact upper-triangular form
-----------------------------

The compact form is ``dimension#values``. Write the upper triangle row by row. For

.. math::

   A=\begin{pmatrix}
   a_{11}&a_{12}&a_{13}\\
   a_{12}&a_{22}&a_{23}\\
   a_{13}&a_{23}&a_{33}
   \end{pmatrix},

the input is:

.. code-block:: text

   3#a11,a12,a13,a22,a23,a33

Exactly :math:`n(n+1)/2` values are required. Values may be integers, fractions, finite decimals, or scientific notation, such as
``-3/5``, ``-0.6``, and ``-6e-1``. All three are converted exactly. A fraction's denominator must be positive, so write ``-1/2``
rather than ``1/-2``.

Compact circular-symmetric form
-------------------------------

A shorter form is available when the matrix is unchanged by a simultaneous cyclic shift of its row and column indices. Supply the
common diagonal followed by one value for each positive circular distance:

.. code-block:: text

   n#c0,c1,c2,...,c_floor(n/2)

For example, ``5#2,3,5`` expands to a 5-by-5 matrix whose first row is ``2,3,5,5,3``. The shorter value count selects this form
automatically.

Matrix Market
-------------

The command line also accepts symmetric Matrix Market ``array`` and ``coordinate`` files. Integer, real, pattern, and complex fields
are supported; complex entries must have zero imaginary part. Decimal and scientific values remain exact.

Run one matrix from the command line
************************************

Run both classifications:

.. code-block:: console

   ./coposit '2#1,-1,1'

The result is:

.. code-block:: text

   copositive=true
   strictly_copositive=false

To ask only one question, use ``--mode non-strict`` or ``--mode strict``:

.. code-block:: console

   ./coposit --mode non-strict '2#1,-1,1'
   ./coposit --mode strict '2#1,-1,1'

The command also accepts a file path or standard input:

.. code-block:: console

   ./coposit matrix.mtx
   ./coposit -

Timeouts and diagnostics
------------------------

Set a wall-clock limit in seconds with ``--timeout``:

.. code-block:: console

   ./coposit --timeout 30 matrix.mtx

A timeout is unresolved. The command emits no Boolean answer and exits with status ``124``. Add ``--diagnostics`` to print the
current phase and work counters to standard error about once per second:

.. code-block:: console

   ./coposit --diagnostics --timeout 30 matrix.mtx

Diagnostics describe completed work; they are not an estimate of the remaining time.

Run one matrix from Python
**************************

``check()`` accepts compact text, inline Matrix Market text, or a file path. It checks both properties by default:

.. code-block:: python

   from pycoposit import check

   result = check("2#1,-1,1")
   if result["status"] == 0:
       print(result["is_copositive"])
       print(result["is_strictly_copositive"])
   else:
       print(result["error_message"])

Request one property with ``mode="copositive"`` or ``mode="strictly_copositive"``. The unrequested result is ``None``.

Run several matrices
********************

The explicit-model research interface can process matrices sequentially or across worker processes. Results from
``run_multiprocessing()`` arrive in completion order, so attach ``matrix_id`` values rather than relying on list position:

.. code-block:: python

   from pycoposit import MPConfig, Matrix, run_multiprocessing

   matrices = [
       Matrix("2#1,-1,1", matrix_id=1),
       Matrix("2#1,0,1", matrix_id=2),
   ]
   results = run_multiprocessing(
       "improved_nbc_x6",
       matrices,
       mp_config=MPConfig(workers=2),
       mode="both",
   )
   for result in results:
       print(result["matrix_id"], result["is_copositive"], result["is_strictly_copositive"])

See :doc:`python-api` for the complete wrapper contract. Research builds can select retained models explicitly; ordinary
applications should use ``check()`` and leave exact preprocessing enabled.

Interpret failures
******************

Always inspect ``status`` before consuming a Python result. Invalid input, an execution failure, a timeout, a node limit, or an
internal failure leaves the requested classifications as ``None``. None of these conditions means that the matrix is not
copositive.

There is no fixed mathematical dimension limit in the maintained representation. The search space can nevertheless grow
exponentially, so a valid high-dimensional input may require impractical time or memory.
