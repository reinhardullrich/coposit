# Zischg–Dickinson

Classification: Coposit-created strict adaptation of Dickinson's 2019 certificate model using the Level 2 negative-graph reduction
derived from Zischg and Bomze.

Public mode boundary: this Coposit-created model supports only `strictly_copositive`. Calling
`solve(A, copositivity_mode::copositive)` throws `std::invalid_argument` instead of applying strict rules to an ordinary query.

## Idea In Plain Language

Dickinson constructs exact vectors that cover principal supports. This variant first asks whether the negative-entry graph induced
by a generated support is connected. A disconnected support cannot introduce a new failure once its smaller connected components
are accounted for, because all cross-component matrix entries are nonnegative. Such a support is skipped before both Dickinson's
coverage scan and its exact factorization. Connected supports follow the existing Dickinson certificate algorithm unchanged.

The complete matrix is not decomposed. This is Level 2 inside the cardinality-ordered traversal, not the one-time Level 1 split.

## Name And Sources

The model combines:

- Johannes Zischg and Immanuel M. Bomze, “Novel shortcut strategies in copositivity detection: Decomposition for quicker positive
  certificates,” *Operations Research Perspectives* 14 (2025), 100324,
  [DOI 10.1016/j.orp.2024.100324](https://doi.org/10.1016/j.orp.2024.100324);
- Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its Applications* 569 (2019), 15–37,
  [DOI 10.1016/j.laa.2018.12.025](https://doi.org/10.1016/j.laa.2018.12.025).

Zischg and Bomze prove ordinary component decomposition. Coposit's
[local note](../../research/STRICT_COPOSITIVITY_GRAPH_AND_DUPLICATE_ROW_REDUCTIONS.md) supplies the strict extension and Level 2
derivation. The rest of this model is copied from `dickinson_2019`, the exact strict adaptation of Dickinson's Algorithms 1 and 2.
Because the Level 2 rule changes traversal, this is not the published Dickinson baseline.

## Level 2 Rule

Define $G^-(A)$ by an edge $i-j$ exactly when $a_{ij}<0$. For a support $I$, if $G^-(A)[I]$ has components
$C_1,\ldots,C_r$, then every cross block $A[C_s,C_t]$ is entrywise nonnegative. For all $x\geq0$ supported on $I$,

\[
x^TAx=\sum_t x_t^TA[C_t,C_t]x_t+2\sum_{s<t}x_s^TA[C_s,C_t]x_t.
\]

The cross terms cannot create a new nonpositive value. Thus strict copositivity is determined by connected induced supports. In
increasing-cardinality enumeration, every component of a disconnected support is smaller. Skipping the disconnected support is
therefore exact.

For a copositive matrix, every minimal nonnegative zero also has connected negative graph: if its support were disconnected, all
component forms and cross terms would be nonnegative and sum to zero, so one smaller component would itself be a zero. This
contradicts minimality. Dickinson's strict conclusion from minimal zeros is therefore preserved.

Orders one through three retain their cheap exact direct checks. From order four onward the packed connectivity test runs before
Dickinson coverage; a disconnected support avoids both the signature scan and the solve/nullspace branch.

## Dickinson Certificate State

For a generated vector $u$, retain only

\[
\operatorname{supp}(u)
\quad\text{and}\quad
N_A(u)=\{i:(Au)_i\geq0\}.
\]

It covers a later support $I$ exactly when

\[
\operatorname{supp}(u)\subseteq I\subseteq N_A(u).
\]

Both sets use packed multiword supports. Signatures are grouped by their lowest nonzero index, and eligible buckets are searched
newest first. Level 2 does not create a certificate for a skipped support; this can only remove later pruning, not create an invalid
certificate.

## Processing A Connected Uncovered Support

Let $C=A[I,I]$.

- If $C$ is nonsingular, solve $Cw=\mathbf1$ exactly. If $w\leq0$, then $-w\geq0$ is a negative witness and the model rejects.
  Otherwise embed $w$ into the full space and retain its coverage signature.
- If $C$ is singular, obtain one nonzero exact null vector from the stopped LDLT factorization and orient it to have a positive
  component. Retain its signature.
- Return `false` immediately when the embedded vector is nonnegative and has zero quadratic value; otherwise retain its signature.

The model returns `true` after the complete reduced traversal. Dickinson's Lemma 5.2 and Corollary 5.3 ensure that a completed
certificate contains every minimal zero up to positive scaling; Level 2 omits no minimal zero because its support must be connected.
Consequently any nonnegative zero returns `false` as soon as it is generated, while completion without one proves strict copositivity.

## Packed Connectivity

The exact sign graph is built once. Each adjacency row and each generated support stores $\lceil n/64\rceil$ words. A reusable
packed breadth-first search first accepts a complete negative graph or a root adjacent to the rest of the support, then unions the
adjacency sets of one frontier, intersects with the unreached vertices, and fails connectivity when no next frontier exists. This
is a graph test on the induced support; it never permutes,
extracts, or solves a Level 1 component matrix.

## Complete Decision Flow

```text
validate a nonempty square symmetric integer matrix A
build the packed negative-entry adjacency once
for support sizes k = 1,...,n in numeric-mask order:
    if k <= 3 and the direct exact principal test fails: return false
    if k > 3 and the induced negative graph is disconnected: skip I
    if an earlier Dickinson signature covers I: skip I
    factor A[I,I]
    solve A[I,I] w = 1, or derive one null vector
    if the solve gives w <= 0: return false
    if the generated vector is a nonnegative zero: return false
    retain its support/product signs
return true
```

## Source Behavior And Coposit Changes

Retained from `dickinson_2019` are the support order, coverage theorem, solve and singular branches, strict minimal-zero conclusion,
packed signature representation, exact LDLT arithmetic, low-order rejection, and termination rules. The sole mathematical change
is the Level 2 skip placed before coverage for order-four-or-larger supports. The graph adjacency and reusable BFS buffers are
representation-only additions.

The model adds no Level 1 split, Hadeler determinant rule, FracESSA forbidden-support rule, cone subdivision, duplicate-row
contraction, or floating heuristic.

## Termination And Limits

The finite support family bounds the traversal. Exact arithmetic gives deterministic signs, and cooperative timeout checkpoints
return an unresolved timeout separately. The packed graph has no dimension-63 limit. The worst case remains exponential when most
induced negative graphs are connected and Dickinson signatures cover little.

## Known Difficult Inputs

A complete or nearly complete negative graph gives Level 2 almost no opportunity. Dickinson remains expensive when generated
vectors have $N_A(u)$ only slightly larger than their supports, so few later supports are covered. Many connected singular
principal matrices require exact kernel work, and very large integers enlarge every factorization and matrix-vector product.
