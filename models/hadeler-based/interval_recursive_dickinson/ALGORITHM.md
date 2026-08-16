# Interval-Recursive Dickinson

Classification: coposit-created exact CP/SCP classification variant of Dickinson 2019.

Mode boundary: `copositive` and `strictly_copositive` select one predicate; `both` classifies both in one traversal and is the
analysis-interface default.

## Idea In Plain Language

Dickinson associates an uncovered support with a certificate interval. Every support inside that interval needs no principal-system
calculation. Dickinson 2019 nevertheless constructs every support and asks afterward whether a certificate covers it.

This model changes only that enumeration step. It recursively chooses excluded and included indices. Each certificate is installed
as a forbidden Boolean cube. As soon as a recursive branch is wholly inside one cube, the complete branch is abandoned before its
individual supports are constructed. Only an uncovered leaf is converted to an index vector and sent to Dickinson's unchanged
exact calculation.

## Name And Sources

“Interval-recursive” describes the implementation: Dickinson intervals guide a recursive support generator. The model began as a
complete copy of `models/hadeler-based/dickinson_2019`; the exact linear algebra, sign decisions, witnesses, and certificate formula remain the
same.

The mathematical certificate comes from Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its
Applications* 569 (2019), 15–37, DOI `10.1016/j.laa.2018.12.025`, especially Algorithms 1–2 and Theorem 4.6. The recursive
exclude/include order follows FracESSA's non-circular forbidden-support generator, locally preserved in
`models/hadeler-based/fracessa/solver.cpp`. General lower-and-upper blocking is the standard Boolean-cube interpretation used by
AllSAT enumeration; one cube's complement is one blocking clause.

## Certificate Intervals

For a nonempty support $I$, Dickinson computes a vector $u$ supported within $I$. Define

$$
L(u)=\operatorname{supp}(u),
\qquad
U(u)=N_A(u)=\{i:(Au)_i\geq0\}.
$$

The certificate covers exactly

$$
[L(u),U(u)]=\{J:L(u)\subseteq J\subseteq U(u)\}.
$$

For each index, this interval has one of three states:

- required present when the index lies in $L$;
- required absent when the index lies outside $U$;
- optional when the index lies in $U\setminus L$.

Thus the interval is one Boolean cube. Avoiding it is equivalent to satisfying the blocking clause

$$
\left(\bigvee_{i\in L}\neg x_i\right)\lor
\left(\bigvee_{j\notin U}x_j\right).
$$

Many certificates form a union of cubes. A support is emitted only when it belongs to none of them.

## Recursive Support State

The outer loop retains Dickinson 2019's increasing cardinalities $k=1,\ldots,n$. For one $k$, recursion visits bit positions from
high to low and takes the exclusion branch before inclusion. Unpruned leaves therefore retain the baseline's increasing numeric-mask
order.

The mutable state is:

- one packed partial support;
- the number of lower bits still undecided;
- the number of further included bits required to reach $k$;
- every certificate interval stored as packed lower and upper supports;
- certificate buckets keyed by the lowest coordinate on which the cube is fixed.

A cube becomes fully determined when its lowest fixed coordinate is assigned. Only that bucket is inspected. If all required-present
bits are selected and no required-absent bit is selected, every descendant lies in the interval and the branch stops.

Certificates are inserted immediately after their support is processed. When an exclusion subtree creates new certificates, each
ancestor checks only those new intervals before entering its inclusion subtree. This catches a new cube whose fixed coordinates were
already decided above the current recursion point. At a completed leaf, a bucketed exact interval-membership check is retained as a
safety-complete final test; covered leaves are not passed to the matrix calculation.

Unlike the upward-only FracESSA rule, absence of uncovered supports at one cardinality does not permit global termination. A bounded
upper endpoint can cover one lattice level without covering larger supports, so every cardinality is started independently.

## Unchanged Dickinson Calculation

For an emitted support $I$, copy the principal matrix $A_I$ once and factor it with the shared fraction-free LDLT implementation.

If $A_I$ is nonsingular, solve exactly

$$
A_Iu=\mathbf1.
$$

The solver returns integer numerators and a positive common denominator. Positive rescaling changes no sign and is discarded. If
$u\leq0$, then $-u$ is a nonnegative negative witness and the model returns `false`.

If $A_I$ is singular, recover one exact nullspace vector and orient it to have a positive component. A nonnegative oriented vector
is a nonnegative zero, so strict copositivity fails immediately.

Every other vector becomes a certificate. The implementation embeds its nonzero coordinates into $L$, computes the full exact
product $Au$, places every nonnegative product coordinate in $U$, and installs $[L,U]$ in the generator. No floating-point decision
or approximate interval is used.

## Complete Decision Flow

1. Receive a parser-guaranteed nonempty square symmetric integer matrix.
2. Reject a non-strict mode request.
3. For each support cardinality, recursively choose absent/present indices in numeric-mask order.
4. Stop a branch when a stored certificate interval contains every descendant.
5. At an uncovered leaf, create the reusable ascending index vector.
6. Run Dickinson 2019's exact principal solve or nullspace calculation.
7. Return `false` on a negative witness or nonnegative zero.
8. Otherwise compute and insert the exact lower/upper certificate interval immediately.
9. Return `true` only after every cardinality has no uncovered support left to process.

The shared connected-component and pre-check pipeline is outside the model. Selecting preprocessing through Python therefore applies
the same preprocessing implementation and settings used with Dickinson 2019.

## Exact Representation And Termination

Matrix arithmetic uses FLINT arbitrary-precision integers. Supports and both endpoints of every interval use the shared dynamic
packed representation with $\lceil n/64\rceil$ words, so there is no dimension-63 limit. The index vector is materialized only for an
uncovered support.

The recursive generator describes a finite binary tree for each cardinality and only deletes branches, so it terminates. Deleting a
branch is exact because one retained Dickinson interval contains every leaf below it. The mathematical conclusion is therefore the
same as strict Dickinson; only already-covered support construction is omitted.

## CP and SCP classification

The selected-predicate `solve` path and combined `classify` path use the same support traversal. A nonsingular subset whose exact
solution is componentwise nonpositive rejects CP and SCP. A singular subset with a nonzero componentwise nonnegative null vector
proves that SCP is false. Strict-only mode stops there; CP and combined mode retain the vector's ordinary Dickinson interval and
continue, because a later support may still contain a negative witness. A completed traversal proves CP; it proves SCP exactly when
no boundary vector was found. Thus `both` is one traversal, not consecutive CP and SCP calls.

## Known Difficult Inputs

The generator recognizes coverage by individual certificate cubes. It does not compile the union of several overlapping cubes into
new collective branches. If no single interval contains a large recursive branch, the generator may descend almost to its leaves
even when the union covers them all. Variable order also matters: a cube whose lowest fixed coordinate is small is recognized late.

Thousands of narrow intervals can make bucket and completed-leaf membership checks substantial. In the worst case no useful branch
is removed and the model retains exponential support traversal in addition to its recursive bookkeeping.
