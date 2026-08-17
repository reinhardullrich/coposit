# CBDD Dickinson Improved One

Classification: coposit-created exact CP/SCP experiment. It copies `cbdd_dickinson`, keeps the same cardinality-first traversal and
chain-reduced BDD representation, and changes only the certificates generated at a singular principal support.

The model supports copositivity (CP), strict copositivity (SCP), and combined classification of both predicates in one traversal.
Analysis and reference-run interfaces default to combined classification when the mode is omitted.

## Idea In Plain Language

Ordinary CBDD Dickinson chooses one vector when a principal matrix is singular. That choice can miss other vectors in the same
kernel whose Dickinson intervals cover different or larger families of supports.

CBDD Dickinson Improved One searches slightly more of the singular solution geometry before it activates any certificate:

1. it tests both orientations of a one-dimensional kernel;
2. it tests whether the singular affine system $A_Ix=\mathbf1$ is consistent and uses one exact particular solution;
3. at nullity two, it enumerates the complete finite family of kernel rays induced by all local-coordinate and outside-product
   hyperplanes;
4. it retains every resulting bounded Dickinson interval that adds coverage to the complete current CBDD union; and
5. it generates the whole local family before activating its intervals.

All other supports use the copied CBDD Dickinson calculation. Nullity above two deliberately retains the original one-vector
singular fallback.

## Name, Status, And Sources

The identifier is `cbdd_dickinson_improved_1`. “CBDD Dickinson” names the copied chain-reduced BDD traversal. “Improved One” is the
project's sequential label for this first certificate-engine experiment; it does not claim measured superiority.

This is not an algorithm published by Dickinson. It is a coposit-created variant copied from `models/hadeler-based/cbdd_dickinson`.
The interval theorem and principal-system decisions come from Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear
Algebra and its Applications* 569 (2019), 15–37, DOI `10.1016/j.laa.2018.12.025`, especially Theorem 4.6 and Algorithms 1–2.
The chain-reduced BDD representation follows Randal E. Bryant, “Chain Reduction for Binary and Zero-Suppressed Decision Diagrams,”
2017, arXiv `1710.06500`, together with the ordinary reduced ordered BDD rules from Bryant's 1986 paper.

The new singular-space derivation, proofs, counterexamples, and rejected extensions are recorded in
[`aidocs/ORDINARY_DICKINSON_CERTIFICATE_ENGINE_RESEARCH.md`](../../../aidocs/ORDINARY_DICKINSON_CERTIFICATE_ENGINE_RESEARCH.md).

## Notation

Let $A\in\mathbb Z^{n\times n}$ be nonempty and symmetric, and let

$$
[n]=\{1,\ldots,n\}.
$$

For a support $I\subseteq[n]$, $A_I$ is the corresponding principal matrix. For an embedded vector $u\in\mathbb R^n$, define

$$
L(u)=\operatorname{supp}(u)=\{i:u_i\neq0\}
$$

and

$$
U(u)=N_A(u)=\{i:(Au)_i\geq0\}.
$$

If $u$ has a positive component, Dickinson's theorem certifies every support in

$$
[L,U]=\{J:L\subseteq J\subseteq U\}.
$$

At a singular support $I$, write

$$
q=\dim\ker A_I.
$$

For $q=2$, let $Z\in\mathbb Z^{|I|\times2}$ be an exact basis of $\ker A_I$, let $R=[n]\setminus I$, and define

$$
G=A_{R,I}Z.
$$

Every root-confined kernel vector is $u_I=Zy$ for $y\in\mathbb R^2$. Its outside products are $Gy$.

## Unchanged CBDD Family Representation

The copied chain-reduced BDD represents the exact union of all retained intervals. A Boolean variable is true when its matrix index
belongs to a support. For one interval $[L,U]$:

- every index in $L$ is forced true;
- every index outside $U$ is forced false; and
- every index in $U\setminus L$ is optional.

Ordinary BDD reduction removes optional variables. CBDD chain reduction additionally compresses consecutive forced-false variables
that share one high continuation. Union and difference are exact range-aware APPLY operations with a unique table and per-operation
memoization.

For cardinality $k$, the model builds the exact family $K_k$ of $k$-element supports. If $C$ is the accumulated certificate union,
then

$$
R_k=K_k\setminus C
$$

contains precisely the supports still requiring an exact Dickinson calculation. The first low-before-high path through $R_k$ gives
the next support.

## Exact Principal Calculation

For each emitted support $I$, the model copies and factorizes $A_I$ with the shared fraction-free LDLT factorization.

### Nonsingular support

The model solves

$$
A_Iu_I=\mathbf1
$$

with an integer numerator vector and a positive common denominator. Positive scaling does not change $L$, $U$, or any sign test, so
the denominator need not be stored in the vector.

- If $u_I\leq0$, then $-u_I\geq0$ is an exact negative quadratic witness. CP and SCP are false.
- Otherwise the embedded vector produces the ordinary interval $[L(u),U(u)]$ exactly as in copied CBDD Dickinson.

### Singular support

The model first performs the affine companion test and then the nullity-specific homogeneous search described below. Every generated
candidate is checked exactly. The intervals remain pending until the complete local search finishes.

## Singular Affine Companion

Using the retained singular factorization, the model tests consistency of

$$
A_Ix=\mathbf1.
$$

If the system is inconsistent, the affine step contributes no candidate. If it is consistent, the factorization returns one integer
numerator solution $x_0$ with a positive common denominator and free transformed coordinates set to zero.

- If $x_0\leq0$, then

  $$
  (-x_0)^TA_I(-x_0)=x_0^T\mathbf1<0,
  $$

  so $-x_0$ is an exact non-copositivity witness and the model stops.
- If $x_0$ has a positive component, its zero extension gives an ordinary bounded Dickinson interval and is added to the pending
  local family.

This first experiment tests only that particular solution. It does not enumerate a nullity-one affine line or a higher-dimensional
affine hyperplane arrangement.

## Homogeneous Search By Nullity

### Nullity one: both orientations

The retained factorization returns one nonzero vector $w\in\ker A_I$. The model tests both $w$ and $-w$.

An orientation with no positive component is inadmissible and is ignored. A nonnegative orientation is a nonnegative zero of the
quadratic form: it disproves SCP immediately, while CP may continue and use its interval. A mixed-sign orientation is admissible and
produces its own interval. Mixed-sign opposite orientations have the same lower support but can have different upper sets.

### Nullity two: complete stacked-line search

The model forms the stacked row matrix

$$
H=
\begin{pmatrix}
Z\\
G
\end{pmatrix}.
$$

Each nonzero row $h=(a,b)$ defines the coefficient line

$$
h^Ty=0,
$$

represented by the exact perpendicular vector

$$
y=(b,-a)^T.
$$

The vector is divided by the greatest common divisor of its entries and given a canonical sign. This deduplicates proportional rows.
Both orientations of every remaining line are tested.

For each orientation, the local vector is $u_I=Zy$. Its lower endpoint is obtained from the nonzero entries of $u_I$. Every index in
$I$ belongs to the upper endpoint because $A_Iu_I=0$. For an outside index $j\in R$, the model uses the already projected product

$$
(Au)_j=G_{j,*}y
$$

instead of repeating the full matrix-vector product.

The stacked-flat domination theorem proves that, for every admissible root-confined kernel vector, one of these line candidates has
a lower endpoint no larger and an upper endpoint no smaller. Thus the enumerated family dominates every ordinary interval obtainable
from the two-dimensional root kernel.

A nonnegative line candidate has the same SCP meaning as in the nullity-one case. An orientation without a positive component is
ignored.

### Nullity greater than two: copied fallback

The model deliberately does not enumerate $\binom{n}{q-1}$ stacked flats. It recovers one exact nullspace vector, orients it to have
a positive component, performs the copied singular Dickinson decision, and generates its ordinary interval. This preserves the
original CBDD behavior while keeping the first experiment bounded.

## Local Batch And Exact Marginal Coverage

The affine and homogeneous searches first construct a local list of exact endpoint pairs. No local interval affects which other
candidate vectors are generated.

After generation, each interval $Q=[L,U]$ is tentatively united with the canonical CBDD root $C$. If

$$
C\cup Q=C,
$$

then $Q$ adds no support to the complete current union and is discarded. Canonical root equality makes this an exact global
redundancy test, including coverage supplied jointly by several incomparable intervals. No support is enumerated for the test.

Otherwise the new union root is retained and $Q$ is subtracted from the current cardinality family. Processing the local candidates
sequentially after generation preserves exactly the union of the whole pending family.

The diagnostics histogram records only intervals that add marginal coverage. Its tuple is

$$
(k,d,|U|,\text{count}),
\qquad
d=|U|-|L|,
$$

where $k=|I|$ is the support cardinality that generated the interval.

## Safe Cardinality Expiration

Each retained interval is tagged with $u=|U|$ and also united into an expiry bucket $E_u$. Before the model starts cardinality
$k$, it performs

$$
C\leftarrow C\setminus\bigcup_{u<k}E_u
$$

and clears those buckets. This cannot change any present or future decision: every $J\in[L,U]$ satisfies
$|J|\leq|U|=u$, so an interval with $u<k$ contains no support of cardinality $k$ or larger. Intersections with intervals that
remain live are harmless for the same reason; every removed support is below the traversal frontier.

Expiration changes only the live decision-diagram roots. The private arena and unique table retain already allocated nodes until the
matrix call ends, so this reduces later union and difference operands but is not garbage collection. After expiration, the marginal
coverage test may retain a later interval only because it reintroduces already-past supports; this can add work but cannot change a
current or future support decision.

## Complete Decision Flow

1. Traverse support cardinalities from one through $n$.
2. Construct the exact remaining CBDD family $R_k=K_k\setminus C$.
3. Extract the next uncovered support $I$ and factor $A_I$ exactly.
4. If $A_I$ is nonsingular, solve $A_Iu_I=\mathbf1$, reject a nonpositive solution, and otherwise queue its interval.
5. If $A_I$ is singular, test the affine companion. Stop on its exact negative witness or queue its admissible particular solution.
6. Search the homogeneous kernel according to nullity: both orientations for $q=1$, all distinct stacked lines for $q=2$, or the
   copied one-vector fallback for $q>2$.
7. In SCP or combined mode, record any nonnegative kernel vector as proof that SCP is false. Strict-only mode stops immediately.
8. After the complete local search, retain only intervals that add exact marginal CBDD coverage and subtract them from $R_k$.
9. Continue until a negative witness is found or every remaining support has been exhausted.
10. Exhaustion proves CP, SCP, or both according to the selected mode and any earlier nonnegative zero.

## Exact Representation And Termination

All matrix, nullspace, affine-solve, projected-product, greatest-common-divisor, and sign calculations use FLINT arbitrary-precision
integers. Supports use the shared dynamic packed representation and therefore have no fixed-width dimension limit.

Every retained interval is a valid Dickinson interval. Every accepted iteration removes at least the currently emitted support from
a finite cardinality family. Extra candidate searches can remove more supports but cannot remove an uncertified support, so the model
terminates when sufficient time and memory are available.

Combined classification is one traversal. It does not call separate CP and SCP predicates internally.

## Deliberately Excluded Ideas

This model does not perform:

- affine breakpoint enumeration;
- stacked-flat enumeration above nullity two;
- singular principal-superset lifting;
- covered-support or upper-endpoint look-ahead;
- delayed activation across support cardinalities;
- circuit-guided coordinate-adding lifts; or
- a SAT, ZDD, or plain BDD backend.

Those are separate experiments. Their absence cannot change correctness because the copied CBDD Dickinson traversal remains the
fallback.

## Known Difficult Inputs

Nullity-two supports can generate up to $n$ distinct unoriented stacked lines and up to two admissible candidates per line. Although
each outside endpoint scan then costs only $O(n)$ projected arithmetic operations, one support can still add quadratic exact work and
many incomparable intervals.

The affine particular solution may be weak even when another point in the same affine family gives a much broader interval. Nullity
above two still depends on one arbitrary exact kernel vector and therefore retains the copied model's opportunity loss there.

CBDD size remains sensitive to variable order and interval overlap. Scattered forced-absent indices may share little chain structure,
and nodes made unreachable by later union operations remain allocated until the matrix call ends. A stronger certificate engine can
therefore increase CBDD memory pressure when it emits many narrow, poorly aligned intervals.

Finally, bounded intervals can suppress a support whose later candidate would extend beyond the current upper endpoint. This model
does not implement covered-support look-ahead, so the positive-definite upper-escape family in the research note remains a known
limitation.
