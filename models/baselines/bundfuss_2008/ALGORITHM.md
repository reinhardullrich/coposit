# Bundfuss 2008

Classification: exact non-strict implementation and strict adaptation of a published simplicial-partition method, with branching pinned to
Salmerón's 2018 source.

## Decision Modes

Both modes keep the same minimum negative edge, three exact $\lambda$ candidates, rational edge point, two children, evaluation
order, and LIFO work list. Non-strict mode rejects $g_{ii}<0$ and $g_{ij}^2>g_{ii}g_{jj}$; strict mode rejects
$g_{ii}\leq0$ and $g_{ij}^2\geq g_{ii}g_{jj}$. Thus equality is a valid boundary in non-strict mode and a counterexample in strict
mode. Entrywise-nonnegative Gram matrices are accepted after the corresponding diagonal check.

`solve(A, mode)` defaults to `strictly_copositive` and returns one Boolean predicate. The non-strict mode restores the paper's
non-strict decision rules, but the concrete same-dimensional refinement can still approach an interior boundary zero indefinitely;
timeout and node-limit outcomes remain unresolved rather than `false`.

## What The Algorithm Does

The algorithm checks the quadratic form on the standard simplex, which is a bounded cross-section of the nonnegative orthant. It
keeps a list of simplices. A simplex is finished when the quadratic interactions between all its vertices are nonnegative. When an
edge is still troublesome but does not yet prove failure, the algorithm inserts one carefully chosen point on that edge and splits
the simplex into two children.

In plain terms, it repeatedly zooms in near negative-looking edges. It returns `true` only after every generated simplex has been
certified, and it returns `false` as soon as a vertex or edge supplies a nonpositive direction.

## Name And Sources

The identifier follows coposit's `<first-author>_<year>` rule. `bundfuss` names Stefan Bundfuss, the paper's first author, and
`2008` is its publication year.

Primary mathematical reference:

- Stefan Bundfuss and Mirjam Dür, “Algorithmic Copositivity Detection by Simplicial Partition,” *Linear Algebra and its
  Applications* 428(7), 1511–1523 (2008), [DOI 10.1016/j.laa.2007.09.035](https://doi.org/10.1016/j.laa.2007.09.035).

The paper specifies the simplicial-partition method family. This model's concrete formulas and control flow are pinned to J. M. G.
Salmerón's `bundfuss` implementation in
[`copositivity-detection-bundfuss-faces`](https://github.com/josmangarsal/copositivity-detection-bundfuss-faces), introduced in
[commit `5537fd94768efbce85b3225b05bf39db8d81a332`](https://github.com/josmangarsal/copositivity-detection-bundfuss-faces/commit/5537fd94768efbce85b3225b05bf39db8d81a332)
in 2018. The upstream repository contains no captured license, so coposit does not copy its source into this maintained model. The
independently written exact experiment is retained under
[`experiments/copositivity_bundfuss_flint_2026-08-07/flint/`](../../../experiments/copositivity_bundfuss_flint_2026-08-07/flint/).

## Problem And Node Representation

Because $x^TAx$ is homogeneous, non-strict or strict copositivity on the nonnegative orthant is equivalent to nonnegativity or
positivity on the standard simplex. A node is a simplex with vertices $v_1,\ldots,v_n$ and Gram matrix

\[
G_{ij}=v_i^TAv_j.
\]

The initial simplex has the coordinate vectors as vertices, so its Gram matrix is $A$. The implementation stores only a positive
integer multiple of the generally rational Gram matrix. Positive scaling preserves all signs, comparisons, and split parameters.

## Decision Flow

### 1. Check the simplex vertices

In strict mode, a diagonal satisfying

\[
g_{ii}\leq0,
\]

then $v_i$ is a nonzero nonnegative vector with nonpositive quadratic value. The matrix is not strictly copositive.
Non-strict mode rejects only $g_{ii}<0$.

### 2. Accept a nonnegative Gram matrix

If every off-diagonal entry is nonnegative, then $G\geq0$ entrywise. Together with the already verified positive diagonal, this
certifies $x^TAx>0$ throughout the simplex. The node needs no subdivision.

### 3. Select and inspect the split edge

Otherwise choose the numerically smallest off-diagonal entry

\[
\gamma=g_{ij}<0.
\]

Let

\[
\alpha=g_{ii},\qquad \beta=g_{jj}.
\]

Strict mode rejects when

\[
\gamma^2\geq\alpha\beta,
\]

the restriction to the edge between $v_i$ and $v_j$ has a nonpositive value, so strict copositivity fails.
Non-strict mode rejects only the strict inequality $\gamma^2>\alpha\beta$.

### 4. Calculate the new edge point

When γ² < αβ, calculate the three positive rational candidates

\[
\lambda_1=\frac{-\gamma}{\alpha-\gamma},\qquad
\lambda_2=\frac{\beta-\gamma}{\alpha-2\gamma+\beta},\qquad
\lambda_3=\frac{\beta}{\beta-\gamma},
\]

and select

\[
\lambda=\max\!\left(\lambda_1,\min(\lambda_2,\lambda_3)\right).
\]

The new point on the selected edge is

\[
w=\lambda v_i+(1-\lambda)v_j.
\]

These are the exact formulas used by Salmerón's `bundfuss` route. coposit reduces the numerator and denominator of λ by their
greatest common divisor but does not alter its value.

The three candidates have a geometric meaning. Along the edge, $\lambda_1$ is where $w^TAv_i=0$, $\lambda_3$ is where
$w^TAv_j=0$, and $\lambda_2$ is the stationary point of $w^TAw$. The combined maximum/minimum rule places the stationary point
inside the interval allowed by the two endpoint-interaction conditions. It is not an arbitrary interpolation parameter.

### 5. Create the two child simplices

The edge point divides the current simplex into two children:

- replace $v_i$ with $w$ and retain $v_j$;
- retain $v_i$ and replace $v_j$ with $w$.

All other vertices remain unchanged. Both child simplices must pass. If $\lambda=p/d$ in lowest terms and $q=d-p$, coposit stores
$d^2$ times the rational child Gram matrix. Its entries involving the new vertex are

\[
d^2 w^TAv_k=d\left(p g_{ik}+q g_{jk}\right),
\qquad
d^2 w^TAw=p^2\alpha+2pq\gamma+q^2\beta.
\]

Every unchanged entry is multiplied by $d^2$. Removing the common integer content afterward only changes the positive scale.

Each child is inspected immediately, first the child replacing $v_i$ and then the child replacing $v_j$. Rejected children stop
the complete decision. Certified children disappear. Children requiring another split are placed on a LIFO work list; because the
first unresolved child is pushed before the second, the second is the next node recursively expanded.

The common rational split data—$p$, $d$, the new Gram row, and the new diagonal—is calculated once. The first child is then
constructed and inspected. The second child is constructed only if the first did not reject the matrix. This lazy sibling
construction preserves both inspection order and LIFO traversal; it only avoids building a sibling that cannot be reached after an
already decisive first child.

## Correctness Idea

The current simplex is exactly the union of its two children. A negative vertex or selected two-vertex restriction supplies a real
nonnegative witness. An entrywise nonnegative Gram matrix certifies the complete simplex in barycentric coordinates. Repeating
these steps proves the selected predicate only after every generated simplex has been certified.

## Exact Arithmetic And Termination

If λ has denominator $d$, the child Gram matrices are multiplied by $d^2$, keeping every entry integral. Their common integer
content is then removed. This changes only scale, not the represented quadratic form or any algorithmic choice.

Each reached child is still independently denominator-cleared and content-reduced before inspection. Delaying the second allocation
therefore changes neither its stored matrix nor the arithmetic seen by later nodes.

The refinement keeps the same dimension. coposit allows at most 50,000 open nodes. Before splitting one node into its two
unfinished children, it returns the distinct unresolved `node_limit` outcome if those children would raise the count above that
limit. A timed model-companion run may also observe the shared signal flag at node and matrix-row boundaries and return a distinct
timeout outcome. Neither resource outcome is a negative classification.

## Known Difficult Inputs

Bundfuss can fail to make finite diagnostics on a copositive boundary matrix whose zero lies inside repeatedly refined simplices rather
than at one of the generated vertices. The algorithm keeps inserting edge points and approaching the zero, but its strict vertex and
edge checks may never expose the zero exactly.

Corpus matrix **9161**, the Brás-Eichfelder-Júdice matrix M5, is the retained example. Its nonnegative zero is not forced to appear
through the preserved minimum-edge and λ construction, so the binary same-dimensional refinement can continue indefinitely.

Raw minimum-edge selection is also sensitive to matrix scaling: the most negative entry need not be the pair closest to the strict
two-generator boundary after its diagonal sizes are taken into account. The algorithm may repeatedly refine an unhelpful edge while
another part of the simplex controls the actual difficulty.

## Fidelity Boundary

The 2008 paper establishes the non-strict-copositivity simplicial-partition family and its convergence properties. Salmerón's later
program fixes the exact minimum-edge choice, the three λ candidates, their combined rule, the two children, and their traversal.
The maintained code was compared with that concrete `bundfuss` route and implements those choices exactly using integers.

Preparing the shared split coefficients once and constructing the second sibling only after the first inspection are
representation and scheduling optimizations. They do not change a selected edge, λ value, child, inspection, or traversal decision.

Non-strict mode follows the paper's boundary inequalities. Strict mode is coposit's adaptation: diagonal equality and two-generator
equality reject because either one supplies a nonnegative zero. The model must therefore not be described as a line-for-line
implementation of a strict algorithm from the 2008 paper. It does not implement Salmerón's separate monotonicity-enhanced `zbund`
route and adds no low-dimensional shortcut, SNC slice, Dutour ratio split, graph reduction, or other solver.

Timeout checkpoints are enabled only in timed model-companion builds. Standalone model and test builds compile the same calls to
no-ops, so they add no timer thread, clock read, or signal handler. The fixed 50,000-open-node guard applies to every build and is a
coposit resource boundary; it supplies no mathematical decision.
