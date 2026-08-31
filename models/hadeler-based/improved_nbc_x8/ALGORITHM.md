# Improved NBC-X8

Classification: coposit-created exact CP/SCP experiment. Improved NBC-X8 copies the complete traversal and curvature policy of the
current `improved_nbc_x6` incumbent, but replaces X6's Halfspace-Rays and targeted-LP widening on nonsingular low-frontier supports
with one weighted-halfspace root LP relaxation.

The public identifier is `improved_nbc_x8`. “Improved NBC” names the resumable Boolean-lattice backend. “X8” distinguishes this
relaxed-membership Dickinson experiment from X6's greedy exact rays and targeted continuous LP proposals. The model implements
`copositive`, `strictly_copositive`, and one-pass `both` classification; `both` is the analysis default.

## Idea In Plain Language

The model alternates between small and large supports. For a low support (I), it first asks whether curvature already removes every
superset of (I). If so, no Dickinson optimization is useful: curvature reaches the full support ([n]) immediately.

Otherwise, for nonsingular (A_I), X8 first solves the traditional Dickinson system

$$
A_Iu_I=\mathbf1.
$$

If its upper endpoint is already ([n]), that exact certificate is inserted immediately. If any upper index is missing, X8 solves
the root LP relaxation of a local maximum-halfspace MILP. The relaxed optimum proposes a different nonnegative right-hand side; X8
does not branch on fractional membership variables. Its resulting Dickinson interval is admitted only after exact integer
reconstruction and verification.

The high frontier is unchanged from X6. It uses a floating-point positive-semidefiniteness screen, but only exact factorization can
install a downward certificate. Floating point can therefore save work or propose a better Dickinson vector, but it cannot classify
a matrix or remove a support by itself.

## Sources And Model Boundary

X8 is an independent copy of [`improved_nbc_x6`](../improved_nbc_x6/ALGORITHM.md). It preserves X6's Improved NBC support backend,
alternating low/high traversal, pair precheck, exact curvature tests, singular-support policy, downward verification, certificate
storage, combined classification, and diagnostics contract.

Dickinson intervals come from Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its Applications*
569 (2019), 15–37, DOI [`10.1016/j.laa.2018.12.025`](https://doi.org/10.1016/j.laa.2018.12.025), especially Theorem 4.6 and
Algorithms 1–2. The relaxed maximum-halfspace formulation and its use at every residual nonsingular Dickinson support are coposit
changes. X8 uses the root relaxation from the shared in-house dense simplex/MILP implementation and performs no branch-and-bound.

## Boolean-Lattice Traversal

Every nonempty (I\subseteq[n]) identifies a principal matrix (A_I) and a simplex face. Two Improved NBC searches start at
cardinalities (1) and (n). The model alternates one open low support and one open high support. Every exact certificate enters
both searches immediately.

The low frontier can add:

- an upward curvature closure ([I,[n]]); or
- a Dickinson interval ([L,U]).

The high frontier can add a downward closure ([\varnothing,I]) only after exact positive-definiteness or the maintained exact
singular positive-semidefinite consistency test. A floating rejection at the high frontier adds no clause; that support remains
available to the complete low traversal.

## Curvature First

Let (B=A_I), and let

$$
\mathcal T_I=\{v\in\mathbb R^{|I|}:\mathbf1^Tv=0\}
$$

be the tangent space of the face. If (v^TBv) is not positive for every nonzero (v\in\mathcal T_I), then (I) cannot be the
support of a minimal-support global minimizer. The same is true for every superset, so X8 installs the upward closure

$$
[I,[n]].
$$

The exact fraction-free (LDL^T) factorization decides this condition without explicitly constructing a tangent basis. A
nonsingular (B) has positive-definite reduced Hessian precisely when either (B) is positive definite, or (B) has one negative
eigenvalue and

$$
\mathbf1^TB^{-1}\mathbf1<0.
$$

If curvature supplies the upward closure, X8 does not construct or optimize a Dickinson interval.

## Ordinary Dickinson Certificate

Suppose (B) is nonsingular and survives curvature pruning. X8 solves (Bu_I=\mathbf1), embeds (u_I) in
(mathbb R^n) with zeros outside (I), and computes (Au) exactly. For any admissible certificate vector (u), define

$$
L(u)=\operatorname{supp}(u),
\qquad
U(u)=\{j\in[n]:(Au)_j\ge0\}.
$$

Dickinson's theorem certifies every support (J) in

$$
[L(u),U(u)]=\{J:L(u)\subseteq J\subseteq U(u)\}.
$$

If (U(u)=[n]), X8 stores this interval immediately. No inverse matrix, MILP, ray search, or lower-endpoint shrinking is then run.

## Root Relaxation Proposal

Assume the ordinary endpoint is not full. The retained factorization solves all coordinate right-hand sides, giving the columns of
(B^{-1}). For a nonnegative, nonzero right-hand side (b), define

$$
u_I(b)=B^{-1}b,
\qquad
g_j^T=A_{j,I}B^{-1}\quad(j\notin I).
$$

Then

$$
(Au(b))_j=g_j^Tb.
$$

X8 normalizes (b) to the closed simplex

$$
\mathbf1^Tb=1,
\qquad b_i\ge0.
$$

The normalization excludes (b=0), while allowing boundary points. Nonnegativity is sufficient because ((Au)_i=b_i\ge0) for
(i\in I). Boundary points matter: they can make coordinates of (u=B^{-1}b) exactly zero and thereby shrink (L(u)). Every
right-hand side reached by X6's positive coordinate rays lies in this simplex after normalization.

For every outside index (j\notin I), a membership variable (z_j) represents the corresponding halfspace. The formulation also
records the two halfspaces (u_i\ge0) and (-u_i\ge0) for every (i\in I). In the integer model, at least one of this pair is always
true and both are true exactly when (u_i=0), so the second satisfied halfspace counts a coordinate removed from (L). X8 relaxes
every membership variable to (0\le z_j\le1).

Let (m=n-|I|) and (W=m+1). Outside-product halfspaces have weight (W+1), while each sign halfspace for (u_i) has weight (W).
The weighted objective of the underlying integer model equals a constant plus

$$
W\bigl(|U|-|L|\bigr)+|U\setminus I|.
$$

It therefore represents lexicographic maximization of (D=|U|-|L|) and then (|U|). Each halfspace is represented by the usual
big-(M) constraint

$$
r^Tb\ge-M_r(1-z_r).
$$

Each big-(M) value is obtained from the minimum of its row over the closed simplex. Rows that are always satisfied or impossible to
satisfy are removed before the root LP.

The shared solver solves exactly one dense two-phase simplex relaxation. The relaxed objective is only an upper bound because
fractional memberships can partially count incompatible halfspaces. X8 ignores those fractional membership values after the solve:
it takes the relaxation's right-hand side (b), computes the actual interval it produces, and performs no branch-and-bound. If the
candidate numerically reaches the absolute lexicographic ceiling ((D,|U|)=(n-1,n)), it is already the best possible proposal.
Only the caller's global cooperative timeout may interrupt the root LP. An internal size or arithmetic failure is reported
explicitly rather than silently switching to another widening method.

The relaxed optimum is not a claim about the integer MILP optimum. Exact copositivity correctness does not depend on the quality of
this numerical proposal.

## Exact Reconstruction

Rows are independently scaled before conversion to binary64. If the MILP finds a point that numerically improves the all-ones
incumbent, X8 normalizes its largest coordinate to one, multiplies all coordinates by (10^9), and rounds them to nonnegative integer
coefficients. It then reconstructs (u=B^{-1}b) and (Au) with exact integer arithmetic.

The candidate replaces the all-ones vector only when its exact interval is better under the maintained X6 order: larger width

$$
D=|U|-|L|,
$$

then larger (|U|) on a tie. If rounding destroys the numerical improvement, X8 keeps the already valid all-ones interval. Thus a
numerical error can lose optional pruning, but it cannot add an invalid certificate.

## Singular Supports

The inverse-based MILP is not defined when (A_I) is singular. X8 therefore retains X6's exact singular branch. It obtains one exact
kernel vector, applies the exact reduced-Hessian curvature test, orients a mixed-sign kernel vector toward the larger upper endpoint,
and installs only the resulting exact Dickinson interval. A nonnegative kernel vector proves failure of strict copositivity but not
ordinary copositivity.

## Complete Decision Flow

1. Install the exact pair-curvature closures.
2. Alternate one uncovered low support and one uncovered high support.
3. At a low support, factor (A_I) exactly.
4. Stop on an exact negative witness; otherwise install an upward closure when curvature allows it.
5. For a residual singular support, use the exact kernel branch.
6. For a residual nonsingular support, compute the exact all-ones Dickinson interval.
7. If its upper endpoint is full, insert it immediately; otherwise solve one root LP relaxation and exactly verify any improvement.
8. At a high support, use floating point only to nominate an exact downward check.
9. Continue until Improved NBC reports that every nonempty support is covered.

Because every installed region has an exact proof and the low traversal remains exhaustive, X8 is a complete exact CP/SCP classifier
whenever it finishes. A global timeout remains unresolved; it is never converted into a negative classification.

## Known Difficult Inputs

- A weak relaxation can assign fractional membership to incompatible halfspaces, so its proposed right-hand side may produce little
  or no exact interval improvement.
- Dense LP tableaus can dominate memory before the outer support traversal has made useful progress.
- Nearly active halfspaces can make the binary64 optimum difficult to reconstruct at the fixed integer scale. The safe result is the
  ordinary exact interval, so the expensive MILP may yield no additional pruning.
- Singular supports cannot use this inverse-based MILP and retain the one-vector kernel policy inherited from X6.
