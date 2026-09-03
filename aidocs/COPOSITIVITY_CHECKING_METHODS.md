# Methods That Actually Check Copositivity Membership

**Search date:** 2026-08-31

## 1. Selection rules and how to read the table

This review collects general algorithms that accept an arbitrary real symmetric matrix and can establish copositivity (CP), strict
copositivity (SCP), or both. It is not limited to papers presented as copositivity tests: a standard quadratic programming (StQP) or
general quadratic programming method is included when applying it to the simplex gives a rigorous membership decision. An existing
software implementation is not required, but the paper must specify an executable procedure rather than only an existence theorem or
reformulation.

We admit three kinds of procedure:

- a **general complete solver**, which is guaranteed in exact mathematics to decide at least one of CP and SCP on every input;
- a **general partial solver**, which can establish results on both sides but may terminate inconclusively or lack a finite termination
  guarantee;
- a **general certificate hierarchy**, which searches an explicitly expanding family of positive certificates, although failure to
  find one does not refute membership.

This excludes methods that require a special matrix class, single fixed structural, inner-cone, or relaxation screens, standalone
negative-witness heuristics, local candidates without a global certificate, and copositive reformulations that assume an unspecified
membership oracle.
The excluded approaches and representative papers are listed in Section 2. Fixed screens may still be valuable preprocessing, and
negative-witness searches may be useful components of a solver; they are simply not membership procedures of the kind catalogued here.

Exact CP and SCP decisions together determine the full classification: CP false means NCP, CP true with SCP false means boundary CP,
and SCP true implies CP. In the table, **Yes** means that the algorithm always decides that predicate in exact mathematics;
**Sometimes** means a structural possibility of an inconclusive result, not merely a practical timeout or exhausted memory; and **No**
means that the framework does not decide the predicate. The entries describe the capability of the framework, including an immediate
strict-inequality variant when available, rather than only the predicate emphasized by its authors.

Within each capability group, methods are ordered by section number, and every method appears exactly once. The main surveys used to
organize the literature are [Hiriart-Urruty and Seeger](https://doi.org/10.1137/090750391),
[Dür](https://optimization-online.org/wp-content/uploads/2009/11/2464.pdf), and
[Bomze](https://www.sciencedirect.com/science/article/pii/S0377221711003705). The complexity background is given by
[Murty and Kabadi](https://doi.org/10.1007/BF02592948).

| Algorithm | Section | CP | SCP |
|---|---:|---:|---:|
| **General complete solvers** |  |  |  |
| Gaddum's matrix-game recursion | 3.1 | Yes | Yes |
| Cottle-Habetler-Lemke recursion | 3.2 | Yes | Yes |
| Hadeler-Bomze-Danninger support recursions | 3.3 | Yes | Yes |
| Dickinson's published algorithm | 3.4 | Yes | Yes |
| Kaplan and the Pareto spectrum | 3.5 | Yes | Yes |
| Exhaustive KKT or face enumeration | 3.6 | Yes | Yes |
| Väliaho's principal-pivot criteria | 3.7 | Yes | Yes |
| COPOMATRIX | 4.1 | Yes | Yes |
| Anstreicher and Peng MILP | 7.1 | Yes | No |
| Gondzio-Yıldırım and generic StQP MILP | 7.2 | Yes | Yes |
| Scozzari-Tardella exact clique algorithm | 8.1 | Yes | Yes |
| Finite KKT branch-and-bound for general QP | 8.2 | Yes | Yes |
| De Klerk-Pasechnik exact LP | 8.4 | Yes | Yes |
| Nie-Yang-Zhang moment-SOS detection | 9.3 | Yes | Sometimes |
| Exact symbolic membership | 10 | Yes | Yes |
| **General partial solvers** |  |  |  |
| Block and Schur-complement recursion | 4.2 | Sometimes | Sometimes |
| Simplicial partition | 5.1 | Sometimes | Sometimes |
| Cone splitting and difference-of-convex subdivision | 5.2 | Sometimes | Sometimes |
| Adaptive ellipsoid approximation | 5.3 | Sometimes | Sometimes |
| Brás-Eichfelder-Júdice LCP procedures | 6.1 | Sometimes | Sometimes |
| Two-phase LPLCC StQP algorithm | 6.2 | Sometimes | Sometimes |
| Spatial branch-and-bound for StQP and general QP | 8.3 | Sometimes | Sometimes |
| SDP/QP global-optimality screening | 8.5 | Sometimes | Sometimes |
| **General certificate hierarchies** |  |  |  |
| Pólya coefficient certificates | 9.1 | Sometimes | Sometimes |
| Parrilo SOS certificates | 9.2 | Sometimes | Sometimes |
| Newer SOS membership certificates | 9.4 | Sometimes | Sometimes |
| Generic polynomial-optimization hierarchies | 9.5 | Sometimes | Sometimes |

## 2. Approaches considered but excluded

The following approaches are relevant to copositivity, but do not satisfy the selection rule in Section 1:

- **Fixed cone-membership screens:** PSD, entrywise-nonnegative, SPN, and LP-detectable inner-cone tests are preprocessing, not a
  general solver or expanding hierarchy. See [Diananda](https://doi.org/10.1017/S0305004100036185) and
  [Tanaka and Yoshise](https://doi.org/10.1007/s10479-017-2720-z).
- **Single fixed DNN or SDP relaxations:** these give useful lower bounds but need not decide the original problem. See
  [Gökmen and Yıldırım](https://doi.org/10.1007/s10107-020-01611-0).
- **Local, randomized, or negative-witness searches:** finding \(x\ge0\) with \(x^TAx<0\) proves NCP, but failure proves nothing. See
  [Badenbroek and de Klerk](https://doi.org/10.1007/s10957-022-02034-x).
- **Graph-specific reductions and solvers:** the [Motzkin-Straus theorem](https://doi.org/10.4153/CJM-1965-053-6) is powerful for
  graph-derived matrices but is not a general symmetric-matrix checker.
- **Reformulations that assume cone membership:** [Burer's copositive representation](https://doi.org/10.1007/s10107-008-0223-z)
  moves the difficulty into a copositive-cone constraint and therefore requires a membership procedure.
- **Structural classifications and matrix constructions:** these provide theory and test instances rather than a general checker. See
  [Johnson and Reams](https://doi.org/10.13001/1081-3810.1245), [Hildebrand](https://doi.org/10.1016/j.laa.2012.04.017), and
  [Dickinson, Dür, Gijben, and Hildebrand](https://doi.org/10.1016/j.laa.2012.10.014).

## 3. Support, spectral, and finite algebraic procedures

### 3.1 Gaddum's matrix-game recursion

[Gaddum (1958)](https://msp.org/pjm/1958/8-3/pjm-v8-n3-p04-p.pdf) proves parallel recursive criteria. After the proper principal
submatrices have passed the corresponding recursion, CP is equivalent to a nonnegative value of an associated two-person zero-sum
matrix game, while SCP is equivalent to a positive value. That value is obtained by linear programming.

- **Membership output:** full NCP/boundary-CP/SCP classification when both recursions are evaluated.
- **Why it is complete:** recursion eventually reaches every relevant principal submatrix.
- **Exactness:** exact rational LP makes both decisions exact.

### 3.2 Cottle-Habetler-Lemke recursion

Under the recursive assumption that every order-\(n-1\) principal submatrix is copositive, the
Cottle-Habetler-Lemke criterion says

\[
A\text{ is copositive}
\iff
\det A\ge0\quad\text{or}\quad \operatorname{adj}(A)\text{ has a negative entry}.
\]

Equivalently, under the same premise, \(A\) is non-copositive exactly when \(A^{-1}\) exists and is entrywise nonpositive. See
[*On Classes of Copositive Matrices*](https://www.sciencedirect.com/science/article/pii/0024379570900029) and the
[survey](https://doi.org/10.1137/090750391).

- **Membership output:** full NCP/boundary-CP/SCP classification; the original paper gives parallel characterizations of CP and SCP.
- **Exactness:** determinants and sign tests can be performed exactly for integer or rational matrices.

### 3.3 Hadeler-Bomze-Danninger support recursions

- [Hadeler (1983)](https://www.sciencedirect.com/science/article/pii/0024379583900952) gives recursive conditions for both
  copositivity and strict copositivity using principal supports and generalized eigenvalue/all-ones systems.
- [Bomze (1987)](https://doi.org/10.1080/02522667.1987.10698891) studies the recursive structure, while
  [Bomze (1996)](https://doi.org/10.1016/0024-3795%2895%2900165-4) develops block-pivot shortcuts.
- Danninger's *A Recursive Algorithm for Determining (Strict) Copositivity of a Symmetric Matrix* gives distinct non-strict and
  strict recursions; the bibliographic record is in the [conference volume](https://d-nb.info/901006696/04).

These methods directly support full CP/SCP classification when their non-strict and strict conditions are evaluated.

### 3.4 Dickinson's published algorithm

- [Dickinson (2019)](https://doi.org/10.1016/j.laa.2018.12.025) gives a finite CP/NCP algorithm whose certificate may cover an
  interval of principal supports. The paper presents the algorithm as a CP decision procedure, but its minimal-zero results also make
  the same finite certificate a strictness test. An [open manuscript](https://research.utwente.nl/files/87825628/cop_cert.pdf) is
  available.

If the completed certificate contains a nonnegative zero, the matrix is not SCP; if it contains none, Dickinson's Lemma 5.2 and
Corollary 5.3 exclude every minimal nonnegative zero. Recording this while constructing the CP certificate therefore gives a complete
one-pass CP/SCP classification.

### 3.5 Kaplan and the Pareto spectrum

[Kaplan](https://www.sciencedirect.com/science/article/pii/S0024379500001385) proves that \(A\) is copositive if and only if no
principal submatrix has a negative eigenvalue with a strictly positive eigenvector. In Pareto-spectral language,

\[
A\text{ is CP}\iff\text{every Pareto eigenvalue is nonnegative},
\]

and \(A\) is SCP if and only if every Pareto eigenvalue is positive.

- **Membership output:** full NCP/boundary-CP/SCP classification.
- **Cost:** direct enumeration is exponential in the number of supports.
- **Exactness:** zero eigenvalues and positivity of algebraic eigenvectors require exact or certified treatment.

### 3.6 Exhaustive KKT or face enumeration

Every minimizer of the StQP lies in the relative interior of some simplex face \(\Delta_I\). On every nonempty support \(I\), solve the
stationarity equations, retain feasible stationary points, and compare their objective values. Including face boundaries recursively
gives the exact global minimum.

- **Membership output:** full classification from the sign of the least value.
- **Published variants:** Hadeler/Kaplan are structured realizations; [G.-Tóth, Hendrix, and Casado](https://doi.org/10.1007/s10100-021-00737-6)
  use top-down and bottom-up face traversal with convexity and monotonicity pruning.
- **Cost:** exponentially many faces in the worst case.

### 3.7 Väliaho's principal-pivot criteria

[Väliaho (1986)](https://doi.org/10.1016/0024-3795%2886%2990246-6) gives finite principal-pivot criteria for CP, SCP, and
copositive-plus matrices. Evaluating the CP and SCP criteria gives a full classification for a general symmetric matrix.

## 4. Coordinate, cone, and block recursion

### 4.1 COPOMATRIX

[Xu and Yao's COPOMATRIX](https://arxiv.org/abs/1011.2039) projects away coordinates, transforms the remaining cone, and recursively
tests the resulting lower-dimensional problems.

- **Membership output:** complete CP versus NCP decision.
- **Strictness:** the paper explicitly states that the same recursion gives an analogous strict-copositivity algorithm. Running the
  non-strict and strict variants gives full classification.
- **Complexity:** simple exponential growth in the published analysis.

### 4.2 Block and Schur-complement recursion

[Bomze's block-pivot method](https://doi.org/10.1016/0024-3795%2895%2900165-4) and his
[Perron-Frobenius/block criterion](https://doi.org/10.1016/j.laa.2008.02.003) replace one orthant problem by lower-dimensional
problems on transformed cones. When all children are explored, this is a membership procedure; when only favorable blocks are tried,
it is a positive shortcut.

## 5. Geometric subdivision as a membership test

### 5.1 Simplicial partition

Simplicial algorithms partition \(\Delta_n\). On each simplex with vertex matrix \(V\), they either prove \(V^TAV\) copositive through
a tractable sufficient cone, find a negative point, or subdivide the simplex.

Representative methods are:

- [Bundfuss and Dür (2008)](https://doi.org/10.1016/j.laa.2007.09.035);
- [Žilinskas and Dür (2011)](https://doi.org/10.1080/10556788.2010.544310), using depth-first traversal;
- [Sponsel, Bundfuss, and Dür (2012)](https://doi.org/10.1007/s10898-011-9766-2), with stronger cell tests;
- [Tanaka and Yoshise (2015)](https://optimization-online.org/2014/04/4328/), with LP-representable subcones;
- [Safi, Nabavi, and Caron (2021)](https://doi.org/10.1007/s10898-021-01092-1), with modified partition and LP tests.

For the usual shrinking partitions:

- a negative point proves **NCP**;
- finite coverage by positive cells proves **CP**;
- every SCP matrix is eventually covered, so the method eventually proves **SCP**;
- a boundary CP matrix may subdivide forever.

Therefore this is generally not a complete boundary classifier, even though every finite positive or negative answer is a valid
membership answer.

### 5.2 Cone splitting and difference-of-convex subdivision

Polyhedral-cone splitting uses the same principle with cones rather than simplex cells. A complete tree can decide membership when its
local bounds are valid and refinement converges appropriately.

Mathieu Dutour Sikirić's source-level
[`PairDecomposition`](https://github.com/MathieuDutSik/polyhedral_common/commit/33ce96e4d0589f340a0fbfd7824ff70f9a2ce093)
implementation repeatedly replaces a troublesome generator pair by their sum and covers the parent cone with two child cones. Its
later [`TestStrictCopositivity`](https://github.com/MathieuDutSik/polyhedral_common/commit/d2252bc89d991fa6df9750ac9647e19b6a9aca02)
path supplies practical strict and non-strict tests. Every finite closed tree is an exact membership proof, but the pure same-order
refinement has no known discrete termination measure; a run that does not close remains unresolved.

[Bomze and Eichfelder (2013)](https://optimization-online.org/2010/01/2523/) write \(A=A_+-A_-\), derive LP/convex-QP bounds, and
use \(\omega\)-subdivision in a global branch-and-bound algorithm.

- A valid lower bound \(\ge0\) proves **CP**; a strictly positive lower bound proves **SCP**.
- A feasible negative point proves **NCP**.
- A remaining gap around zero is unresolved.

### 5.3 Adaptive ellipsoid approximation

[Deng, Fang, Jin, and Xing (2013)](https://doi.org/10.1016/j.ejor.2013.02.031) approximate the relevant quadratic problem by conic
programs over unions of ellipsoids and refine the approximation.

- **Membership use:** a feasible upper bound below zero proves NCP; a valid lower bound at least zero proves CP, and a strictly
  positive lower bound proves SCP.
- **Limitation:** the published numerical procedure is tolerance-qualified and does not by itself certify exact boundary CP.

## 6. Complementarity algorithms

### 6.1 Brás-Eichfelder-Júdice LCP procedures

The KKT conditions for the StQP form a linear complementarity problem (LCP). [Brás, Eichfelder, and Júdice
(2016)](https://doi.org/10.1007/s10589-015-9772-2) derive CP and SCP conditions from special LCPs and process them by complementary-basis
enumeration, Lemke pivots, or MILP.

- A suitable LCP solution can prove NCP or failure of SCP.
- Proving that the relevant LCP has no solution can prove CP or SCP.
- The enumerative procedure can in principle exhaust its search, but the Lemke and practical MILP procedures may be inconclusive.

The published family is therefore a genuine general partial membership solver. Its mathematical conclusions are exact when the LCP
existence or nonexistence claim is exact; numerical degeneracy and equality at zero require certification.

### 6.2 Two-phase LPLCC StQP algorithm

[Júdice, Sessa, and Fukushima (2024)](https://doi.org/10.1007/s10898-024-01423-y) solve StQP in two phases. The first phase searches
for successively better stationary points through a linear program with linear complementarity constraints (LPLCC). The second uses
reformulation-linearization bounds and enumeration to seek a global-optimality certificate.

- A feasible point with negative value proves NCP.
- A rigorous nonnegative lower bound or a proved absence of an improving complementarity solution proves CP.
- A rigorous positive lower bound proves SCP; a verified zero optimum proves boundary CP.
- The published implementation may stop without a certificate and uses a positive tolerance, so it is a partial exact membership
  method unless its numerical conclusions are independently certified.

## 7. MILP and MIQP membership tests

### 7.1 Anstreicher and Peng MILP

[Anstreicher (2021)](https://doi.org/10.1016/j.laa.2020.09.002) searches for an almost-copositive principal submatrix and expresses
the recognition problem as a MILP. [Peng (2022)](https://doi.org/10.1016/j.ejco.2022.100037) compares it with the
Gondzio-Yıldırım formulation and proposes an improved Anstreicher-style model.

- **Membership output:** CP versus NCP in exact MILP mathematics.
- **Strictness:** the MILPCOP optimum is positive exactly for NCP and zero for both boundary CP and SCP, so this formulation does not
  decide strictness. Globally minimizing the StQP does, but that is the different formulation in Section 7.2.
- **Numerical issue:** a MILP solver applies feasibility and optimality tolerances, so equality at zero needs independent validation.

These are the two papers in the dedicated Anstreicher/Peng copositivity-MILP line. Hildebrand's work supplies structural results and
difficult matrices, not another dedicated MILP membership algorithm.

### 7.2 Gondzio-Yıldırım and generic StQP MILP

[Gondzio and Yıldırım](https://arxiv.org/abs/1810.02307) give two MILP reformulations of the StQP: a KKT/complementarity model and a
piecewise-linear-overestimator model. [Xia, Vera, and Zuluaga](https://doi.org/10.1287/ijoc.2018.0883) give the generic quadprogIP
reformulation for nonconvex quadratic programs.

When these models globally minimize \(x^TAx\) on \(\Delta_n\), the optimum is \(\mu(A)\). Therefore:

- optimum \(<0\): **NCP**;
- optimum \(=0\): **CP but not SCP**;
- optimum \(>0\): **SCP**.

The formulation is a full membership classifier in exact mathematics. CPLEX, Gurobi, SCIP, or another floating-point solver returns a
numerical version whose zero case must be certified separately.

## 8. Global StQP and general-QP algorithms used as membership tests

### 8.1 Scozzari-Tardella exact clique algorithm

[Scozzari and Tardella (2008)](https://doi.org/10.1016/j.dam.2007.09.020) prove that some global StQP minimizer has support forming a
clique in the matrix's convexity graph. Their exact algorithm searches the resulting constrained minimum-weight clique problem. Because
it returns the global value \(\mu(A)\), its exact sign gives full CP/SCP classification. The paper also gives a heuristic variant, which
does not by itself qualify as a membership solver.

### 8.2 Finite KKT branch-and-bound for general QP

Finite branch-and-bound algorithms for general nonconvex quadratic programming can be applied directly to the StQP. They branch on the
complementarity choices in the KKT conditions and bound the resulting nodes by semidefinite or polyhedral-semidefinite relaxations.

- [Burer and Vandenbussche (2008)](https://doi.org/10.1007/s10107-006-0080-6) give a finite SDP-based branch-and-bound algorithm.
- [Burer and Chen (2012)](https://optimization-online.org/2011/02/2945/) combine finite KKT branching with
  polyhedral-semidefinite completely positive relaxations for general nonconvex QP.

On the simplex, exact finite exhaustion gives the global value \(\mu(A)\), so these general-QP algorithms become complete CP/SCP
classifiers even though they were not introduced as copositivity algorithms.

### 8.3 Spatial branch-and-bound for StQP and general QP

A spatial branch-and-bound solver partitions the simplex or a containing box, computes a valid lower bound on every active region,
maintains feasible upper bounds, and discards regions whose lower bounds cannot improve the incumbent. Common bounds use convex
envelopes, McCormick inequalities, reformulation-linearization, eigenvalue convexification, SDP, or DNN relaxations.

Representative StQP algorithms include [Bomze (2002)](https://doi.org/10.1023/A:1013886408463),
[Liuzzi, Locatelli, and Piccialli (2019)](https://doi.org/10.1080/10556788.2017.1341504), and
[Bonami, Lodi, Schweiger, and Tramontani (2019)](https://doi.org/10.1137/16M107428X). General nonconvex-QP spatial solvers apply for
the same reason. BARON, SCIP, Couenne, ANTIGONE, CPLEX, and Gurobi are software examples.

- A negative feasible point proves NCP.
- A rigorous nonnegative lower bound proves CP, and a positive one proves SCP.
- A zero witness plus a rigorous nonnegative lower bound proves boundary CP.
- Mere convergence to a tolerance may never decide exact equality at zero.

Thus spatial branch-and-bound is a valid general partial membership method. A particular finite exact implementation becomes a complete
classifier, but the generic numerical procedure should not be labelled complete merely because it reports an approximate global optimum.

### 8.4 De Klerk-Pasechnik exact LP

[De Klerk and Pasechnik](https://optimization-online.org/wp-content/uploads/2005/03/1087.pdf) give an exact exponentially sized LP
reformulation of the StQP and polynomial-size LP relaxations with finite convergence for each fixed instance.

- **Membership output:** full classification from the exact sign of the LP optimum.
- **Cost:** exponential formulation size or increasing relaxation level.

### 8.5 SDP/QP global-optimality screening

[Nowak](https://doi.org/10.18452/2684) combines an SDP and a convex QP to produce a lower bound and a sufficient global-optimality
criterion for an StQP candidate.

- lower bound \(\ge0\): proves **CP**;
- lower bound \(>0\): proves **SCP**;
- feasible negative candidate: proves **NCP**;
- failed criterion or a gap around zero: unresolved.

Thus it is a genuine membership screen, but not by itself a complete classifier on every input.

## 9. SOS and polynomial-optimization membership methods

### 9.1 Pólya coefficient certificates

Pólya-type hierarchies multiply the homogeneous quadratic form by a power of \(\sum_i x_i\) and require all resulting coefficients to
be nonnegative. They give LP-representable inner approximations of the copositive cone; see
[Bomze and de Klerk](https://pure.uvt.nl/ws/portalfiles/portal/844599/solvingst.pdf).

- A successful coefficient test proves **CP**.
- Every SCP matrix is eventually certified at some level.
- Some boundary CP matrices may never be certified.
- Failure at a fixed level is inconclusive and never proves NCP.

### 9.2 Parrilo SOS certificates

Set

\[
p_A(z)=\sum_{i,j}a_{ij}z_i^2z_j^2.
\]

Then \(A\) is copositive exactly when \(p_A\) is globally nonnegative.
[Parrilo](https://www.mit.edu/~parrilo/pubs/files/Parrilo-Semidefinite%20programming%20based%20tests%20for%20matrix%20copositivity.pdf)
tests whether \(p_A(z)(\sum_i z_i^2)^r\) is SOS.

- A successful SOS identity proves **CP**.
- The hierarchy certifies every SCP matrix eventually.
- [Laurent and Vargas](https://arxiv.org/abs/2205.05381) show that the union of the classical Parrilo cones is still a strict subset
  of the copositive cone for \(n\ge6\); it is not a complete boundary test.
- [Muramatsu, Waki, and Tunçel](https://arxiv.org/abs/1304.0065) use perturbation to obtain certificates around difficult boundary
  matrices, but the unperturbed exact-zero decision remains distinct.

### 9.3 Nie-Yang-Zhang moment-SOS detection

[Nie, Yang, and Zhang (2018)](https://doi.org/10.1137/17M115308X) give a specialized semidefinite-relaxation algorithm that, after
finitely many levels, either returns a copositivity certificate or a refuting point.

- **Membership output as published:** complete CP versus NCP detection, including boundary matrices.
- **Strictness:** a positive relaxation bound proves SCP. A nonnegative bound proves CP but may be zero before the hierarchy has
  reached the exact positive optimum. Full boundary-versus-strict classification therefore requires continuing until exactness is
  certified, or separately testing whether a zero exists.
- **Practical qualification:** a floating-point SDP solution still needs flatness/rank recognition and exact or certified
  reconstruction before it is an exact software answer.

### 9.4 Newer SOS membership certificates

- [Schweighofer and Vargas (2024)](https://arxiv.org/abs/2310.12853) introduce test-state SOS certificates and prove strong
  low-order and graph-specific membership results.
- [Ahmadi, Dash, Hua, and Stellato (2026)](https://arxiv.org/abs/2605.28674) use disjunctive low-degree SOS certificates and combine
  them with branch-and-bound for matrix copositivity.

At a fixed level these are positive membership certificates. They become classifiers only with the theorem-specific finite-convergence
result or a complete outer branch-and-bound procedure.

### 9.5 Generic polynomial-optimization hierarchies

Lasserre moment-SOS, Putinar/Schmüdgen SOS, Handelman LP, RLT, and Bernstein hierarchies apply directly to \(\mu(A)\). Cheaper
restrictions such as DSOS/SDSOS, and alternative nonnegativity certificates such as
[SONC](https://arxiv.org/abs/1607.06010) or SAGE, can also certify particular transformed quadratic forms; they do not change the
logical status of a fixed-level test.

- a certified lower bound \(\ge0\) proves CP;
- a certified lower bound \(>0\) proves SCP;
- a feasible negative point proves NCP;
- asymptotic convergence alone does not provide a finite exact answer when \(\mu(A)=0\).

They are valid membership machinery, but only a hierarchy with finite termination or a separately verified zero certificate is a full
classifier.

## 10. Exact symbolic membership

Copositivity is the first-order formula

\[
\forall x\;[(x\ge0)\Rightarrow x^TAx\ge0].
\]

Therefore cylindrical algebraic decomposition, real quantifier elimination, critical-point methods, Gröbner bases/resultants followed
by exact real-root isolation, and rigorous interval branch-and-bound are exact membership procedures in principle. Strictness is
decided by the analogous strict formula, or by testing whether a nonzero nonnegative zero exists after CP has been established.

- **Membership output:** full CP/SCP/NCP classification.
- **Use in practice:** small matrices, validation of delicate zero cases, or proof backends for numerical candidates.
- **Limitation:** symbolic and algebraic-number growth usually makes them much more expensive than matrix-specific methods.
