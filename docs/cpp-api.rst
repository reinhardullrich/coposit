C++ API
#######

The public C++ interface is intentionally small: construct an exact integer matrix and call one synchronous function. The function
uses the current incumbent and complete preprocessing pipeline.

Core types
**********

.. doxygenenum:: coposit::copositivity_mode
   :project: coposit

.. doxygenstruct:: coposit::copositivity_result
   :members:
   :project: coposit

.. doxygenclass:: coposit::matrix_integer
   :members:
   :project: coposit

Classifier
**********

.. doxygenfunction:: coposit::check
   :project: coposit

The default mode is ``both``. In a single-property call, the unrequested member of :class:`coposit::copositivity_result` is empty.
An unresolved resource failure is reported as an exception or process-level failure by the surrounding interface; it is never
converted into ``false``.

Minimal native example
**********************

Add ``cpp/`` as a CMake subdirectory and link the incumbent target:

.. code-block:: cmake

   add_subdirectory(path/to/coposit/cpp coposit-build)
   target_link_libraries(my_program PRIVATE coposit::incumbent)

Then construct a symmetric exact matrix:

.. code-block:: cpp

   #include <coposit/coposit.hpp>

   #include <iostream>

   int main()
   {
       coposit::matrix_integer matrix(2, 2);
       matrix(0, 0) = 1;
       matrix(0, 1) = -1;
       matrix(1, 0) = -1;
       matrix(1, 1) = 1;

       const coposit::copositivity_result result = coposit::check(matrix);
       std::cout << std::boolalpha
                 << "copositive=" << *result.is_copositive << '\n'
                 << "strictly_copositive=" << *result.is_strictly_copositive << '\n';
   }

Input boundary
**************

Direct callers must supply a nonempty square symmetric :class:`coposit::matrix_integer`. :func:`coposit::check` validates this public
boundary. Entries are arbitrary-precision integers stored by FLINT.

Rational matrices can be multiplied by one common positive denominator before the call. This preserves copositivity and strict
copositivity because it preserves the sign of :math:`x^\mathsf{T}Ax`. The command-line and Python parsers perform this scaling
automatically for fractions, finite decimals, and scientific notation.
