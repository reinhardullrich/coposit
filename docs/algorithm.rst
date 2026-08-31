Algorithm Overview
##################

coposit converts copositivity into exact minimization over the simplex, then covers the relevant faces with certificates instead of
testing every nonnegative vector. The production algorithm combines exact preprocessing, curvature pruning, Dickinson intervals,
and a fixed-cardinality Boolean support search. Its internal model identifier is deliberately not part of the public interface.

From the orthant to the simplex
*******************************

Let :math:`A\in\mathbb{R}^{n\times n}` be symmetric and let

.. math::

   \Delta_n=\left\{x\in\mathbb{R}^n:x\geq0,\ \mathbf{1}^\mathsf{T}x=1\right\}.

Because :math:`x^\mathsf{T}Ax` is homogeneous of degree two, every nonzero nonnegative vector can be scaled into
:math:`\Delta_n`. Consequently,

.. math::

   A\text{ is copositive}
   \quad\Longleftrightarrow\quad
   \min_{x\in\Delta_n}x^\mathsf{T}Ax\geq0,

and strict copositivity is equivalent to the minimum being strictly positive.

**Intuition.** Scaling removes an irrelevant radial coordinate. What remains is a compact simplex whose faces correspond exactly to
nonempty coordinate supports.

Exact preparation
*****************

The parser clears denominators and creates one arbitrary-precision integer matrix. Multiplication by that common positive
denominator preserves the signs needed for both classifications. The complete preprocessing pipeline then applies exact inexpensive
tests, searches for exact negative witnesses, and splits the graph of negative off-diagonal entries into independent connected
components when possible.

If preprocessing decides a component, the search stops there. Otherwise the production solver receives an exact unresolved principal
matrix. Every final answer and every pruning certificate remains exact.

**Intuition.** Cheap global structure is removed before the exponential support search begins, but no numerical approximation is
allowed to turn into a mathematical decision.

Supports and frontiers
**********************

A nonempty support :math:`I\subseteq\{1,\ldots,n\}` identifies the simplex face on which exactly the coordinates in :math:`I` may
be positive. In the worst case there are :math:`2^n-1` such supports.

The production solver alternates between two fixed-cardinality frontiers:

* the **low frontier** visits small supports and seeks certificates that prune upward to supersets;
* the **high frontier** visits large supports and seeks certificates that prune downward to subsets.

An improved non-blocking-clause (NBC) generator returns one uncovered support of the requested cardinality at a time. Exact
certificates are inserted as Boolean intervals, and redundant certificates are compacted after a cardinality layer is complete.

**Intuition.** Small faces are good starting points for eliminating many larger faces; large faces are good starting points for
eliminating many smaller faces. Alternating tries both opportunities without materializing the Boolean lattice.

Low-frontier processing
***********************

For a low support :math:`I`, the solver forms the Hessian of the quadratic form restricted to the tangent space of its simplex face.
If this reduced Hessian is not positive definite, exact curvature theory excludes every superset of :math:`I` as the first support of
a minimal negative witness. The full upward closure is pruned.

If the face is strictly convex, curvature alone does not give that upward closure. The same exact factorization is then reused to
construct a Dickinson interval :math:`[L,U]`: every support :math:`J` with :math:`L\subseteq J\subseteq U` is covered by one exact
certificate.

The production solver enlarges that interval in three stages:

1. a Halfspace-Rays search chooses a promising positive right-hand side;
2. a targeted continuous linear program proposes one additional upper index at a time;
3. every proposal is reconstructed and accepted only with exact integer arithmetic, after which the lower endpoint is shrunk while
   preserving the exact upper endpoint.

The final interval is selected by width :math:`|U|-|L|`, with larger :math:`|U|` breaking a tie.

**Intuition.** Curvature is the strongest outcome because it removes the complete upward cone. When that is impossible, Dickinson's
certificate still jumps across a contiguous part of the lattice. Numerical optimization merely points to a possibly wider jump;
exact arithmetic decides whether the jump exists.

High-frontier processing
************************

For a high support, a binary64 factorization first screens whether the reduced Hessian could be positive semidefinite. A negative
floating pivot merely skips optional downward pruning and cannot affect completeness. A possible positive-semidefinite result is
recomputed exactly. Only an exact stationary point together with exact positive-semidefinite reduced curvature creates a downward
certificate covering every subset of the support.

The high frontier does not construct Dickinson intervals. A rejected high-frontier candidate remains available to the low frontier,
so the screen cannot hide an exact upward certificate.

**Intuition.** Most large faces do not justify expensive exact downward work. The floating screen avoids that cost, while exact
verification protects every actual deletion from the search space.

Termination and answers
***********************

The search finishes when neither frontier has an uncovered nonempty support. At that point the exact preprocessing results and exact
support certificates cover the complete simplex. During the search:

* an exact negative vector proves that :math:`A` is not copositive;
* an exact nonzero zero proves that :math:`A` is not strictly copositive;
* complete exact coverage proves the requested nonnegative or positive statement.

The ``both`` mode performs one traversal and returns both answers. A timeout or resource limit interrupts the proof and therefore
returns an unresolved status rather than a Boolean classification.

**Intuition.** A negative or zero witness settles the problem locally. A positive conclusion is harder: the solver must account for
every possible support, either directly or through a valid exact certificate.

Sources and complete specification
**********************************

The interval certificate comes from Peter J. C. Dickinson, `A New Certificate for Copositivity
<https://doi.org/10.1016/j.laa.2018.12.025>`_, *Linear Algebra and its Applications* 569 (2019), 15--37. The convex-face connection
uses the standard quadratic-programming geometry developed, for example, by Andrea Scozzari and Fabio Tardella in `A clique
algorithm for standard quadratic programming <https://doi.org/10.1016/j.dam.2007.09.020>`_, *Discrete Applied Mathematics* 156
(2008), 2439--2448.

The `step-by-step Dickinson guide
<https://github.com/reinhardullrich/coposit/blob/main/docs/DICKINSON_ALGORITHM_STEP_BY_STEP.md>`_ develops that certificate from
first principles. Model-specific algorithm documents remain available in the source tree for reproducible research, without making
one experimental identifier part of the public API.
