.. meta::
   :google-site-verification: y-jxZ5iHl68HnoLBfi3KcwCVvWzJZrMyrSujsfR2L5c
   :description: Exact copositivity and strict-copositivity testing for symmetric matrices.

coposit
#######

**Exact copositivity testing for symmetric matrices**

coposit decides whether a real symmetric matrix is copositive, strictly copositive, or both. Every final classification and
certificate is verified with exact arithmetic. It provides standalone command-line programs, a Python package, and a C++ API.

.. image:: ../logo.png
   :alt: coposit logo
   :width: 620px
   :align: center

.. container:: project-actions

   * `View coposit on GitHub <https://github.com/reinhardullrich/coposit>`_
   * `Install pycoposit from PyPI <https://pypi.org/project/pycoposit/>`_

Mathematical problem
********************

Let :math:`A\in\mathbb{R}^{n\times n}` be symmetric. The two properties are:

.. list-table::
   :header-rows: 1
   :widths: 22 78

   * - Property
     - Definition
   * - Copositive (CP)
     - :math:`x^\mathsf{T}Ax\geq 0` for every componentwise nonnegative vector :math:`x`.
   * - Strictly copositive (SCP)
     - :math:`x^\mathsf{T}Ax>0` for every nonzero componentwise nonnegative vector :math:`x`.

Strict copositivity is stronger. A matrix can therefore be copositive without being strictly copositive.

Numerical contract
******************

Integer, fractional, finite-decimal, and scientific-notation inputs are converted to one exact integer-scaled matrix. Floating-point
work may nominate optional pruning opportunities, but it never establishes a classification or certificate. A timeout or resource
limit remains unresolved; it is never reported as ``false``.

The public interfaces select the current production solver internally. Its identity is not part of the public contract and may
change between releases. The repository also retains literature baselines and experimental models for reproducible research.

Quick start
***********

Install the Python package:

.. code-block:: console

   python -m pip install pycoposit

Then check a matrix:

.. code-block:: python

   from pycoposit import check

   result = check("2#1,-1,1")
   print(result["is_copositive"])           # True
   print(result["is_strictly_copositive"])  # False

The compact input stores the upper triangle of

.. math::

   A=\begin{pmatrix}1&-1\\-1&1\end{pmatrix}.

Prebuilt command-line packages are available from the `GitHub releases page
<https://github.com/reinhardullrich/coposit/releases>`_. Continue with :doc:`getting-started` for installation, matrix formats,
result interpretation, timeouts, and batch execution.

Documentation
*************

.. toctree::
   :maxdepth: 1
   :caption: Documentation

   getting-started
   algorithm
   python-api
   cpp-api

Project links
*************

* `Source code <https://github.com/reinhardullrich/coposit>`_
* `Standalone releases <https://github.com/reinhardullrich/coposit/releases>`_
* `Python package <https://pypi.org/project/pycoposit/>`_
* `Issue tracker <https://github.com/reinhardullrich/coposit/issues>`_
* `GPL-3.0-or-later license <https://github.com/reinhardullrich/coposit/blob/main/LICENSE>`_
