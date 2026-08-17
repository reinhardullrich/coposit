# Upper-Endpoint CBDD Dickinson

Classification: coposit-created exact CP/SCP experiment. It copies `cbdd_dickinson` and adds one bounded lookahead operation: before
activating a newly computed Dickinson interval `[L,U]`, it processes the principal support `U` once when `U` is larger than the
current support and `A_U` is nonsingular.

The model supports non-strict copositivity (CP), strict copositivity (SCP), and combined classification of both predicates in one
traversal. Combined classification is the default when the analysis interface omits the mode.

## Idea In Plain Language

A Dickinson vector found on a support `I` certifies every support between its true vector support `L` and the set `U` of indices whose
matrix products are nonnegative. Ordinary CBDD Dickinson immediately inserts that complete interval into its decision diagram. This
can hide a useful calculation: the largest covered support `U` may itself produce a second certificate reaching beyond the old upper
endpoint or using a smaller lower endpoint.

Upper-Endpoint CBDD Dickinson performs exactly one lookahead solve before inserting the original certificate:

1. Compute the valid interval `[L,U]` at the emitted support `I`.
2. If `U` is strictly larger than `I` and has not already been probed, factor `A_U`.
3. If `A_U` is nonsingular, solve `A_Uv=1` and construct its complete Dickinson interval.
4. Insert the probe interval, then insert the original interval.
5. Do not recursively probe the probe interval.

A singular upper endpoint is simply ignored by the lookahead. The unchanged CBDD traversal remains the complete correctness
fallback. The experiment therefore asks one narrow question: can one exact solve at the maximal support about to be hidden recover
enough additional interval coverage to repay its cost?

## Name, Status, And Sources

The identifier is `upper_endpoint_cbdd_dickinson`.

- “Upper endpoint” refers to probing `U=N_A(u)`, the maximal support in the newly generated Dickinson interval.
- “CBDD” identifies the chain-reduced binary decision diagram used for exact interval-union storage and uncovered-support generation.
- “Dickinson” identifies the certificate theorem and principal-support calculation.

This is not an algorithm published by Dickinson. It is an isolated coposit experiment copied from the local
`models/hadeler-based/cbdd_dickinson` implementation. The upper-endpoint probe is motivated by the exact opportunity-loss analysis in
`aidocs/ORDINARY_DICKINSON_CERTIFICATE_ENGINE_RESEARCH.md`, including the positive-definite family in which an early bounded interval
hides a ceiling certificate at its own upper endpoint.

The mathematical and representation sources are:

- Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its Applications* 569 (2019), 15–37,
  [DOI 10.1016/j.laa.2018.12.025](https://doi.org/10.1016/j.laa.2018.12.025), especially Theorem 4.6 and Algorithms 1–2.
- Randal E. Bryant, “Graph-Based Algorithms for Boolean Function Manipulation,” *IEEE Transactions on Computers* C-35(8), 1986,
  677–691.
- Randal E. Bryant, “Chain Reduction for Binary and Zero-Suppressed Decision Diagrams,” 2017,
  [arXiv:1710.06500](https://arxiv.org/abs/1710.06500).

No external decision-diagram or lookahead implementation was copied. The CBDD is the local reconstruction already maintained by the
parent model; the upper-endpoint policy and its tests are local coposit additions.

## Dickinson Certificate

Let `A` be a symmetric integer matrix of order `n`. For a full vector `u`, define

$$
L(u)=\operatorname{supp}(u)=\{i:u_i\ne0\}
$$

and

$$
U(u)=N_A(u)=\{i:(Au)_i\ge0\}.
$$

An admissible vector has at least one positive coordinate. Dickinson's theorem certifies the Boolean interval

$$
[L,U]=\{J:L\subseteq J\subseteq U\}.
$$

The current processed support `I` always satisfies

$$
L\subseteq I\subseteq U,
$$

because the vector is supported inside `I` and its products on `I` are either `1` in the nonsingular case or `0` in the singular
case. Thus `U` is a valid principal support and is the unique largest support covered by this particular interval.

## Base Cardinality Traversal

The copied CBDD traversal visits nonempty supports in increasing cardinality. For each cardinality `k`, a CBDD represents all
`k`-element supports. A second CBDD represents the union `C` of every activated Dickinson interval. The remaining family is

$$
R_k=K_k\setminus C.
$$

The first low-before-high path in `R_k` gives the next support. Within one cardinality this preserves increasing numeric-mask order.
After a valid interval is inserted, CBDD union extends `C` and CBDD difference removes the interval from the active `R_k`.

CBDD nodes store a top and bottom variable together with low and high children. Consecutive forced-absent variables share one OR-chain
node. A unique table canonicalizes `(top,bottom,low,high)` nodes, and each union or difference operation memoizes operand pairs.

The lookahead does not alter this representation or enumeration order. It only inserts one additional valid interval before the
interval that triggered the probe.

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
matrix call ends, so this reduces later union and difference operands but is not garbage collection.

## Exact Calculation At An Emitted Support

For an emitted support `I`, copy and factor the principal matrix `A_I` using exact fraction-free LDLT.

### Nonsingular support

Solve

$$
A_Iu_I=\mathbf1.
$$

The implementation stores integer numerators with one positive common denominator. The denominator can be omitted from every sign
and support test.

If

$$
u_I\le0,
$$

then `-u_I` is a nonnegative negative quadratic witness. Both CP and SCP are false and traversal stops.

Otherwise embed `u_I` by zero outside `I`, compute `Au` exactly, and construct `[L(u),U(u)]`.

### Singular support

Recover one exact nonzero vector in `ker A_I` and orient it to have a positive coordinate. If it is nonnegative, it is a nonnegative
zero and disproves SCP. Combined classification records that fact and continues the same traversal to decide CP.

The oriented kernel vector still creates its ordinary Dickinson interval. The upper-endpoint policy is applied to that interval in
the same way as to a nonsingular root certificate; only the probed matrix `A_U` is required to be nonsingular.

## The Upper-Endpoint Probe

Let the just-computed root certificate be `[L,U]`.

### Eligibility

The probe is skipped when

$$
|U|=|I|.
$$

In this case `I=U`, so the interval hides no strict principal superset of its generating support.

The model also keeps a call-local ordered set of probed upper supports. If the same `U` arises from another certificate, its principal
matrix and deterministic nonsingular solution would be identical, so the duplicate probe is skipped exactly.

### Nonsingular probe

For a new larger upper support, copy and factor `A_U`. If it is nonsingular, solve

$$
A_Uv=\mathbf1.
$$

If `v<=0`, then `-v` is an exact nonnegative negative witness and both predicates are false.

Otherwise embed `v`, compute

$$
L_U=L(v),
\qquad
U_U=U(v),
$$

and activate the exact interval

$$
[L_U,U_U].
$$

The new upper endpoint `U_U` may contain indices outside the original `U`; the new lower endpoint `L_U` may also omit indices required
by the original `L`. Either phenomenon can make the probe interval non-dominated. The CBDD union handles overlap and global
redundancy exactly; the model does not estimate interval value from its cardinality alone.

### Singular probe

If `A_U` is singular, the probe stops immediately. It does not extract a nullspace vector, solve an affine family, lift the support,
or invoke another model. Ordinary cardinality traversal remains responsible for any support not removed by valid intervals.

### No recursive lookahead

The interval generated at `U` is inserted directly. Its own upper endpoint is not probed. This restriction is part of the experiment,
not a mathematical requirement. It prevents one root certificate from starting an uncontrolled chain of increasingly large exact
factorizations.

## Worked Opportunity-Loss Example

Consider

$$
A=
\begin{pmatrix}
1&2&-1\\
2&5&-4\\
-1&-4&6
\end{pmatrix}.
$$

At the singleton `I={1}`, the solution is `u_1=1`. Its complete products are `(1,2,-1)`, so

$$
[L,U]=[\{1\},\{1,2\}].
$$

Activating this interval hides the support `{1,2}`. The upper-endpoint probe instead solves

$$
\begin{pmatrix}1&2\\2&5\end{pmatrix}^{-1}\mathbf1
=
\begin{pmatrix}3\\-1\end{pmatrix}.
$$

Its product with the third row is

$$
(-1,-4)
\begin{pmatrix}3\\-1\end{pmatrix}
=1,
$$

so the probe produces

$$
[\{1,2\},\{1,2,3\}].
$$

This is information that the original interval does not contain. The example uses a positive-definite matrix, demonstrating that
nonsingularity and positive definiteness do not make immediate interval activation opportunity-free.

## Complete Decision Flow

1. Initialize the CBDD covered family and the requested CP/SCP classification state.
2. Visit uncovered supports in increasing cardinality and low-before-high CBDD order.
3. Factor `A_I` and construct the ordinary Dickinson vector or a decisive negative witness.
4. Record a nonnegative singular vector as an SCP counterexample while continuing CP classification when required.
5. Construct the exact root interval `[L,U]`, but do not activate it yet.
6. When `U` is a new strict superset of `I`, factor `A_U`.
7. If `A_U` is nonsingular, solve it, stop on a negative witness, or activate its exact interval.
8. If `A_U` is singular, take no lookahead action.
9. Activate the original interval `[L,U]`.
10. Continue until a decisive witness is found or every remaining support is exhausted.

The probe interval is activated before the root interval because the calculation is specifically intended to occur before `[L,U]`
can hide its upper endpoint.

## Exactness, Diagnostics, And Termination

All factorizations, solves, products, signs, supports, and CBDD operations are exact. No floating-point approximation or tolerance is
used. Supports use the shared dynamic packed representation and impose no fixed dimension limit.

Cooperative timeout checks occur during support generation, principal copying, factorization, product scans, and CBDD operations. A
timeout remains unresolved and is never converted to a negative classification.

Production diagnostics record both root and probe certificates in the ordinary `(k,d,|U|)` distribution, using the support
cardinality where each system was actually solved. Test-only source diagnostics distinguish probe attempts, duplicate upper endpoints,
singular upper endpoints, probe certificates, and probe negative witnesses.

Termination follows from three finite structures:

- the Boolean support lattice is finite;
- every accepted ordinary iteration removes at least its current support;
- each distinct upper endpoint is probed at most once, and probes never recurse.

## Known Difficult Inputs

The probe pays for an additional exact factorization whenever certificates expose many distinct larger upper endpoints. If those
principal matrices are singular, their intervals are globally redundant, or their new coverage is small, the model performs nearly
the complete base traversal after spending the additional arithmetic.

Large upper endpoints can be much more expensive to factor than the small root that produced them. Arbitrary-precision coefficient
growth may therefore dominate even though only one probe is attempted per support.

One upper-endpoint solve is not a complete lookahead method. A useful hidden certificate may live at an intermediate covered support,
may require a singular affine family, or may appear only after several extensions. Conversely, immediately activating the probe
certificate can itself hide another non-dominated certificate. The unchanged traversal preserves correctness but does not recover
all opportunity that a full covered-support search could discover.

Finally, the copied CBDD may still grow exponentially under an unfavorable variable order or a family of weakly shared intervals.
Upper-endpoint certificates can improve or worsen that representation depending on their overlap pattern; interval width alone does
not predict CBDD size.
