# SAT-Halfspace-Rays Wide Dickinson

Classification: coposit-created exact CP/SCP experiment. It combines SAT-Halfspace-Rays Dickinson's exact right-hand-side search
with Wide-Certificate Dickinson's parameterized interval-retention rule.

Public mode boundary: `copositive` and `strictly_copositive` select one predicate. `both` classifies both predicates in one
traversal and is the analysis-interface default. The model requires one integer percentage parameter
\(p\in\{0,\ldots,100\}\); the first experiment uses \(p=50\).

## Idea In Plain Language

An early Dickinson certificate may cover only a small part of the Boolean support lattice while suppressing a later support that
would produce a much stronger certificate. Explicit recursive look-ahead can become another exhaustive support search. This model
uses the simpler policy already tested by Wide-Certificate Dickinson: a weak certificate blocks only the support just processed,
leaving later supports available to be discovered by the normal cardinality traversal.

The vector used to generate each candidate interval is nevertheless the strongest one found by SAT-Halfspace-Rays Dickinson. The
model first maximizes the upper endpoint cardinality \(|U|\), uses interval width as the tie-breaker, and tries at most two
synthesized rays after coordinate search stalls. Only then does the percentage rule decide whether to retain the resulting complete
interval or merely the current support.

## Name And Sources

The identifier is `sat_halfspace_rays_wide_dickinson`:

- **SAT** names the persistent CaDiCaL support generator;
- **Halfspace-Rays** names the exact right-hand-side coordinate search and its at most two synthesized directions;
- **Wide** names the selective interval-retention rule; and
- **Dickinson** identifies Peter J. C. Dickinson's certificate theorem.

The model is an isolated copy of
[`sat_halfspace_rays_dickinson`](../sat_halfspace_rays_dickinson/ALGORITHM.md). Its only new search-policy decision is copied from
[`wide_certificate_sat_dickinson`](../wide_certificate_sat_dickinson/ALGORITHM.md): narrow generated intervals are represented by
an exact-support clause instead of their complete Dickinson interval.

The certificate mathematics is from Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its
Applications* 569 (2019), 15–37, DOI `10.1016/j.laa.2018.12.025`, especially Theorem 4.6 and Algorithms 1–2. The flexible
positive right-hand side is permitted by the note following those algorithms. The coordinate search, synthesized rays, and
wide-certificate threshold are coposit experiments, not claims about Dickinson's published algorithm.

CaDiCaL 2.2.1 provides incremental SAT solving. Exact cardinalities use the same Batcher bitonic sorting network as the source SAT
models.

## Dickinson Certificate And Retention Rule

For the exact full vector \(u\) produced at a principal support \(I\), define

$$
L(u)=\operatorname{supp}(u),
\qquad
U(u)=\{j:(Au)_j\geq0\}.
$$

Dickinson's theorem certifies every support in

$$
[L,U]=\{J:L\subseteq J\subseteq U\}.
$$

Let

$$
d=|U|-|L|,
\qquad
k=|I|,
$$

where \(n\) is the matrix order. For the configured percentage \(p\), the complete interval is retained exactly when

$$
d>\left\lfloor\frac{p(n-k)}{100}\right\rfloor.
$$

The strict inequality is intentional. At the initial value \(p=50\), this becomes

$$
d>\left\lfloor\frac{n-k}{2}\right\rfloor.
$$

The threshold uses the true lower endpoint \(L\), not the processed support \(I\). Hence zeros in the chosen vector may make
\(|L|<k\) and allow \(d>n-k\). The integer implementation evaluates the percentage without forming an overflow-prone product.

If the interval fails the threshold, the SAT representation receives only

$$
[I,I].
$$

This prevents SAT from returning the already processed support again but deliberately leaves every other support available.
Diagnostics record every generated true \((k,d,|U|)\) as `certificate_k_d_u_counts` and separately record only complete intervals
that pass the threshold as `accepted_certificate_k_d_u_counts`. The exact-support fallback \([I,I]\) is not counted as an accepted
certificate.

## Exact Halfspace-Rays Candidate

For a nonsingular principal matrix \(A_I\), any strictly positive right-hand side \(b\) gives

$$
A_Ix=b.
$$

After embedding \(x\) into the full vector \(u\), the identities \((Au)_I=b>0\) guarantee \(I\subseteq U(u)\). The model
factors \(A_I\) once, solves the all-ones system and all \(k\) unit systems, and reuses those solutions to search exact
right-hand-side directions.

Along each coordinate direction, every entry of \(x\) and \(Au\) is affine in one nonnegative parameter. The model enumerates
all exact sign-change breakpoints and accepts only strict lexicographic improvements in

$$
(|U|,d).
$$

When a complete coordinate pass stalls, it keeps at most

$$
\min\{k,64,\lceil3\sqrt n\rceil\}
$$

promising coordinate-ray points. It ranks complementary pairs by gained and lost upper indices, constructs the best two exact
combined directions, sweeps them from the unchanged stalled point, and accepts only the better strict improvement. Thus every
nonsingular support performs at most two synthesized-ray sweeps.

If \(A_I\) is singular, the halfspace search is skipped. The model recovers one exact nullspace vector, handles a nonnegative zero
as an SCP failure, and otherwise compares both orientations and retains the one with larger \(|U|\).

Every sign, breakpoint, solve, witness, and certificate endpoint uses exact arbitrary-precision integer arithmetic.

## SAT Representation And Traversal

Each matrix index has one Boolean support variable. A single Batcher sorting network exposes exact-cardinality outputs, and
cardinalities are traversed in increasing order.

A retained interval \([L,U]\) is blocked by one clause that is false exactly when \(L\subseteq J\subseteq U\):

$$
\bigvee_{i\in L}\neg z_i
\;\lor\;
\bigvee_{j\notin U}z_j.
$$

The existing cardinality output for \(|U|+1\) is appended when \(|U|<n\), making the clause automatically inactive after the last
cardinality it can cover. A rejected narrow interval instead uses the same representation with \(L=U=I\), so it expires after
the current layer.

One CaDiCaL instance, its compatible learned clauses, the sorting network, and retained interval clauses persist throughout the
matrix call.

## Complete Decision Flow

1. Receive a parser-validated symmetric integer matrix and configured percentage \(p\).
2. Build the SAT cardinality network once.
3. For \(k=1,\ldots,n\), ask SAT for an uncovered support \(I\) of cardinality \(k\).
4. Factor \(A_I\) exactly and stop on a decisive negative witness.
5. For a nonsingular support, maximize \((|U|,d)\) by coordinate sweeps and at most two synthesized-ray sweeps.
6. For a singular support, use the exact orientation rule described above.
7. Compute \(L\), \(U\), and \(d\) exactly.
8. Retain \([L,U]\) when the percentage inequality holds; otherwise retain only \([I,I]\).
9. Continue at the same cardinality until SAT reports no uncovered support, then advance.
10. Exhausting all cardinalities proves the remaining requested classifications.

Combined mode begins with CP and SCP provisionally true. A nonnegative zero clears SCP only. A negative witness clears both and
terminates. The model therefore returns only `(true,true)`, `(true,false)`, or `(false,false)`.

## Termination And Limits

Every processed support adds at least its exact-support clause, so SAT cannot return it again. The Boolean support family is finite;
the algorithm therefore terminates given sufficient time and memory. A timeout remains unresolved and is never converted into a
negative classification.

All matrix arithmetic uses FLINT arbitrary-precision integers. Packed supports have no fixed 63-bit limit. CaDiCaL's signed integer
literal range remains an explicit upper limit on the SAT encoding.

## Known Difficult Inputs

- A high threshold can reject most intervals and force exponentially many exact support calculations.
- A low threshold approaches ordinary SAT-Halfspace-Rays Dickinson and can again let a weak early interval hide a stronger future
  certificate.
- The threshold measures individual interval width, not additional coverage after overlap with all previously retained intervals.
  A wide interval may therefore contribute little new pruning.
- Halfspace-Rays work is more expensive than the all-ones Dickinson calculation: a support of size \(k\) requires \(k\) exact
  unit solves and direction products before up to two synthesized sweeps.
- Higher-nullity singular supports still use one deterministic nullspace vector and may generate weak intervals.
- Highly symmetric inputs can leave many interchangeable supports after narrow intervals are rejected, causing SAT and exact
  arithmetic costs to grow rapidly.
