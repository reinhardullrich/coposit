# BPQY Designated Zeros And Dickinson Certificate Geometry

## Finding

The Bomze–Peng–Qiu–Yıldırım (BPQY) matrices show that exact arithmetic removes numerical ambiguity but does not remove combinatorial
difficulty. For Dickinson-type copositivity tests, the support size of a designated zero controls both how late the algorithm can
encounter that zero and how much of the remaining support lattice its certificate can cover.

BPQY already report empirically that their exact mixed-integer quadratic models become slower as the support size $\rho_0$ of the
designated optimum increases. The explanation below—the width of Dickinson's certificate interval and its effect in an exact-integer
copositivity checker—is not given in their paper. It is an internal conclusion from coposit's exact runs and diagnostics trace, not a
claim of general research novelty beyond the sources checked here.

Primary source:

- Immanuel M. Bomze, Bo Peng, Yuzhou Qiu, and E. Alper Yıldırım, “Tighter yet more tractable relaxations and nontrivial instance
  generation for sparse standard quadratic optimization,” *Mathematical Programming Computation* 17 (2025), 617–651;
  [arXiv:2406.01239](https://arxiv.org/abs/2406.01239).

The exact local reconstruction is retained in
[`generate_bpqy_julia185_matrices.jl`](../testdata/archive/generate_bpqy_julia185_matrices.jl) and
[`import_bpqy_julia185_matrices_2026_08_14.py`](../testdata/archive/import_bpqy_julia185_matrices_2026_08_14.py).

## Construction And Exact-Materialization Qualification

BPQY prescribe a simplex point $x\geq0$ with

$$
\mathbf1^Tx=1,
\qquad
|\operatorname{supp}(x)|=\rho_0,
$$

and construct a matrix $Q$ for which $x$ is the designated optimum. With the experiment's choice $\lambda=0$, the ideal
construction lies on the copositive boundary and satisfies

$$
Qx=0,
\qquad
x^TQx=0.
$$

The experiment varies

$$
n\in\{25,50\},
$$

with $\rho_0\in\{6,12,19\}$ for $n=25$ and $\rho_0\in\{12,25,38\}$ for $n=50$. It generates PSD, SPN, and COP classes. The COP
class is deliberately indefinite and outside the SPN cone, so a positive-semidefinite test or exact DNN relaxation cannot dispose of
it.

The 450 matrices retained by coposit are not symbolic versions of the ideal real construction. They are exact primitive integer
representatives of the Julia 1.8.5 Float64 outputs. Exactness begins after that rounding. Consequently, the intended zero can become a
very small positive or negative value, and the maintained truth fields remain unset until an exact checker decides the materialized
matrix.

## Why Exact Arithmetic Does Not Make The Case Easy

Dickinson enumerates principal supports in increasing cardinality. Exact arithmetic guarantees that every solve, sign, zero, and
certificate is correct, but the algorithm can still face exponentially many supports.

Let

$$
R=\operatorname{supp}(x),
\qquad
|R|=\rho_0.
$$

For an ideal boundary matrix, strict mode cannot encounter the designated zero before reaching cardinality $\rho_0$. Before that
cardinality, the unpruned search space contains

$$
\sum_{k=1}^{\rho_0-1}\binom{n}{k}
$$

supports. For $n=25$:

| $\rho_0$ | Supports before cardinality $\rho_0$ |
| ---: | ---: |
| 6 | 68,405 |
| 12 | 11,576,915 |
| 19 | 33,308,925 |

There are only $2^{25}-1=33,554,431$ nonempty supports altogether. A support-19 zero therefore occurs almost at the end of the raw
increasing-cardinality traversal. Exact recognition of the zero does not help before the algorithm reaches its support.

## Certificate Width

A Dickinson vector $u$ covers precisely the Boolean-lattice interval

$$
\operatorname{supp}(u)\subseteq I\subseteq N_Q(u),
\qquad
N_Q(u)=\{i:(Qu)_i\geq0\}.
$$

For an ideal zero $Qx=0$,

$$
N_Q(x)=\{1,\ldots,n\}.
$$

Its certificate therefore covers every superset of $R$, a total of

$$
2^{n-\rho_0}
$$

supports. At order 25:

| $\rho_0$ | Supersets covered by the ideal zero certificate |
| ---: | ---: |
| 6 | 524,288 |
| 12 | 8,192 |
| 19 | 64 |

This is the central mechanism. A small-support zero is a powerful lower endpoint for pruning. A large-support zero both arrives late
and covers few supersets. The same mechanism can survive when Float64 materialization moves the matrix into the strict interior: the
near-zero principal support produces an exact Dickinson vector, but the certificate still covers only

$$
2^{|N_Q(u)|-|\operatorname{supp}(u)|}
$$

supports.

The actual coordinate positions also matter to the CBDD representation. BPQY shuffle the designated support. Two supports of equal
cardinality can therefore produce different ordered-diagram sharing and traversal behavior even though the cardinality effect remains
the dominant trend observed so far.

## Ten-Second Exact Results

The 450 exact integer materializations were run with `cbdd_zed_dickinson` in ordinary-copositivity mode, preprocessing disabled, and a
ten-second cutoff.

| BPQY group | Completed | Timed out |
| --- | ---: | ---: |
| COP, order 25 | 32 | 43 |
| PSD, order 25 | 32 | 43 |
| SPN, order 25 | 75 | 0 |
| COP, order 50 | 0 | 75 |
| PSD, order 50 | 0 | 75 |
| SPN, order 50 | 75 | 0 |
| **Total** | **214** | **236** |

The SPN rows are not evidence that their ideal class is easy for this exact checker: Float64 materialization introduced a negative
diagonal into every retained SPN row, so all are rejected immediately.

For the order-25 COP and PSD rows together, completion is strongly ordered by designated support size:

| $\rho_0$ | Completed within 10 s | Timed out |
| ---: | ---: | ---: |
| 6 | 46 | 4 |
| 12 | 18 | 32 |
| 19 | 0 | 50 |

## Digit Growth Is Not The Discriminator

All 225 order-25 materializations have similar exact coefficient and elimination sizes:

| Group | Median input-entry digits | Median maximum fraction-free elimination digits |
| --- | ---: | ---: |
| COP completed | 18 | 461 |
| COP timeout | 18 | 465 |
| PSD completed | 18 | 442 |
| PSD timeout | 18 | 453 |
| SPN completed immediately | 18 | 451 |

The largest input entries have 19–22 decimal digits, while whole-matrix Bareiss elimination reaches roughly 424–511 digits. This
growth is real but does not separate completed rows from timeouts. The instant SPN decisions have essentially the same potential
elimination size. The principal cause of the timeout pattern is therefore certificate coverage and support enumeration, not unusual
arbitrary-precision growth.

## Matrix 12580 Diagnostics Trace

Matrix 12580 is the order-25 COP instance with $\rho_0=6$ and seed 6. It is dense, has no negative diagonal, no failing order-two
principal submatrix, and a connected negative-entry graph. Its maximum input coefficient has 19 digits; whole-matrix fraction-free
elimination reaches 438 digits.

The intended six-coordinate zero is visible in the exact materialization as an almost singular principal support. The corresponding
stationary objective is positive but only about $3.66\times10^{-19}$ relative to the largest input coefficient. For the exact
Dickinson vector generated on that support,

$$
|\operatorname{supp}(u)|=6,
\qquad
|N_Q(u)|=17,
$$

so this one rounded-materialization certificate covers $2^{17-6}=2,048$ supports rather than the ideal construction's 524,288.

A no-preprocessing diagnostics run completed non-strict copositivity in 27.779 seconds:

| Elapsed | Cardinality | Emitted exact supports | Retained certificates | Cumulative CBDD operations |
| ---: | ---: | ---: | ---: | ---: |
| 10 s | 9/25 | 34,521 | 34,520 | 21.0 million |
| 20 s | 11/25 | 64,186 | 64,185 | 34.5 million |
| 27.779 s | 25/25 | 80,576 | 80,576 | 39.5 million |

The run returned `is_copositive=true`. The original database result remains the historical ten-second timeout; this diagnostic run
did not overwrite it or determine strict copositivity.

The trace shows active combinatorial work rather than numerical stalling. CBDD certificates prune almost all of the full
$33.55$-million-support lattice, but 80,576 exact principal-support calculations and 39.5 million diagram operations are still enough
to exceed ten seconds.

## What BPQY Already Report And What This Adds

BPQY report for their exact MIQP formulations that, at fixed $n$ and fixed ratio $\rho/\rho_0$, solution time increases as $\rho_0$
increases across PSD, SPN, and COP instances. They also report 600-second timeouts for some order-25 SPN and COP instances and some
order-50 PSD instances. In that paper, “exact model” means a formulation that solves the original optimization problem rather than a
relaxation; it does not mean arbitrary-precision rational arithmetic.

The coposit experiment adds a different observation: the $\rho_0$ effect persists in an exact-integer copositivity checker that does
not solve BPQY's sparse optimization model and does not use its sparsity parameter $\rho$. For Dickinson, the mechanism is explicit:

1. increasing-cardinality traversal reaches a large-support zero late;
2. a certificate with lower endpoint of size $\rho_0$ can cover at most $2^{n-\rho_0}$ supersets when its upper endpoint is full;
3. Float64 materialization can make $N_Q(u)$ smaller still, narrowing the interval;
4. shuffled support positions can reduce ordered CBDD sharing;
5. exact arithmetic makes every decision correct but cannot remove this Boolean-lattice workload.
