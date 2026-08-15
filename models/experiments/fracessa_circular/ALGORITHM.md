# FracESSA Circular Global-Minimum Model

Classification: coposit-created exact copositivity and strict-copositivity model for symmetric circulant matrices.

Input boundary: the model deliberately assumes that its matrix is circulant. It does not spend quadratic time checking that
precondition. The normal parser still guarantees a nonempty square symmetric matrix, and the experiment selector is responsible for
supplying only circular inputs. The common diagonal may be any exact value; it is not required to be zero.

## Idea In Plain Language

For an input matrix $A$, the model changes the sign and treats

$$
Q=-A
$$

as a symmetric game matrix. It searches the exact first-order stationary points of the quadratic objective on the standard simplex.
The largest stationary payoff of $Q$ is the negative of the global simplex minimum of $A$. Its sign gives all three possible results:

| Largest payoff of $Q$ | Global minimum of $A$ | Classification |
|---:|---:|---|
| positive | negative | not copositive |
| zero | zero | copositive but not strictly copositive |
| negative | positive | strictly copositive |

The ordinary `fracessa` experiment may inspect every surviving coordinate support. This circular version uses the same exact
candidate equations and payoff logic but removes symmetry-equivalent supports before solving them. Rotating or reflecting a support
does not create a different mathematical problem for a circular matrix, so the model generates one binary bracelet for each complete
rotation-and-reflection orbit. Some matrices have further exact index-multiplier symmetries; those equivalent bracelets are removed
as well.

## Name And Sources

The model is named `fracessa_circular` because it takes FracESSA's circular support path and applies the earlier coposit
global-minimum adaptation to it. It is an experiment, not a literature baseline and not the FracESSA program itself.

The implementation was adapted from the current local FracESSA revision
`3dafe81335b816feffdca47ef24b693054f40af5`, principally:

- `cpp/include/fracessa/support_generator_circular.hpp`;
- `cpp/include/fracessa/circular_affine_symmetry.hpp`;
- `cpp/src/fracessa.cpp`; and
- `cpp/tests/test_supports.cpp` plus its independent reference bracelet generator.

FracESSA's production bracelet recursion follows S. Karim, Z. Alamgir, and S. M. Husnine, “Generating Bracelets with Fixed
Density,” 2014. The exact-candidate and accepted-support pruning structure originates in Immanuel M. Bomze, “Detecting All
Evolutionarily Stable Strategies,” *Journal of Optimization Theory and Applications* 75(2), 1992, pages 313–329.

## Circular Input Contract

There is a row $c$ such that

$$
A_{ij}=c_{(j-i)\bmod n}.
$$

Because $A$ is symmetric,

$$
c_d=c_{n-d}.
$$

The diagonal is the independent value $c_0$. None of the circular reductions assumes $c_0=0$.

Every cyclic shift $i\mapsto i+b$ and reflection $i\mapsto -i+b$ is therefore an exact permutation symmetry of $A$ and $Q$.
It preserves candidate feasibility, payoff, support cardinality, and the strict-superset relation used for pruning.

The model accepts the normal integer matrix object. A compact FracESSA circular string and a full symmetric Matrix Market file reach
the same solver after parsing. No parser metadata is needed because this model's identity itself selects the circular path.

## Why The Simplex Minimum Decides Both Predicates

Every nonzero nonnegative vector can be divided by its positive coordinate sum to reach the standard simplex. Homogeneity then gives

$$
A\text{ is copositive}
\iff \min_{x\in\Delta_n}x^TAx\geq0,
$$

and

$$
A\text{ is strictly copositive}
\iff \min_{x\in\Delta_n}x^TAx>0.
$$

The simplex is compact, so a global minimum exists. Equivalently, a global maximum of $x^TQx$ exists and satisfies the first-order
conditions described next.

## Exact First-Order Candidate

For a nonempty support $S$, a full-simplex KKT candidate for maximizing $x^TQx$ satisfies

$$
Q_Sx_S=u\mathbf1,
\qquad
\mathbf1^Tx_S=1,
\qquad
x_S>0,
\qquad
(Qx)_k\leq u\quad(k\notin S).
$$

The common support payoff $u$ is also the objective value $x^TQx$. These are first-order conditions only. The model neither needs nor
computes ESS status, a Hessian inertia, or any local second-order classification.

The lowest index in $S$ is the reference strategy. Eliminating its probability gives one symmetric reduced system

$$
Hy=r,
$$

with

$$
H_{ij}=Q_{ij}-Q_{mj}+Q_{mm}-Q_{im},
\qquad
r_i=Q_{mm}-Q_{im}.
$$

The shared fraction-free LDLT implementation factors this exact integer matrix and solves one right-hand side. The candidate is
rejected if the system is singular, any recovered support probability is nonpositive, or any outside strategy has a payoff larger
than the common support payoff. No rational candidate vector is materialized.

Rejecting singular reduced systems does not lose the global value. Among all global maximizers choose one with inclusion-minimal
support. A null direction of its reduced system would preserve both normalization and objective value until one positive coordinate
reached zero, producing a global maximizer with smaller support. Thus at least one minimal global maximizer has a nonsingular reduced
system.

## Direct Bracelet Generation

A binary support word represents a subset of the coordinate indices. Two words are in the same bracelet when a cyclic rotation or a
reflection transforms one into the other. The generator emits only the numerically smallest support in each bracelet.

It does not enumerate $2^n$ raw supports and canonicalize them afterwards. FracESSA's fixed-density recursion constructs bracelet
representatives directly, one cardinality at a time. It carries:

- the positions of the selected bits;
- the current necklace period;
- the longest relevant palindromic prefix; and
- the comparison with the partially known reversed word.

A branch is rejected as soon as its reversal proves that it cannot be the smallest bracelet representative. This retains the direct
output-sensitive reduction used by FracESSA instead of paying for every member of every dihedral orbit.

## Exact Affine-Multiplier Reduction

Rotations and reflections are always present. A particular circular matrix may have more symmetry. For every multiplier $a$ with

$$
\gcd(a,n)=1,
$$

the index relabeling $i\mapsto ai\pmod n$ is tested exactly against row zero. It is retained only when

$$
A_{0d}=A_{0,(ad)\bmod n}
$$

for every offset $d$. Multipliers that differ only by sign are already related by reflection, so only one representative of each such
class is needed.

Before solving a generated bracelet, the model transforms it by every retained multiplier and rejects it if any transformed
rotation or reflection is numerically smaller. This is an exact orbit test, not a heuristic based on repeated values.

## Candidate-Orbit Pruning

When one bracelet support produces a KKT candidate, all its symmetry images are also candidates with the same payoff. Every strict
superset of an accepted candidate support can be omitted from the later search. To apply that rule correctly, the model must register
the complete orbit, not only the canonical bracelet.

For each distinct affine image, the implementation first finds its canonical bracelet and then stores every distinct rotation and
reflection in the forbidden-support buckets. During later bracelet construction, a branch is pruned at the first moment its partial
support completes any forbidden subset.

This full expansion is necessary. A larger canonical bracelet may contain a rotated or reflected candidate without containing the
candidate's canonical orientation.

The payoff proof is the same as in the ordinary FracESSA adaptation. If KKT candidates $p$ and $x$ have nested supports
$T\subseteq S$ and payoffs $u$ and $v$, KKT inequalities for $p$, equalities for $x$, and symmetry of $Q$ give

$$
v=p^TQx=x^TQp\leq u.
$$

Therefore a candidate with negative payoff can only have negative-payoff KKT candidates in its strict supersets. A zero-payoff
candidate can only have nonpositive-payoff candidates in its strict supersets. Those supersets cannot change the remaining CP or SCP
decision and may be pruned.

## Direct Tests Through Order Three

For every surviving bracelet representative of support size at most three, the model first applies the shared exact small-principal
criterion. A noncopositive principal face proves that the whole matrix is not copositive. A copositive but non-strict face proves that
the whole matrix is not strictly copositive, although non-strict copositivity still requires the remaining search.

The test is needed only on the representative because every omitted member is related by an exact matrix automorphism. It does not
create a forbidden-support rule unless the normal full KKT test also accepts the support.

## Complete Decision Flow

```text
assume A is a symmetric circulant matrix
Q = -A
zero_found = false
detect every exact affine multiplier symmetry of A

for each surviving fixed-density bracelet representative S:
    reject S if an affine image has a smaller bracelet representative
    if |S| <= 3:
        classify the principal face A_S exactly
        if A_S is not copositive: return not copositive
        if A_S is not strictly copositive: record zero_found
    solve the exact reduced KKT system on S
    reject S if it is singular, has a nonpositive support probability,
        or fails an outside-payoff inequality
    if the exact payoff of Q is positive: return not copositive
    if the exact payoff is zero: record zero_found
    install every affine, rotated, and reflected support image as a pruning rule

if zero_found: return copositive but not strictly copositive
return strictly copositive
```

For a strict-only query, the model returns immediately after the first exact zero or positive payoff. For a copositive-only or
combined query, equality is recorded and traversal continues until a positive payoff is found or the reduced search is exhausted.

## Representation And Termination

Supports use coposit's runtime-sized packed storage with

$$
\left\lceil\frac{n}{64}\right\rceil
$$

64-bit words. Rotation, reflection, numeric comparison, affine images, and subset checks work across word boundaries. There is no
fixed dimension-63 limit.

There are finitely many bracelet representatives, and every callback performs finite exact arithmetic. Forbidden-support pruning and
affine filtering only remove callbacks. The algorithm therefore terminates in exact arithmetic when no resource limit intervenes.
Cooperative timeout checkpoints return an unresolved timeout rather than a Boolean result.

When progress reporting is enabled, the model publishes its current support cardinality, emitted bracelet representatives, bracelets
discarded by exact affine symmetry, exact KKT systems examined, and accepted candidates. It deliberately reports no percentage:
forbidden-orbit pruning changes the future bracelet set during traversal, so there is no fixed denominator analogous to Dickinson's
$2^n-1$ raw supports. Disabled progress performs no atomic telemetry operations.

## Source Behavior And coposit Changes

Retained from current FracESSA:

- direct fixed-density bracelet generation;
- exact rotation/reflection orbit expansion for candidate pruning;
- exact matrix-preserving affine-multiplier detection and representative filtering;
- cardinality-first support order;
- exact reduced first-order equations and outside-payoff inequalities; and
- accepted-candidate strict-superset pruning.

Changed for this coposit experiment:

- use $Q=-A$ and classify the sign of the global minimum of $A$;
- support arbitrary common diagonal values;
- return CP and SCP decisions, including both in one traversal;
- stop early at the first sign that completes the selected query;
- remove all ESS output, candidate materialization, second-order work, and logging; and
- use coposit's dynamic packed support and shared exact LDLT implementation.

## Known Difficult Inputs

The reduction is useful only when the input really is circulant; applying the model to an ordinary symmetric matrix invalidates the
orbit equivalence and its answer has no contract.

A generic circulant row may preserve no affine multipliers beyond identity, leaving only the rotation/reflection reduction. Large
support boundary zeros can still require many bracelet representatives before equality is reached. Large arbitrary-precision entries
make each exact factorization expensive even when symmetry greatly reduces the number of supports. If accepted candidates are rare,
little forbidden-orbit pruning occurs and much of the bracelet space remains.
