# SAT-Halfspace-MILP Dickinson

Classification: coposit-created exact copositivity and strict-copositivity experiment. It keeps Dickinson's exact certificate,
cardinality-order support traversal, and the persistent SAT representation from `sat_dickinson`. On each suitable nonsingular
support it replaces the all-ones right-hand side by a bounded mixed-integer optimization that tries to maximize the certificate's
upper endpoint. A floating-point result is only a proposal; the model admits a certificate only after exact integer reconstruction
and verification.

The public identifier is `sat_halfspace_milp_dickinson`. “SAT” names the representation of uncovered supports, “halfspace” names
the inequalities that define the upper endpoint, “MILP” names the mixed-integer linear optimization, and “Dickinson” identifies the
certificate theorem. The model supports `copositive`, `strictly_copositive`, and combined `both` classification in one traversal.

## Idea In Plain Language

Dickinson examines principal supports in increasing cardinality. One exact solve produces an interval of supports that no longer
need separate examination. The usual nonsingular solve uses the all-ones vector on the right-hand side. Any strictly positive
right-hand side is valid, however, and different choices can make many more entries of the full matrix-vector product nonnegative.

This model asks a small MILP to choose that right-hand side. Its objective is only to maximize the number of nonnegative entries in
the full product, hence to maximize the size of the Dickinson upper endpoint. The MILP is deliberately bounded by time, nodes, and
memory. Failure to find a better proposal leaves the ordinary all-ones Dickinson certificate unchanged. Thus the optimization can
change pruning and running time, but cannot remove the complete exact fallback traversal.

## Sources And Model Boundary

The certificate mathematics is from Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its
Applications* 569 (2019), 15–37, DOI `10.1016/j.laa.2018.12.025`, especially Theorem 4.6, Algorithms 1–2, and the observation that
the nonsingular right-hand side may be any strictly positive vector.

The persistent SAT support representation is copied from [`sat_dickinson`](../sat_dickinson/ALGORITHM.md). CaDiCaL 2.2.1 supplies
incremental SAT solving, and a Batcher bitonic sorting network enforces the current support cardinality. The MILP formulation, its
small model-local branch-and-bound solver, and exact reconstruction are coposit changes. The model does not link SCIP, GLPK, or
another general optimization package.

## Dickinson Certificates

For an embedded vector $u\in\mathbb R^n$, define

$$
L(u)=\operatorname{supp}(u),
\qquad
U(u)=\{j:(Au)_j\geq0\}.
$$

If $u$ has a positive entry and $L(u)\subseteq U(u)$, every support $J$ satisfying

$$
L(u)\subseteq J\subseteq U(u)
$$

is certified. The interval contains $2^{|U|-|L|}$ supports. SAT index variable $x_i$ is true exactly when index $i$ belongs to the
current support. The whole interval is excluded by the single blocking clause

$$
\bigvee_{i\in L}\neg x_i\;\lor\;\bigvee_{i\notin U}x_i.
$$

The model retains the cardinality-aware form used by `sat_dickinson`, so an interval whose upper endpoint has already fallen below
the current cardinality is inactive without rebuilding the solver.

## Nonsingular Support And Halfspace MILP

Let $I\subseteq[n]$, $k=|I|$, and suppose the principal matrix $A_I$ is nonsingular. For a strictly positive vector $b$, solve

$$
A_Ix=b
$$

and embed $x$ into $u\in\mathbb R^n$ by placing zeros outside $I$. Then

$$
(Au)_I=b>0,
$$

so $I\subseteq U(u)$ automatically. Only the outside rows remain:

$$
(Au)_{I^c}=A_{I^c,I}A_I^{-1}b.
$$

The model factors $A_I$ once. It first solves the ordinary all-ones system. If optimization is attempted, the same factorization
solves all unit systems and obtains exact integer columns proportional to $A_I^{-1}e_r$. Their embedded full products give a matrix
$P$ proportional to $A_{I^c,I}A_I^{-1}$.

Positive scaling of $b$ does not matter, so the numerical search uses the positive simplex

$$
b_r\geq\varepsilon,
\qquad
\sum_{r=1}^k b_r=1,
$$

where $\varepsilon=\min(10^{-7},1/(2k))$. For every outside row $j$, a binary variable $z_j$ records whether

$$
(Pb)_j\geq0.
$$

The objective is

$$
\max\sum_{j\notin I} z_j.
$$

The rows are independently scaled by powers of two before conversion to `double`. The model-local solver uses dense two-phase
simplex relaxations inside depth-first branch and bound. It branches only on fractional $z_j$, explores the satisfied branch first,
and prunes a node when its relaxation cannot beat the exact all-ones incumbent. Each call is capped at 10,000 nodes and 20
milliseconds. The optional inverse/product workspace, scaled input, and each simplex tableau are also bounded; an oversized problem
skips the MILP.

These bounds make the search a heuristic: reaching its internal “optimal” state means that this bounded floating search completed,
not that floating arithmetic is an exact proof of the mathematical MILP optimum.

## Exact Reconstruction And Safety

A numerical point never enters SAT directly. The model rounds its positive coefficients to integers at four scales,

$$
10^6,\quad10^9,\quad10^{12},\quad10^{15},
$$

and reconstructs both $x$ and $Au$ from the exact arbitrary-precision direction columns. It counts $U$ again using exact signs. The
candidate replaces the all-ones vector only if its exact $|U|$ is strictly larger. Common integer content is removed before storage.

If an exactly reconstructed $x$ is nonpositive, then $-x\geq0$ and

$$
(-x)^TA_I(-x)=x^Tb<0,
$$

which is an exact non-copositivity witness. Otherwise the exactly recomputed $L$ and $U$ form the only certificate inserted into
SAT. Numerical infeasibility, timeout, node exhaustion, memory guard, rounding failure, or a non-improving proposal all fall back to
the ordinary exact certificate.

## Singular Supports

The MILP is not used when $A_I$ is singular. The model recovers one exact nullspace vector. A nonnegative orientation proves that
strict copositivity is false. For a mixed-sign vector it computes both possible upper-endpoint sizes from $Au$ and chooses the sign
with the larger $|U|$; a tie keeps the factorization orientation. It then inserts the resulting exact Dickinson interval.

## Complete Decision Flow

1. Ask incremental SAT for an uncovered support of cardinality $k$, beginning with $k=1$.
2. Factor its principal matrix exactly.
3. On a singular support, use the exact nullspace branch above.
4. On a nonsingular support, compute the all-ones Dickinson vector and its exact product.
5. If the support has at least two indices, the upper endpoint is not already full, and the workspace fits, run the bounded MILP.
6. Accept only a strictly better exactly reconstructed upper endpoint; otherwise keep the all-ones result.
7. Stop on an exact negative witness, update strict classification on an exact nonnegative zero, or add the exact interval clause.
8. Continue through all cardinalities. If SAT reports no uncovered support in any remaining layer, return the positive classification.

The Boolean lattice is finite. Every accepted interval only removes supports. MILP limits do not affect termination because every
support still has the ordinary Dickinson fallback.

## Known Difficult Inputs

- A support with many outside halfspaces can make the branch-and-bound tree large; the node and time limits then return the best
  proposal found so far or no proposal.
- Large exact inverse columns can make row conversion and exact reconstruction expensive even when the numerical MILP is small.
- Near-zero halfspace products are sensitive to floating proposal quality. Exact reconstruction may reject a numerically attractive
  point, preserving correctness but losing the hoped-for pruning.
- On supports where the all-ones certificate is already strong, constructing all inverse columns costs more than it saves. The model
  therefore skips optimization when the upper endpoint is already full and bounds the optional workspace.
- Singular supports of nullity greater than one still use one factorization-provided nullspace vector; the MILP does not search the
  kernel cone.
