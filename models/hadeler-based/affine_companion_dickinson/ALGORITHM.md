# Affine-Companion Dickinson

Classification: coposit-created exact copositivity experiment. It is an isolated copy of `kernel_cone_dickinson` and keeps that
model's complete ceiling-pruned Dickinson traversal. Its additional calculation searches the affine solution family
$A_Ix=\mathbf1$ at a singular principal support, before the copied homogeneous search examines $\ker A_I$.

The model supports copositivity (CP), strict copositivity (SCP), and combined classification in one traversal. Analysis and
reference-run interfaces default to combined classification when the mode is omitted.

## Idea In Plain Language

At a nonsingular support $I$, Dickinson obtains one exact vector by solving $A_Ix=\mathbf1$. At a singular support, the ordinary
algorithm instead chooses a homogeneous vector from $\ker A_I$. This misses a useful intermediate possibility: the singular system

$$
A_Ix=\mathbf1
$$

may still have infinitely many solutions. A solution can have a smaller true support than a nonsingular solution found at a later,
larger principal set, while producing the same kind of Dickinson certificate.

Affine-Companion Dickinson therefore does three things at a singular support:

1. Decide exactly whether $A_Ix=\mathbf1$ is consistent.
2. If the nullity is one, search the entire affine line of solutions and select a feasible member with minimum support.
3. If the nullity is greater than one, test one exact particular solution as a deliberately small first experiment.

It then performs the copied ordinary homogeneous Dickinson calculation and, for nullity greater than one, the copied exact
kernel-cone search. Failure of the affine shortcut never removes a support or changes a decision; the ordinary traversal remains the
complete fallback.

## Name, Status, And Sources

The identifier is `affine_companion_dickinson`. “Affine companion” distinguishes the family $A_Ix=\mathbf1$ from the homogeneous
kernel $A_Iz=0$. “Dickinson” identifies the certificate and support traversal to which the calculation is added.

This is not an algorithm published by Dickinson. It was copied from `models/hadeler-based/kernel_cone_dickinson` and adds the affine
calculation described here. Its mathematical certificate comes from:

- Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its Applications* 569 (2019), 15–37,
  [DOI 10.1016/j.laa.2018.12.025](https://doi.org/10.1016/j.laa.2018.12.025), especially Theorem 4.6 and Algorithms 1–2.

The forbidden-support traversal was inherited through Ceiling-Pruned Dickinson from FracESSA's `NonCircularSupportGenerator`,
locally reconstructed from FracESSA revision `95e0ec019cf11a60c6423508e8768536a0b88860`, file
`cpp/include/fracessa/supports.hpp`. Its candidate-search origin is described by Immanuel M. Bomze, “Detecting All Evolutionarily
Stable Strategies,” *Journal of Optimization Theory and Applications* 75(2), 1992, 313–329.

The derivation and its relation to singular lifting, kernel circuits, and later nonsingular supports are recorded in
`aidocs/SINGULAR_LIFT_DICKINSON_RESEARCH.md`.

## Notation And Dickinson Certificates

Let $A\in\mathbb Z^{n\times n}$ be nonempty and symmetric. For $I\subseteq[n]$, let $A_I$ be the principal matrix on $I$. Embed a
vector $u_I$ into $\mathbb R^n$ by putting zeros outside $I$, and define

$$
L(u)=\operatorname{supp}(u),
\qquad
U(u)=\{j:(Au)_j\geq0\}.
$$

Dickinson's certificate covers every support in the Boolean interval $[L(u),U(u)]$. This model stores only **ceiling
certificates**, for which $U(u)=[n]$. Such a certificate forbids every later support containing $L(u)$.

The support generator visits nonempty supports in increasing cardinality. Certificates found while processing cardinality $k$
remain pending until all roots of cardinality $k$ have finished. They become active only at cardinality $k+1$, so an extra affine
certificate cannot suppress a peer root in the layer that discovered it.

## Ordinary Root Calculation

For a nonsingular $A_I$, the model solves $A_Iw=\mathbf1$ with retained exact fraction-free LDLT. If $w\leq0$, then $-w\geq0$ is a
negative quadratic witness and both CP and SCP are false. Otherwise the model computes the outside products exactly and retains the
certificate only if they are all nonnegative.

For a singular $A_I$, the model first performs the affine calculation below. It then takes one exact nonzero $w\in\ker A_I$ and
orients it to have a positive component. A one-sided kernel vector is a nonnegative zero and therefore disproves SCP. Its ceiling
condition is checked exactly. When the nullity is greater than one, the copied kernel-cone calculation subsequently searches further
homogeneous directions.

## Exact Consistency Test

Let $Z\in\mathbb Z^{|I|\times q}$ contain an exact basis of $\ker A_I$. Because $A_I$ is symmetric,

$$
\operatorname{range}(A_I)=(\ker A_I)^\perp.
$$

Therefore

$$
A_Ix=\mathbf1\text{ is consistent}
\quad\Longleftrightarrow\quad
z^T\mathbf1=0\text{ for every }z\in\ker A_I.
$$

The implementation checks the coordinate sum of every column of $Z$. A nonzero sum proves inconsistency and ends only the affine
step. If every sum is zero, the retained singular LDLT factorization solves the consistent system without refactoring $A_I$. It
returns an integer numerator $X$ and a positive integer denominator $D$ satisfying

$$
A_IX=D\mathbf1.
$$

Positive scaling is irrelevant to signs and support, so the algorithm works directly with $X$.

## Complete Search When The Nullity Is One

Let $z$ span $\ker A_I$. Every solution with the same positive right-hand-side scale is

$$
x(t)=X+t z,
\qquad t\in\mathbb R.
$$

For an outside row $j\notin I$, define

$$
b_j=A_{j,I}X,
\qquad
g_j=A_{j,I}z.
$$

The ceiling condition is the exact one-dimensional system

$$
b_j+t g_j\geq0\qquad(j\notin I).
$$

Each inequality gives a lower bound, an upper bound, no restriction, or immediate infeasibility. Intersecting them produces one
closed interval, possibly unbounded. All bounds are stored as integer numerator-denominator pairs with positive denominators and
compared by cross multiplication; no rational or floating-point approximation is used.

Within a nonempty feasible interval, coordinate $i$ becomes zero only at

$$
t_i=-\frac{X_i}{z_i}
$$

when $z_i\ne0$. The implementation retains the feasible breakpoints, sorts them exactly, and selects a value shared by the largest
number of coordinates. Coordinates with $X_i=z_i=0$ are zero for every $t$ and do not affect this comparison. Thus the selected
member has minimum support among all feasible vectors on this affine line. If no coordinate breakpoint is feasible, every feasible
member has the same support; the implementation uses $t=0$ when possible and otherwise one finite interval endpoint.

For $t=a/b$ with $b>0$, the stored integer candidate is

$$
u_I=bX+a z.
$$

It satisfies $A_Iu_I=bD\mathbf1>0$ and, by construction, every outside product is nonnegative. Hence its zero extension has
$U(u)=[n]$ and gives a sound ceiling certificate with lower endpoint $L(u)$.

## Partial Search When The Nullity Is Greater Than One

For $q>1$, the feasible affine family is the polyhedron

$$
\{X+Zy:A_{[n]\setminus I,I}(X+Zy)\geq0\}.
$$

This first companion model does not add an exact general-purpose polyhedral solver. It tests only the particular solution $X$
obtained by setting the free transformed LDLT coordinates to zero. If all outside products are nonnegative, $X$ gives a ceiling
certificate. Otherwise the affine step ends without pruning.

This restriction affects performance only. It does not affect correctness or completeness of the full copositivity decision because
the ordinary Dickinson traversal continues. A future experiment may search vertices or circuits of this higher-dimensional affine
polyhedron if measurements justify the additional exact arithmetic.

## Copied Homogeneous Kernel-Cone Search

For $q>1$, after the affine step and the ordinary selected null vector, the model also searches homogeneous directions. With

$$
G=A_{[n]\setminus I,I}Z,
$$

the feasible coefficient cone is $\{y:Gy\geq0\}$.

- If $\ker G\ne\{0\}$, the model obtains a full-matrix kernel vector and greedily shrinks its nonzero coordinates to one exact
  dependent-column circuit before emitting a certificate.
- If $\ker G=\{0\}$, the cone is pointed. Zero projected rows and positively proportional duplicate inequalities are removed
  exactly. For $q=2$, an angular cross-product sweep tests the two cone boundaries while preserving all one-sided-zero checks. For
  $q>2$, opposite inequalities remain distinct for feasibility but share one active equality hyperplane. Every $(q-1)$-hyperplane
  active set with one-dimensional nullspace supplies a possible extreme ray; rays are normalized, deduplicated, tested in both
  orientations, and retained only after exact feasibility checks.

No lifted support graph is built. Directions requiring new nonzero coordinates outside $I$ remain the responsibility of the
ordinary traversal.

## Complete Decision Flow

1. Visit nonempty root supports in increasing cardinality, skipping supports forbidden by certificates from earlier layers.
2. Factor $A_I$ exactly.
3. If it is nonsingular, solve $A_Iw=\mathbf1$, reject a nonpositive solution, and retain a ceiling certificate when possible.
4. If it is singular, build a complete exact nullspace basis.
5. Test consistency of $A_Ix=\mathbf1$ from the basis sums.
6. If consistent and $q=1$, search the complete feasible affine line and retain a minimum-support ceiling certificate.
7. If consistent and $q>1$, test the retained factorization's one particular affine solution.
8. Perform the ordinary singular Dickinson null-vector calculation and record any nonnegative zero.
9. If $q>1$, perform the copied homogeneous kernel-cone search.
10. Activate all certificates from this root layer only when the next cardinality begins.
11. Return after a decisive negative or zero witness in the requested mode, or after every non-forbidden support is exhausted.

## Correctness And Predicate Decisions

The consistency criterion is exact for symmetric matrices. Every affine certificate admitted by the model satisfies a positive
constant product on its root coordinates and nonnegative products outside the root, so its upper endpoint is exactly the full index
set. The affine calculation therefore adds only sound Dickinson certificates.

An affine candidate with every coordinate nonpositive is a negative witness: after negation it is nonnegative, and its quadratic
value is negative because its product on $I$ is a positive constant. Such a candidate rejects CP and SCP. Homogeneous one-sided
vectors have quadratic value zero, so they reject SCP but not CP.

Combined mode records these outcomes in one support traversal. A timeout or resource limit remains unresolved and is never converted
to `false`.

## Exact Arithmetic, Storage, And Diagnostics

All factorizations, nullspaces, products, bound comparisons, gcds, supports, and sign decisions use arbitrary-precision integers.
The singular consistent solve replays the retained fraction-free LDLT elimination and performs no second matrix factorization. The
nullity-one affine search costs one outside-product scan plus $O(|I|\log|I|)$ exact breakpoint sorting.

The outside-by-nullspace product $G$ is built lazily. An inconsistent affine system, or a higher-nullity particular solution that
already decides or certifies the root, does not pay for $G$ unless the later homogeneous kernel-cone search actually needs it.

Matrices and arithmetic scratch are reused across supports. Cooperative timeout checkpoints cover support generation, exact
factorization and solves, outside products, affine bounds, breakpoint construction, circuit reduction, active-set enumeration, and
nullspace calls.

Standard diagnostics report visited, covered, and processed supports, retained certificates, and the joint root-cardinality and
free-index distribution. Focused test-only diagnostics additionally distinguish consistent and inconsistent affine systems, affine
certificate support sizes, projected-constraint reduction, homogeneous rays, persistent circuits, and discarded certificates. The
test-only events are compiled out of production builds.

## Known Difficult Inputs

The affine test adds work at every singular root. If most singular systems are inconsistent, or if their feasible affine candidates
do not reach the ceiling, the model pays for a full nullspace basis and obtains no new pruning. The coordinate-sum consistency check
prevents the more expensive solve in the inconsistent case, but it cannot avoid the basis construction that also serves the
homogeneous search.

For nullity greater than one, the tested particular solution depends on the deterministic factorization coordinates and may miss a
much better member of the affine polyhedron. This is the principal mathematical incompleteness of the shortcut, although not of the
overall Dickinson decision.

The copied homogeneous active-set search can inspect combinatorially many row sets when $q>2$. Highly symmetric singular matrices,
large exact entries, and many dependent projected inequalities can make that cost dominate.

Finally, the model retains only ceiling certificates. Ordinary Dickinson intervals with smaller upper endpoints may provide better
local pruning on many inputs. Affine-Companion Dickinson is therefore an experiment in recovering unusually wide certificates, not
a claim that ceiling-only storage dominates ordinary Dickinson.
