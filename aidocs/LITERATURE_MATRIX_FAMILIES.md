# Literature-Derived Copositive Matrix Families

**Status:** research guide  
**Corpus snapshot:** 2026-08-07  
**Scope:** 850 literature-derived source occurrences represented by 849 distinct matrices at IDs 9162, 9163, and 9657-10504 in
`testdata/copos_testdata.sqlite3`

## Evidence Boundary

This guide records how the imported matrix families are constructed, which properties their authors prove, and which difficulties or
unusual features the cited literature reports. It deliberately contains no coposit timing, node-count, timeout, or comparative solver
result. Those experiments belong in a later benchmark report.

The word *difficult* is used narrowly. An extreme or exceptional matrix is structurally important, but that alone does not prove that a
particular algorithm will take a long time on it. This guide calls a family difficult only when the source makes such a claim or proves
that a named approximation or certificate cannot handle it. Otherwise it states the unusual mathematical property and leaves empirical
difficulty open.

The rows have three different kinds of provenance:

1. **Printed example:** the matrix itself appears in the source, apart from multiplication by a positive scalar.
2. **Exact point from a published family:** the paper gives a parametrization, and the corpus chooses rational parameters satisfying its
   hypotheses.
3. **Local variant:** coposit applies a source-preserving operation, such as positive diagonal congruence, or takes a finite sample from an
   unbounded published construction. Such choices are identified below and are not attributed to the authors.

For trigonometric families, exact rational rows were usually obtained with the half-angle substitution

\[
 t=\tan(\phi/2),\qquad \cos\phi=\frac{1-t^2}{1+t^2},\qquad
 \sin\phi=\frac{2t}{1+t^2}.
\]

Rational sines and cosines were assembled exactly, denominators were cleared, and the upper triangle was divided by its integer gcd.
Multiplication by a positive scalar does not change copositivity, strict copositivity, zeros, exceptionality, or extremality.

## Terminology And Preserved Operations

For a real symmetric matrix \(A\):

- \(A\) is **copositive** if \(x^T A x\geq 0\) for every \(x\geq 0\).
- It is **strictly copositive** if the inequality is strict for every nonzero \(x\geq 0\).
- A nonzero \(u\geq 0\) with \(u^T A u=0\) is a **zero**. A matrix with a zero lies on the boundary of the copositive cone and is not
  strictly copositive.
- A zero is **minimal** if no zero has strictly smaller support.
- \(A\) is **SPN** if it is a sum of a positive semidefinite matrix and an entrywise nonnegative matrix.
- A copositive matrix outside the SPN cone is **exceptional**.
- A nonzero copositive matrix is **extremal** if it generates an extreme ray of the copositive cone.
- A strictly copositive matrix is **perfect copositive** if its copositive minimum on nonzero nonnegative integer vectors and the vectors
  attaining that minimum determine the matrix uniquely.

If \(D\) is a positive diagonal matrix and \(P\) a permutation matrix, then \(A\mapsto PDADP^T\) preserves the preceding cone properties.
It also transports zeros without changing their supports under \(D\), up to the corresponding coordinate scaling. Several corpus batches
use this equivalence to obtain exact matrices with varied coefficient scales.

## Corpus Inventory

| Construction group | Rows | Orders | Corpus IDs | Provenance |
|---|---:|---:|---|---|
| Horn | 1 | 5 | 9162 | classical printed matrix |
| Hoffman-Pereira | 200 | 6-10 | 9163, 9757-9955 | one named example plus graph-catalog instances |
| Hildebrand historical Case 34 | 25 | 6 | 9657-9681 | exact family points with local diagonal congruences |
| Väliaho almost-strict equality | 29 | 5-12 | 9682-9710 | local exact representatives of the published class |
| Afonin-Hildebrand-Dickinson Case 13.1 | 7 | 6 | 9711-9717 | exact family points with local diagonal congruences |
| Afonin-Hildebrand-Dickinson Case 18 | 20 | 6 | 9718-9737 | exact family points with local diagonal congruences |
| Kostyukova-Tchemisova extensions | 23 | 7-9 | 9738-9756, 9956-9959 | printed examples plus local diagonal congruences |
| Strekelj-Zalar matrix \(C\) | 1 | 5 | 9960 | printed rationalized example |
| Hildebrand non-Horn \(\operatorname{COP}(5)\) family | 24 | 5 | 9961-9984 | exact family points |
| Baston all-orders and cyclic families | 72 | 11-64 | 9985-10056 | finite exact sample of two published constructions |
| Johnson-Reams generalized Horn family | 157 | 7-999 | 10057-10129, 10161-10244 | finite sample of an unbounded construction |
| Dickinson-de Zeeuw cop-irreducible graphs | 31 | 7-10 | 9957, 10130-10131, 10133-10160 | selected rows from Table 2; ID 9957 also represents removed duplicate ID 10132 |
| Dickinson Case 9 | 3 | 6 | 10245-10247 | exact family points |
| Hildebrand-Afonin matrices outside \(K_6^{(1)}\) | 3 | 6 | 10248-10250 | exact nearby points from the proved component |
| Laurent-Vargas direct sums | 7 | 6-45 | 10251-10257 | exact finite sample of an unbounded construction |
| Hildebrand circulant support-\(n-2\) family | 41 | 7-25 | 10258-10287, 13024-13034 | exact family points |
| Dannenberg-Schurmann perfect lifts | 200 | 3-104 | 10305-10504 | two printed seeds and 198 theorem-backed lifts |

The total is 844 source occurrences represented by 843 distinct matrices. Dickinson-de Zeeuw occurrence 10132 is a simultaneous
row-and-column permutation of Kostyukova-Tchemisova matrix 9957, so the maintained corpus stores only ID 9957 with both origins. The
construction groups are fewer than the database's family labels because the two Dannenberg-Schurmann components
and several order-6 subfamilies are most naturally explained together.

## 1. Horn And Hoffman-Pereira Matrices

### Construction

The Horn matrix is the order-5 cyclic matrix

\[
 H=J-2A(C_5),
\]

so its diagonal and non-cycle off-diagonal entries are \(+1\), while cycle edges are \(-1\). Equivalently,

\[
 x^T Hx=\left(\sum_{i=1}^5x_i\right)^2-4\sum_{i=1}^5x_i x_{i+1},
\]

with cyclic indices.

Hoffman and Pereira study symmetric unit-diagonal matrices with entries in \(\{-1,0,1\}\). A convenient graph formulation, also used by
Peng, starts with a simple graph \(G\) and defines

\[
 A_{ii}=1,\qquad
 A_{ij}=\begin{cases}
 -1,&ij\in E(G),\\
 +1,&i,j\text{ have a common neighbor in }G,\\
 0,&\text{otherwise.}
 \end{cases}
\]

The graph must be triangle-free for copositivity. The extremality test used by the importer builds an auxiliary graph whose vertices are
the zero entries of \(A\): two zero pairs are adjacent when they share one endpoint and their other endpoints form an edge of \(G\).
Every connected component of this auxiliary graph must be non-bipartite; in the no-zero special case the corresponding non-bipartiteness
condition is applied to \(G\). This is the finite graph characterization behind the enumeration, not a heuristic hardness filter.

### Published properties and significance

Hoffman and Pereira characterize copositivity, copositive-plus membership, positive semidefiniteness, and extremality throughout this
\(\{-1,0,1\}\) class. Their paper also gives a counterexample to a Baumert conjecture about zeros. Hildebrand later proves that the
unit-diagonal extremal matrices whose minimal-zero supports all have cardinality two are exactly these \(\{-1,0,1\}\) matrices; positive
diagonal scaling gives the full class without unit-diagonal normalization.

The Horn matrix and the named order-7 Hoffman-Pereira matrix are exceptional extreme boundary matrices. Their two-coordinate zeros make
the equality structure particularly explicit. Peng does make an algorithmic claim here: the Hoffman-Pereira construction is used to form
a *hard problem set* for two MILP copositivity tests, with Horn and the named Hoffman-Pereira examples included. Thus this family has both
structural and published computational motivation.

### Corpus realization

ID 9162 is Horn and ID 9163 is the named order-7 Hoffman-Pereira matrix. The other 199 rows were generated from Brendan McKay's catalogs
of non-isomorphic connected graphs:

- every new qualifying class through order 9: 1 at order 6, 3 at order 7, 10 at order 8, and 55 at order 9;
- the first 130 qualifying order-10 classes in McKay catalog order.

The order-10 catalog contains 11,716,571 connected unlabeled graphs. The 130 rows are neither random nor ranked by observed hardness: the
importer streams the catalog, applies the published structural test, and stops after the first 130 qualifying new classes. Horn and the
order-7 cycle class already in the corpus are excluded. Many more exact order-10 candidates can therefore be generated later from the
retained catalog.

Sources: [Hoffman-Pereira publication page](https://research.ibm.com/publications/on-copositive-matrices-with-1-0-1-entries),
[Hildebrand's cardinality-two characterization](https://arxiv.org/abs/1707.08862),
[Peng's hard test set](https://doi.org/10.1016/j.ejco.2022.100037), and the retained
[McKay catalog notes](../research/data/mckay_connected_graphs/README.md).

## 2. Väliaho Almost-Strict Equality Matrices

### Construction

Väliaho calls a copositive matrix *almost strictly copositive* when it is not strictly copositive but every principal submatrix of order
\(n-1\) is strictly copositive. The corpus uses the local exact representative

\[
 A=D(nI-J)D,
\]

where \(D\) is a positive integer diagonal matrix. The particular geometric, Fibonacci-like, and prime-like diagonal sequences in IDs
9682-9710 are coposit sampling choices, not matrices printed by Väliaho.

The construction realizes the definition exactly. The core \(nI-J\) is positive semidefinite of rank \(n-1\), with kernel generated by
the all-ones vector. Hence \(A\) has the strictly positive zero \(D^{-1}\mathbf 1\). Every proper principal submatrix is positive definite,
so the equality first appears only when all coordinates are present.

### Published properties and significance

These matrices are positive semidefinite boundary matrices, not exceptional matrices. Their value is the full-support equality: all
immediate principal subproblems are strict, while the full problem is exactly non-strict. Väliaho states that almost copositive-plus and
almost strictly copositive matrices are crucial in deriving criteria for copositivity, and studies them using principal pivoting and
quadratic programming. That is a source-backed reason to retain the class; no local timing claim is needed.

Source: [Väliaho, *Almost copositive matrices*](https://doi.org/10.1016/0024-3795(89)90402-3).

## 3. Exceptional Order-6 Trigonometric Families

### Classification background

Afonin, Hildebrand, and Dickinson give a complete classification of the extreme rays of \(\operatorname{COP}(6)\). Their analysis starts
from 44 candidate minimal-zero support sets; 19 are realized by exceptional extremal matrices. After a permutation and positive diagonal
scaling, each realized class has unit diagonal and a semitrigonometric formula. The final classification states that every extreme ray is
equivalent to one of the listed forms and, conversely, every form satisfying its parameter restrictions is extreme.

This is why the order-6 rows use angles and zero-support patterns rather than arbitrary coefficients. Size-two zero supports force an
entry \(-1\); size-three supports produce singular positive semidefinite \(3\times3\) trigonometric blocks, with minimal-zero coordinates
given by sines of the associated angles.

The classification itself does not claim that every member is slow for every copositivity algorithm. Its source-backed significance is
that these are certified exceptional extreme boundary strata, and extreme rays are the decisive objects when testing whether an inner
approximation of a cone is exact.

Source: [Afonin-Hildebrand-Dickinson 2021](../research/papers/Afonin_Hildebrand_Dickinson_2021_exceptional_COP6_classification.pdf).

### 3.1 Historical Case 34

The earlier case note uses the cyclic minimal-zero supports

\[
 123,\ 234,\ 345,\ 456,\ 561,\ 612.
\]

The unit-diagonal matrix has adjacent entries \(-\cos\phi_i\), next-nearest entries \(\cos(\phi_i+\phi_{i+1})\), and three remaining
entries \(a=A_{14}\), \(b=A_{25}\), \(c=A_{36}\). They are fixed by

\[
\begin{aligned}
a&=-\min\{\cos(\phi_1+\phi_2+\phi_3),\cos(\phi_4+\phi_5+\phi_6)\},\\
b&=-\min\{\cos(\phi_2+\phi_3+\phi_4),\cos(\phi_1+\phi_5+\phi_6)\},\\
c&=-\min\{\cos(\phi_3+\phi_4+\phi_5),\cos(\phi_1+\phi_2+\phi_6)\}.
\end{aligned}
\]

The angles are positive, adjacent cyclic sums are below \(\pi\), and their total is below \(2\pi\). The paper prints all six minimal
zeros as sine vectors on the supports above and proves copositivity. It proves extremality when the total angle is not \(\pi\), or, in the
total-\(\pi\) case, when enough of the paired cosine minima are equalities. In the later complete classification this historical support
set is Case 13; “Case 34” is retained in the corpus name so the original note remains traceable.

IDs 9657-9681 use two rational half-angle cores and 25 local positive diagonal congruences. The diagonal choices are not from the paper.

Source: [Hildebrand's Case 34 note](https://membres-ljk.imag.fr/Roland.Hildebrand/c6classification/case34.pdf).

### 3.2 Classification Cases 13.1 And 18

Case 13.1 has the same six consecutive size-three supports as the historical Case 34 form, but the classification resolves the choices of
the three long-range entries into a specific analytic component. Its entries are signed cosines of one-, two-, and three-angle sums. The
six positive angles obey adjacent-sum and total-sum inequalities plus three comparisons between complementary triple sums.

Case 18 has eight size-three minimal-zero supports:

\[
123,\ 234,\ 345,\ 145,\ 125,\ 346,\ 146,\ 126.
\]

It is parameterized by five positive angles with total below \(\pi\), together with a sixth signed angle satisfying
\(-\phi_3<\phi_6<\phi_2\). Its entries are again cosines of signed angle sums.

IDs 9711-9717 and 9718-9737 are exact rational half-angle points satisfying these source conditions, followed by local positive diagonal
congruences. The paper supplies the families and proves exceptionality and extremality; the particular rational points and diagonals are
coposit choices.

### 3.3 Dickinson Case 9

Dickinson's Section 7 studies the Case-9 support pattern

\[
15,\ 26,\ 123,\ 234,\ 345,\ 456.
\]

After normalization, the matrix is expressed through five positive angles \(\theta_i\) and four auxiliary angles \(\psi_i\). Dickinson
derives necessary and sufficient inequalities for copositivity, then sharper equalities and strict inequalities for extremality. The four
three-coordinate minimal zeros have sine coordinates; \(e_1+e_5\) and \(e_2+e_6\) give the two size-two zeros.

The surrounding paper introduces a copositivity certificate that works for every copositive matrix using finitely many linear systems and
inequalities. The author warns that it can be exponentially large in general, while some exceptional extreme matrices have relatively
small certificates. This is a statement about the certificate method, not a claim that the three retained Case-9 rows are universally
hard.

IDs 10245-10247 are three exact rational half-angle points from the extremal subfamily.

Source: [Dickinson 2019](../research/papers/Dickinson_2019_new_certificate_for_copositivity.pdf).

### 3.4 A Case-13.1 Component Outside Parrilo's First Level

Hildebrand and Afonin connect the order-6 classification to Parrilo's first sum-of-squares inner approximation \(K_6^{(1)}\). They show
that only five of the 22 main exceptional-extreme components are essential, and then derive the complete \(K_6^{(1)}\) certificate
constraints on the essential Case-13.1 component. One scalar constraint, denoted \(m_{136}\) in their calculation, can be negative even
when all copositivity and extremality conditions hold. Their printed angle example therefore proves

\[
 \operatorname{COP}(6)\setminus K_6^{(1)}\ne\varnothing.
\]

This is a precise published failure of a named inner approximation, not an empirical hardness inference. IDs 10248-10250 are three nearby
exact rational half-angle points for which the same inequalities and negative certificate constraint hold; they are not decimal copies of
the paper's printed matrix.

Source: [Hildebrand-Afonin 2024](../research/papers/Hildebrand_Afonin_2024_structure_COP6.pdf).

## 4. Kostyukova-Tchemisova Higher-Order Extensions

### Construction

Given \(A\in\operatorname{COP}(n)\) and \(a\geq0\), the paper defines

\[
B(A,a)=
\begin{pmatrix}
A&Aa\\
a^TA&a^TAa
\end{pmatrix}.
\]

For \((t,t_0)\geq0\),

\[
 (t,t_0)^T B(A,a)(t,t_0)=(t+t_0a)^T A(t+t_0a),
\]

so copositivity is immediate. The substantive results analyze the minimal-zero graph of \(A\) and give sufficient and necessary rank and
support conditions under which extremality is preserved. The extension can be iterated.

Example 5 begins with the order-8 graph matrix

\[
 A=\alpha(G)(I+A_G)-J
\]

for Dickinson-de Zeeuw graph \((8,3,2)\). The authors list its 11 normalized minimal zeros, prove the base matrix extremal, and construct
two non-equivalent order-9 extremes using

\[
 a=(1,0,0,0,1,0,0,0)^T,\qquad b=(1,0,0,0,2,0,0,0)^T.
\]

The distinct numbers of minimal zeros prove that the two extensions are not equivalent. The corpus also includes the exact order-7
extension printed in Example 1.

### Published properties and significance

The paper is motivated by the lack of general higher-order constructions preserving both copositivity and extremality: simple block
embeddings commonly lose extremality. These examples are certified exceptional extreme boundary matrices with explicitly analyzed zero
structures. The authors describe extreme rays as closely related to challenging copositive and completely positive programs, but do not
publish solver-hardness measurements for these particular examples.

### Corpus realization

IDs 9956-9959 are the four distinct exact matrices taken from Examples 1 and 5: the order-7 extension, the order-8 base, and both order-9
extensions. IDs 9738-9756 are 19 positive diagonal congruences of the printed \(B(A,b)\). Those 19 diagonals are local scale variants, not
additional examples from the paper.

Source: [Kostyukova-Tchemisova 2026](../research/papers/Kostyukova_Tchemisova_2026_extremal_copositive_generation.pdf).

## 5. Strekelj-Zalar Matrix \(C\)

### Construction

Strekelj and Zalar first construct exceptional doubly nonnegative matrices by compressing a multiplication operator associated with a
nonnegative cosine polynomial. A rational order-5 seed is certified not completely positive by a negative pairing with Horn.

They then search for a matrix \(C\) satisfying two conditions against that seed \(A^{(5)}\):

\[
 \langle A^{(5)},C\rangle<0,
 \qquad
 \left(\sum_i x_i^2\right)^k q_C(x)\text{ is a sum of squares}.
\]

The sum-of-squares condition proves copositivity. Since SPN and doubly nonnegative cones are dual, the negative inner product proves that
\(C\) is not SPN. The paper solves the feasibility problem with \(k=1\), rationalizes the result, and prints the exact order-5 matrix stored
as ID 9960 after clearing denominators.

### Published properties and significance

The stated source conclusion for the printed \(C\) is exceptional copositivity. The broader construction produces exceptional copositive
matrices in every order at least five. The paper does not claim that this particular rational matrix is extremal or publish a
copositivity-solver hardness result for it. Its relevance is that it is produced from an explicit SOS certificate while simultaneously
separating from the SPN cone.

Source: [Strekelj-Zalar 2025](../research/papers/Strekelj_Zalar_2025_exceptional_copositive_construction.pdf).

## 6. Hildebrand's Complete Non-Horn \(\operatorname{COP}(5)\) Family

### Construction

Hildebrand completes Baumert's implicit order-5 characterization. Besides SPN extreme rays and the Horn orbit, every extreme ray is,
up to permutation and positive diagonal scaling,

\[
 A=PDT(\phi)DP^T.
\]

The unit-diagonal matrix \(T(\phi)\) is cyclically determined by the first-row pattern

\[
 (1,\ \sin\phi_4,\ -\cos(\phi_4+\phi_5),\ -\cos(\phi_2+\phi_3),\ \sin\phi_3).
\]

The angles satisfy explicit open inequalities: each \(\phi_j>-\pi/2\), adjacent cyclic sums are negative, the total is below
\(-3\pi/2\), and five further trigonometric expressions are positive. The paper proves the converse as well: every permitted point is an
exceptional extreme ray outside the Horn orbit.

### Published properties and significance

Each matrix has five isolated zeros with cyclic support patterns

\[
11001,\ 11100,\ 01110,\ 00111,\ 10011.
\]

The family forms a 10-dimensional variety with 12 smooth components. On the unit-diagonal section each component is homeomorphic to the
parameter simplex; its vertices meet the Horn orbit and positive semidefinite extreme rays. This describes the complete non-SPN,
non-Horn extreme boundary of \(\operatorname{COP}(5)\). The paper discusses the general difficulty of accessing the copositive cone but
does not claim uniform solver hardness for every member.

IDs 9961-9984 use 24 rational choices \(\phi_j=-2\arctan t_j\), then clear denominators exactly. They are derived family points, not 24
tables printed in the paper.

Source: [Hildebrand 2012](../research/papers/Hildebrand_2012_extreme_rays_COP5.pdf).

## 7. Baston's Basic Extreme \(\{\!-1,+1\}\) Forms

### Characterization

Baston calls an extreme form *basic* when no two matrix rows are identical. For a symmetric unit-diagonal \(\{\!-1,+1\}\) matrix, his
criterion can be expressed through the graph of \(-1\) entries:

- the graph has no triangle;
- every \(+1\) off-diagonal pair has a vertex joined by \(-1\) to both endpoints.

These conditions are necessary and sufficient for the form to be copositive and extreme. Baston also proves that every pair of variables
in a basic extreme form of order at least five lies in a five-variable restriction equivalent to Horn, and that zeros cover every index
pair.

### All-orders construction

For order \(3p\), start with an all-ones matrix and set the following symmetric pairs to \(-1\), using one-based indices:

\[
\begin{aligned}
 &(1,j), &&2\leq j\leq p+2,\\
 &(i,p+2i-1),(i,p+2i), &&2\leq i\leq p,\\
 &(p+2i-1,p+2i+2r),(p+2i,p+2i+2r-1),
 &&1\leq i<p,\ 1\leq r\leq p-i.
\end{aligned}
\]

Baston's deletion argument gives orders \(3p-1\) and \(3p-2\) by deleting, respectively, index \(p+2\), or indices \(p+1,p+2\), from
the order-\(3p\) form. Theorem 4.1 proves basic extreme forms for all sufficiently large orders; the paper's final existence statement is
that basic forms exist for every order at least eight, with a unique class at order eight and none at orders six or seven.

### Distinct cyclic construction

For \(n=3m+2\), Baston's second construction is

\[
 q(x)=\left(\sum_{i=1}^{n}x_i\right)^2
 -2\sum_i x_i\bigl(x_{i+1}+x_{i+4}+\cdots+x_{i+3m+1}\bigr),
\]

with cyclic indices. Thus the matrix is \(-1\) at cyclic offsets \(1,4,\ldots,3m+1\) and \(+1\) elsewhere. For \(m=1\) this is Horn;
for larger parameters the paper obtains further basic extreme forms, including forms not equivalent to the first construction.

### Corpus realization and significance

IDs 9985-10038 contain the first construction at every order 11-64. IDs 10039-10056 contain the cyclic construction for
\(m=3,\ldots,20\), orders 11-62. These endpoints are local finite sampling decisions.

The source significance is the existence of exceptional extreme equality matrices with only \(\pm1\) coefficients in arbitrary order,
not a reported runtime result.

Source: [Baston 1969](../research/papers/Baston_1969_extreme_copositive.pdf).

## 8. Johnson-Reams Generalized Horn Matrices

### Construction

Johnson and Reams develop a completion method based on overlapping principal blocks that are copositive, non-strict, and attain their
simplex minimum in the interior of the relevant face. Their odd-order Horn extension is

\[
 Q_n=J-2A(C_n),\qquad n\geq5\text{ odd},
\]

or

\[
 x^TQ_nx=\left(\sum_i x_i\right)^2-4\sum_i x_i x_{i+1}.
\]

The proof of exceptionality uses a completion obstruction. A positive semidefinite completion would force alternating \(+1,-1\) entries
around the last column; an odd cycle cannot close consistently.

### Published properties and significance

Every odd-order member is exceptional and lies on the boundary, with zeros such as \(e_i+e_{i+1}\). The paper explicitly warns against
an easy overstatement: its generalized Horn matrices at orders 7 and 9 are exceptional but not extreme, because they decompose as a
nonnegative matrix plus an extreme matrix. The named order-7 Hoffman-Pereira matrix printed separately in the same paper is extreme and
is not the generalized Horn \(Q_7\).

The published value of the construction is an exceptional family in every odd order and an illustration of the overlapping-interior-block
completion method. The paper does not report that runtime must grow with order.

### Corpus realization

The family is unbounded. coposit stores every odd order from 7 through 151, then an irregular roughly-ten-order sample beginning
163, 175, 181, and 199 and ending at 999. The 157-row endpoint and gaps are local coverage decisions.

Source: [Johnson-Reams 2008](../research/papers/Johnson_Reams_2008_constructing_copositive.pdf).

## 9. Dickinson-de Zeeuw Stable-Set Matrices

### Construction

For a graph \(G\) with stability number \(\alpha(G)\), define

\[
 Z_G=\alpha(G)(I+A_G)-J.
\]

Its diagonal and graph-edge entries are \(\alpha(G)-1\), and its nonedge entries are \(-1\). The Motzkin-Straus stability formulation
implies that \(Z_G\) is copositive and lies on the boundary. Indicator vectors of maximum stable sets give zeros.

The paper calls a graph *cop-irreducible* precisely when \(Z_G\) is irreducible with respect to the SPN cone. Its main theorem says this is
equivalent to three graph properties:

- \(G\) is connected;
- \(G\) is \(\alpha\)-critical: deleting any graph edge raises the stability number;
- \(G\) is \(\alpha\)-covered in the paper's sense: every complement edge, equivalently every nonedge of \(G\), belongs to a maximum
  stable set.

### Published properties and reported difficulty

The authors enumerate 57,459 unlabeled cop-irreducible graphs through order 13. Of the 26,863 co-point-determining cases, all but three
generate extreme copositive matrices. They explicitly test these matrices against inner approximations of the copositive cone and report
that they provide difficult instances. They also observe that the worst cases depend on which approximation is used, so the paper does
not define a universal hardness ranking.

This is one of the strongest source-backed reasons for the corpus inclusion: the paper both proves the structural property and evaluates
the family as adversarial input for copositive inner approximations.

### Corpus realization

IDs 10130-10160 are the 31 Table-2 examples with stability number 3 or 4, at orders 7-10. Rows with other stability numbers in that table
were not selected for this batch. Each graph6 record and its stability number are retained in the row source.

Source: [Dickinson-de Zeeuw 2021](../research/papers/Dickinson_de_Zeeuw_2021_generating_irreducible.pdf).

## 10. Laurent-Vargas Matrices Outside Every Parrilo Level

### Construction

Parrilo's cones \(K_n^{(r)}\) are sum-of-squares inner approximations to \(\operatorname{COP}(n)\). Laurent and Vargas prove the direct-sum
obstruction:

> If \(M_1\) is copositive but not in \(K_n^{(0)}\), and the copositive matrix \(M_2\) has a nonzero nonnegative zero, then
> \(M_1\oplus M_2\) is outside \(K_{n+m}^{(r)}\) for every \(r\).

Taking \(M_1=H\), the Horn matrix, gives the smallest examples. One choice is \(H\oplus[0]\). For \(m\geq2\), the paper uses

\[
 H\oplus\frac{1}{m-1}(mI_m-J_m),
\]

where the all-ones vector is a zero of the second block. The corpus multiplies the whole matrix by \(m-1\), storing the integer-equivalent

\[
 (m-1)H\oplus(mI_m-J_m).
\]

### Published properties and reported difficulty

These matrices are copositive boundary matrices outside the union of *all* Parrilo levels. The paper describes this as bad behavior under
adding a zero row or column and uses it to prove strict inclusion

\[
 \bigcup_r K_n^{(r)}\subsetneq\operatorname{COP}(n),\qquad n\geq6.
\]

This is an exact impossibility result for the entire hierarchy, stronger than a slow convergence observation.

IDs 10251-10257 use \(m=1,2,3,5,10,20,40\), giving orders 6, 7, 8, 10, 15, 25, and 45. The seven choices are a local sparse sample of an
unbounded source construction.

Source: [Laurent-Vargas 2023](../research/papers/Laurent_Vargas_2023_Parrilo_exactness.pdf).

## 11. Hildebrand Circulant Matrices With Zero Supports Of Size \(n-2\)

### Construction

Fix a positive palindromic vector \(u\in\mathbb R_+^{n-2}\), and place its cyclic shifts into \(n\) vectors whose supports are obtained by
removing one edge of the cycle \(C_n\). The face consists of copositive matrices vanishing on all these vectors. Hildebrand connects this
face to a periodic linear system and its Floquet multipliers.

For the explicit circulant construction, choose angles \(\zeta_1<\cdots<\zeta_m\) on the unit circle with an alternating arc condition.
Form the positive palindromic polynomial

\[
 p(x)=\prod_{j=1}^{m}(x^2-2x\cos\zeta_j+1)
\]

for odd \(n\), and include an additional factor \((x+1)\) for even \(n\). Its coefficients give \(u\). The first row of the circulant
matrix is then an explicit weighted cosine sum; cyclic symmetry determines all remaining entries. Theorem 9 handles odd order. Theorem 8
handles even order, where the relevant face has an additional positive semidefinite ray and its exceptional extremal boundary is selected
by setting the paper's extra coefficient to zero.

The corpus specializes to equally spaced angles \(\zeta_j=\theta+j\alpha\), using rational
\(\tan(\theta/4)\) and \(\tan(\alpha/4)\). Exact polynomial positivity, arc conditions, and a printed zero are checked by the importer
before denominator clearing.

### Published properties and significance

Every retained matrix is exceptional, extremal, circulant, and on the boundary. Its cyclic minimal zeros have support cardinality
\(n-2\), much larger than the size-two Hoffman-Pereira zeros. The paper distinguishes *regular* forms, whose zeros are exactly the chosen
cyclic rays, from *degenerate* forms with additional zeros. It proves degenerate forms extremal in all orders and regular forms extremal
only in odd order, then constructs explicit extremal examples for every order at least five.

The paper calls characterization of all positive coefficient polynomials satisfying the angle conditions difficult and says only limited
results are known. It does not make a general solver-runtime claim for the resulting matrices.

The current corpus contains 30 original parameter points at orders 7-14 and IDs 13024-13034, exactly one point at every order 15-25.
The higher-order representatives come from the low-digit portion of a finite exact rational grid, but the selection deliberately varies
\(\theta\) and \(\alpha\) rather than repeating one mechanically minimal parameter pattern. The dated migration records every choice,
checks the theorem conditions and an exact zero, and preserves the original 17-point panel only through its historical generator.

Source: [Hildebrand 2017](../research/papers/Hildebrand_2016_circulant_zero_pattern.pdf).

## 12. Dannenberg-Schurmann Perfect Copositive Lifts

### Perfectness and the lift

For a strictly copositive matrix \(Q\), define

\[
 \min_{\mathrm{COP}}Q=\min\{v^TQv:v\in\mathbb Z_+^n\setminus\{0\}\}.
\]

The matrix is perfect copositive when this minimum and its nonnegative integer minimizers determine \(Q\) uniquely. Such matrices are the
vertices of the copositive Ryshkov polyhedron and are used to obtain rational certificates for completely positive matrices.

Write a perfect copositive matrix as

\[
 Q=\begin{pmatrix}M&m\\m^T&\mu\end{pmatrix}.
\]

If it has a minimal vector whose last coordinate is at least two, Lemma 5.1 duplicates the last coordinate:

\[
 \widetilde Q=
 \begin{pmatrix}
 M&m&m\\
 m^T&\mu&\mu\\
 m^T&\mu&\mu
 \end{pmatrix}.
\]

The quadratic form depends on the sum of the final two coordinates. The lemma proves that \(\widetilde Q\) is again perfect copositive and
preserves the copositive minimum. Its minimal vectors arise by splitting the old last coordinate in all nonnegative integer ways.

### The two seeds

The first printed seed is the indefinite SPN matrix

\[
 I=\begin{pmatrix}
 2&-5&4\\
 -5&14&-9\\
 4&-9&6
 \end{pmatrix}.
\]

It is perfect copositive, lies in \((\mathrm{PSD}+N)\setminus(\mathrm{PSD}\cup N)\), and has the qualifying minimal vector
\((0,1,2)^T\). Corollary 5.6 and the component-preservation lemma give such matrices in every order at least three.

The second printed seed is the exceptional perfect certificate

\[
 E=\frac13\begin{pmatrix}
 366&-300&197&147&-81\\
 -300&246&-161&123&69\\
 197&-161&106&-82&39\\
 147&123&-82&66&-33\\
 -81&69&39&-33&18
 \end{pmatrix}.
\]

It has a qualifying minimal vector \((1,0,0,0,4)^T\). Corollary 5.7 proves exceptional perfect copositive matrices in every order at least
five. The corpus stores the primitive integer numerator rather than the paper's \(1/3\) scale.

### Published properties and unusual behavior

Every matrix produced by the lemma remains strictly and perfectly copositive. At the same time, every nontrivial lift has two identical
trailing rows and columns and is therefore singular; its kernel contains the mixed-sign difference of the duplicated coordinates. This is
not a contradiction to strict copositivity, which only excludes nonzero *nonnegative* kernel vectors. The component-preservation result
keeps the \(I\) family SPN but neither positive semidefinite nor nonnegative, and keeps the \(E\) family exceptional.

The source presents these as phenomena absent from classical positive-definite perfect-form theory and as tools for rational completely
positive certificates and the copositive neighborhood graph. It does not publish a generic copositivity-solver hardness claim for the
lifts.

IDs 10305-10404 contain \(I\) and 99 lifts through order 102. IDs 10405-10504 contain \(E\) and 99 lifts through order 104.

Source: [Dannenberg-Schurmann 2023](../research/papers/Dannenberg_Schuermann_2023_perfect_copositive.pdf).

## 13. What The Literature Actually Says Is Adversarial

The source-backed reasons are different and should not be collapsed into one word:

| Feature | Families | What is known before local benchmarking |
|---|---|---|
| Exact equality between strict and non-strict behavior | Väliaho; all boundary families | Väliaho explicitly calls the almost-strict class crucial for criteria; zeros prove non-strictness exactly. |
| Outside the SPN cone | all exceptional families | PSD-plus-nonnegative certificates cannot establish membership. This is structural, not a runtime theorem. |
| Extreme rays | Hoffman-Pereira, order-5/order-6 classes, Baston, circulant, many graph rows | Inner approximations are exact only if they contain all extremes; the sources use extremes to study cone geometry. |
| Published hard test set | Hoffman-Pereira/Peng | Peng explicitly constructs the family as a hard test bed for two MILP checks. |
| Published difficult approximation instances | Dickinson-de Zeeuw | The paper reports poor inner-approximation behavior and warns that the hardest cases depend on the approximation. |
| Outside a named SOS level | Hildebrand-Afonin | The order-6 examples provably fail \(K_6^{(1)}\). |
| Outside every level of an SOS hierarchy | Laurent-Vargas | The direct sums provably lie outside \(\bigcup_rK_n^{(r)}\). |
| Strict but singular and perfect | Dannenberg-Schurmann | A source-proved phenomenon that separates strict copositivity from positive definiteness; no runtime conclusion is asserted. |
| Arbitrary-order exceptional boundary constructions | Baston; Johnson-Reams; Hildebrand circulant | They provide scalable structural stress families; the papers do not claim monotone runtime growth. |

## 14. Retained Research Not Represented By Separate Corpus Rows

Baumert's 1966 and 1967 papers are retained because they establish the extension operation and the implicit order-5 extreme family on
which later work builds. The 1967 printed non-\(\pm1\) example contains radicals, so it is not itself an exact integer corpus row. It was
not replaced by a decimal or rational approximation. The 24 Hildebrand order-5 rows instead use the later complete trigonometric
parametrization of Baumert's family and choose parameter values that become exact rational matrices.

This distinction also explains why the retained paper count is larger than the number of independently named corpus families: some papers
justify a construction, classification, or exclusion decision without contributing a separately stored matrix.

Sources: [Baumert 1966](../research/papers/Baumert_1966_extreme_copositive.pdf) and
[Baumert 1967](../research/papers/Baumert_1967_extreme_copositive_II.pdf).

## Reproduction Map

The exact finite selections are reproducible from:

- `testdata/archive/import_exceptional_matrices_2026_08_07.py` for the McKay/Hoffman-Pereira enumeration, four Kostyukova-Tchemisova examples,
  and Strekelj-Zalar \(C\);
- `testdata/archive/import_literature_extremes_2026_08_07.py` for Hildebrand \(\operatorname{COP}(5)\), both Baston families, Johnson-Reams, and
  Dickinson-de Zeeuw;
- `testdata/archive/import_hard_literature_matrices_2026_08_07.py` for Dickinson Case 9, Hildebrand-Afonin, Laurent-Vargas, and Hildebrand's
  circulant family;
- `testdata/archive/import_perfect_copositive_lifts_2026_08_07.py` for both Dannenberg-Schurmann lift chains.

The earlier IDs 9657-9756 were imported before these four consolidated reproducibility scripts. Their complete parameter and diagonal
choices remain embedded in each database row's `source` field. This guide describes their construction and provenance but does not
recast those local choices as published examples.
