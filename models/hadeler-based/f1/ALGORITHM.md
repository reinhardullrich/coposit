# F1: Full-Upward Curvature and Dickinson Search

Classification: coposit-created exact copositivity and strict-copositivity experiment.

`f1` means **full-upward experiment 1**. The model keeps only certificates of the form

$$
[L,[n]]=\{J:L\subseteq J\subseteq[n]\}.
$$

Because every retained certificate is a complete upward closure, F1 uses FracESSA's small recursive forbidden-support generator. It
does not need SAT, a Boolean decision diagram, or a general interval data structure.

F1 supports separately selected copositivity, strict copositivity, and combined classification of both predicates in one traversal.
Shared preprocessing remains outside the model.

## Idea in Plain Language

F1 visits supports from small cardinality to large cardinality. It tries to prove that every larger support containing the current
one can be skipped. There are two ways to obtain such a proof:

1. **Curvature:** if the quadratic form is not strictly convex along the current simplex face, that support and every superset can be
   excluded from the search for a smallest nonpositive face.
2. **Dickinson:** choose an exact right-hand side on the current face and solve the corresponding linear system. Keep the Dickinson
   certificate only when its upper endpoint reaches the full index set.

Ordinary Dickinson often stops in the middle of the Boolean lattice. F1 therefore applies the existing Halfspace-Rays search to the
right-hand side. It also tests curvature at the ordinary, Halfspace, and Rays upper endpoints. Every successful endpoint-curvature
test creates another complete upward closure.

The intuition is simple: bounded intervals require expensive general set bookkeeping. Complete upward closures can instead be
remembered as forbidden lower supports and removed directly while supports are generated.

## Sources and Classification

F1 is an isolated copy of [`sat_c3`](../sat_c3/ALGORITHM.md) with the exact staged right-hand-side search and endpoint-curvature checks
from [`sat_c4`](../sat_c4/ALGORITHM.md). It is a coposit-created variant, not a faithful implementation of one published algorithm.

The Dickinson certificate comes from Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its
Applications* 569 (2019), 15–37, especially Theorem 4.6 and Algorithms 1–2.

The generator follows FracESSA's `NonCircularSupportGeneratorMultiword`, read locally from
`cpp/include/fracessa/support_generator_non_circular.hpp` at revision `41dfa8fffde9d42456a9df478893e70af04d5b0c`. Its support-
enumeration origin is Immanuel M. Bomze, “Detecting All Evolutionarily Stable Strategies,” *Journal of Optimization Theory and
Applications* 75(2), 1992, 313–329.

The reduced-curvature condition is the standard second-order condition for quadratic optimization on a simplex face. Its use for
copositivity is the minimal-support argument developed in
[`aidocs/COPOSITIVITY_CURVATURE_HADELER_DICKINSON.md`](../../../aidocs/COPOSITIVITY_CURVATURE_HADELER_DICKINSON.md).

## Notation

Let $A\in\mathbb Z^{n\times n}$ be symmetric and let $I\subseteq[n]$ be nonempty. Write $A_I$ for the principal matrix on $I$.

For a vector $u$ supported inside $I$, define

$$
L(u)=\{i:u_i\ne0\},
\qquad
U(u)=\{j:(Au)_j\geq0\}.
$$

Dickinson certifies the Boolean interval $[L(u),U(u)]$. F1 stores it only when $U(u)=[n]$.

The tangent space of the simplex face on $I$ is

$$
\mathcal T_I=\{v\in\mathbb R^{|I|}:\mathbf1^Tv=0\}.
$$

The reduced curvature is positive definite exactly when

$$
v^TA_Iv>0
\quad\text{for every nonzero }v\in\mathcal T_I.
$$

## Full Upward Curvature Certificates

A smallest support carrying a nonpositive simplex minimum must have positive-definite reduced curvature. A negative tangent direction
prevents a minimum. A flat tangent direction permits movement to a smaller face without increasing the value.

If reduced curvature fails on $I$, the same bad tangent direction remains present after adding zero coordinates. It therefore fails
on every $J\supseteq I$. F1 stores the complete upward closure $[I,[n]]$.

Before traversal, every pair $\{i,j\}$ is checked using the exact one-dimensional tangent curvature

$$
a_{ii}+a_{jj}-2a_{ij}.
$$

If this value is nonpositive, the pair is installed immediately as a forbidden lower support. This is the order-two special case of
the general reduced-curvature test and avoids factorizing that pair later.

For every emitted support, F1 reuses the exact fraction-free $LDL^T$ factorization of $A_I$:

- when $A_I$ is nonsingular, reduced curvature is positive definite exactly when $A_I$ is positive definite, or $A_I$ has exactly
  one negative eigenvalue and $\mathbf1^TA_I^{-1}\mathbf1<0$;
- when $A_I$ is singular, it is positive definite exactly when $A_I$ is positive semidefinite, has nullity one, and its null vector
  $z$ satisfies $\mathbf1^Tz\ne0$.

Every accepted curvature certificate is exact.

## Halfspace-Rays Dickinson Search

For a nonsingular $A_I$, ordinary Dickinson starts with the exact solution

$$
A_Ix=\mathbf1.
$$

F1 calculates $Ax$ in the full matrix and scores the resulting certificate lexicographically by

1. larger $|U(x)|$;
2. for equal $|U(x)|$, larger distance $|U(x)|-|L(x)|$.

The retained factorization solves all coordinate systems $A_Id_r=e_r$ at once. Along every exact ray

$$
x(t)=x+t d_r,\qquad t\geq0,
$$

the signs of $x(t)$ and $Ax(t)$ change only at exact rational breakpoints. F1 sorts those breakpoints, evaluates one representative
from every open interval and every breakpoint, and accepts the best exact score. Coordinate sweeps repeat while they improve the
incumbent.

The Rays addition keeps a dimension-dependent shortlist of useful coordinate candidates,

$$
\min\bigl(|I|,64,\lceil3\sqrt n\rceil\bigr),
$$

preferring candidates that gain many new upper indices and lose few current ones. It selects at most two complementary candidate
pairs, combines each pair into one nonnegative right-hand-side direction, and performs one exact breakpoint sweep per combined ray.
No LP or MILP is solved.

F1 tests the upper endpoint after three stages:

1. the ordinary all-ones solution;
2. the completed coordinate Halfspace sweeps;
3. the combined Rays sweeps.

For each distinct proper endpoint $U$ with at least three indices, a scaled binary64 reduced-Hessian factorization first screens out
clearly curvature-good endpoints. Two-index endpoints need no second test because the exact pair prepass already classified them. A
remaining candidate is rebuilt and factorized exactly; only an exact failure of positive definiteness stores $[U,[n]]$. An early
endpoint-curvature hit does not stop the right-hand-side search, because the final Dickinson vector can still have a smaller lower
endpoint. Endpoint checks are also skipped when the current support already supplied a stronger curvature closure.

Finally, F1 stores the optimized Dickinson certificate only when every entry of $Ax$ is nonnegative. The stored lower support is
$L(x)$. All bounded certificates are discarded.

## Singular Supports

When $A_I$ is singular, F1 uses one exact nullspace vector. It chooses the sign with the larger Dickinson upper endpoint. A
componentwise nonnegative null vector is an exact copositive zero, so strict copositivity fails while ordinary copositivity remains
open.

The singular support receives the same exact reduced-curvature test. Its nullspace Dickinson certificate is retained only if its
upper endpoint is the full set. Halfspace-Rays is not applied because there is no unique inverse system.

## Negative and Zero Decisions

If an exact Dickinson vector becomes componentwise nonpositive, it is an exact negative witness and copositivity fails immediately.
If a componentwise nonnegative exact vector has zero quadratic value, strict copositivity fails immediately. These decisions are made
even when the associated bounded Dickinson interval is discarded.

No floating-point calculation establishes a truth value, a certificate, or a pruning decision. The endpoint screen may only avoid an
optional curvature closure; every retained closure is recomputed and verified exactly.

## Forbidden-Support Generator

The generator enumerates nonempty supports in increasing cardinality. A retained certificate contributes only its lower support. New
lower supports become active before the next cardinality. While descending the recursive generation tree, completing a forbidden
lower support skips the entire remaining branch.

The model never compares a completed candidate against every certificate. Each rule is kept in the bucket of its lowest set bit and
is checked at the first recursion point where it can become complete.

If a cardinality emits no support, every support of that size contains an active forbidden lower support. Every larger support does
too, so traversal terminates successfully. Packed supports use as many 64-bit words as necessary.

## Complete Decision Flow

1. Install every exact pair-curvature upward closure.
2. Generate remaining supports in increasing cardinality.
3. Copy and exactly factor the current principal matrix.
4. Return a negative classification on an exact negative witness; record failure of strict copositivity on an exact zero.
5. Store the current support's full curvature closure when reduced curvature is not positive definite.
6. For a nonsingular support, run ordinary, Halfspace, and Rays stages. At each distinct endpoint, store a full curvature closure when
   exact reduced curvature fails.
7. Store the final Dickinson closure only when its upper endpoint is $[n]$.
8. For a singular support, test only its exact curvature and nullspace Dickinson certificates.
9. Activate queued forbidden lower supports at the next cardinality and continue until a decision or exhaustion.

## Diagnostics

Shared runtime diagnostics report visited and skipped supports, exact support work, retained certificates, singular supports, and the
generating-cardinality/free-index distribution.

Focused source diagnostics distinguish pair curvature, current-support curvature, the three endpoint-curvature stages, Halfspace
improvements, combined rays, full-ceiling Dickinson certificates, duplicate ceiling certificates, and discarded bounded intervals.

## Exactness and Termination

All accepted signs, inertia results, witnesses, zeros, and pruning certificates use arbitrary-precision integers. The finite support
generator emits every unpruned support at most once, so F1 terminates when given sufficient time and memory.

## Known Difficult Inputs

F1 can approach exhaustive enumeration when reduced curvature stays positive definite and optimized Dickinson vectors miss even one
outside index. In that regime, the Halfspace-Rays work is paid at many supports without creating a retained closure.

Endpoint-curvature candidates can have reduced Hessians much larger than the generating support. The binary64 screen avoids exact work
for clearly curvature-good endpoints, but ambiguous large endpoints still require an exact factorization that may gain no pruning.

Many incomparable full upward closures can still consume memory. F1 deliberately does not maintain an antichain, run LP or MILP
extensions, search backward through a Dickinson interval, perform KKT walks, prune downward, or apply Schur reduction.
