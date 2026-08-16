# Kernel-Cone Dickinson

Classification: coposit-created exact copositivity experiment. It keeps the ordinary ceiling-pruned Dickinson traversal as its
complete decision procedure and adds one exact calculation at a singular principal matrix whose nullity is greater than one. That
calculation searches the entire nullspace of the current principal matrix for ceiling certificates, without lifting the support
through a DFS or BFS graph.

The model supports copositivity (CP), strict copositivity (SCP), and combined classification of both predicates in one traversal.
Analysis and reference-run interfaces default to combined classification when the mode is omitted.

## Idea In Plain Language

Dickinson processes an index set $I$ by solving

$$
A_Iw=\mathbf 1
$$

when $A_I$ is nonsingular, or by choosing one vector from $\ker A_I$ when it is singular. One chosen nullspace vector can be a poor
representative when $\dim\ker A_I>1$: another direction in the same nullspace may have nonnegative products with every row of the
full matrix and therefore certify every larger support containing its own support.

Kernel-Cone Dickinson looks for those directions directly. Let the columns of $Z$ be an exact basis of $\ker A_I$. Every root-kernel
vector is then $u_I=Zy$. Its products outside $I$ are

$$
A_{[n]\setminus I,I}u_I=A_{[n]\setminus I,I}Zy=Gy.
$$

Consequently, the useful coefficient directions form the exact polyhedral cone

$$
C_I=\{y:Gy\geq0\}.
$$

Every nonzero $y\in C_I$ for which $Zy$ has a positive component gives a valid Dickinson ceiling certificate. The model handles the
cone in two cases:

1. If $\ker G\ne\{0\}$, then some nonzero root-kernel vector is also in the kernel of the full matrix. The model extracts a small
   dependent column circuit and emits its ceiling certificate directly.
2. If $\ker G=\{0\}$, then $C_I$ is pointed. The model enumerates its possible extreme rays from exact active constraint sets, checks
   both orientations, and emits every distinct feasible ray it finds.

If this extra search produces nothing useful, nothing is rejected or skipped merely because of that failure. Ordinary Dickinson
processing continues and remains the correctness fallback.

## Name, Status, And Sources

The identifier is `kernel_cone_dickinson`. “Kernel” refers to $\ker A_I$; “cone” refers to $C_I=\{y:Gy\geq0\}$ in the coefficient
space of an exact nullspace basis. “Dickinson” identifies the certificate theorem and ordinary support calculation on which the
experiment rests.

This is not an algorithm published by Dickinson. It is a coposit-created variant copied from
`models/hadeler-based/ceiling_pruned_dickinson` at local coposit revision `02c72e4e0cc35189b32b54b30e185ce2b3558846`. The copied model
combines Dickinson's certificate with FracESSA's forbidden-support generator and retains only certificates whose upper endpoint is
the complete index set. Kernel-Cone Dickinson changes only the treatment of a singular root with nullity greater than one.

The mathematical certificate comes from:

- Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its Applications* 569 (2019), 15–37,
  [DOI 10.1016/j.laa.2018.12.025](https://doi.org/10.1016/j.laa.2018.12.025), especially Theorem 4.6 and Algorithms 1–2.

The forbidden-support traversal was inherited through Ceiling-Pruned Dickinson from FracESSA's
`NonCircularSupportGenerator`, locally reconstructed from FracESSA revision `95e0ec019cf11a60c6423508e8768536a0b88860`, file
`cpp/include/fracessa/supports.hpp`. Its candidate-search origin is described by Immanuel M. Bomze, “Detecting All Evolutionarily
Stable Strategies,” *Journal of Optimization Theory and Applications* 75(2), 1992, 313–329.

The derivation, rejected lifting experiments, open questions, and the distinction between root-confined and newly supported lift
directions are recorded in `aidocs/SINGULAR_LIFT_DICKINSON_RESEARCH.md`. That research note is explanatory source material; this file
is the authoritative description of the maintained model.

## Notation

Let $A\in\mathbb Z^{n\times n}$ be nonempty and symmetric, and write

$$
[n]=\{1,\ldots,n\}.
$$

For $I\subseteq[n]$, $A_I$ is the principal matrix on $I$. For a full vector $u\in\mathbb R^n$, define

$$
L(u)=\operatorname{supp}(u)=\{i:u_i\ne0\}
$$

and

$$
U(u)=N_A(u)=\{i:(Au)_i\geq0\}.
$$

Dickinson's vector covers the Boolean interval

$$
[L(u),U(u)]=\{J:L(u)\subseteq J\subseteq U(u)\}.
$$

The copied support generator stores only a **ceiling certificate**, for which

$$
U(u)=[n].
$$

Such a certificate covers the complete upward family

$$
[L(u),[n]]=\{J:L(u)\subseteq J\}.
$$

Throughout the kernel-cone step:

- $k=|I|$ is the current root cardinality;
- $q=\dim\ker A_I$ is its nullity;
- $Z\in\mathbb Z^{k\times q}$ is an exact full-column-rank basis of $\ker A_I$;
- $R=[n]\setminus I$ is the set of outside indices;
- $G=A_{R,I}Z\in\mathbb Z^{|R|\times q}$ is the outside-product matrix; and
- $H=\ker G$ is the persistent coefficient kernel.

The implementation never assumes the empirically frequent inequality $|U|-|L|\leq n-k$. It is not an invariant and is not used in
a decision, enumeration bound, or heuristic.

## Ordinary Ceiling-Pruned Dickinson Traversal

The model visits nonempty supports in increasing cardinality. Within one cardinality, the copied FracESSA generator uses increasing
numeric-mask order. A retained ceiling certificate stores only its lower support $L$ as a forbidden set; any future support containing
$L$ is skipped.

New forbidden sets remain pending until the next root cardinality begins. This matters when one singular root yields several
kernel-cone certificates: every root of the current cardinality remains eligible for processing, and all certificates discovered at
that cardinality become active together at the next cardinality. The extra cone search therefore cannot change which peer roots are
visited.

For an emitted support $I$, let $C=A_I$.

### Nonsingular root

The model solves

$$
Cw=\mathbf1
$$

with exact fraction-free LDLT. The returned integer numerators share one positive denominator, which can be omitted because positive
scaling preserves signs and supports.

If $w\leq0$, then $-w\geq0$ is a negative quadratic witness and both CP and SCP are false. Otherwise the zero extension of $w$ is
tested against every outside row. It is retained only when all outside products are nonnegative, so its upper endpoint is $[n]$.

### Singular root

The copied baseline first takes one exact nonzero vector $w\in\ker C$ and orients it to have a positive component. A nonnegative such
vector is a zero of the quadratic form. It disproves SCP immediately; CP traversal continues because equality is allowed. Its
ceiling condition is tested exactly in the same way as for the nonsingular solution.

If $q=1$, this one vector spans the root kernel, so no further root-confined direction exists. If $q>1$, the kernel-cone step below
examines the remaining geometry before the generator advances to the next support.

## Building The Root Kernel Cone

The retained exact LDLT factorization constructs all $q$ basis columns of $Z$. For every outside row $r\in R$, the model computes

$$
G_{r,*}=A_{r,I}Z.
$$

For $y\in\mathbb R^q$, embed $Zy$ into the full coordinate space by setting all coordinates outside $I$ to zero. The rows inside $I$
annihilate it because $A_IZ=0$, while its outside products are $Gy$. Hence

$$
A\begin{pmatrix}Zy\\0_R\end{pmatrix}
=
\begin{pmatrix}0_I\\Gy\end{pmatrix}.
$$

The exact ceiling condition is therefore $Gy\geq0$. No full product scan is needed after $G$ has been built.

The map $y\mapsto Zy$ is injective because the basis columns of $Z$ are independent. Thus coefficient rays can be normalized and
deduplicated in $y$-space without identifying two different root vectors.

## Case One: A Persistent Full-Matrix Kernel

The model computes

$$
H=\ker G.
$$

If $h\in H\setminus\{0\}$, then $u_I=Zh$ satisfies both $A_Iu_I=0$ and $A_{R,I}u_I=0$. Its zero extension is therefore a genuine
full-matrix kernel vector:

$$
Au=0.
$$

Both orientations satisfy the ceiling inequality. A vector with mixed signs is a Dickinson certificate but not a nonnegative zero.
A one-sided vector can be oriented nonnegative and proves immediately that SCP is false.

Rather than retaining the arbitrary first vector returned for $H$, the implementation cheaply shrinks its support to a fundamental
column circuit:

1. Start with the nonzero coordinates of $Zh$.
2. Regard the corresponding full columns of $A$ as a tall matrix.
3. Try deleting each column. Keep the deletion exactly when the remaining columns are still dependent.
4. When no tried deletion preserves dependence, compute the one-dimensional exact nullspace of the remaining tall matrix.

The remaining columns are minimally dependent within the starting support. Their exact dependency gives a full-kernel vector with a
usually smaller lower endpoint. The greedy deletion order is deterministic; the result need not be the globally smallest circuit of
$A$, and finding such a circuit is not required for correctness.

One direct circuit certificate is enough for the minimal version. The model does not enumerate every circuit contained in $H$.

## Case Two: A Pointed Kernel Cone

If $H=\ker G=\{0\}$, then

$$
C_I=\{y:Gy\geq0\}
$$

contains no line: if both $y$ and $-y$ were feasible, then $Gy=0$ and $y\in H$. The cone is therefore pointed. It may contain no
nonzero vector, or it may have one or more extreme rays.

Every extreme ray of a pointed polyhedral cone in $\mathbb R^q$ has active constraint normals spanning a subspace of rank $q-1$.
Consequently, some $q-1$ linearly independent rows of $G$ annihilate a coefficient vector on that ray. The model uses this exact
finite characterization.

Before enumerating active sets, the implementation removes zero rows, divides every remaining row by the positive gcd of its
entries, and removes identical primitive rows. Thus positive scalar multiples of the same inequality are represented once. These
operations leave the cone unchanged; rows of opposite sign remain distinct because they impose different inequalities.

For $q>2$, active-set generation separately identifies opposite primitive rows as one equality hyperplane. The two oriented
inequalities remain in the cone and in every feasibility check, but choosing either row for the equation $g^Ty=0$ gives the same
hyperplane. Enumerating that hyperplane once avoids redundant exact nullspace calculations without changing the candidate rays.

### Active-set enumeration

For every subset $S$ of $q-1$ distinct active hyperplanes:

1. Form $G_S$.
2. Compute $\ker G_S$ exactly.
3. Continue unless its nullity is exactly one, equivalently unless $\operatorname{rank}G_S=q-1$.
4. Take its nonzero integer generator $y$.
5. Divide all coordinates by their greatest common divisor and orient the first nonzero coordinate positively. This is the canonical
   projective representative.
6. Skip the candidate if that representative has already been seen.
7. Compute the signs of every component of $Gy$ exactly.
8. If $Gy\geq0$ and $Zy$ has a positive component, retain $Zy$.
9. Otherwise, if $Gy\leq0$ and $Zy$ has a negative component, retain $-Zy$.

Testing both signs is necessary because the canonical projective orientation is chosen only for deduplication and need not be the
feasible cone orientation.

For $q=2$, the implementation replaces repeated full feasibility scans with an exact angular sweep. It sorts the distinct primitive
row directions by half-plane and cross product, finds whether they fit in a closed semicircle, and tests only the two boundary
normals of the resulting planar cone. To preserve the strict-copositivity behavior of the general active-set procedure, it still
materializes the perpendicular of every distinct row and checks whether that root vector is one-sided; only the expensive scan of
all projected inequalities is restricted to the actual cone boundaries.

If the cone is $\{0\}$, no candidate orientation passes. The kernel-cone step then returns normally and the ordinary Dickinson
traversal continues.

## Strict And Non-Strict Decisions

Every vector generated by the extra step lies in $\ker A_I$, so its zero extension has

$$
u^TAu=u_I^TA_Iu_I=0.
$$

If such a vector is one-sided, one of its orientations is nonnegative. This is a direct witness that $A$ is not strictly copositive,
independently of whether the outside products permit a ceiling certificate. Strict-only mode stops. Combined mode records
`is_strictly_copositive=false` and continues the same traversal to decide CP.

Mixed-sign kernel vectors do not decide SCP, but a feasible orientation can still certify a ceiling family for the CP proof.

A nonsingular $w\leq0$ remains the only negative witness in the copied root calculation. It rejects CP and therefore also SCP. If
the traversal completes, the CP certificate is complete. In combined mode, the remembered presence or absence of a nonnegative zero
distinguishes `{CP=true, SCP=false}` from `{CP=true, SCP=true}`. The model does not implement combined classification as two hidden
predicate runs.

## Complete Decision Flow

1. Initialize CP and SCP as true when combined classification is requested.
2. Visit root supports $I$ in increasing cardinality and numeric-mask order, skipping supports forbidden by certificates from earlier
   cardinalities.
3. Factor $A_I$ exactly.
4. If it is nonsingular, solve $A_Iw=\mathbf1$, reject on $w\leq0$, and retain $w$ only if it reaches the ceiling.
5. If it is singular, construct the ordinary Dickinson null vector, record any nonnegative zero, and retain it only if it reaches the
   ceiling.
6. If the singular nullity is one, continue with the next root.
7. If the nullity is greater than one, build $Z$, $G$, and $H=\ker G$.
8. If $H\ne\{0\}$, extract and retain one exact full-kernel circuit.
9. If $H=\{0\}$, reduce equivalent projected inequalities, then enumerate, normalize, deduplicate, orient, and verify all
   rank-$(q-1)$ active-set rays; use the exact angular boundary sweep when $q=2$.
10. Keep every new lower support pending until the next root cardinality.
11. If no extra certificate is found, simply continue ordinary Dickinson traversal.
12. Return after a decisive witness, or after every non-forbidden support has been exhausted.

## Exact Arithmetic, Storage, And Timeout Behavior

All ranks, nullspaces, products, gcds, support tests, and sign decisions use arbitrary-precision integers. Principal solves use the
shared fraction-free LDLT implementation. General rectangular nullspaces use FLINT's exact `fmpz_mat_nullspace`. There are no
floating-point tolerances or reconstructed rational approximations.

The implementation reuses matrices and one integer accumulator across roots and rays. It materializes one $(q-1)\times q$ active
matrix at a time and stores only canonical projective ray keys for the current root. It does not materialize the complete cone or a
lifted support graph.

There is no fixed limit on $n$, $q$, the number of active sets, or integer size. Cooperative timeout checkpoints occur while generating
supports, building $G$, deleting circuit columns, enumerating active sets, scanning candidate products, copying principal data, and
around exact nullspace calls. A timeout remains a resource outcome and is never converted into `false`.

## What The Extra Search Proves And What It Does Not

Every retained vector is independently verified to satisfy the exact Dickinson ceiling conditions. Therefore every added forbidden
support is sound.

When $H=0$, active-set enumeration is complete for the extreme rays of the root-confined cone $C_I$. If that cone has any nonzero
feasible vector outside the nonpositive orthant, at least one admissible extreme ray is found.

The search is not a complete replacement for singular-support lifting. A useful vector on a larger support may require nonzero
coordinates outside $I$ and need not lie in the zero extension of $\ker A_I$. Such a direction is outside $C_I$ by construction.
Kernel-Cone Dickinson deliberately does not build a graph of lifted matrices to search for it. Missing such a certificate can only
cause more ordinary Dickinson work; it cannot cause an incorrect classification.

The model also does not search a consistent affine family $A_Ix=\mathbf1$ when $A_I$ is singular. That companion problem is distinct
from the homogeneous cone $A_Ix=0$ and remains a separate prospective experiment.

The persistent-kernel branch emits one small circuit, not every possible circuit. Additional circuits could prune different upward
families, but enumerating them is deferred unless evidence shows that their extra coverage repays their exact rank cost.

## Diagnostics

The standard diagnostics tracker reports support-lattice work:

- `visited`: emitted supports plus the exact number skipped by forbidden branches;
- `covered`: supports skipped by active forbidden supports;
- `processed`: roots sent to exact Dickinson work;
- `certificates`: retained ceiling certificates; and
- `certificate_k_d_counts`: generating root cardinality and the free-index count $d=n-|L|$.

Source diagnostics used by the focused model test additionally distinguish persistent-kernel dimension, the number of distinct
primitive projected constraints, persistent circuit emission, active-ray emission, projective duplicates, discarded ordinary
certificates, and retained ceiling certificates. These test-only events are compiled out of production builds.

## Known Difficult Inputs

The active-set search can be combinatorially expensive. If $m'$ distinct nonzero primitive projected constraints remain after exact
reduction, then for nullity $q>2$ it may inspect

$$
\binom{m'}{q-1}
$$

row sets. Many dependent projected rows can still make most of those exact nullspace calculations redundant before projective ray
deduplication can help. Highly symmetric singular matrices are a natural source of this behavior. The planar $q=2$ path avoids this
repeated-feasibility cost, but higher nullities retain the combinatorial active-set enumeration.

The persistent-kernel circuit reduction repeatedly ranks tall exact column matrices. Large integer coefficients or a large initial
dependency support can make this arithmetic expensive even though only one certificate is emitted.

If most singular roots have nullity greater than one but their root-confined cones are empty, non-admissible, or produce lower
supports already dominated by earlier certificates, the model pays the cone-search cost and then performs almost the same ordinary
traversal as Ceiling-Pruned Dickinson.

Conversely, a matrix may possess excellent ceiling certificates only after new coordinates are introduced. Direct kernel-cone
geometry cannot see those directions because it keeps the vector support inside the current root. This model then falls back to the
ordinary cardinality traversal rather than following the lifted support graph.

Finally, even useful cone certificates can be numerous. The copied forbidden-support generator stores them without maintaining a
minimal antichain. Redundant lower supports can therefore increase memory and branch-checking work; subset minimization is not part
of this experiment.
