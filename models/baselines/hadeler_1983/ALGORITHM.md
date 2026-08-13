# Hadeler 1983

Classification: exact optimized FracESSA baseline for Hadeler's principal-submatrix criterion.

## Decision Modes

Hadeler proves parallel inductive criteria for non-strict and strict copositivity, so the subset traversal and factorization are shared.
For non-strict mode, once all proper principal faces have passed, a principal matrix $C$ fails exactly when

\[
\det C<0,\qquad \operatorname{adj}C\geq0.
\]

The implementation first passes every $\det C\geq0$ case. For $\det C<0$ it solves $Cy=-\mathbf1$ once; the displayed adjugate
condition is equivalent here to $y>0$, which supplies the negative witness. A singular principal matrix therefore needs no
nullspace work in non-strict mode. Strict mode instead uses $\det C\leq0$ and $\operatorname{adj}C>0$: negative determinants use the
same solve, while determinant zero requires nullity one and a full-support same-sign kernel vector. The direct order-one through
order-three rules make the same mode-dependent distinction.

`solve(A, mode)` defaults to `strictly_copositive` and returns only the requested Boolean. `classify(A)` answers both predicates in
one non-strict-complete traversal. Its result contains `is_copositive` and `is_strictly_copositive`; the only possible pairs are

| `is_copositive` | `is_strictly_copositive` | Meaning |
|---|---|---|
| `false` | `false` | a negative nonnegative direction exists |
| `true` | `false` | the matrix is copositive but has a nonzero nonnegative zero |
| `true` | `true` | the matrix is strictly copositive |

The pair `false, true` is mathematically impossible. The combined traversal does not call `solve` twice: it continues for the
non-strict proof after remembering the first strict-only failure. A selected strict solve is therefore still cheaper on boundary
matrices because it can stop at that first zero.

## What The Algorithm Does

Hadeler's criterion reduces strict copositivity to checks on principal submatrices. The algorithm visits them from smallest to
largest. When it reaches a principal matrix $C$, every proper principal part of $C$ has already passed. Under that assumption,
Hadeler's theorem says that the determinant and one positive adjugate condition completely decide whether $C$ introduces a new
nonpositive direction.

The direct theorem mentions the full adjugate matrix. The optimized FracESSA implementation does not construct it. One exact linear
solve handles a negative determinant, and exact rank plus one kernel vector handle a zero determinant.

## Name And Sources

The identifier follows coposit's `<first-author>_<year>` rule. It names K. P. Hadeler and the publication year of:

- K. P. Hadeler, “On Copositive Matrices,” *Linear Algebra and its Applications* 49, 79–89 (1983),
  [DOI 10.1016/0024-3795(83)90095-2](https://doi.org/10.1016/0024-3795(83)90095-2), especially Theorem 3.

The local [paper transcription](../../../research/papers/Hadeler_1983.md) and
[source PDF](../../../research/papers/Hadeler_1983.pdf) preserve the mathematical source.

The implementation is pinned to FracESSA commit `36902a3d` (“Optimize exact copositivity checks”), the last optimized Hadeler
revision before commit `0ff31efb` replaced the final fallback with the cone model. The reusable exact factorization entered
FracESSA in commit `a4ec782b` and is maintained as `cpp/include/coposit/fraction_free_ldlt.hpp`.

## The Inductive Criterion

Hadeler calls a matrix strictly copositive of order $k$ when all of its order-$k$ principal submatrices are strictly
copositive. Theorem 3 states that, once an order-$m$ matrix $C$ is known to be strictly copositive on every proper principal
face, it fails strict copositivity exactly in the remaining interior case characterized by

\[
\det C\leq0
\quad\text{and}\quad
\operatorname{adj}(C)>0
\]

entrywise. Visiting principal subsets in increasing size ensures that the theorem's prerequisite is true whenever the algorithm
uses this criterion.

## Processing One Principal Matrix

Orders one through three use direct exact criteria. For a larger principal matrix $C$, the implementation performs one
fraction-free LDLT factorization and separates three determinant cases.

### Positive determinant

If

\[
\det C>0,
\]

Hadeler's failure condition is impossible. This principal subset passes immediately.

### Negative determinant

If

\[
\det C<0,
\]

solve the single system

\[
Cy=-\mathbf1.
\]

If every component of $y$ is positive, then $y$ is an explicit nonnegative negative witness:

\[
y^TCy=-y^T\mathbf1<0.
\]

The matrix is therefore not strictly copositive. If at least one component of $y$ is nonpositive, the current principal subset
passes Hadeler's criterion.

This one right-hand side is the retained optimized form of the adjugate test. The algorithm never computes $C^{-1}$, a full
adjugate, or all cofactors.

Why is one right-hand side enough? Hadeler's equivalent statement says that failure under the inductive hypothesis occurs exactly
when, for every $b>0$, there is an $x>0$ with $Cx=\lambda b$ and $\lambda\leq0$. Failure therefore implies it for
$b=\mathbf1$. When $\det C<0$, the unique candidate with $\lambda=-1$ is precisely the solution of
$Cy=-\mathbf1$. Conversely, a positive solution is already the displayed negative witness. No other right-hand side is needed to
decide this subset.

### Zero determinant

If

\[
\det C=0,
\]

use the stopped fraction-free LDLT factorization to obtain the exact rank.

- If the nullity is not one, the strict failure form of Hadeler's adjugate condition is absent, so the subset passes.
- If the nullity is one, recover one exact null vector from the retained partial factorization. If it contains a zero or mixed signs,
  the subset passes.
- If the one-dimensional nullspace is spanned by a vector whose entries are all nonzero and have the same sign, orient that vector
  nonnegatively. It is a nonzero vector $z\geq0$ with $Cz=0$, hence $z^TCz=0$, and strict copositivity fails.

For a singular symmetric matrix of nullity one, the adjugate is a scalar multiple of $zz^T$. Its strict entrywise sign condition
can therefore occur only when every component of the null vector is nonzero and has one sign. If the nullity is larger, the rank is
at most $m-2$ and the adjugate is zero. This is why the nullspace branches are the exact singular counterpart of Hadeler's
adjugate condition.

## Small Orders

- Order one requires a nonnegative diagonal in non-strict mode and a positive diagonal in strict mode.
- Order two requires the corresponding diagonal signs and either a nonnegative off-diagonal entry or a nonnegative/positive
  determinant for non-strict/strict mode.
- Order three uses the exact closed criterion derived in Hadeler's paper. The implementation evaluates only integer products and
  determinant/minor signs; it does not introduce square roots or floating-point tolerances.

Concretely, after all three order-two faces pass, a positive determinant accepts. When the determinant is nonpositive, the code
forms the six distinct entries of the symmetric adjugate and rejects exactly when all six are positive. This is Theorem 3 applied
directly at order three.

These direct rules avoid factorization overhead but make exactly the same decision as the selected Hadeler theorem.

coposit keeps the mode-aware rules in `cpp/include/coposit/small_copositivity.hpp`; the original strict-only helper remains available
to strict-only variants. The mode-aware helper accepts either a complete matrix of order at most three or an index set selecting
such a principal matrix, so the caller does not have to allocate and copy a temporary matrix. Hadeler still invokes the rules at
exactly the same points in its cardinality-first traversal.

## Complete Traversal

1. Visit principal-subset sizes $1,2,\ldots,n$.
2. Within one size, use the numeric order of the corresponding bit masks.
3. Apply the direct rule through order three.
4. For every larger subset, factor once and apply the positive-, negative-, or zero-determinant branch above.
5. Return `false` at the first failing principal subset.
6. Return `true` only after every nonempty principal subset passes.

The worst case checks all

\[
2^n-1
\]

nonempty principal subsets, so the method is finite but exponential.

## Combined Classification

`classify(A)` follows the non-strict-copositivity traversal because an early nonnegative zero answers only the strict question: a
different support could still contain a negative value. It carries one additional Boolean, initially `true`, for strict
copositivity.

For an order-at-most-three principal subset, the direct helper evaluates both exact predicates. A non-strict failure immediately
returns `{false, false}`. A strict-only failure changes the remembered strict result to `false`, but the traversal continues. For a
larger subset with negative determinant, the same solve $Cy=-\mathbf1$ decides the non-strict failure condition and therefore both
results at once. For determinant zero, non-strict Hadeler always passes. While the strict result is still possible, the traversal
also performs the existing nullity-one same-sign kernel test; finding such a kernel vector records a strict-only failure. Once
strict failure has been established, later singular subsets skip this strict-only nullspace work.

If every principal subset passes the non-strict criterion, the final pair is `{true, remembered_strict_result}`. Thus the combined
operation shares all subset enumeration, matrix extraction, determinant factorization, and negative-determinant solves; it adds
only the strict singular checks that are still necessary. Like `solve`, it assumes the parser supplied a nonempty square symmetric
matrix and does not repeat that validation.

## Known Difficult Inputs

Every strictly copositive matrix forces Hadeler to inspect the complete principal-subset family: there is no failing subset that can
end the traversal early. The work therefore grows as $2^n-1$, even when the matrix has a simple global proof of strict
copositivity. Corpus matrix **793** is a stored strictly copositive matrix of order 16; by construction of this traversal, it requires
all

\[
2^{16}-1=65535
\]

principal-subset checks.

Rejected matrices can be difficult for the same reason when their first witness has large support. If every smaller principal
subset passes, the algorithm performs almost the complete exponential traversal before reaching the failing subset. Numerous
singular or large-coefficient principal matrices add exact kernel-vector and big-integer factorization work but do not reduce the
number of subsets.

## Exact Arithmetic And Fidelity

The factorization, solve numerator, determinant, rank, and null vector use exact integers. The solve has a positive common
denominator, so testing its numerator signs is exact. A dynamic index vector replaces FracESSA's former fixed-width subset mask;
this preserves the old traversal where it existed while removing the dimension-63 representation limit.

Extracting the order-one, order-two, and order-three formulas into the shared low-order header changes only their source location.
The formulas and Hadeler control flow are unchanged. Dickinson 2019 and FracESSA also call this exact helper as an explicitly
documented coposit shortcut; that reuse does not make their remaining algorithms part of the Hadeler baseline.

The principal-matrix and one-column solution storage are reused across subsets of the same order. Only the lower triangle of each
principal matrix is copied because the symmetric fraction-free LDLT implementation reads and overwrites that triangle exclusively.
These allocation and copy reductions do not change the represented matrix, factorization, traversal, or Hadeler decision.

Hadeler's paper supplies the theorem, not this exact software traversal. Increasing-cardinality principal subsets are what establish
the theorem's inductive premise. The one-system and singular branches are exact consequences of the theorem and are pinned to the
optimized FracESSA source; they are not claimed as Hadeler's own implementation. coposit reuses the partial LDLT rank and recovers
the single required kernel vector by exact triangular back-substitution instead of restoring the matrix and running a second full
nullspace elimination. This changes no Hadeler decision. The baseline adds no connected-component decomposition,
positive-definiteness shortcut, Z-matrix rule, sign certificate, cone split, Danninger reduction, or other solver.

Timed native-module builds observe a shared signal flag at principal-subset, factorization, solve, and matrix-row boundaries and
return a distinct timeout outcome. Standalone model and test builds compile those checkpoints to no-ops, so they add no timer thread,
clock read, signal handler, or changed subset decision.
