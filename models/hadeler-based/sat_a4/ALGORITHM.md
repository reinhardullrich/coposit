# SAT-A4: Monotone-Only Dickinson

Classification: coposit-created exact copositivity and strict-copositivity experiment.

`sat_a4` is the fourth experiment derived from the right-hand-side view of Dickinson certificates. “Monotone-only” means that it
starts from Dickinson's ordinary all-ones right-hand side and tries only to enlarge that candidate's upper endpoint. It performs no
coordinate breakpoint sweep, synthesized-ray search, shortlist search, objective LP, or MILP.

The model supports `copositive`, `strictly_copositive`, and combined `both` classification in one traversal. Shared preprocessing is
external; `model::solve` starts with the algorithm below.

## Idea In Plain Language

For a support $I$, ordinary Dickinson solves one exact linear system and obtains a vector $u$. That vector certifies every support
between a lower endpoint $L(u)$ and an upper endpoint $U(u)$. A larger $U(u)$ skips more of the Boolean lattice.

A4 asks one narrow question before storing the certificate:

> Can another strictly positive right-hand side preserve every index already in $U(u)$ and add one more index?

This is a monotone question: an accepted change may enlarge $U$, but may never lose an index already present. A small binary64
feasibility LP proposes a right-hand side. The proposal has no authority: A4 converts it to exact integer coefficients and accepts it
only after exact matrix products prove the claimed inclusion. If no exact extension is found, A4 stores the original Dickinson
certificate unchanged.

## Sources

The certificate and cardinality traversal come from Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra
and its Applications* 569 (2019), 15–37, especially Theorem 4.6 and Algorithms 1–2.

The persistent SAT representation was copied from the local `sat_dickinson` model. It uses CaDiCaL 2.2.1 and one Batcher bitonic
sorting network for exact-cardinality assumptions. The monotone feasibility mechanism was isolated from the local `sat_a3` model;
A4 deliberately omits the Halfspace-Rays stage that precedes it there.

## Dickinson Certificate

Embed the support vector into the full dimension by filling unused coordinates with zero. Define

$$
L(u)=\operatorname{supp}(u),
\qquad
U(u)=\{j:(Au)_j\geq 0\}.
$$

Dickinson's theorem certifies the interval

$$
[L(u),U(u)]=\{J:L(u)\subseteq J\subseteq U(u)\}.
$$

SAT represents membership of index $i$ by a Boolean variable $x_i$. One clause excludes the complete interval:

$$
\bigvee_{i\in L(u)}\neg x_i
\;\lor\;
\bigvee_{i\notin U(u)}x_i.
$$

For a bounded endpoint $|U|<n$, the clause also contains the existing sorting-network output that becomes true above cardinality
$|U|$. Thus an expired interval is satisfied automatically without scanning or deleting stored clauses.

## Exact All-Ones Candidate

For an uncovered nonsingular principal support $I$, A4 factorizes $A_I$ exactly and solves

$$
A_Ix=\mathbf 1.
$$

The fraction-free LDLT solve returns integer numerators and one positive common denominator. If $x\leq0$, then $-x$ is a
nonnegative negative witness and copositivity fails.

Otherwise A4 computes the full exact product $Au$ and records the initial endpoint $F=U(u)$. This is the only incumbent used by the
monotone search; A4 does not run the Halfspace-Rays optimizer first.

## Monotone Enlargement

The same retained factorization solves all unit right-hand sides at once. Its columns are exact scaled directions

$$
v_r=A_I^{-1}e_r,
$$

and A4 computes their full products. For outside indices this gives the matrix

$$
P=A_{I^c,I}A_I^{-1}.
$$

Any positive coefficient vector $b$ defines another exact support candidate

$$
x=A_I^{-1}b.
$$

To preserve the current endpoint $F$ and force one omitted target $t$, A4 seeks

$$
b>0,
\qquad
(Pb)_j\geq0\quad(j\in F\cap I^c),
\qquad
(Pb)_t\geq0.
$$

These constraints are homogeneous. The numerical proposer fixes a harmless scale by writing $b=\mathbf1+y$ with $y\geq0$, turning
the task into a pure feasibility LP. There is no objective, binary variable, or big-$M$ constant.

Rows are scaled independently before the binary64 solve. A proposed point is normalized and rounded at four integer scales. For each
reconstruction A4 recomputes $x$ and $Au$ exactly, rejects any nonpositive coefficient, checks every preserved row and the target
exactly, and accepts only a strictly better endpoint. Common integer content is removed after acceptance.

After one extension, A4 rebuilds $F$ from the new exact product and tries again. Therefore accepted endpoints form a strict chain

$$
F_0\subsetneq F_1\subsetneq\cdots\subseteq[n],
$$

so at most $n-|F_0|$ extensions can be accepted on one support.

The numerical work has a 20 ms support-local deadline and an eight-million-entry scaled-tableau bound. Reaching either bound simply
keeps the current exact incumbent; it does not change the classification.

## Singular Supports

The singular branch is unchanged from SAT-Dickinson. A4 obtains one exact nullspace vector, reverses its sign only when necessary to
give it a positive entry, and uses that vector's ordinary Dickinson interval. A nonnegative nullspace vector is a nonnegative zero:
strict copositivity fails, while ordinary copositivity may continue. Monotone enlargement is not run on singular supports because
$A_I^{-1}$ does not exist.

## Traversal And Termination

One persistent CaDiCaL instance enumerates uncovered supports in increasing cardinality. Temporary assumptions on the shared sorting
network enforce the active cardinality. Every exact Dickinson interval becomes one persistent blocking clause. When SAT proves one
cardinality exhausted, the traversal advances to the next.

All matrix decisions use arbitrary-precision integers. Binary64 is used only to propose coefficient vectors that are independently
verified exactly. Every processed support either returns a decisive witness or blocks at least itself, so the finite Boolean search
terminates when sufficient time and memory are available. Timeouts and resource limits remain unresolved results.

## Known Difficult Inputs

A4 pays for all inverse-direction products even when the monotone feasibility problems find no exact enlargement. It is therefore
unfavorable when the all-ones endpoint is already locally maximal or when improvements require first losing an existing upper index.

The feasibility probes test omitted indices one at a time and stop at the first exact extension. They do not find a globally largest
endpoint. Numerically delicate feasible regions may also fail to yield a reconstructible integer point within the fixed deadline;
this loses only an optimization opportunity, never correctness.

As in every SAT-Dickinson variant, many narrow certificates can still produce an exponential number of exact factorizations and a
large persistent clause database.
