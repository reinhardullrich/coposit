# Affine Right-Hand-Side Paths And Dickinson Certificate Geometry

## Status And Purpose

This is a mathematical research note about the exact right-hand-side search used by the SAT-Halfspace Dickinson experiments. It
records the relation between a Dickinson interval's lower endpoint $L$, upper endpoint $U$, and free-index count $d$, and explains
why the observed lower support usually falls by only one even when the search explicitly prefers a smaller $L$.

The main conclusion is simple but important:

> Changing the score used to select a point does not change the dimension of the searched family. A one-dimensional affine path
> generically crosses one zero-coordinate hyperplane at a time. Two or more simultaneous zeros require special algebraic alignment,
> a preserved earlier zero, or a genuinely multi-parameter search.

This phenomenon is related to rank and singularity of a small path-consistency system. It is **not** equivalent to singularity of the
processed principal matrix.

The underlying Dickinson certificate and the permission to use a positive right-hand side other than $\mathbf1$ come from Peter
J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its Applications* 569 (2019), 15–37, especially
Theorem 4.6 and the note following Algorithms 1 and 2. The path objectives, experiments, and geometric results below are coposit
research.

Related local notes are:

- [`SINGULAR_LIFT_DICKINSON_RESEARCH.md`](SINGULAR_LIFT_DICKINSON_RESEARCH.md), for homogeneous kernel lifting and consistent
  singular affine systems;
- [`BPQY_DICKINSON_CERTIFICATE_GEOMETRY.md`](BPQY_DICKINSON_CERTIFICATE_GEOMETRY.md), for the support-lattice difficulty of the
  order-25 and order-50 BPQY matrices; and
- [`FULL_MATRIX_FACTORIZATION_AND_RHS_DICKINSON_CERTIFICATES.md`](../research/FULL_MATRIX_FACTORIZATION_AND_RHS_DICKINSON_CERTIFICATES.md),
  for full-matrix right-hand-side searches and the first derivation of the free-index identity.

## 1. Dickinson Intervals

Let $A\in\mathbb Z^{n\times n}$ be symmetric. For a vector $u\in\mathbb R^n$, define

$$
L(u)=\operatorname{supp}(u)=\{i:u_i\neq0\}
$$

and

$$
U(u)=N_A(u)=\{j:(Au)_j\geq0\}.
$$

If $u$ has at least one positive coordinate, Dickinson's theorem certifies every support in the Boolean interval

$$
[L,U]=\{J:L\subseteq J\subseteq U\}.
$$

Write

$$
\ell=|L|,
\qquad
h=|U|,
\qquad
d=h-\ell.
$$

The $d$ indices in $U\setminus L$ are free: each may be present or absent independently. Therefore the interval contains exactly

$$
2^d
$$

supports. The coordinate positions still matter because two intervals of equal size can cover different supports.

## 2. Positive Right-Hand Sides

Let $I\subseteq[n]$ be the principal support currently being processed, let $k=|I|$, and assume that $A_I$ is nonsingular. Instead
of solving only the usual system $A_Ix=\mathbf1$, choose any strictly positive right-hand side $b>0$ and solve

$$
A_Ix=b.
$$

Embed $x$ into $u\in\mathbb R^n$ by retaining the coordinates in $I$ and placing zero outside $I$. Then

$$
(Au)_I=b>0.
$$

### Theorem 2.1: the processed support is always contained in the upper endpoint

For every strictly positive $b$,

$$
I\subseteq U(u).
$$

#### Proof

For $i\in I$, the embedded product satisfies $(Au)_i=(A_Ix)_i=b_i>0$. Hence every index in $I$ belongs to $U(u)$. $\square$

The lower endpoint can be smaller than $I$ because some coordinates of $x$ may be exactly zero:

$$
L(u)\subseteq I.
$$

Thus a positive right-hand side protects the upper endpoint on the current support but does not force full local support.

## 3. The Exact Free-Index Identity

Define

$$
z=k-|L|
$$

as the number of zero coordinates inside the principal solution, and define

$$
r=n-|U|
$$

as the number of indices excluded from the upper endpoint. Because $I\subseteq U$ and $L\subseteq I$,

$$
\begin{aligned}
d
  &=|U|-|L|\\
  &=(n-r)-(k-z)\\
  &=(n-k)+z-r.
\end{aligned}
$$

Therefore

$$
\boxed{d=(n-k)+z-r}
$$

and

$$
\boxed{d>n-k\quad\Longleftrightarrow\quad z>r.}
$$

This separates two effects:

- every vanished local coordinate contributes $+1$ to $d$;
- every index missing from $U$ contributes $-1$ to $d$.

Consequently, a zero coordinate is necessary for $d>n-k$, but it is not sufficient. If $U=[n]$, then $r=0$ and

$$
d-(n-k)=z.
$$

For a ceiling certificate, the excess over $n-k$ counts the vanished local coordinates exactly.

## 4. The Affine Coordinate Paths Used By The Experiment

Write

$$
B=A_I^{-1}.
$$

From a current positive right-hand side $b$, the search changes one coordinate at a time:

$$
b(t)=b+t e_s,
\qquad t\geq0.
$$

The corresponding principal solution is

$$
x(t)=B b(t)=x+tq,
\qquad
x=Bb,
\qquad
q=Be_s.
$$

The full embedded product is affine as well. Hence $L$ or $U$ can change only at an exact zero of one of these affine coordinate
functions. For a local solution coordinate with $q_i\neq0$, the root is

$$
t_i=-\frac{x_i}{q_i}.
$$

Only positive roots occur on the searched ray. Exact rational comparison groups equal roots without floating-point tolerances.

### Theorem 4.1: exact criterion for several local zeros on one path segment

At a point $t>0$, the zero coordinates of $x(t)$ are precisely

$$
\{i:x_i=0,\ q_i=0\}
\;\cup\;
\left\{i:q_i\neq0,\ -\frac{x_i}{q_i}=t\right\}.
$$

Therefore two or more local coordinates vanish together only if at least one of the following happens:

1. two or more positive breakpoint ratios are exactly equal;
2. an already-zero coordinate is preserved because the chosen direction also has zero in that coordinate; or
3. both mechanisms occur together.

#### Proof

The equation $x_i+tq_i=0$ has the displayed unique solution when $q_i\neq0$. If $q_i=0$, it holds for all $t$ exactly when
$x_i=0$. Taking the union over the coordinates gives the result. $\square$

For two fresh zeros $i$ and $j$, the coincidence condition is

$$
-\frac{x_i}{q_i}=-\frac{x_j}{q_j},
$$

or equivalently

$$
x_iq_j-x_jq_i=0.
$$

This is the vanishing determinant

$$
\det
\begin{pmatrix}
x_i&q_i\\
x_j&q_j
\end{pmatrix}=0.
$$

So there is a singularity in a small matrix of **path coefficients**. It is not the statement $\det A_I=0$.

### Theorem 4.2: generic one-zero behavior

Assume on one path segment that:

1. no zero coordinate of $x$ is preserved by $q$; and
2. all positive ratios $-x_i/q_i$ are distinct.

Then $x(t)$ has at most one zero coordinate for every $t>0$. In particular,

$$
|L(x(t))|\geq k-1.
$$

The two assumptions fail only when exact polynomial equalities hold among the entries of $A_I$ and $b$. Unless the matrix family
forces such an identity, an absolutely continuous perturbation satisfies each equality with probability zero. Because one run
examines only finitely many coordinate segments, the whole coordinate search generically encounters only simple, one-coordinate
zero events.

This explains the experiments: preferring a smaller $L$ changes which point is selected, but every proposed move is still confined
to one line. The objective cannot create the additional degree of freedom needed to satisfy another independent zero equation.

The cumulative nature of the path does not normally accumulate zeros either. If one accepted point has $x_i=0$, then the next
coordinate direction immediately makes that coordinate nonzero unless $(Be_s)_i=0$. Retaining the old zero while acquiring a new
one therefore requires another exact cofactor relation.

### Theorem 4.3: zero supports have the expected codimension in right-hand-side space

For each local coordinate $i$, define

$$
H_i=\{b:(Bb)_i=0\}.
$$

Because $B$ is nonsingular, any $m$ rows of $B$ are linearly independent. Hence the intersection of $m$ distinct coordinate-zero
hyperplanes,

$$
\bigcap_{i\in Z}H_i,
\qquad |Z|=m,
$$

has codimension $m$ in right-hand-side space. A generic line meets codimension-one hyperplanes but misses every intersection of
codimension two or greater. More generally, an $m$-parameter affine family has the correct number of degrees of freedom to impose
$m$ independent zeros, although positivity may still make the intersection inadmissible.

## 5. The Relation To Singularity

### 5.1 Multiple zeros do not require a singular principal matrix

Consider

$$
A_I=
\begin{pmatrix}
1&1&1\\
1&3&0\\
1&0&3
\end{pmatrix}.
$$

Its leading principal determinants are $1$, $2$, and $3$, so it is positive definite. Nevertheless,

$$
A_I^{-1}\mathbf1=
\begin{pmatrix}
1\\0\\0
\end{pmatrix}.
$$

Thus a nonsingular, positive-definite principal matrix can produce two exact zero coordinates. Singularity of $A_I$ is not necessary.

For a fixed right-hand side, a zero in $A_I^{-1}b$ says that a Cramer numerator vanishes while $\det A_I\neq0$. A preserved zero
in a direction $A_I^{-1}e_s$ similarly says that a cofactor of $A_I$ vanishes. These are exact algebraic relations, but neither is
matrix singularity.

### 5.2 What is genuinely singular

For a one-parameter path, asking for zeros on a set $Z$ means solving

$$
x_Z+tq_Z=0
$$

for one scalar $t$. This system is consistent exactly when

$$
\operatorname{rank}(q_Z)
=
\operatorname{rank}\begin{pmatrix}q_Z&-x_Z\end{pmatrix}.
$$

When $|Z|\geq2$, consistency forces every $2\times2$ minor of the augmented path-coefficient matrix to vanish. This is the precise
rank degeneracy behind simultaneous roots.

### 5.3 The true lower-support matrix may be singular or nonsingular

Let $L=\operatorname{supp}(x)\subsetneq I$. Restricting $A_Ix=b$ to the nonzero coordinates gives

$$
A_Lx_L=b_L>0.
$$

This identity does not force $A_L$ to be singular. It says that the same certificate vector really lives on the smaller support $L$
and is a valid positive-right-hand-side Dickinson candidate there.

There is an algorithmic qualification: ordinary Dickinson at $L$ normally uses $\mathbf1$, not the particular vector $b_L$.
Therefore the earlier traversal at $L$ reproduces this exact candidate automatically only when $b_L$ is a positive scalar multiple
of $\mathbf1$, or when its own right-hand-side search reaches $b_L$.

If $A_L$ **is** singular and $A_Lx_L=b_L$ is consistent, then its solution set is an affine family

$$
x_0+\ker A_L.
$$

This is the affine-companion mechanism studied in the singular-lift note. It can expose a smaller-support candidate directly at $L$.
It is related to the present geometry, but it is not the reason that a nonsingular one-dimensional path usually loses only one
coordinate.

## 6. Why A Multi-Parameter Search Is Different

Choose a set $R$ of $p$ right-hand-side coordinates and vary them together:

$$
b(\alpha)=b+E_R\alpha,
\qquad
x(\alpha)=x+B E_R\alpha,
$$

where $E_R$ contains the selected unit columns. To force the coordinates in $Z$ to zero, solve

$$
B_{Z,R}\alpha=-x_Z.
$$

### Theorem 6.1: exact multi-zero feasibility criterion

A parameter vector producing $x_Z(\alpha)=0$ exists exactly when

$$
\operatorname{rank}(B_{Z,R})
=
\operatorname{rank}\begin{pmatrix}B_{Z,R}&-x_Z\end{pmatrix}.
$$

If $|Z|=|R|$ and $B_{Z,R}$ is nonsingular, the parameter vector is unique. It is admissible for the current positive-direction
search only if $\alpha\geq0$; more generally, the final right-hand side must satisfy $b+E_R\alpha>0$.

Because $B$ is nonsingular, every set of rows $Z$ has rank $|Z|$. Hence some equally sized column set $R$ makes $B_{Z,R}$
nonsingular. This proves that the linear equations can always be solved with a suitable unrestricted set of parameters. It does
**not** prove positivity of the resulting right-hand side, admissibility of the vector, or a useful upper endpoint.

The cost is combinatorial: one must choose zero coordinates $Z$, direction coordinates $R$, or solve a sparse-feasibility problem.
Changing from a $U$-first score to an $L$-first score while retaining one-coordinate moves does not perform this search.

## 7. Pushing The Upper Endpoint Higher

Increasing $|U|$ is usually more valuable than decreasing $|L|$. It places the certificate higher in the Boolean lattice and can
cover additional future cardinalities without relying on exact zero coordinates in the principal solution.

Let


$$
O=[n]\setminus I
$$

and define the outside-product matrix

$$
M=A_{O,I}A_I^{-1}.
$$

For a positive right-hand side $b$, the embedded vector satisfies

$$
(Au)_O=Mb.
$$

The upper-endpoint problem is therefore exactly

$$
\max_{b>0}
\#\{j\in O:m_j^Tb\geq0\},
$$

where $m_j^T$ is one row of $M$. Since $I\subseteq U$ automatically,

$$
|U|=k+\#\{j\in O:m_j^Tb\geq0\}.
$$

Positive scaling of $b$ changes no sign. The search can therefore be normalized to the open simplex

$$
b>0,
\qquad
\mathbf1^Tb=1.
$$

Each outside index defines one linear halfspace. The sign cells of this halfspace arrangement are precisely the regions on which
$U$ is constant.

### Why maximizing $U$ normally leaves $|L|=k$

The two endpoints depend on different linear forms. The upper endpoint is determined by the signs of $Mb$, whereas a local index
$i\in I$ leaves $L$ only when

$$
(A_I^{-1}b)_i=0.
$$

That equality is one exact hyperplane in right-hand-side space. Maximizing $|U|$ does not impose it. An upper-optimal sign cell is
normally an open region containing points for which every coordinate of $A_I^{-1}b$ is nonzero, and hence $L=I$ and $|L|=k$.

The width tie-break can prefer a smaller $L$ only when the searched path actually reaches a solution-coordinate zero without losing
the upper score. In the BPQY experiment, lowering $|L|$ by one commonly also lowered $|U|$ by one or more. Upper-first therefore
retained the dense candidate. Thus $|L|=k$ is generic behavior, not an implementation restriction.

### Theorem 7.1: positive coordinate additions can reach every positive target ray

Let $b>0$ be the current right-hand side and let $\widehat b>0$ be any target. There is a scalar $c>0$ such that

$$
c\widehat b-b\geq0.
$$

Hence the ray of every positive target can be reached from $b$ by nonnegative coordinate additions.

#### Proof

Choose

$$
c\geq\max_i\frac{b_i}{\widehat b_i}.
$$

Then $c\widehat b_i\geq b_i$ for every coordinate, so the difference is nonnegative. The endpoints $\widehat b$ and
$c\widehat b$ produce the same $L$ and $U$ because they differ only by positive scaling. $\square$

The current coordinate directions are therefore not a reachability limitation. The limitation is **greedy acceptance**. A target
cell with a larger $U$ may require several coordinate changes whose intermediate endpoints do not improve $(|U|,d)$. The maintained
path rejects such an intermediate move and never reaches the better cell, even though a simultaneous nonnegative update could reach
it.

### A full ceiling is only a linear-feasibility question

The strongest upper endpoint, $U=[n]$, exists exactly when

$$
Mb\geq0
$$

has a strictly positive solution. After normalization, this can be decided by the linear program

$$
\begin{aligned}
\text{maximize}\quad &\delta\\
\text{subject to}\quad
&Mb\geq0,\\
&b_i\geq\delta &&(i\in I),\\
&\mathbf1^Tb=1.
\end{aligned}
$$

A full ceiling exists if and only if the optimum satisfies $\delta>0$. If the resulting $x=A_I^{-1}b$ has a positive coordinate,
it gives a ceiling Dickinson certificate. If instead $x\leq0$, it gives the stronger conclusion of an immediate negative witness.

This full-ceiling test is convex and exact over rational data. It is much easier than maximizing a partial upper endpoint. coposit
does not currently contain an exact LP solver, so this is a mathematical option rather than a proposed shared pre-check.

### Maximizing a partial upper endpoint is the hard problem

When no full ceiling exists, maximizing the number of satisfied rows of $M$ is a maximum-feasible-subsystem problem. The objective
is discrete and nonconvex. A mixed-integer linear model can solve it globally, but that would introduce exactly the solver dependency
that the maintained core deliberately avoids.

There is a useful intermediate feasibility test. Let

$$
S(b)=\{j\in O:m_j^Tb\geq0\}
$$

be the currently satisfied outside indices. For one excluded index $q$, ask whether a positive $b'$ exists such that

$$
m_j^Tb'\geq0
\qquad
\text{for every }j\in S(b)\cup\{q\}.
$$

If it does, $b'$ preserves the complete current upper endpoint and adds at least $q$. Repeating this test reaches an
inclusion-maximal feasible upper set. It need not reach a maximum-cardinality set: a larger solution may require discarding one
current index to gain several others.

### The cheapest bounded escape is two synthesized rays

The full two-dimensional arrangement below is not the first experiment to try. The maintained one-coordinate sweeps already
evaluate useful breakpoint points along every direction. Keep the best **nonzero** candidate with a changed upper set from each
direction temporarily, including a candidate that does not improve the current lexicographic score.

Pair directions whose proposed upper-set changes complement one another. Useful pairs may gain different excluded indices, or may
gain the same index while losing different current indices. If one pair has selected exact step lengths $t_p$ and $t_q$, it defines
the nonnegative combined direction

$$
w=t_p e_p+t_q e_q
$$

and the corresponding ray

$$
b(s)=b+sw,
\qquad s\geq0.
$$

The principal and full-product directions are just the corresponding exact linear combinations of already-computed unit-solve
directions. No factorization, inverse, LP, or two-dimensional geometry is added. The existing exact breakpoint sweep can be reused
unchanged.

The implemented deterministic selection rule in `sat_halfspace_rays_dickinson` is:

1. retain at most
   $$
   s(n,k)=\min\{k,64,\lceil3\sqrt n\rceil\}
   $$
   changed-upper-set candidates, preferring more gained and fewer lost indices;
2. materialize each shortlisted upper support once, then score every pair by union gains, common losses, total gains, and union losses;
3. choose the best two distinct pairs and form their exact combined directions;
4. sweep both rays once from the same stalled point; and
5. keep only the better result, and only if it strictly improves $(|U|,d)$ lexicographically.

The square-root shortlist lets the number of pair scores grow approximately linearly with $n$ until the hard cap of 64 candidates.
The cap limits the pair scan to 2,016 pairs even when $n$ and $k$ are in the thousands. Pair scoring uses packed upper supports rather
than exact integers; only the two selected rays pay for exact breakpoint sweeps.

A normal coordinate pass scans $k$ unit directions. This bounded escape scans two further combined directions, for exactly $k+2$
ray scans from the stalled point. Its ray-scan overhead is therefore about $2/k$: roughly 8% at $k=25$ and 4% at $k=50$, before
accounting for the factorization that is reused. To preserve this bound, the two escape rays are selected together and the better
result is chosen; the second is not regenerated after accepting the first.

Either combined ray can cross a greedy coordinate barrier because two changes are applied simultaneously. The method is still a
heuristic: the best region may require different weights, more directions, or a temporary decrease in the score. Failure to improve
changes no classification and adds no certificate.

#### Exact example: every coordinate ray stalls while one combined ray reaches the ceiling

Let $k=3$, start from

$$
b=
\begin{pmatrix}1\\1\\1\end{pmatrix},
$$

and suppose the three outside-product rows are

$$
M=
\begin{pmatrix}
-2&2&1\\
2&-2&1\\
1&1&-3
\end{pmatrix}.
$$

At the starting point,

$$
Mb=
\begin{pmatrix}1\\1\\-1\end{pmatrix},
$$

so two of the three outside indices belong to $U$. Along the first coordinate ray,

$$
M(b+t e_1)=
\begin{pmatrix}
1-2t\\
1+2t\\
-1+t
\end{pmatrix}.
$$

The first row becomes negative before the third becomes nonnegative. Hence this ray never satisfies more than two outside rows. By
symmetry, the same is true for the second coordinate ray. The third ray makes the already-negative third row still more negative,
so it also never improves $|U|$.

Now use the combined direction $e_1+e_2$:

$$
M\bigl(b+t(e_1+e_2)\bigr)
=
\begin{pmatrix}
1\\
1\\
-1+2t
\end{pmatrix}.
$$

At $t\geq\tfrac12$, all three outside rows are nonnegative. The current coordinate rays are locally maximal at two satisfied rows,
while one combined ray reaches the full ceiling. This $M$ is realizable by a symmetric matrix, for example by taking $A_I=I$ and
using $M$ as the outside-to-inside block.

### The next exact solver-free extension is a two-coordinate arrangement search

Choose two right-hand-side coordinates $p$ and $q$ and search the plane

$$
b(\alpha,\beta)=b+\alpha e_p+\beta e_q,
\qquad
\alpha,\beta\geq0.
$$

Every outside product is

$$
m_j^Tb(\alpha,\beta)
=
m_j^Tb+\alpha m_{jp}+\beta m_{jq}.
$$

Its zero set is a line in the $(\alpha,\beta)$ plane. The local coordinates of $A_I^{-1}b(\alpha,\beta)$ are affine lines too.
An exact traversal of this two-dimensional line arrangement can therefore find the best $(|U|,d)$ cell in the selected coordinate
plane, including a cell that is invisible to both separate coordinate rays.

This search also reuses the already-computed unit solves and full direction products. It needs no new factorization and no floating-point
geometry. Its expense is the number of coordinate pairs and arrangement cells: there are $\binom{k}{2}$ planes, and a plane with
$m$ relevant lines can have $O(m^2)$ cells. It is appropriate as a bounded diagnostic before it is considered for a model.

The two-coordinate upper search is more directly useful than the two-coordinate lower-support search in Section 6. It targets the
quantity that the primary objective actually wants and does not depend on the rare event of making several solution coordinates
exactly zero.

## 8. The Three Certificate Objectives

The triple experiment records three lexicographic scores:

1. **upper first:** maximize $(|U|,d)$;
2. **distance first:** maximize $(d,|U|)$; and
3. **lower first:** minimize $|L|$, then maximize $d$.

The maintained triple model follows only the upper-first trajectory. It remembers the best candidate under all three scores among
the points encountered on that one trajectory. It does not run three independent paths.

### Theorem 8.1: upper-first and distance-first agree when lower sizes span at most one

Suppose every compared candidate has lower size in $\{s,s-1\}$ for some $s$. Then lexicographic upper-first and distance-first give
the same weak ordering of the size pairs. If both rules retain the earlier candidate on an exact tie, they select the same winner.

#### Proof

For candidates $a$ and $b$,

$$
d_a-d_b=(|U_a|-|U_b|)-(|L_a|-|L_b|).
$$

The lower-size difference has absolute value at most one. If $|U_a|>|U_b|$, the upper-size difference is a positive integer. Hence
$d_a\geq d_b$; equality is resolved by the distance-first tie-break in favor of the larger $|U_a|$. If the upper sizes are equal,
both rules select the smaller lower endpoint because it has larger $d$. $\square$

Thus the two objectives can differ only after the search produces candidates whose lower sizes differ by at least two. This is why
upper-first and distance-first were exactly redundant in the order-25 experiments.

### Lower-first can disagree without being better

Reducing $|L|$ can also reduce $|U|$. Since

$$
d=|U|-|L|,
$$

a one-coordinate reduction in $L$ buys nothing in raw interval size if $U$ also loses one coordinate. Even equal values of
$(|L|,|U|,d)$ do not imply equal certificates because the coordinate sets can differ.

## 9. Subsumption, Overlap, And Marginal Coverage

The interval $[L_1,U_1]$ contains $[L_2,U_2]$ exactly when

$$
L_1\subseteq L_2
\qquad\text{and}\qquad
U_2\subseteq U_1.
$$

If the intervals overlap, their intersection is

$$
[L_1\cup L_2,\ U_1\cap U_2]
$$

provided

$$
L_1\cup L_2\subseteq U_1\cap U_2.
$$

Its size is then

$$
2^{|U_1\cap U_2|-|L_1\cup L_2|}.
$$

These formulas give exact pairwise redundancy. They do not give the true marginal coverage against thousands of earlier
certificates; SAT or a decision diagram represents that complete union. Therefore a locally non-subsumed second certificate can
still add no globally new support while increasing clause propagation and storage work.

## 10. The Order-25 BPQY Two-Path Experiment

Temporary exact diagnostics compared two genuine trajectories on two representative order-25 BPQY matrices:

- **Path A:** the maintained upper-first trajectory;
- **old lower:** the minimum-$|L|$ candidate merely observed along Path A; and
- **Path B:** a separate trajectory whose accepted coordinate update minimizes $|L|$ first and maximizes $d$ second.

The matrix traversal itself remained Path A. Path B was diagnostic only. Preprocessing was disabled, no result rows were written,
and the temporary instrumentation was removed after the experiment.

Matrix 12574 was classified CP and SCP. Matrix 12619 was classified neither CP nor SCP.

### 10.1 Aggregate results

| Measurement | Matrix 12574 | Matrix 12619 |
| --- | ---: | ---: |
| Exact nonsingular supports compared | 842 | 1,566 |
| Upper-first and distance-first identical | 842 (100%) | 1,566 (100%) |
| Independent lower-first equals old lower | 804 (95.5%) | 1,470 (93.9%) |
| Independent lower-first equals upper-first | 713 (84.7%) | 1,406 (89.8%) |
| New and not subsumed by the old local winners | 15 (1.8%) | 74 (4.7%) |
| Lower support drops by zero | 718 | 1,409 |
| Lower support drops by one | 124 | 157 |
| Lower support drops by two or more | 0 | 0 |

The independent lower-first path never increased $|U|$ relative to the upper-first winner on these supports. On matrix 12574 its
$|U|$ loss ranged from zero to seven; on matrix 12619 it ranged from zero to sixteen. No additional negative witness was found.

For matrix 12574 the complete upper-size-loss distribution was: no loss on 713 supports, loss one on 40, loss two on 43, loss three
on 27, loss four on 11, loss five on 6, and loss seven on 2. No loss of six occurred.

The current single-path runs took roughly 0.13 s and 0.27 s, while the two-path diagnostic took roughly 0.199 s and 0.411 s. The
added path therefore cost about 50% on both matrices. These are exploratory wall-clock observations, not production benchmarks, but
they show the size of the added exact search work.

### 10.2 Representative size triples

The following entries show $(|L|,|U|,d)$; equal triples can still represent different coordinate sets.

| Matrix | $k$ | Upper-first | Old lower on upper-first path | Independent lower-first |
| ---: | ---: | ---: | ---: | ---: |
| 12574 | 5 | $(5,22,17)$ | $(4,21,17)$ | $(4,21,17)$ |
| 12574 | 4 | $(4,24,20)$ | $(3,22,19)$ | $(3,19,16)$ |
| 12619 | 3 | $(3,20,17)$ | $(2,18,16)$ | $(2,19,17)$ |
| 12619 | 5 | $(5,21,16)$ | $(4,20,16)$ | $(4,21,17)$ |

The first row also contained cases where the two lower candidates had the same size triple but different coordinate sets. This is
why cardinalities alone cannot decide subsumption or marginal value.

## 11. What The Experiment Establishes

### Proven mathematics

1. A positive principal right-hand side guarantees $I\subseteq U$.
2. The exact width identity is $d=(n-k)+z-r$.
3. A one-dimensional path loses two local coordinates at once only through coincident roots or a preserved prior zero.
4. This coincidence is a rank condition on path coefficients, not singularity of $A_I$.
5. A nonsingular, even positive-definite, matrix can produce several zero solution coordinates.
6. If all encountered lower sizes differ by at most one, upper-first and distance-first are exactly equivalent.
7. A genuine multi-parameter path can impose several zeros by solving a small exact linear system, subject to positivity and
   certificate admissibility.

### Empirical findings limited to the two BPQY matrices

1. Every observed lower contraction was zero or one.
2. Upper-first and distance-first selected the same exact certificate everywhere.
3. A separate lower-first trajectory produced few locally new, non-subsumed intervals.
4. The separate path commonly paid for a smaller $L$ by losing coordinates from $U$.
5. The extra path added substantial exact work and found no additional witness.

### Interpretation

The absence of a drop to $k-2$ is not mysterious and does not show that the code failed to search the breakpoints. Both paths still
use one-coordinate RHS moves. Generically, each such line reaches only one coordinate hyperplane at a time. An $L$-first objective
selects among those intersections; it does not create an intersection of two hyperplanes.

The BPQY observations are consistent with the inverse path data being in general position: no repeated positive ratios and no
direction zeros preserved an earlier solution zero in the examined supports. This is a plausible structural explanation, not yet a
proved property of the entire BPQY construction.

## 12. Implemented Bounded Experiment

A second complete one-dimensional lower-first path is not supported by the present evidence. The implemented experiment therefore
targets $U$ directly and stops at the first bounded level:

1. keep the current upper-first coordinate path as the baseline;
2. when a complete coordinate pass stalls, synthesize the two best distinct rays from complementary stored direction candidates;
3. sweep both from the same stalled point, keep the better result, and measure its additional $|U|$, global interval coverage, and
   exact time;
4. retain a result only if it strictly improves $(|U|,d)$ lexicographically.

The two synthesized rays are a bounded search that can cross a greedy coordinate barrier without adding an external optimizer. A
future full two-coordinate arrangement could determine whether the cheap choices of weights left substantial gains behind. Only if
the bounded experiment proves useful should the more expensive pair-zero search from Section 6 be tested. That secondary search deliberately forces
$|L|\leq k-2$ by solving $B_{Z,R}\alpha=-x_Z$, but its naive $O(k^4)$ choice count targets a less valuable and empirically rarer
event.

## 13. Open Questions

1. Do BPQY's exact integer materializations satisfy a structural noncoincidence property for the inverse ratios, or did the two
   sampled matrices merely behave generically?
2. Can symmetry classes identify the rare repeated-ratio groups without examining all pairs?
3. Can the best pair $(Z,R)$ be predicted from exact minors, outside-product violations, or the current SAT frontier without an
   $O(k^4)$ scan?
4. How often does an interval with $|L|\leq k-2$ add globally uncovered supports after all earlier-cardinality certificates are
   considered?
5. When the contracted matrix $A_L$ is singular, is affine-family support reduction cheaper than a two-parameter nonsingular path?
6. How often does a two-coordinate upper search escape a cell in which every one-coordinate sweep is locally maximal?
7. Does preserving all current upper indices and adding one more usually succeed, or are the difficult matrices dominated by
   exchange barriers that require losing one index before gaining several?

Until these questions have favorable evidence, the mathematically conservative choice is to keep the single upper-first path and
treat multiple retained objectives as cheap observations along that path, not as a reason to run additional full trajectories.
