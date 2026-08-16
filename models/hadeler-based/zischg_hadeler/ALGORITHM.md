# Zischg–Hadeler

Classification: coposit-created exact CP/SCP adaptation of the optimized Hadeler 1983 baseline using the Level 2 negative-graph reduction
derived from Zischg and Bomze.

Mode boundary: `copositive` and `strictly_copositive` select one predicate; `both` classifies both in one traversal and is the
analysis-interface default.

## Idea In Plain Language

Hadeler visits every principal support from small to large. Zischg's negative-sign graph records only the dangerous interactions:
two indices are adjacent exactly when their off-diagonal matrix entry is negative. If the graph induced by the current support is
disconnected, all entries between its components are nonnegative. The current principal matrix is strictly copositive exactly when
its smaller component matrices are strictly copositive.

Those component supports have lower cardinality and have already passed. The model therefore skips Hadeler's factorization for the
disconnected support. Connected supports use the unchanged optimized Hadeler calculation.

This is Level 2 only. The complete input matrix is never split into separate solver calls.

## Name And Sources

`zischg_hadeler` combines Johannes Zischg's negative-sign-graph decomposition with the Hadeler baseline. The published sources are:

- Johannes Zischg and Immanuel M. Bomze, “Novel shortcut strategies in copositivity detection: Decomposition for quicker positive
  certificates,” *Operations Research Perspectives* 14 (2025), 100324,
  [DOI 10.1016/j.orp.2024.100324](https://doi.org/10.1016/j.orp.2024.100324), especially Proposition 1.7 and Theorem 1.8;
- K. P. Hadeler, “On Copositive Matrices,” *Linear Algebra and its Applications* 49 (1983), 79–89,
  [DOI 10.1016/0024-3795(83)90095-2](https://doi.org/10.1016/0024-3795(83)90095-2), especially Theorem 3.

Zischg and Bomze state the component theorem for non-strict copositivity of the complete matrix. coposit's
[local derivation](../../../research/STRICT_COPOSITIVITY_GRAPH_AND_DUPLICATE_ROW_REDUCTIONS.md) proves the strict version and its
cardinality-ordered Level 2 corollary. Johannes Zischg's 2023 thesis is retained locally as
`research/papers/Zischg_Johannes_2023_Copositivity_Testing.pdf`.

The Hadeler implementation is copied from `hadeler_1983`, which is pinned to FracESSA commit `36902a3d`. The Zischg reduction makes
this a new coposit variant rather than a faithful Hadeler baseline.

## Negative Graph And Level 2 Proof

For a symmetric matrix $A$, define

\[
G^-(A)=(V,E),\qquad \{i,j\}\in E\iff i\ne j\text{ and }a_{ij}<0.
\]

For a support $S$, the graph used by Level 2 is the induced graph $G^-(A)[S]$. If its connected components are
$C_1,\ldots,C_r$, then every cross-component entry of $A[S,S]$ is nonnegative. Writing a nonnegative vector in component blocks gives

\[
x^TA[S,S]x
=\sum_t x_t^TA[C_t,C_t]x_t+2\sum_{s<t}x_s^TA[C_s,C_t]x_t.
\]

The cross terms are nonnegative. Hence $A[S,S]$ is strictly copositive exactly when every component matrix $A[C_t,C_t]$ is strictly
copositive.

Supports are enumerated by increasing cardinality. If $G^-(A)[S]$ is disconnected, every $C_t$ is a proper subset of $S$ and has
already passed. Induction over $|S|$ therefore certifies the disconnected support without another Hadeler calculation. Orders one
through three retain their direct exact rules; Level 2 is used before the factorization path from order four onward.

## Packed Connectivity Check

The graph is built once by exact sign tests on one triangle of $A$. Every vertex owns a packed adjacency support containing
$\lceil n/64\rceil$ unsigned words. For a generated support, a packed breadth-first search starts at its lowest index:

1. return immediately for a complete negative graph, or when the root is adjacent to every other support vertex;
2. place the remaining support vertices in `unreached`;
3. union the adjacency sets of the current frontier;
4. intersect that union with `unreached`;
5. reject connectivity if the intersection is empty before all vertices are reached;
6. otherwise remove the next frontier and repeat.

The three scratch supports and frontier-index vector are reused. The check does not allocate per generated support and has no
fixed-width dimension limit.

## Hadeler Processing For A Connected Support

Let $C=A[S,S]$. Every proper principal face has already passed.

- Orders one through three use coposit's exact direct criteria.
- If $\det C>0$, the support passes.
- If $\det C<0$, solve one retained exact system $Cy=-\mathbf1$. The support fails exactly when every component of $y$ is positive.
- If $\det C=0$ and the nullity is one, obtain one exact kernel vector. The support fails exactly when all its entries are nonzero
  and have one sign. Other nullities pass this Hadeler condition.

The factorization is fraction-free LDLT. Solve numerators have a positive common denominator, so every sign decision is exact. Only
one right-hand side or one LDLT-derived null vector is used; no inverse, full adjugate, or nullspace basis is constructed.

## Complete Decision Flow

```text
receive a parser-guaranteed nonempty square symmetric integer matrix A
build the packed negative-entry adjacency once
for support sizes k = 1,...,n:
    visit supports in numeric-mask order
    if k <= 3: apply the direct exact principal test
    else if the induced negative graph is disconnected: skip this support
    else: apply the optimized Hadeler determinant/solve/nullspace test
    if any processed support fails: return false
return true
```

Timeout checkpoints occur at support, graph-frontier, graph-construction-row, principal-copy, factorization, and solve boundaries.
A timeout is unresolved and is never returned as `false`.

## Source Behavior And coposit Changes

Retained unchanged from `hadeler_1983` are cardinality and mask order, low-order criteria, determinant branches, the one-system
replacement for the adjugate, the singular nullity rule, exact arithmetic, and immediate rejection. The only mathematical change is
the Level 2 skip before an order-four-or-larger Hadeler calculation. Packed graph storage and reused scratch supports are
representation optimizations.

The model deliberately omits Level 1 input decomposition, duplicate-row contraction, positive-definiteness or Z-matrix shortcuts,
cone subdivision, Dickinson certificates, and FracESSA pruning.

## Termination And Limits

There are at most $2^n-1$ generated supports. Every connectivity test and every retained exact calculation is finite, so the model
terminates in exact arithmetic. If the negative graph is complete, every induced support is connected and Level 2 saves no Hadeler
work. The exponential support family and arbitrary-precision factorization remain the practical limits.

## CP and SCP classification

The graph skip does not change Hadeler's decision states. Nonsingular Hadeler witnesses reject CP and SCP. A one-dimensional nullspace
with a same-sign nonzero basis vector proves that SCP is false. Strict-only mode stops there; CP and combined mode remember the
boundary and continue, because another principal subset may still reject CP. A completed traversal proves CP and proves SCP exactly
when no boundary subset was found. Thus `both` uses one connected-support traversal.

## Known Difficult Inputs

Dense negative-entry graphs are the worst structural case: almost every induced support remains connected, so the graph scan adds
work without reducing the Hadeler search. Large-support failures still appear late in cardinality order. Singular connected
principal matrices and matrices with very large integer entries additionally make the retained exact LDLT work expensive. Sparse
connected graphs can contain many disconnected induced supports, but no benefit is guaranteed for a particular numeric matrix.
