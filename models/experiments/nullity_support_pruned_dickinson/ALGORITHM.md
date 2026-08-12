# Nullity Support-Pruned Dickinson

Classification: Coposit-created exact strict-copositivity variant of `support_pruned_dickinson`.

Public mode boundary: this Coposit-created model supports only `strictly_copositive`. Calling
`solve(A, copositivity_mode::copositive)` throws `std::invalid_argument` instead of applying strict rules to a non-strict query.

## Idea In Plain Language

Dickinson's algorithm may choose any admissible nullspace vector when a principal matrix is singular. The choice does not affect
correctness, but it affects how many later supports the resulting certificate covers. The source model chooses the vector obtained
from the first free coordinate of its exact LDLT factorization.

This variant chooses more deliberately:

- for nullity one, compare both signs of the unique null direction;
- for nullity two, examine every distinct exact coverage signature in the complete nullspace and retain the best one;
- for larger nullity, compare both signs of every exact LDLT basis vector and retain the best one.

The remaining Dickinson mathematics and the recursive globally-covered-support prune are copied unchanged from
`support_pruned_dickinson`.

## Name And Sources

The descriptive identifier is `nullity_support_pruned_dickinson`. “Nullity” is the dimension of a matrix's nullspace. “Support
pruned” records that the model starts from the separate `support_pruned_dickinson` variant and retains its recursive elimination of
globally covered supports.

The model uses:

- Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its Applications* 569, 15–37 (2019),
  [DOI 10.1016/j.laa.2018.12.025](https://doi.org/10.1016/j.laa.2018.12.025). Theorem 4.6 and Algorithms 1 and 2 supply the certificate
  condition and explicitly permit any nonzero singular null vector outside the nonpositive orthant.
- `models/experiments/support_pruned_dickinson/`, copied as the complete starting implementation. Its recursive support generator comes from
  FracESSA revision `95e0ec019cf11a60c6423508e8768536a0b88860`, particularly `cpp/include/fracessa/supports.hpp` and the
  support-forbidding orchestration in `cpp/src/fracessa.cpp`.
- `cpp/include/coposit/fraction_free_ldlt.hpp`, whose retained exact factorization now recovers either one null vector or a complete
  exact nullspace basis without refactorization.

This is not a historical Dickinson baseline. The nullspace scoring and search are Coposit changes. The maintained
`dickinson_2019` and `support_pruned_dickinson` models remain independent.

## Mathematical Problem

For a nonempty symmetric integer matrix $A\in\mathbb Z^{n\times n}$, decide whether

$$
x^TAx>0\qquad\text{for every nonzero }x\geq0.
$$

For a support $I\subseteq[n]$, let $C=A_I$. A vector $w$ calculated for $C$ is embedded in the full space as $u$ by putting zeros
outside $I$. Define

$$
S(u)=\operatorname{supp}(u),
\qquad
N_A(u)=\{j:(Au)_j\geq0\}.
$$

Dickinson's certificate covers exactly the interval

$$
[S(u),N_A(u)]=\{J:S(u)\subseteq J\subseteq N_A(u)\}.
$$

When $Cw=0$, every component of $(Au)_I$ is zero. Therefore

$$
S(u)\subseteq I\subseteq N_A(u),
$$

so every nonzero mixed-sign null vector, in either orientation, is a valid certificate for the current support.

## What “Best Coverage” Means

Let

$$
s=|S(u)|,
\qquad
q=|N_A(u)|,
\qquad
m=|I|.
$$

The complete Dickinson interval contains

$$
2^{q-s}
$$

supports, because every index in $N_A(u)\setminus S(u)$ can be included or omitted independently. Many supports smaller than the
current cardinality have already been traversed, however. The primary score therefore counts covered supports of strictly larger
cardinality:

$$
F_I(u)=\sum_{r=m+1}^{q}\binom{q-s}{r-s}.
$$

Candidates are compared lexicographically by:

1. larger $F_I(u)$;
2. larger interval width $q-s$;
3. larger upper-set size $q$;
4. deterministic discovery order when all three values tie.

The first value is an exact count, stored as a FLINT arbitrary-precision integer. It does not overflow when $n$ is large.

This score intentionally does not count remaining supports of the current cardinality and does not subtract overlap with older
certificates. Counting the exact union of all previous coverage intervals could itself require exponential work. Thus the
nullity-two sweep is globally optimal for the stated single-certificate score, not a claim that it minimizes total runtime under
all prior-certificate states.

## Exact Nullspace Basis

Suppose the retained fraction-free LDLT factorization has rank $r<m$ and nullity $k=m-r$. In transformed coordinates, the basis
recovery performs one back substitution for each free coordinate:

1. set that free coordinate to the absolute determinant of the nonsingular leading $r\times r$ block;
2. leave the other free coordinates zero;
3. solve the $r$ pivot equations backwards by exact division;
4. reverse every symmetric swap and addition performed during pivot selection.

The common scaling keeps all columns integral. Positive or negative rescaling does not change a Dickinson signature. Multiplying
$C$ by the resulting $m\times k$ basis matrix gives the exact zero matrix, and the $k$ columns are independent.

The model calculates the full products of all basis vectors once:

$$
P=A_{[:,I]}B.
$$

Every later candidate and full product is then an exact linear combination of columns of $B$ and $P$. The selected product is
retained for certificate construction, avoiding a second multiplication by $A$.

## Nullity One

When $k=1$, every nonzero null vector is a scalar multiple of one basis vector $p$.

- If $p$ or $-p$ is nonnegative, it is a nonzero zero of the quadratic form and the model returns `false` immediately.
- Otherwise $p$ has mixed signs. Both $p$ and $-p$ satisfy Dickinson's admissibility condition. Their supports are equal, but their
  outside product signs are reversed. The model scores both orientations and retains the better one.

This is an exhaustive search because there is only one projective null direction.

## Nullity Two: Complete Exact Sweep

Let $p,q$ be the two exact basis columns. Every projective null direction has the form

$$
w(t)=p+tq,
\qquad t\in\mathbb R\cup\{\infty\}.
$$

For each local coordinate and full-product coordinate, the relevant value is linear in $t$:

$$
p_i+tq_i,
\qquad
(Ap)_j+t(Aq)_j.
$$

Its sign changes only at the exact rational root

$$
t=-\frac{p_i}{q_i}
\quad\text{or}\quad
t=-\frac{(Ap)_j}{(Aq)_j},
$$

when the corresponding direction coefficient is nonzero. Between two consecutive roots, every support bit and product-sign bit is
constant. At a root, a coordinate or product is exactly zero and can produce a distinct, better signature. Therefore the complete
finite search evaluates:

1. both basis directions $p$ and $q$, including $t=0$ and $t=\infty$;
2. one exact point before the smallest root;
3. every distinct root;
4. one exact mediant strictly between each pair of consecutive roots;
5. one exact point after the largest root;
6. both orientations of every mixed-sign candidate.

Roots are represented as a signed numerator and positive denominator and sorted by exact cross multiplication. A rational candidate
$t=a/b$ is never stored as a rational vector. The model materializes the positively scaled integer vector

$$
b p+a q
$$

and the corresponding product $bAp+aAq$. Evaluating all open sign intervals, all boundary roots, and the direction at infinity
exhausts every possible signature in the two-dimensional nullspace. The selected vector is therefore a global maximizer of the
stated coverage score.

If any swept direction is one-signed, one orientation is a nonnegative zero and strict copositivity fails immediately.

## Nullity Greater Than Two

For $k>2$, exact global optimization would require traversing the faces of a central hyperplane arrangement in projective dimension
$k-1$. Its size can grow combinatorially with both $n$ and $k$.

This model deliberately uses the bounded policy requested for the first implementation:

1. evaluate every exact LDLT basis column;
2. reject immediately if either orientation is a nonnegative zero;
3. otherwise score both signs of every column;
4. retain the best signed basis vector.

The former first-free-coordinate vector is the first basis column, so the chosen score can never be worse than the copied model's
score. The search is not globally optimal over arbitrary combinations of three or more basis vectors; that limitation is explicit
and keeps the high-nullity work proportional to the nullity rather than combinatorial.

## Recursive Support Pruning

The copied support generator visits supports by increasing cardinality and, within one cardinality, increasing numeric bit mask.
Every certificate retains its complete interval signature $(S(u),N_A(u))$ for non-strict Dickinson lookup.

When

$$
N_A(u)=[n],
$$

the interval covers every superset of $S(u)$. The generator queues $S(u)$ as a forbidden support and, starting with the next
cardinality, cuts off a recursion branch as soon as it contains that support. A bounded signature with $N_A(u)\neq[n]$ is never
promoted to an upward pruning rule.

Pending activation between cardinalities, lowest-index buckets, branch order, early termination after an empty cardinality, packed
dynamic supports, and non-strict interval lookup are unchanged from `support_pruned_dickinson`.

## Complete Decision Flow

1. Receive a parser-guaranteed nonempty square symmetric integer matrix.
2. For complete inputs through order three, return the shared exact strict-copositivity result.
3. Generate surviving supports in cardinality and numeric-mask order.
4. Apply the shared direct strict test through support size three.
5. Skip a support covered by a retained Dickinson signature.
6. Factor the uncovered principal matrix once with exact fraction-free LDLT.
7. If it is nonsingular, solve $A_Iw=\mathbf1$ and reject when $w\leq0$.
8. If it is singular, recover the exact basis and apply the nullity-one, nullity-two, or higher-nullity selection rule above.
9. Reject immediately upon finding a nonzero nonnegative null vector.
10. Retain the selected signature and queue its lower support for recursive pruning exactly when its full product is nonnegative.
11. Continue until every surviving support has been processed.
12. Return `true` only after the completed strict Dickinson certificate contains no negative witness or nonnegative zero.

## Exact Arithmetic, Timeouts, And Limits

All matrix entries, nullspace columns, breakpoints, candidate vectors, products, binomial counts, and comparisons use exact FLINT
integers. Rational breakpoints are used only as numerator-denominator pairs whose positive scaling produces integer vectors. No
floating-point sign or score affects a result.

Supports use $\lceil n/64\rceil$ packed 64-bit words and impose no dimension-63 limit. Timed Python modules check the cooperative
timeout flag at support-recursion, factorization, basis-product, candidate, and matrix-row boundaries. A timeout remains unresolved
and is never returned as `false`.

## Source Behavior And Coposit Changes

Unchanged from Dickinson and `support_pruned_dickinson`:

- coverage condition $S(u)\subseteq I\subseteq N_A(u)$;
- nonsingular solve $A_Iw=\mathbf1$ and its negative-witness rule;
- admissibility of a singular null vector;
- strict nonnegative-zero termination and completed-certificate conclusion;
- recursive pruning only for globally covering signatures;
- support order, exact arithmetic, packed representations, and public validation.

Added only in this model:

- complete exact basis recovery from the retained singular factorization;
- coverage scoring of signed null vectors;
- exhaustive projective signature search at nullity two;
- best-signed-basis selection above nullity two;
- reuse of the selected basis-combination product during signature construction.

The model does not alter nonsingular right-hand sides, add cone subdivision, Frank–Wolfe witnesses, graph decomposition, KKT
conditions, ESS tests, or FracESSA game normalization.

## Known Difficult Inputs

The nullity optimization has no effect on nonsingular principal matrices and no effect when singular supports are already covered.
Matrices whose difficulty comes mainly from nonsingular supports can therefore behave exactly like `support_pruned_dickinson` while
paying no nullspace-search cost.

At nullity two, the number of exact breakpoints is linear in the full matrix dimension, but materializing and scoring one candidate
per interval and boundary is quadratic in that dimension. Very large matrices with many outside sign changes can make this work
substantial even though it avoids combinatorial nullspace enumeration.

For nullity greater than two, the best coverage direction may require a linear combination of several basis columns. This model
does not search those combinations, so it can still retain a narrow signature even when a wider one exists elsewhere in the
nullspace.

The recursive generator still helps only when a selected certificate has a globally nonnegative product. Bounded signatures use
non-strict lookup, and matrices with many uncovered supports can still approach the full power-set traversal. Large integer entries
can independently make exact factorization, basis recovery, and product arithmetic expensive.
