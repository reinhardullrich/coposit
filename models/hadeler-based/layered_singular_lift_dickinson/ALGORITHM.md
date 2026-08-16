# Layered Singular-Lift Dickinson

Classification: coposit-created exact CP/SCP experiment. It copies `ceiling_pruned_dickinson`, then adds a depth-first search through
singular principal supersets when the ordinary singular calculation has nullity greater than one.

Mode boundary: `copositive` and `strictly_copositive` select one predicate; `both` classifies CP and SCP in one traversal and is the
default when an analysis or reference-run interface omits the mode.

## Idea In Plain Language

Dickinson associates an exact vector $u$ with a principal support and certifies every set between

$$
L(u)=\operatorname{supp}(u)
\quad\text{and}\quad
U(u)=\{j:(Au)_j\geq0\}.
$$

The copied ceiling-pruned model keeps only certificates with $U(u)=[n]$. Such a certificate says that every future support containing
$L(u)$ is already settled, so one packed forbidden set $L(u)$ can remove the entire upward family.

A singular principal matrix with a multidimensional nullspace presents a choice: the exact factorization returns only one nullspace
vector, and that arbitrary direction may stop below the ceiling even though another null direction would reach it. This experiment
does not optimize over the whole nullspace. Instead, it enlarges the principal support one index at a time. Every added row and column
adds exact constraints to the nullspace. The search follows singular children depth first until a child has nullity one. At nullity
one there is a unique projective direction, so the model checks both signs of that direction. If either sign reaches the ceiling, it
stores the support of that final vector.

The outer Dickinson traversal remains cardinality ordered. Certificates found while processing roots of cardinality $k$ remain
pending until every surviving root of cardinality $k$ has been processed. Thus a certificate found below root $\{1,2\}$ cannot hide
root $\{1,3\}$ before that second root has had its own chance to produce a better certificate.

## Name And Sources

The identifier is `layered_singular_lift_dickinson`.

- “Layered” names the barrier between discovery and activation: all ceiling certificates found from outer cardinality $k$ are
  activated together only when cardinality $k+1$ begins.
- “Singular lift” means enlarging a singular principal support through singular principal supersets until the nullspace has one
  dimension.
- “Dickinson” identifies the exact vector certificate that makes every retained prune valid.

The exact certificate and the principal-support decision come from Peter J. C. Dickinson, “A New Certificate for Copositivity,”
*Linear Algebra and its Applications* 569 (2019), 15–37,
[DOI 10.1016/j.laa.2018.12.025](https://doi.org/10.1016/j.laa.2018.12.025), especially Theorem 4.6 and Algorithms 1–2.

The cardinality-ordered forbidden-support generator comes from the local reconstruction in
`models/hadeler-based/ceiling_pruned_dickinson/solver.cpp`. Its traversal is mathematically copied from FracESSA's
`NonCircularSupportGenerator`, inspected locally at FracESSA revision `95e0ec019cf11a60c6423508e8768536a0b88860` in
`cpp/include/fracessa/supports.hpp`.

This model is not described by Dickinson's paper and is not a literature baseline. The singular-superset DFS, first-nullity-one
stopping rule, layer barrier, per-layer lifted-support cache, and pending-antichain minimization are coposit changes.

## Mathematical Problem And Notation

Let $A\in\mathbb Z^{n\times n}$ be symmetric and let $[n]=\{1,\ldots,n\}$. The model decides

$$
A\in\operatorname{COP}_n
\iff
x^TAx\geq0\quad\text{for every }x\geq0,
$$

and, when requested,

$$
A\in\operatorname{int}(\operatorname{COP}_n)
\iff
x^TAx>0\quad\text{for every nonzero }x\geq0.
$$

For a nonempty index set $I\subseteq[n]$, $A_I$ denotes the principal matrix on $I$. A local vector $w\in\mathbb R^I$ is embedded
in $\mathbb R^n$ by putting zero outside $I$; the embedded vector is called $u$. Define

$$
L(u)=\{i:u_i\ne0\},
\qquad
U(u)=N_A(u)=\{j:(Au)_j\geq0\}.
$$

Dickinson's vector certifies the Boolean interval

$$
[L(u),U(u)]=\{J\subseteq[n]:L(u)\subseteq J\subseteq U(u)\}.
$$

This model retains the interval only when $U(u)=[n]$. Its retained family is then

$$
[L(u),[n]]=\{J:L(u)\subseteq J\}.
$$

The lower endpoint is always the support of the vector that actually passed the ceiling test. It is not the outer root support and
not necessarily the lifted principal support.

The algorithm uses two distinct cardinalities. The **root $k$** is $k=|I|$, where ordinary Dickinson started the lift. The
**lifted $k$** is $t=|T|$ for the principal support $T\supseteq I$ where a candidate vector is actually obtained. For every stored
certificate the diagnostics retain $(k,t,|U|,|L|)$ explicitly.

## Outer Cardinality Traversal

The outer generator visits nonempty supports $I$ in increasing cardinality. Within one cardinality it recursively chooses indices
from high to low, exploring exclusion before inclusion; this is increasing numeric-mask order.

Active forbidden supports are bucketed by their smallest index. When the current recursive partial support gains index $i$, only the
bucket whose smallest index is $i$ can become newly contained. If an active forbidden support $F$ is contained, every completion of
that branch contains $F$, so the generator skips the entire branch and exactly counts its cardinality-$k$ completions.

New certificates are not active immediately. They enter a pending collection. At the start of the next outer cardinality:

1. the pending supports are moved into the active lowest-index buckets;
2. generation starts for the new cardinality.

The delay is essential to the experiment's purpose. Immediate activation is mathematically safe because it removes only certified
supports, but it can remove another root of the current cardinality whose singular lift would produce a smaller lower endpoint and
therefore a stronger future prune.

Once a pending lower becomes active, it also suppresses a future lifted state that contains that lower. This avoids factoring a
principal support already covered by an earlier layer. Pending lowers do not participate in this lifted-state test: during their own
discovery layer, the search remains frozen so overlapping roots and lifted states can still expose a stronger lower endpoint.

The pending collection is kept as a minimal antichain. For two pending lower supports $F$ and $G$:

- if $F\subseteq G$, then $G$ is redundant and is not inserted;
- if $G\subset F$, then $F$ is removed and $G$ is retained.

This gives the largest valid upward prune represented by the discovered pending supports without changing the layer barrier.

## Ordinary Dickinson Calculation At A Root

For every emitted outer support $I$, copy and exactly factor $A_I$.

### Nonsingular root

If $A_I$ is nonsingular, solve

$$
A_Iw=\mathbf1.
$$

The fraction-free LDLT solve produces integer numerators and one positive denominator. The denominator can be ignored for support and
sign tests because it scales the vector positively. If $w\leq0$, Dickinson's decision rule supplies a non-copositivity witness and
the model stops with CP and SCP both false. Otherwise the vector receives the ceiling test below.

### Singular root

If $A_I$ is singular, the retained exact factorization constructs one nonzero $w\in\ker(A_I)$ and orients it to have a positive
component. A nonnegative $w$ is a nonzero nonnegative zero:

$$
w\geq0,
\qquad
w^TA_Iw=0.
$$

It disproves SCP but not CP. A strict-only call stops immediately. Combined classification records `strictly_copositive=false` and
continues deciding CP.

The ordinary vector is still tested for a ceiling certificate. If the nullity is one, root processing then ends. If the nullity is
greater than one, the singular-lifting DFS begins from $I$.

## Singular-Lifting DFS

Suppose the current singular support is $T$ and

$$
q(T)=|T|-\operatorname{rank}(A_T)>1.
$$

For every index $j\notin T$, in increasing index order, form the principal child

$$
T'=T\cup\{j\}.
$$

Each child is factored exactly.

- If $A_{T'}$ is nonsingular, that branch ends.
- If $A_{T'}$ is singular and has nullity greater than one, recurse immediately into $T'$ before considering the next sibling.
- If $A_{T'}$ has nullity one, recover its unique exact kernel ray, test it, and end that branch.

Every first-visited singular support examines every index outside it. Different addition orders and different outer roots can reach
the same lifted support, so a call-wide `std::set` of packed supports records every lifted state. The exact principal system and its
complete descendant search are performed only on the first visit. Later routes skip the state and its whole subtree even when the
first visit found no certificate: $A_T$, its nullspace, and the child family $\{T\cup\{j\}:j\notin T\}$ depend only on $T$, not on
the route used to reach it. Outer roots remain eligible because the cache suppresses only repeated lifting work, not ordinary outer
Dickinson processing.

The DFS deliberately does not restrict a state's children to indices larger than the last added index. Such a canonical path rule
would be unsafe here: a smaller-index intermediate support can be nonsingular and terminate one ordering even though adding that
index directly to a different singular state produces a singular child. Enumerating every outgoing child once and deduplicating the
resulting state gives the complete singular-support directed acyclic graph.

Bordering a symmetric $m\times m$ matrix by one row and column can raise rank by at most two while dimension rises by one. Therefore
nullity can fall by at most one per lift:

$$
q(T')\geq q(T)-1.
$$

In particular, a nullity-three root cannot become nullity one after one added index; it needs at least two lifts. This is why the
order-45 worked example below first succeeds on an order-eight principal support rather than order seven.

The search deliberately stops at the first nullity-one matrices on each branch. It does not continue through a nullity-one child
that fails the ceiling test, and it does not search arbitrary linear combinations while nullity remains greater than one. Those are
separate possible experiments, not hidden behavior in this model.

## Nullity-One Direction And Both Signs

When $A_T$ has nullity one, every nonzero kernel vector is a scalar multiple of one exact vector $w$. Positive scaling changes no
sign, while negative scaling exchanges the two orientations. Therefore:

- if one orientation is nonnegative, record failure of SCP and test that nonnegative orientation;
- if $w$ has mixed signs, test both $w$ and $-w$ because their lower supports agree but their outside product signs are opposite.

This exhausts all projectively distinct null directions at that lifted support. It does not claim that the lifted support was the
best possible enlargement of the outer root; the DFS visits every singular enlargement reachable before a nullity-one stop.

## Ceiling Test And The Stored Lower Support

For a candidate on support $T$, the principal equations already determine the products on $T$. In the singular case they are zero.
The model computes every outside product exactly:

$$
(Au)_r=\sum_{i\in T}a_{ri}w_i,
\qquad r\notin T.
$$

The first negative product rejects that orientation because $U(u)\ne[n]$. No bounded Dickinson interval is stored.

If every outside product is nonnegative, then $U(u)=[n]$. The model constructs

$$
L(u)=\{i\in T:w_i\ne0\}
$$

and submits this exact lower support to the pending antichain. Zero coordinates are omitted. If the lifted matrix has cardinality
$t$ but the kernel vector has only $\ell<t$ nonzero coordinates, the stored set has cardinality $\ell$. Storing $T$ instead would
discard valid pruning strength. Storing the original root would generally be invalid because the certificate theorem proves the
interval from $L(u)$, not from an arbitrary subset of it.

## Worked Singular-Lift Structure

Corpus matrix 9647 is an order-45 MANN/Steiner instance. One observed singular root has cardinality six and nullity three. The exact
factorization's one-vector rule selects a two-coordinate kernel direction whose upper endpoint has size 44, so the copied
ceiling-only model discards it.

A successful depth-two lift uses

$$
T=\{1,4,13,14,19,20,22,23\},
$$

where $A_T$ has rank seven and nullity one. Its unique kernel direction has

$$
L(u)=\{13,14,19,20,22,23\},
\qquad
U(u)=[45].
$$

The model therefore queues the cardinality-six set $L(u)$, not the cardinality-eight lifted support $T$. Once its outer layer has
finished, that one lower support forbids every later support containing those six indices.

## Diagnostics

The model uses the standard support diagnostics:

- `visited`: outer supports emitted plus outer branches skipped by active lower supports;
- `covered`: the outer supports represented by those skipped branches;
- `processed`: all exact principal systems, equal to `outer_processed + lifted_processed`;
- `outer_processed`: exact systems from the ordinary cardinality traversal;
- `lifted_processed`: first-visited lifted principal systems;
- `lift_duplicate_skips` and `lift_covered_skips`: lifted routes discarded by the call-wide cache or an active lower support;
- `lift_cache_size`: distinct lifted supports remembered by the call-wide cache;
- `lift_dimension` and `lift_depth`: the most recently published lifted $k$ and its distance from the root $k$;
- `lift_maximum_dimension` and `lift_maximum_depth`: the largest lifted $k$ and distance reached;
- `certificates`: ceiling lower supports accepted when discovered; a later smaller pending lower can supersede an earlier counted
  lower before activation; and
- `certificate_root_k_lifted_k_u_l_counts`: accepted certificates grouped as
  `(root_k,lifted_k,|U|,|L|,count)`.

Because lifted systems are extra exact work outside the outer generator, `processed` can exceed `visited`. `visited` and `covered`
continue to describe only the ordinary Boolean-lattice traversal. In this ceiling-only experiment $|U|=n$, but it remains explicit
so the record states the complete certificate geometry. The diagnostics do not claim an ETA.

## Exactness, Correctness, And Termination

All factorization, nullspace recovery, matrix-vector products, comparisons, and signs use arbitrary-precision integers. There is no
floating-point arithmetic or tolerance.

Every removed support contains a lower endpoint $L(u)$ of an exact Dickinson certificate with $U(u)=[n]$. It therefore lies in the
certified interval $[L(u),[n]]$. The lifting search can discover additional valid certificates and can change when later supports are
skipped, but it cannot turn an uncertified support into a removed support.

For fixed $n$, both the outer support family and the singular-superset graph are finite. The call-wide visited set prevents repeated
paths from refactoring or expanding the same lifted support. Given sufficient time and memory, the model terminates with the same CP/SCP
classification as exhaustive Dickinson traversal.

## Known Difficult Inputs

The lifting tree can be enormous. If many principal supersets remain singular with nullity greater than one and few nullity-one
directions reach the ceiling, the model pays for a large additional family of exact factorizations without gaining pruning. The
call-wide visited set prevents duplicate work but can itself approach the size of a large part of the Boolean lattice.

The model is especially vulnerable when singular structure is abundant but its useful kernel combinations require searching inside
a high-dimensional nullspace rather than adding principal constraints. The DFS does not optimize arbitrary basis combinations, so it
may explore many lifts before a unique direction appears.

Conversely, unstructured matrices are usually nonsingular on most supports. They obtain little or no lifting benefit because every
lift branch ends immediately, while the root factorization still follows the copied ceiling-pruned algorithm.

Corpus matrix 9647 is both the motivating example and a warning: it contains useful depth-two lifts, but its MANN/Steiner symmetry
also creates many overlapping singular paths. The cache avoids refactoring the same lifted support through any later route, yet the
number of distinct lifted supports can still dominate runtime and memory.
