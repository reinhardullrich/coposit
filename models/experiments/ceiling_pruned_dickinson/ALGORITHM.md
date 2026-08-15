# Ceiling-Pruned Dickinson

> **Current model boundary:** the former model-local Z-matrix stage now runs once in shared preprocessing, not inside
> `model::solve`. The Zed-stage sections below document that historical construction; current model-local execution starts with the
> forbidden-support traversal. This prevents duplicate maximal-clique enumeration.

Classification: coposit-created exact copositivity and strict-copositivity experiment. It combines Dickinson's exact principal-support
certificate with FracESSA's forbidden-support generator, but retains only Dickinson certificates whose upper endpoint is the full index
set.

Public mode boundary: `ceiling_pruned_dickinson` supports separately selected copositivity (CP), strict copositivity (SCP), and combined
classification of both predicates in one traversal.

## Current Model-Local Decision Flow

1. Receive a parser-validated nonempty square symmetric integer matrix and enforce the model's supported mode.
2. Initialize the model-specific support-family representation or generator.
3. Traverse uncovered supports in cardinality order and perform the documented exact Dickinson calculation.
4. Return the negative result on a decisive witness; otherwise store the model's documented certificate and continue.
5. Return the positive result after every remaining support is exhausted.

Shared preprocessing, including the Z-matrix check, wraps this flow and either decides before entry or delegates the unchanged matrix here.

## Idea In Plain Language

Dickinson visits nonempty index sets from small cardinality to large cardinality. For an index set $I$, it solves one exact principal
linear system or, when that system is singular, constructs one exact nullspace vector. A valid vector $u$ certifies an interval of
supports

$$
[L(u),U(u)]=\{J:L(u)\subseteq J\subseteq U(u)\}.
$$

Here $L(u)$ is the support of $u$, and $U(u)$ contains every row on which $Au$ is nonnegative. Normal Dickinson pruning retains every
such interval. This experiment retains only the special case

$$
U(u)=\{1,\ldots,n\}.
$$

Then the certified interval is the complete upward cone

$$
[L(u),[n]]=\{J:L(u)\subseteq J\}.
$$

It can be represented by the single forbidden support $L(u)$. FracESSA's generator can skip every future support containing that
forbidden support without a BDD, ZDD, interval list, or repeated scan of all retained certificates. A certificate not reaching the
ceiling is discarded. This deliberately avoids using a narrow certificate to hide a support that might later produce a much wider
certificate.

Before support generation, the model runs the existing rejection-only maximal-Zed check. A Zed block that proves failure stops the
model. A Zed block that passes is discarded and supplies no pruning rule.

## Name And Sources

The identifier is `ceiling_pruned_dickinson`. “Ceiling” means that the certificate upper endpoint is the full Boolean-lattice ceiling
$[n]$. “Pruned” means that the lower endpoint is stored as a forbidden support and removes all of its strict supersets from later
generation.

The exact principal-support calculation and certificate formula come from Peter J. C. Dickinson, “A New Certificate for
Copositivity,” *Linear Algebra and its Applications* 569 (2019), 15–37, DOI `10.1016/j.laa.2018.12.025`, especially Theorem 4.6 and
Algorithms 1–2. The optional Zed-block rejection follows Dickinson's Lemma 6.2, Algorithm 3, and Theorem 6.5.

The forbidden-support traversal is copied mathematically from FracESSA's `NonCircularSupportGenerator`, read locally at revision
`95e0ec019cf11a60c6423508e8768536a0b88860` from `cpp/include/fracessa/supports.hpp`. Its origin in the FracESSA candidate search is
described by Immanuel M. Bomze, “Detecting All Evolutionarily Stable Strategies,” *Journal of Optimization Theory and Applications*
75(2), 1992, 313–329. The local coposit source used for the copy is `models/experiments/fracessa/solver.cpp`.

The model began as an isolated copy of `models/experiments/cbdd_zed_dickinson`. Its exact Dickinson solve, singular-vector rule,
copositivity decisions, maximal-Zed scan, Zed component split, and Motzkin–Straus bypass were preserved. The CBDD was removed and the
FracESSA generator inserted. No external source describes this exact combination or its ceiling-only retention policy.

## Notation

Let $A\in\mathbb Z^{n\times n}$ be the nonempty symmetric input and let

$$
[n]=\{1,\ldots,n\}.
$$

For a vector $u\in\mathbb R^n$, define

$$
L(u)=\operatorname{supp}(u)=\{i:u_i\ne0\},
$$

and

$$
U(u)=N_A(u)=\{i:(Au)_i\geq0\}.
$$

For $L\subseteq U$, the Boolean interval

$$
[L,U]=\{J\subseteq[n]:L\subseteq J\subseteq U\}
$$

contains exactly the supports certified by Dickinson's vector. A ceiling certificate has $U=[n]$. Its number of free indices is

$$
d=|U|-|L|=n-|L|.
$$

The generator visits supports $I$ by increasing cardinality $k=|I|$. Within one cardinality it follows FracESSA's increasing
numeric-mask order.

## Rejection-Only Maximal-Zed Check

The model first builds the compatibility graph

$$
\{i,j\}\in E(H_A)\quad\Longleftrightarrow\quad a_{ij}\leq0.
$$

A principal block $A_J$ is a Zed matrix exactly when $J$ is a clique of this graph. Bron–Kerbosch search with pivoting enumerates all
maximal cliques. Its state is the current clique $R$, possible extensions $P$, and already handled extensions $X$. With a pivot
$p\in P\cup X$, it branches on $P\setminus N(p)$. A state with $P=X=\varnothing$ emits one maximal block. Blocks of order one are
left to the normal support traversal.

Inside a maximal Zed block, the model forms connected components using only strictly negative entries. Cross-component entries are
zero: they are nonpositive because the block is Zed and nonnegative because no negative edge connects the components. The block is
therefore positive definite exactly when every component is positive definite, and positive semidefinite exactly when every
component is positive semidefinite.

Each component is factorized by exact fraction-free LDLT:

- SCP requires positive definiteness;
- CP requires positive semidefiniteness;
- combined mode rejects both predicates if positive semidefiniteness fails and rejects only SCP if the component is singular
  positive semidefinite.

A passing Zed block is not stored as a positive certificate. This is why this stage is “negative Zed”: it may reject, but it never
removes work from the later support generator.

An exact two-value Motzkin–Straus detector bypasses the maximal-Zed enumeration when every diagonal and graph non-edge has one common
nonnegative value and every graph edge has one common negative value. In that construction maximal Zed blocks are graph cliques, so
the scan can itself be the dominant exponential search. The bypass changes only the precheck; the complete ceiling-pruned Dickinson
traversal still runs.

## FracESSA Forbidden-Support Generator

The generator owns:

- one dynamic packed support for the current recursive choice;
- a pending list of newly discovered forbidden supports; and
- one bucket per possible lowest index for active forbidden supports.

For cardinality $k$, it recursively decides variables from high index to low index. It explores exclusion before inclusion, which
gives increasing numeric masks. When index $i$ is included, it becomes the current support's new lowest selected index. Only the
forbidden supports whose own lowest index is $i$ can become newly complete at that step. Testing that one bucket is sufficient: a
forbidden support with a larger lowest index would already have been detected earlier, and one with a smaller lowest index cannot yet
be contained.

Suppose an active forbidden support $F$ is contained in the current partial support. Every completion of that recursive branch also
contains $F$, so the whole branch is skipped. No generated support is compared against every retained certificate.

New forbidden supports are pending until the next cardinality begins. This preserves FracESSA's simple traversal invariant. A
distinct support of the current cardinality cannot be a strict superset of the support just processed. In this model $L(u)$ can be
smaller than the processed support when $u$ has zero coordinates; delaying activation may then perform harmless extra work in the
same cardinality, but it cannot omit a required support.

If a cardinality emits no support, every support of that size contains an active forbidden support. Every larger support contains at
least one support of that size and is therefore forbidden as well. The generator terminates immediately and the proof is complete.

Packed supports contain `ceil(n/64)` machine words. The traversal has no dimension-63 restriction.

## Exact Dickinson Calculation

For every emitted support $I$, copy the principal matrix $A_I$ and factor it exactly.

### Nonsingular principal matrix

If $A_I$ is nonsingular, solve

$$
A_Iu_I=\mathbf1.
$$

Fraction-free LDLT returns integer numerators and a common positive denominator. The common denominator need not be stored in the
certificate because multiplying $u$ by a positive scalar does not change $L(u)$ or the signs of $Au$.

If $u_I\leq0$, then the zero-extended vector $-u\geq0$ supplies the negative witness used by Dickinson's decision rule, and the model
returns `false`.

### Singular principal matrix

If $A_I$ is singular, construct one exact nonzero vector in $\ker(A_I)$ and orient it so that it has a positive component. If the
oriented vector is nonnegative, its zero extension satisfies

$$
u\geq0,
\qquad
u\ne0,
\qquad
u^TAu=0.
$$

This disproves SCP. In CP mode equality is permitted, so the traversal continues and the vector may still yield a ceiling
certificate. In combined mode SCP becomes false while the same traversal continues to decide CP.

A singular vector with mixed signs follows the same certificate construction as the nonsingular solution.

## Ceiling Test And Pruning Rule

Embed the principal solution into $\mathbb R^n$ by putting zeros outside $I$. For $r\in I$, the principal solve already proves
$$(Au)_r>0$$ in the nonsingular case and $$(Au)_r=0$$ in the singular case, up to the harmless common positive scale. The model
therefore computes only the outside-support products

$$
(Au)_r=\sum_{i\in I}a_{ri}u_i.
$$

It stops at the first negative outside-support product. Then $U(u)\ne[n]$: Dickinson's interval is valid, but this model discards it
and records no pruning rule.
The generator simply continues to the next support.

If every product is nonnegative, then $U(u)=[n]$ and Dickinson certifies

$$
[L(u),[n]].
$$

The model stores only $L(u)$ as a forbidden support. Every future support $J$ with $L(u)\subseteq J$ is skipped. This is exact because
such a $J$ lies in Dickinson's certified interval. The current support need not be removed explicitly: the generator has already
emitted it and never revisits it.

This policy is intentionally stricter than a width threshold. A certificate is retained only when its upper endpoint equals the
actual full index set, regardless of the size of $L(u)$ or the ratio $d/(n-|I|)$.

## Historical Complete Decision Flow Before Z-Stage Extraction

1. Receive a parser-guaranteed nonempty square symmetric integer matrix.
2. Select CP, SCP, or combined classification.
3. Detect the exact Motzkin–Straus two-value pattern. If present, bypass only the maximal-Zed check.
4. Otherwise enumerate every maximal principal Zed block of order at least two.
5. Split each block by strictly negative connectivity and factor each component exactly. Return `false` when the selected
   definiteness condition fails; in combined mode retain a possible CP result after SCP alone fails.
6. Start the forbidden-support generator at cardinality one.
7. For each emitted support, solve the exact Dickinson principal system or construct one nullspace vector.
8. Stop on a negative witness. In SCP mode also stop on a nonnegative zero; in combined mode clear only SCP and continue.
9. Use the principal equation for the entries of $Au$ on $I$ and compute the remaining entries exactly, stopping at the first
   negative entry. If every entry is nonnegative, store $L(u)$ as pending forbidden support and record the ceiling certificate;
   otherwise discard it.
10. Activate pending forbidden supports when the next cardinality starts and skip every recursive branch that contains one.
11. Return `true` for the selected predicate after all unpruned supports are exhausted. Combined mode returns the two accumulated
    Boolean results.

The shared connected-component and general precheck pipeline is external to the model. Analysis runs may enable or disable it
independently. The model-local negative-Zed stage described above remains part of this algorithm.

## Progress And Diagnostics

When progress or diagnostic capture is enabled, the generator reports:

- `visited`: emitted supports plus the exact number of supports skipped by forbidden branches;
- `covered`: supports skipped by forbidden branches;
- `processed`: emitted supports sent to the exact Dickinson calculation;
- `certificates`: retained ceiling certificates;
- `zed_blocks_tested`: maximal Zed blocks completed by the rejection-only precheck; and
- `certificate_k_d_counts`: the sparse joint distribution of generating cardinality $k$ and free-index count $d=n-|L|$.

The coverage denominator is the $2^n-1$ nonempty supports. Skipped branch sizes are exact binomial coefficients, saturated only when
the telemetry exceeds 64-bit range; saturation never affects the mathematical traversal. Diagnostic campaigns store the one-second
progress history and the final sparse distribution through the standard Python results path.

## Exactness And Termination

All matrix arithmetic and sign decisions use arbitrary-precision integers. The Zed definiteness check and every principal solve use
the shared exact fraction-free LDLT implementation. No floating-point proposal or tolerance enters the decision.

The ceiling scan reuses one exact-integer accumulator, skips the rows whose sign is already fixed by the principal equation, and
constructs the packed lower support only after the scan succeeds. These are representation-only optimizations: they do not change
which products are tested, which certificates are retained, or the traversal order.

The model removes only supports covered by a valid Dickinson certificate with $U=[n]$. Discarding other valid certificates can add
work but cannot remove a necessary test. The generator visits each remaining support at most once, so the algorithm is finite when
allowed sufficient time and memory.

The model stores only ceiling-certificate lower supports. It does not allocate a decision diagram or materialize the full covered
family. Memory is therefore proportional to the packed forbidden supports and their buckets, apart from exact factorization scratch
space.

## Known Difficult Inputs

If useful certificates have even one negative outside product, none reaches the ceiling and the model degenerates to exhaustive
principal-support enumeration. This can require nearly all $2^n-1$ supports even though ordinary Dickinson intervals would have
pruned many bounded regions.

Dense structured copositive BPQY instances such as corpus matrix 12649 are a reproducible example for this experiment: the important
question is whether their later supports generate enough full-upper-endpoint certificates to compensate for discarding every bounded
interval. The model was created specifically to observe that behavior without CBDD construction obscuring the support calculation.

The negative-Zed precheck has a separate exponential risk because a graph may have exponentially many maximal cliques. The exact
Motzkin–Straus detector bypasses its known two-value graph-matrix case, but other matrices can still induce a large maximal-clique
family without matching that pattern.

Finally, a large number of distinct ceiling certificates can itself consume memory: the generator stores every retained lower
support until the call ends. It performs no subset-minimization between forbidden supports. A newly retained lower support that
contains an older one is redundant, but checking or maintaining a minimal antichain would add work to the hot path and is not part of
this first experiment.
