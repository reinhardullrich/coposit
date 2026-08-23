# SAT-C3: Exact Upward-Closure Traversal

Classification: coposit-created exact copositivity and strict-copositivity experiment. SAT-C3 combines exact face-curvature pruning
with exact Dickinson ceiling certificates. It keeps only complete upward closures and therefore uses FracESSA's much simpler
forbidden-support generator instead of a SAT solver.

The `sat_c3` identifier preserves the sequence of the SAT-C experiments. The word `SAT` is historical here: this model deliberately
contains no SAT solver, Boolean decision diagram, downward pruning, KKT walk, or search backward through a Dickinson interval.

Public mode boundary: `sat_c3` supports separately selected copositivity (CP), strict copositivity (SCP), and combined classification
of both predicates in one traversal. Shared preprocessing is an external caller concern.

## Idea In Plain Language

The model visits nonempty supports from small cardinality to large cardinality. Every support that is actually emitted is checked with
exact integer arithmetic. The same exact factorization is then used for two possible pruning rules:

1. **Curvature:** if the quadratic form is not strictly convex along the current simplex face, no support containing the current one
   can be the smallest support on which copositivity or strict copositivity fails. The entire upward closure is removed.
2. **Dickinson:** the exact Dickinson vector is tested against every row of the full matrix. It is retained only when its upper
   endpoint is the full index set. Its complete upward closure is then removed.

If neither rule reaches the full ceiling, the model stores nothing and continues. The current support has nevertheless been examined
exactly and the generator never emits it again.

The intuition is that SAT-C3 accepts only hereditary information: once a lower support is forbidden, every larger support containing
it is also forbidden. That is exactly the shape handled cheaply by FracESSA's generator.

## Sources And Classification

The model is an isolated copy and extension of `models/hadeler-based/ceiling_pruned_dickinson`. Its exact Dickinson calculation comes
from Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its Applications* 569 (2019), 15–37,
DOI `10.1016/j.laa.2018.12.025`, especially Theorem 4.6 and Algorithms 1–2.

The forbidden-support traversal is copied mathematically from FracESSA's `NonCircularSupportGenerator`, read locally at revision
`95e0ec019cf11a60c6423508e8768536a0b88860` from `cpp/include/fracessa/supports.hpp`. Its support-enumeration origin is described by
Immanuel M. Bomze, “Detecting All Evolutionarily Stable Strategies,” *Journal of Optimization Theory and Applications* 75(2), 1992,
313–329.

The exact reduced-curvature test is the standard second-order condition for quadratic optimization on a simplex face. The copositivity
use is the minimal-support argument documented in `aidocs/COPOSITIVITY_CURVATURE_HADELER_DICKINSON.md`. The combined curvature-plus-
Dickinson policy is a coposit-created variant, not a faithful implementation of one published algorithm.

## Notation

Let $A\in\mathbb Z^{n\times n}$ be symmetric and let $[n]=\{1,\ldots,n\}$. A support is a nonempty set $I\subseteq[n]$, and
$A_I$ is the corresponding principal matrix.

For an exact vector $u$ embedded in $\mathbb R^n$ by putting zeros outside $I$, define

$$
L(u)=\{i:u_i\ne0\},
$$

and

$$
U(u)=\{j:(Au)_j\geq0\}.
$$

Dickinson certifies the Boolean interval

$$
[L(u),U(u)]=\{J:L(u)\subseteq J\subseteq U(u)\}.
$$

SAT-C3 stores this certificate only when $U(u)=[n]$.

For curvature, the tangent space of the simplex face on $I$ is

$$
\mathcal T_I=\{v\in\mathbb R^{|I|}:\mathbf1^Tv=0\}.
$$

The face has positive-definite reduced curvature when

$$
v^TA_Iv>0\qquad\text{for every nonzero }v\in\mathcal T_I.
$$

## Why Curvature Gives An Upward Closure

A smallest support on which the quadratic form has a nonpositive simplex minimum must have positive-definite reduced curvature. If it
had a negative tangent direction, the point would not be a minimum. If it had a flat tangent direction, one could move to the boundary
of the face without increasing the value and obtain a smaller nonpositive support.

Now suppose reduced curvature already fails on $I$. For every superset $J\supseteq I$, the same tangent direction extends to $J$ by
adding zeros. Therefore reduced curvature also fails on $J$. No such $J$ can be a smallest bad support.

Because SAT-C3 traverses supports in increasing cardinality, any smaller bad support not containing $I$ is examined before a skipped
superset. It is therefore safe to forbid

$$
[I,[n]]=\{J:I\subseteq J\}.
$$

The intuition is simple: **adding indices cannot repair failure of positive definiteness on the existing simplex tangent space.**

## Exact Curvature Test

For every emitted $I$, SAT-C3 factorizes $B=A_I$ using exact fraction-free $LDL^T$.

### Nonsingular case

Solve

$$
Bx=\mathbf1
$$

and define $\delta=\mathbf1^TB^{-1}\mathbf1$. The reduced curvature is positive definite exactly when either

1. $B$ is positive definite; or
2. $B$ has exactly one negative eigenvalue and $\delta<0$.

The implementation obtains the inertia and the exact sign of $\delta$ from the retained factorization and integer solution
numerators. If neither condition holds, it stores $I$ as a forbidden lower support.

### Singular case

Let $z$ be the exact nullspace vector returned by the factorization. The reduced curvature is positive definite exactly when

1. $B$ is positive semidefinite;
2. $B$ has nullity one; and
3. $\mathbf1^Tz\ne0$.

If any condition fails, SAT-C3 stores the upward curvature closure from $I$.

No floating-point filter is present. Every curvature decision is exact.

## Exact Dickinson Test

SAT-C3 checks Dickinson even when curvature already supplied an upward closure.

If $A_I$ is nonsingular, it uses the exact solution of

$$
A_Iu_I=\mathbf1.
$$

If $A_I$ is singular, it uses the exact nonzero nullspace vector already recovered for the curvature test and orients it to have a
positive component.

The vector is embedded in the full space by inserting zeros outside $I$. Products on $I$ are known from the principal equation, so
the implementation calculates only the outside products. If any outside product is negative, then $U(u)\ne[n]$ and the valid bounded
Dickinson interval is discarded.

If every outside product is nonnegative, Dickinson gives the complete upward closure

$$
[L(u),[n]].
$$

SAT-C3 stores $L(u)$ as a forbidden lower support. If curvature already stored $I$ and $L(u)=I$, the Dickinson closure is an exact
duplicate and is not stored twice. If zeros make $L(u)$ smaller than $I$, the stronger Dickinson closure is retained as well.

This model uses Dickinson's ordinary exact vector. It does not run Halfspace-Rays, LP, MILP, multiple-nullspace, or singular-lifting
optimization.

## Negative And Zero Decisions

The exact local calculation remains mandatory even when neither pruning rule succeeds.

In the nonsingular case, if the solution of $A_Iu_I=\mathbf1$ is componentwise nonpositive, its negation is the exact negative witness
used by Dickinson's decision rule, so CP and SCP both fail.

In the singular case, an oriented componentwise nonnegative nullspace vector is an exact copositive zero. SCP fails, while CP remains
open and traversal continues in combined or CP mode.

No floating-point result can decide a truth value, accept a certificate, or remove a support.

## Forbidden-Support Generator

The generator visits supports by increasing cardinality and increasing numeric mask within a cardinality. It stores each accepted
lower support in a bucket indexed by its lowest element. During recursive generation, completing a forbidden support skips the whole
remaining branch; the candidate is not compared with every stored certificate.

New forbidden supports become active at the next cardinality. This is sufficient because a distinct support in the current layer
cannot be a strict superset of the support just processed. If Dickinson produced a smaller $L(u)$ through zeros, delayed activation
may cause harmless additional work in the current layer but cannot skip required work.

The model does not store a clause for a support that was merely processed. The recursive generator emits that support once, returns
from its branch, and discards the layer's enumeration state when the cardinality ends. There is consequently no exact-support clause
to retire and no possibility that a processed support is selected again.

If a cardinality emits no support, every support of that size contains an active forbidden lower set. Consequently every larger
support does too, and traversal terminates with a complete proof.

Packed supports use as many 64-bit words as necessary; there is no dimension-63 limit.

## Complete Decision Flow

1. Initialize the exact factorization workspace and the FracESSA-style forbidden-support generator.
2. Generate supports $I$ in increasing cardinality.
3. Copy and exactly factor $A_I$.
4. Return not copositive on an exact negative witness; record not strictly copositive on an exact zero.
5. Test reduced curvature exactly. If it is not positive definite, queue $I$ as an upward forbidden support.
6. Test the ordinary Dickinson vector exactly against the full matrix. If $U(u)=[n]$, queue $L(u)$ unless it duplicates the curvature
   closure already queued for this support.
7. If neither upward rule succeeds, queue nothing and continue.
8. Activate queued forbidden supports when the next cardinality begins.
9. Return the combined exact CP/SCP classification when generation is exhausted.

## Diagnostics

Runtime diagnostics use the shared support tracker. They report generated and skipped supports, exact support work, retained
certificates, and the joint generating-cardinality/free-index distribution.

Focused source diagnostics additionally distinguish:

- `curvature-certificate`;
- `ceiling-certificate`;
- `duplicate-ceiling-certificate`; and
- `discard-certificate`.

## Exactness And Termination

All matrix arithmetic, inertia decisions, signs, witnesses, zeros, and pruning certificates use arbitrary-precision integers. Every
emitted support is checked exactly even when it produces no pruning rule.

Only exact complete upward closures are stored. The generator emits each remaining support at most once, so the algorithm terminates
after finitely many supports when given sufficient time and memory.

## Known Difficult Inputs

If most supports have positive-definite reduced curvature and ordinary Dickinson vectors miss even one outside row, SAT-C3 stores few
forbidden lower supports and approaches exhaustive enumeration of $2^n-1$ supports.

The model deliberately omits Halfspace-Rays optimization. Inputs whose basic right-hand side produces a bounded Dickinson interval
but whose optimized right-hand side reaches the ceiling can therefore be substantially harder than in
`sat_halfspace_rays_dickinson`.

Many distinct full-ceiling certificates can also consume memory. The generator does not maintain a minimal antichain of forbidden
supports because the extra hot-path subset checks may cost more than redundant storage; this is a deliberate first-version limit.
