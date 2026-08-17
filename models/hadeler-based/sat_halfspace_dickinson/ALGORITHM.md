# SAT-Halfspace Dickinson

Classification: coposit-created exact CP/SCP experiment. It combines SAT Dickinson's persistent Boolean-interval representation with
an exact cumulative coordinate search over positive right-hand sides for every nonsingular Dickinson principal system.

Public mode boundary: `copositive` and `strictly_copositive` select one predicate; `both` classifies both in one traversal and is the
analysis-interface default.

## Idea In Plain Language

Dickinson normally solves

$$
A_Ix=\mathbf1
$$

for an uncovered principal support $I$. The all-ones right-hand side is convenient but not mathematically mandatory. Changing it can
change the signs of the zero-extended product $Au$ and therefore change how many future supports the certificate removes.

This model factors $A_I$ once, reuses that factorization to solve every unit right-hand side, and searches each resulting coordinate
direction at every exact point where either the support of $u$ or the sign pattern of $Au$ can change. Unlike `rhs_dickinson`, an
accepted improvement becomes the starting point for the remaining coordinate searches. Complete sweeps repeat until no coordinate
can improve the certificate score. The search is exact coordinate ascent, not a claim of global optimization over every positive
right-hand side.

Every chosen certificate is stored by one persistent CaDiCaL clause. SAT handles only the finite family of uncovered supports; all
matrix calculations and sign decisions remain exact arbitrary-precision integer operations.

## Name, Sources, And Classification

The identifier is `sat_halfspace_dickinson`.

- **SAT** names the persistent support-family representation.
- **Halfspace** refers to the inequalities $(Au)_j\geq0$. Each one is a linear halfspace in right-hand-side coefficient space.
- **Dickinson** identifies the underlying certificate theorem and principal-support traversal.

The model is an independent copy of [`sat_dickinson`](../sat_dickinson/ALGORITHM.md). Its right-hand-side search generalizes the
one-dimensional idea isolated in [`rhs_dickinson`](../rhs_dickinson/ALGORITHM.md). The certificate theorem and permission to use any
strictly positive right-hand side come from Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its
Applications* 569 (2019), 15–37, DOI `10.1016/j.laa.2018.12.025`, especially Theorem 4.6, Algorithms 1–2, and the note following
those algorithms.

CaDiCaL 2.2.1 provides incremental SAT solving. The exact-cardinality layers use the same Batcher bitonic sorting network documented
for `sat_dickinson`.

## Dickinson Certificate

For a full vector $u\in\mathbb R^n$, define

$$
L(u)=\operatorname{supp}(u)
$$

and

$$
U(u)=\{j:(Au)_j\geq0\}.
$$

An admissible vector, meaning one with at least one positive coordinate, certifies every support in

$$
[L(u),U(u)]=\{J:L(u)\subseteq J\subseteq U(u)\}.
$$

The interval contains

$$
2^{d(u)}
\qquad\text{where}\qquad
d(u)=|U(u)|-|L(u)|.
$$

The model scores a candidate first by larger $d(u)$ and then by larger $|U(u)|$. Exact ties retain the current candidate. This is a
deterministic coverage heuristic; overlapping earlier clauses mean that the score need not equal the number of newly removed SAT
assignments.

## Right-Hand-Side Halfspaces

Let $I\subseteq[n]$, $k=|I|$, and suppose $A_I$ is nonsingular. For a strictly positive right-hand side $b$, solve

$$
A_Ix=b
$$

and embed $x$ into $u\in\mathbb R^n$ by putting zero outside $I$. Then

$$
(Au)_I=b
$$

and

$$
(Au)_{I^c}=A_{I^c,I}A_I^{-1}b.
$$

Consequently every outside condition $(Au)_j\geq0$ is a linear halfspace in $b$. Positive scaling of $b$ changes neither $L(u)$ nor
$U(u)$.

A guaranteed global maximum of the number of satisfied halfspaces is a maximum-feasible-subsystem problem. It requires a mixed-
integer linear optimization method in general. This model deliberately does not add such a solver or claim that its coordinate-local
choice is globally widest.

## Reusing One Exact Factorization

After factoring $A_I$ once, the model solves

$$
A_Ix_0=\mathbf1
$$

and all unit systems

$$
A_Id_r=e_r,
\qquad r=1,\ldots,k.
$$

The factorization solves the identity matrix as $k$ right-hand sides, so the vectors $d_r$ are obtained without refactoring $A_I$.
They are the columns of $A_I^{-1}$ up to one shared positive exact scale; the implementation does not construct a rational inverse.

The full direction products

$$
p_r=A(d_r)^I
$$

are calculated once, where $(d_r)^I$ denotes zero extension from $I$ to $[n]$. If the current exact integer representatives are
$x$ and $Au$, then for $t=p/q\geq0$ the candidate representatives are

$$
x'=qx+pd_r
$$

and

$$
Au'=qAu+pp_r.
$$

This preserves a strictly positive principal right-hand side because the search starts at $\mathbf1$ and only adds a nonnegative
multiple of one unit vector. The already-known principal entries $A_Ix$ and $A_Id_r$ are filled directly from the solved right-hand
sides; only rows outside $I$ require matrix-vector products. Direction products are formed on demand, so an absolute-width certificate
or negative witness can stop before unused products are calculated. Common integer content is removed after every accepted update to
control coefficient growth.

## Exact Coordinate Sweep

For one direction $r$, every coordinate of $x+td_r$ and $Au+tp_r$ is affine in $t$. Its zero/nonzero or sign state can change only
at a positive root

$$
t=-\frac ac,
$$

where $a$ is its current value and $c$ its direction value. The model collects all positive exact rational roots from:

1. the $k$ local vector coordinates, because a zero changes $L$;
2. the $n$ full product coordinates, because a sign change changes $U$.

It sorts and groups these roots by exact cross multiplication. It evaluates every root, one exact point in every open interval,
and one point beyond the final root. These samples exhaust every coverage signature on that coordinate ray. Equal roots are processed
as one event group; the implementation updates the counts $|L|$, $|U|$, and the number of positive vector entries from their sign
changes instead of constructing a full exact vector at every sample. It constructs the exact linear combination only for an accepted
improvement. The current point $t=0$ was already scored.

An improving point replaces the current vector before the next coordinate is searched. Complete coordinate sweeps repeat while the
lexicographic pair

$$
(|U|-|L|,|U|)
$$

strictly increases. The pair is bounded, so the improvement loop terminates. It also stops at the absolute maximum width $n-1$.

The final point is coordinate-local. Changing several coefficients together can cross a halfspace arrangement in a way that no
sequence of individually improving moves reaches.

## Witnesses And Singular Supports

If a nonsingular solve with a strictly positive right-hand side produces $x\leq0$, then

$$
(-x)^TA_I(-x)=x^Tb<0.
$$

The zero-extended $-x$ is a nonnegative negative witness, so CP and SCP both fail immediately. This test applies to the baseline and
every searched coordinate point.

If $A_I$ is singular, the halfspace search is not used. The model:

1. recover one nonzero nullspace vector;
2. retain its nonnegative orientation, when one exists, record a zero, and reject SCP;
3. otherwise compute $Au$ once, compare $u$ and $-u$, and choose the orientation with larger $|U|$; and
4. form the ordinary Dickinson interval of that orientation.

For a mixed-sign vector, let $p$, $m$, and $z$ count the positive, negative, and zero entries of $Au$. Because
$A(-u)=-Au$, the two upper sizes are $|U(u)|=p+z$ and $|U(-u)|=m+z$. The implementation therefore chooses $-u$ exactly when
$m>p$ and retains the factorization orientation on a tie. It negates the already computed product when needed; it does not perform
a second matrix product. With nullity greater than one, this still compares only the two orientations of one deterministic kernel
vector rather than searching the full kernel.

CP permits a zero and continues. Combined mode records SCP as false and continues the same traversal to decide CP.

## SAT Interval Representation

Matrix index $i$ is represented by Boolean variable $z_i$. One Batcher sorting network selects the active cardinality. For a
Dickinson interval $[L,U]$, the assignment lies inside the interval exactly when every index in $L$ is selected and every index
outside $U$ is absent. Write the ordinary interval clause as

$$
C(L,U)=
\bigvee_{i\in L}\neg z_i
\;\lor\;
\bigvee_{i\notin U}z_i.
$$

The cardinality network output $y_t$ is true exactly when at least $t+1$ indices are selected. For a bounded interval, the model
stores the equivalent clause

$$
C(L,U)\lor y_{|U|}.
$$

It behaves as the ordinary interval clause through cardinality $|U|$ and becomes satisfied afterward. A bounded clause has length
$n-d+1$; a ceiling interval with $|U|=n$ never expires and retains the original length $n-d$. CaDiCaL remains alive across every
support and cardinality, retaining the clauses and valid learned information without repeatedly activating expired intervals. An
unsatisfiable cardinality layer means that accumulated exact certificates cover every support of that size.

## Complete Decision Flow

1. Build one incremental SAT instance and one exact-cardinality sorting network.
2. For cardinalities $k=1,\ldots,n$, ask SAT for an uncovered support $I$ of size $k$.
3. Factor $A_I$ exactly.
4. On a singular support, apply the unchanged Dickinson nullspace rule.
5. On a nonsingular support, solve the all-ones system and reject on a negative witness.
6. Reuse the factorization for every unit right-hand side and precompute its full product.
7. Perform cumulative exact coordinate sweeps until the score cannot improve.
8. Add the chosen $[L,U]$ interval as one cardinality-aware persistent SAT clause.
9. Continue until SAT proves every cardinality exhausted or an exact witness decides the selected predicate.

All signs, roots, ratios, vector combinations, products, and witness decisions use FLINT arbitrary-precision integers. Runtime
diagnostics record the chosen certificate, not discarded search candidates.

## Known Difficult Inputs

The right-hand-side search adds $k$ triangular solves, $k$ full direction products, and repeated exact breakpoint sweeps to every
nonsingular processed support. On inputs whose all-ones certificates are already wide, this work can cost more than it saves.

Exact coordinate ascent can also create large intermediate integers. Content reduction controls common scaling but cannot prevent
genuine coefficient growth. A sequence of locally wider intervals need not minimize SAT runtime: coordinate positions, overlap with
earlier clauses, and learned-clause behavior all matter.

Finally, narrow certificates remain long SAT clauses. If the matrix admits no useful coordinate improvement, this model inherits
SAT Dickinson's exponential certificate count and potentially large learned-clause database while paying additional search cost.
