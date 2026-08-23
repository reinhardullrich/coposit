# G1: Generator with Bounded Dickinson Guidance

Classification: coposit-created exact copositivity and strict-copositivity experiment.

G1 means **generator experiment 1**. It replaces B3's general SAT coverage engine with two deliberately narrower structures:

1. a FracESSA-style support generator for certificates that prune completely upward to the full index set; and
2. a separate expiring index for bounded Dickinson intervals.

The bounded intervals never remove supports from the generator. They only identify supports whose exact Dickinson work is already
unnecessary. G1 checks each interval's final upper endpoint exactly and records whether the interval can contain a smaller
full-upward curvature root. Only supports covered by such an interval receive the cheap floating-point curvature screen and its
conditional exact verification.

G1 supports separately selected copositivity, strict copositivity, and combined classification in one traversal. Shared
preprocessing remains outside the model.

## Idea in Plain Language

G1 visits supports from small cardinality to large cardinality.

A curvature certificate, or a Dickinson certificate reaching the full ceiling, proves that one lower support and every superset may
be skipped. Those certificates fit the simple recursive generator.

An ordinary Dickinson certificate may stop at a proper upper set. G1 remembers that bounded interval separately. If its upper
endpoint has positive-definite reduced curvature, every smaller face inside the interval does too, so later covered supports can be
skipped completely. Otherwise, a later support inside the interval might be a smaller bad-curvature root and therefore yield a much
stronger full-upward certificate. G1 screens precisely those supports.

The intuition is monotonicity: good curvature descends to smaller faces. An exactly good upper endpoint closes the curvature
question for the entire interval; an exactly bad endpoint only says that a smaller bad root may still be hidden inside it.

After a cardinality is complete, all supports in that layer and every smaller layer are permanently retired. G1 uses them as
don't-care states to compress many full-upward roots into fewer, smaller roots without changing coverage in any unprocessed layer.

The intuition is that completed layers no longer matter individually. Only the boundary between finished and unfinished supports
must remain unchanged.

## Sources and Classification

G1 is an isolated copy of [`f1`](../f1/ALGORITHM.md), extended with bounded Dickinson guidance and cardinality-aware root
compaction. Its exact current-support, curvature, Halfspace, and synthesized-ray calculations therefore retain F1's mathematics.
The bounded-interval separation follows the exact interval semantics used by [`sat_b3`](../sat_b3/ALGORITHM.md).

The Dickinson certificate comes from Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its
Applications* 569 (2019), 15–37, especially Theorem 4.6 and Algorithms 1–2.

The recursive generator follows FracESSA's `NonCircularSupportGeneratorMultiword`, read locally from
`cpp/include/fracessa/support_generator_non_circular.hpp` at revision `41dfa8fffde9d42456a9df478893e70af04d5b0c`. Its support-
enumeration origin is Immanuel M. Bomze, “Detecting All Evolutionarily Stable Strategies,” *Journal of Optimization Theory and
Applications* 75(2), 1992, 313–329.

The compaction is a monotone, cardinality-restricted analogue of the EXPAND phase in Richard Rudell,
“Multiple-Valued Logic Minimization for PLA Synthesis,” UCB/ERL M86/65, 1986. G1 does not implement the rest of Espresso and does
not solve an exact minimum-cover problem.

The reduced-curvature argument is developed in
[`aidocs/COPOSITIVITY_CURVATURE_HADELER_DICKINSON.md`](../../../aidocs/COPOSITIVITY_CURVATURE_HADELER_DICKINSON.md).

## Notation

Let \(A\in\mathbb Z^{n\times n}\) be symmetric, let \([n]=\{0,\ldots,n-1\}\), and let \(I\subseteq[n]\) be nonempty. Write \(A_I\)
for the principal matrix on \(I\).

For a vector \(u\) supported inside \(I\), define

$$
L(u)=\{i:u_i\ne0\},
\qquad
U(u)=\{j:(Au)_j\geq0\}.
$$

Dickinson certifies the Boolean interval

$$
[L(u),U(u)]=\{J:L(u)\subseteq J\subseteq U(u)\}.
$$

A **ceiling root** is a lower set \(F\) whose complete upward closure

$$
\uparrow F=[F,[n]]
$$

is certified. The generator stores only \(F\).

## The Two Coverage Structures

### Full-upward generator

The generator stores exact curvature roots and Dickinson lower endpoints whose upper endpoint is \([n]\). It groups each root by
its lowest index. While recursively constructing a support in decreasing index order, the first moment at which a root becomes
complete removes the whole remaining branch.

New roots become active before the next cardinality. If a new root already contains an active root, it is redundant and is not
stored. Once the active family exceeds the compaction threshold, G1 stops doing potentially quadratic insertion-time redundancy
checks. It admits the remaining roots conservatively and marks the family for the next bounded normalization pass.

### Bounded Dickinson index

When \(U(u)\ne[n]\), G1 stores the exact pair \((L(u),U(u))\) in the bounded index instead of the generator. It first factorizes the
reduced Hessian on \(U(u)\) exactly and stores one Boolean saying whether that Hessian fails positive definiteness. The interval is
placed in one bucket belonging to an index of \(L(u)\); among eligible buckets, G1 chooses the currently smallest bucket.

A candidate \(I\) is covered exactly when

$$
L(u)\subseteq I\subseteq U(u).
$$

If several intervals cover \(I\), G1 screens its curvature when at least one covering interval has a bad upper endpoint. It skips
the screen only when every covering interval has an exactly good upper endpoint.

At the beginning of cardinality \(k\), every interval with \(|U(u)|<k\) is physically removed. It cannot contain a current or
future support.

A bounded interval is never used as evidence for full-upward root compaction. Its coverage may disappear above \(U(u)\).

## Processing a Generator Support

If the support is not in a bounded interval, G1 performs F1's exact work:

1. copy and fraction-free factorize \(A_I\);
2. detect exact negative witnesses and exact zeros;
3. test the current face's reduced curvature;
4. for a nonsingular matrix, solve \(A_Ix=\mathbf1\);
5. perform the exact Halfspace coordinate sweeps and at most two synthesized-ray sweeps;
6. test the traditional, Halfspace, and Rays upper endpoints for exact bad curvature after a floating-point nomination; and
7. store the final Dickinson result in the appropriate coverage structure.

If the current reduced curvature is not positive definite, G1 inserts \(I\) as a ceiling root. Every accepted curvature result uses
the exact factorization.

## Processing a Bounded-Interval Support

An interval-covered support does not repeat the complete exact Dickinson calculation. If every covering interval has an exactly
positive-definite upper endpoint, G1 skips the support without any curvature calculation.

Otherwise, for \(|I|\ge3\), G1 forms the reduced Hessian in scaled binary64 arithmetic and applies an \(LDL^T\) screen.

If the floating calculation looks positive definite, G1 performs no exact work. This can only miss optional pruning: the earlier
exact Dickinson interval already makes complete processing of this support unnecessary.

If floating arithmetic does not clearly establish positive definiteness, G1 factorizes the principal matrix exactly and applies the
same exact reduced-curvature criterion as the ordinary path. Only an exact failure inserts \(I\) as a ceiling root.

Floating point therefore never creates a certificate, witness, truth value, or pruning decision.

## Exact Pair Curvature

Before traversal, every pair \(\{i,j\}\) is checked exactly through

$$
a_{ii}+a_{jj}-2a_{ij}.
$$

A nonpositive value proves failure of strict convexity on that edge and inserts the pair as a ceiling root. This is the order-two
special case of the general reduced-curvature rule.

## Halfspace-Rays Dickinson Search

For nonsingular \(A_I\), G1 starts from the exact solution of \(A_Ix=\mathbf1\). It scores a candidate vector first by larger
\(|U(x)|\), then by larger interval width \(|U(x)|-|L(x)|\).

The retained factorization solves every coordinate direction \(A_Id_r=e_r\). Along each exact ray

$$
x(t)=x+t d_r,\qquad t\geq0,
$$

signs can change only at exact rational breakpoints. G1 checks every breakpoint and every open interval between consecutive
breakpoints. Improving coordinate passes repeat until stable.

The Rays stage keeps at most

$$
\min\bigl(|I|,64,\lceil3\sqrt n\rceil\bigr)
$$

useful coordinate candidates. It selects at most two complementary pairs, combines each pair into a nonnegative right-hand-side
direction, and performs one exact breakpoint sweep for each selected pair.

## Layer-Boundary Root Compaction

Assume cardinality \(k\) has just finished. Every support of size at most \(k\) is now an implicit don't-care state. Let
\(\mathcal F\) be the old and newly activated ceiling roots, and let \(r=k+1\) be the first unprocessed cardinality.

G1 may replace a root by a smaller candidate \(P\) precisely when every first-unprocessed extension of \(P\) is already covered:

$$
\forall T\supseteq P,\quad |T|=\max(r,|P|):
\qquad
\exists F\in\mathcal F\text{ such that }F\subseteq T.
$$

When \(|P|\le r\), checking cardinality \(r\) is enough. Every larger support \(S\supseteq P\) contains an \(r\)-set between \(P\)
and \(S\), and coverage of that intermediate set propagates upward. When \(|P|>r\), the smallest relevant extension is \(P\) itself.

Thus a new synthetic root may cover arbitrary finished supports without claiming a new mathematical certificate for them. It must
preserve exactly the already-certified coverage on every unprocessed layer.

### Greedy multi-level expansion

Compaction runs only when the active root count exceeds

$$
\max(512,8n).
$$

Roots are ordered from smaller to larger cardinality, but expansion itself is breadth-first. In one round, every still-expandable
root may accept at most one index deletion. Only after all such roots have had that chance does G1 begin the next deletion round.
Thus broad one-level reductions take priority over driving one root several levels downward. After each tentative deletion, the same
recursive generator machinery searches for one uncovered support at the first relevant cardinality.

- Finding one uncovered support rejects the deletion immediately.
- Exhausting the recursive search accepts the deletion.
- Exhausting the compaction search budget rejects the unfinished deletion and keeps every result already proved safe.

The deterministic budget for a root family \(\mathcal F\) is

$$
\max(4096,16n|\mathcal F|)
$$

units per layer. The factor \(n\) reflects that an order-\(n\) root has up to \(n\) one-index deletion directions. One unit is
charged for every recursive state and every comparison with a forbidden root, so a large bucket cannot hide unbounded work behind
one nominal recursion node. One root may still shrink by many cardinality levels, and roots originating at unrelated positions may
jointly justify the shrink.

After expansion, G1 sorts the roots and always removes adjacent duplicates. Superset removal has a separate budget of the same form,
with one unit charged per subset comparison. When that budget is exhausted, G1 keeps the remaining safe but possibly redundant
roots and rebuilds the generator's lowest-index buckets. The budgets are deterministic operation counts, not elapsed-time limits.
If the breadth-first search accepts no deletion at all, G1 returns immediately and does not pay for sorting and normalization,
unless activation admitted an unchecked large batch that still needs its one bounded normalization pass.

If the empty root is proved safe, every support in every remaining layer is covered and traversal ends.

The heuristic may miss a smaller representation, but every accepted replacement is exact.

## Singular Supports

For singular \(A_I\), G1 obtains one exact nullspace vector and chooses its sign by the larger Dickinson upper endpoint. A
componentwise nonnegative null vector proves failure of strict copositivity.

The current support receives the exact singular reduced-curvature test. The nullspace Dickinson interval is stored as a ceiling root
when its upper endpoint is \([n]\), and otherwise as a bounded interval. Halfspace-Rays is not applied because \(A_I^{-1}\) does not
exist.

## Complete Decision Flow

1. Install exact pair-curvature ceiling roots.
2. Activate roots queued by the preceding layer.
3. After completing \(k\), optionally compact the root family using all layers of size at most \(k\) as don't-cares.
4. Retire bounded Dickinson intervals whose upper cardinality is below the new layer.
5. Generate the next support not covered by an active ceiling root.
6. If a bounded Dickinson interval contains it, use the stored exact upper-endpoint result: either skip it immediately or run only
   the floating curvature screen and conditional exact curvature check.
7. Otherwise perform the complete exact F1 support calculation and Halfspace-Rays search.
8. Send a full-ceiling Dickinson lower endpoint to the generator; exactly classify the final upper endpoint of a proper interval and
   send the interval plus that routing flag to the bounded index.
9. Stop on an exact decision, an empty remaining layer, an exactly covered future, or exhaustion of all cardinalities.

## Diagnostics

Shared diagnostics report generated and skipped supports, exact support work, retained certificates, singular supports, and sparse
certificate widths.

Focused source diagnostics distinguish bounded intervals that skip or request curvature work, floating and exact covered-support
curvature outcomes, pair and ordinary curvature roots, staged endpoint curvature, Halfspace improvements, combined rays, bounded
and ceiling Dickinson certificates, and root compaction.

## Exactness and Termination

All witnesses, zeros, signs, factorizations, Dickinson intervals, final-upper-endpoint routing decisions, curvature roots, and
compaction acceptance decisions are exact. The floating reduced-Hessian calculation inside a flagged interval may only decline
optional work.

The support traversal is finite. Compaction has a finite deterministic node budget and preserves future coverage, so it cannot remove
an unresolved support.

## Known Difficult Inputs

G1 gains little when bounded Dickinson intervals are narrow. It also pays one exact upper-endpoint reduced-Hessian factorization for
every proper Dickinson interval; this is worthwhile only when the resulting routing flag avoids enough later support work.

Near the Boolean ridge, a large family of bounded intervals can remain active simultaneously. Trigger bucketing and upper-cardinality
retirement reduce lookup work but do not guarantee a small index.

The compaction heuristic can stop at a local expansion or exhaust its work budget before discovering a substantially smaller root
family. It deliberately does not solve exact Boolean minimization, create a second SAT instance, or introduce a BDD or ZDD.
Its operation budgets do not include the unavoidable \(O(|\mathcal F|\log|\mathcal F|)\) root ordering step. Sorting and copying can
therefore still be visible for an exceptionally large family, although quadratic activation and unbounded subset scans are avoided.
