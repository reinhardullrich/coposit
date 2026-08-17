# CBDD-Halfspace Dickinson

Classification: coposit-created exact CP/SCP experiment. It combines CBDD Dickinson's persistent Boolean-interval representation with
the exact cumulative halfspace search from `sat_halfspace_dickinson` for every nonsingular Dickinson principal system.

Public mode boundary: `copositive` and `strictly_copositive` select one predicate; `both` classifies both in one traversal and is the
analysis-interface default.

## Idea In Plain Language

Dickinson normally solves

$$
A_Ix=\mathbf1
$$

for an uncovered principal support $I$. The all-ones right-hand side is convenient but not mathematically mandatory. Changing that
right-hand side can change the signs of the zero-extended product $Au$ and therefore enlarge the interval of future supports removed
by the certificate.

This model copies the halfspace search from [`sat_halfspace_dickinson`](../sat_halfspace_dickinson/ALGORITHM.md) exactly: it factors
$A_I$ once, reuses that factorization for every unit right-hand side, and searches each resulting coordinate direction at every exact
point where the support of $u$ or the sign pattern of $Au$ can change. Improvements accumulate, and complete coordinate sweeps repeat
until no coordinate improves the certificate score.

The difference is only the support-family representation. Instead of storing intervals as SAT clauses, this model stores their union
in the same chain-reduced binary decision diagram used by [`cbdd_dickinson`](../cbdd_dickinson/ALGORITHM.md). Thus this experiment asks
whether the exact halfspace certificate engine and the CBDD family engine work better together than either existing combination.

## Name, Sources, And Classification

The identifier is `cbdd_halfspace_dickinson`.

- **CBDD** means the chain-reduced binary decision diagram representing covered and uncovered support families.
- **Halfspace** refers to the inequalities $(Au)_j\geq0$, which are linear halfspaces in right-hand-side coefficient space.
- **Dickinson** identifies the underlying certificate theorem and cardinality-ordered principal-support traversal.

The model is an independent source copy. Its halfspace calculations and selection policy come from `sat_halfspace_dickinson`; its
support traversal, interval union, cardinality restriction, and upper-cardinality expiry come from `cbdd_dickinson`. The certificate
theorem and permission to use any strictly positive right-hand side come from Peter J. C. Dickinson, “A New Certificate for
Copositivity,” *Linear Algebra and its Applications* 569 (2019), 15–37, DOI `10.1016/j.laa.2018.12.025`, especially Theorem 4.6,
Algorithms 1–2, and the note following those algorithms.

This is a coposit-created variant, not a claimed reconstruction of a literature algorithm.

## Dickinson Certificate

For a full vector $u\in\mathbb R^n$, define

$$
L(u)=\operatorname{supp}(u),
\qquad
U(u)=\{j:(Au)_j\geq0\}.
$$

An admissible vector, meaning one with at least one positive coordinate, certifies every support in

$$
[L(u),U(u)]=\{J:L(u)\subseteq J\subseteq U(u)\}.
$$

The interval contains $2^{d(u)}$ supports, where

$$
d(u)=|U(u)|-|L(u)|.
$$

The halfspace engine scores a candidate first by larger $d(u)$ and then by larger $|U(u)|$. Exact ties retain the current candidate.
This is a deterministic width heuristic; overlap with earlier intervals means it need not maximize newly removed CBDD assignments.

## Right-Hand-Side Halfspaces

Let $I\subseteq[n]$, $k=|I|$, and suppose $A_I$ is nonsingular. For a strictly positive right-hand side $b$, solve

$$
A_Ix=b
$$

and embed $x$ into $u\in\mathbb R^n$ by putting zero outside $I$. Then

$$
(Au)_I=b,
\qquad
(Au)_{I^c}=A_{I^c,I}A_I^{-1}b.
$$

Every outside condition $(Au)_j\geq0$ is therefore a linear halfspace in $b$. Positive scaling of $b$ changes neither $L(u)$ nor
$U(u)$. Globally maximizing the satisfied halfspaces is a maximum-feasible-subsystem problem; this model performs exact coordinate
ascent and does not claim a global optimum.

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

The factorization solves the identity as $k$ right-hand sides, so the $d_r$ are obtained without refactoring $A_I$. They are columns
of $A_I^{-1}$ up to one shared positive exact scale; no rational inverse is constructed.

For the zero extension $(d_r)^I$, let

$$
p_r=A(d_r)^I.
$$

If the current exact integer representatives are $x$ and $Au$, then for $t=p/q\geq0$ the candidate representatives are

$$
x'=qx+pd_r,
\qquad
Au'=qAu+pp_r.
$$

The principal right-hand side remains strictly positive because the search starts at $\mathbf1$ and adds a nonnegative multiple of
one unit vector. Known principal product entries are filled directly; only rows outside $I$ require matrix-vector products. Direction
products are formed on demand, and common integer content is removed after each accepted update to limit coefficient growth.

## Exact Coordinate Sweep

For one direction $r$, every coordinate of $x+td_r$ and $Au+tp_r$ is affine in $t$. Its zero/nonzero or sign state can change only at
a positive root

$$
t=-\frac ac,
$$

where $a$ is its current value and $c$ its direction value. The model collects all positive exact rational roots from the $k$ local
vector coordinates and the $n$ full product coordinates. It sorts and groups them by exact cross multiplication, then evaluates every
root, one exact point in each open interval, and one point beyond the final root. These samples exhaust every coverage signature on
that coordinate ray.

The implementation updates $|L|$, $|U|$, and the number of positive vector entries from sign events. It constructs a full exact
linear combination only when a point improves the lexicographic score

$$
(|U|-|L|,|U|).
$$

An improvement becomes the starting point for the next direction. Complete sweeps repeat while this bounded score strictly improves,
so the search terminates. It also stops at the absolute maximum width $n-1$. The final result is coordinate-local: a simultaneous
change in several right-hand-side coordinates may reach a better cell that no individually improving sequence reaches.

## Witnesses And Singular Supports

If a nonsingular solve with a strictly positive right-hand side produces $x\leq0$, then

$$
(-x)^TA_I(-x)=x^Tb<0.
$$

The zero-extended $-x$ is a nonnegative negative witness, so CP and SCP both fail immediately. The same test applies to searched
coordinate points.

If $A_I$ is singular, the halfspace search is not used. The model:

1. recover one nonzero nullspace vector;
2. retain its nonnegative orientation, when one exists, record a zero, and reject SCP;
3. otherwise compute $Au$ once, compare $u$ and $-u$, and choose the orientation with larger $|U|$; and
4. form the ordinary Dickinson interval of that orientation.

For a mixed-sign vector, let $p$, $m$, and $z$ count the positive, negative, and zero entries of $Au$. Because
$A(-u)=-Au$, the two upper sizes are $|U(u)|=p+z$ and $|U(-u)|=m+z$. The implementation chooses $-u$ exactly when $m>p$ and
retains the factorization orientation on a tie. It negates the existing product when needed rather than multiplying again. With
nullity greater than one, it still compares only the two signs of one deterministic kernel vector.

CP permits a zero and continues. Combined mode records SCP as false and continues the same traversal to decide CP.

## CBDD Interval Representation And Expiry

The CBDD is a reduced directed acyclic graph for Boolean support families. A node records a consecutive chain of variables with one
low and one high successor; unique-table interning merges equal nodes. Union and difference are exact recursive Boolean-family
operations with memoized node pairs.

At cardinality $k$, the model builds the exact-cardinality family $K_k$. If $C$ is the union of active certificate intervals, the
uncovered family is

$$
R_k=K_k\setminus C.
$$

Following a low edge when possible and otherwise a high edge gives one deterministic uncovered support. After solving it, the model
unites its Dickinson interval $[L,U]$ into both $C$ and the current layer's covered family, then subtracts it from $R_k$.

A certificate can cover only supports of cardinality at most $|U|$. The model therefore also stores it in expiry bucket $|U|$. Before
starting cardinality $k$, it removes every bucket with upper cardinality below $k$ from the active union. This is exact: such an
interval has empty intersection with $K_k$ and every later cardinality. Overlap with a still-live interval is preserved because expiry
recomputes active coverage as a Boolean difference rather than deleting shared graph nodes. The monotone node arena is retained;
expiry reduces active roots and operation cost but does not garbage-collect historical nodes.

## Complete Decision Flow

1. Initialize one CBDD support-family engine.
2. For cardinalities $k=1,\ldots,n$, remove expired intervals and form the uncovered family $K_k\setminus C$.
3. Extract one uncovered support $I$ and factor $A_I$ exactly.
4. On a singular support, apply the unchanged Dickinson nullspace rule.
5. On a nonsingular support, solve the all-ones system and reject on a negative witness.
6. Reuse the factorization for all unit right-hand sides and perform cumulative exact coordinate sweeps until the score cannot improve.
7. Add the chosen $[L,U]$ to the current CBDD family and its $|U|$ expiry bucket.
8. Continue until every cardinality is exhausted or an exact witness decides the selected predicate.

All signs, roots, ratios, vector combinations, products, interval operations, and witness decisions are exact. Matrix arithmetic uses
FLINT arbitrary-precision integers. Runtime diagnostics record the chosen certificate and CBDD work, not discarded search candidates.

## Known Difficult Inputs

The halfspace search adds $k$ triangular solves, up to $k$ full direction products, and repeated exact breakpoint sweeps to every
nonsingular processed support. When the all-ones certificate is already wide, this work can cost more than the supports it saves.

Coordinate ascent can create large intermediate integers. Content reduction removes common scaling but not genuine coefficient
growth. A locally wider interval can also overlap earlier coverage heavily, so larger raw width does not guarantee less CBDD work.

If useful certificates remain narrow, the model still approaches exponential support enumeration. The active CBDD can remain large,
and its monotone arena retains expired historical nodes. This variant then pays both the CBDD cost and the additional halfspace search
without obtaining enough extra pruning.
