# Copositivity on the Simplex: Curvature, KKT Points, Hadeler, and Dickinson

## Purpose and intuition

Copositivity asks whether the quadratic form $x^TAx$ is nonnegative for every nonnegative vector $x$. After normalizing
nonzero vectors, this becomes a minimization problem on the standard simplex. Every point of the simplex belongs to a face, and every
face is identified by the set of coordinates that are allowed to be positive. The Boolean lattice of supports is therefore not an
artificial data structure placed on top of the mathematics: it is the face structure of the simplex itself.

The practical problem is to avoid testing every face separately. This paper develops two complementary ways to remove whole regions
of the support lattice:

1. **Curvature tells us where a decisive minimum cannot need to live.** A flat or downward-curving direction on a face remains
   present in every larger face. Those faces cannot be the smallest carrier of the global minimum.
2. **A value certificate tells us where no negative point can live.** Positive semidefiniteness or a verified convex face minimum
   protects a face and every smaller face contained in it.
3. **Hadeler's all-ones solve exposes the first bad support.** Under increasing-cardinality induction, it either gives an explicit
   negative witness or proves that the current support is not the first failure.
4. **Dickinson reuses one first-order vector across many supports.** Its interval is limited by the responses of the unused
   coordinates.
5. **Full-simplex Karush--Kuhn--Tucker (KKT) information joins the two views.** Face stationarity concerns only used coordinates;
   the outside KKT inequalities are one sufficient route for pushing a Dickinson certificate all the way to the full ceiling.

The emphasis throughout is intuition first, proof second. The proofs matter because every pruning step must preserve the final
copositivity decision, but the recurring mental picture is simpler: supports are faces, curvature describes their shape, the
all-ones vector measures their stationary balance, and Dickinson asks how far the same balance certificate survives when new
coordinates are admitted.

Throughout, $A\in\mathbb R^{n\times n}$ is real and symmetric, and $A_I$ is the principal matrix selected by a nonempty support
$I\subseteq[n]$. Section 1 introduces the optimization problem formally.

## **The central map: what moves upward and what moves downward**

> **CUTTING INDICES AWAY CANNOT DESTROY POSITIVE SEMIDEFINITENESS.**
>
> **ADDING INDICES CANNOT REPAIR FAILURE OF POSITIVE DEFINITENESS.**

For nested supports $I\subseteq J$, the first statement sends $A_J\succeq0$ downward to every principal matrix $A_I$. The
second sends $A_I\not\succ0$ upward to every principal matrix $A_J$. The same restriction argument applies to simplex tangent
spaces: a flat or negative tangent direction on $I$, extended by zeros, remains a tangent direction on $J$.

The converses are false, and this is equally important for understanding the search:

> **CUTTING INDICES AWAY MAY REMOVE ALL NEGATIVE DIRECTIONS.**
>
> **AN INDEFINITE LARGE RESTRICTION MAY HAVE POSITIVE-SEMIDEFINITE OR POSITIVE-DEFINITE SMALLER RESTRICTIONS.**

and

> **ADDING INDICES MAY INTRODUCE A FLAT OR NEGATIVE DIRECTION.**
>
> **A POSITIVE-DEFINITE SMALL RESTRICTION MAY BECOME SINGULAR POSITIVE SEMIDEFINITE OR INDEFINITE.**

Intuitively, removing indices restricts the directions in which the quadratic form can move, so it can remove the directions that
caused negative curvature. Adding indices does the opposite: it preserves every old direction but introduces new ones, and one of
those new directions may be flat or negative. Thus bad curvature, once present, persists upward, whereas positive-semidefinite
curvature, once established, persists downward. Bad curvature need not persist downward, and positive definiteness need not persist
upward.

### **The two strict-curvature pruning directions cannot hide each other**

> **POSITIVE-DEFINITE DOWNWARD PRUNING CANNOT HIDE A CURVATURE-UPWARD CERTIFICATE.**
>
> **CURVATURE-UPWARD PRUNING CANNOT HIDE A POSITIVE-DEFINITE DOWNWARD CERTIFICATE.**

The first statement follows because $A_I\succ0$ implies $A_J\succ0$ for every nonempty $J\subseteq I$. Every direction on the
smaller face is also a direction inside the larger principal space, so none of the downward-pruned supports can contain a flat or
negative tangent direction. Consequently none could have supplied the curvature failure required for full upward pruning.

For the reverse statement, suppose curvature-upward pruning starts at $I$. Then some nonzero simplex-tangent direction $v$ satisfies

$$
v^T A_Iv\leq0.
$$

For every $J\supseteq I$, extend $v$ by zeros on $J\setminus I$. The same direction still has nonpositive curvature, so
$A_J\not\succ0$. Therefore none of the upward-pruned supports could have supplied a positive-definite downward certificate.

The intuition is that the two strict-curvature rules move in opposite hereditary directions. Positive definiteness flows downward
through principal submatrices, while one flat or negative direction flows upward by zero extension. Their two pruning cones cannot
conceal a certificate of the opposite strict-curvature kind.

This non-hiding statement is deliberately narrow. A positive-definite face can still satisfy the full outside KKT inequalities and
therefore supply a full-ceiling Dickinson certificate. Positive-definite downward pruning can hide that **KKT/Dickinson** opportunity,
because outside first-order inequalities are not curvature information. Likewise, a downward rule based only on singular positive
semidefiniteness is not a positive-definite rule and may coexist with failed strict convexity. The guarantee above concerns exactly
positive-definite downward pruning versus curvature-upward pruning.

### **The stricter no-hiding rule used by an opportunistic walk**

The general curvature theorem permits upward pruning as soon as the reduced Hessian is not positive definite. That includes a flat
zero eigenvalue. The general downward theorem also permits some singular positive-semidefinite cases. Both are valid pruning rules,
but they are not benign search-order rules: a flat upward closure can hide a larger singular-PSD downward certificate, and a singular
downward closure can hide a smaller flat upward root.

If a heuristic walk is allowed to prune only when it cannot hide either kind of curvature opportunity, the tests must be strict in
opposite directions. Let $Z$ have columns spanning

$$
\mathcal T_I=\{d:\mathbf1^Td=0\},
$$

and let $H_I=Z^TA_IZ$ be the reduced Hessian. “A negative eigenvalue” below always means an eigenvalue of $H_I$, not merely an
eigenvalue of $A_I$.

> **ONE NEGATIVE EIGENVALUE OF $H_I$ IS ENOUGH FOR BENIGN UPWARD PRUNING.**
>
> **POSITIVE DEFINITENESS OF $H_I$, TOGETHER WITH A VERIFIED NONNEGATIVE FACE MINIMUM, IS ENOUGH FOR BENIGN DOWNWARD PRUNING.**
>
> **POSITIVE DEFINITENESS OF $A_I$ IS ENOUGH FOR BENIGN DOWNWARD PRUNING EVEN WITHOUT STATIONARITY.**

The first rule includes an indefinite reduced Hessian: only one strictly negative direction is needed. Extending that direction by
zeros preserves strict negativity in every superset, so no larger support can be positive semidefinite and no downward curvature
certificate can be hidden. By contrast, a negative eigenvalue of the principal matrix $A_I$ alone is insufficient; its negative
direction may fail the tangent constraint $\mathbf1^Td=0$ while $H_I$ remains positive definite.

For the second rule, solve the face-stationarity equations exactly. If the resulting point $x$ is feasible on the face,
$H_I\succ0$, and $\lambda=x^TA_Ix\geq0$, strict convexity makes $x$ the unique face minimum. Every subface is then safe. Because
positive definiteness of the reduced Hessian is inherited by subfaces, the downward closure cannot hide a flat or negative
curvature root below it. The third rule is even simpler: $A_I\succ0$ is inherited by every principal submatrix and proves positive
quadratic value directly.

The final benign decision table is therefore:

| Exact situation at support $I$ | Action of the no-hiding heuristic | Why nothing of the opposite curvature type is hidden |
|---|---|---|
| A feasible exact point has $x^TA_Ix<0$ | Stop: $A$ is not copositive | This is a witness, not pruning. |
| A feasible exact point has $x^TA_Ix=0$ | Record “not strictly copositive”; continue ordinary CP work if needed | This is an exact zero witness. |
| $H_I$ has at least one negative eigenvalue | Prune every superset: $[I,[n]]$ | The same strictly negative tangent direction survives in every superset, so none can supply a PSD or PD downward certificate. |
| $H_I\succ0$, the exact face-stationary point is feasible, and its value is nonnegative | Prune every subset: $[\varnothing,I]$ | Every subface retains positive-definite tangent curvature, so none can supply a flat or negative upward root. |
| $A_I\succ0$, even when the walk stopped without a stationary or KKT point | Prune every subset: $[\varnothing,I]$ | Every principal submatrix of a positive-definite matrix is positive definite. |
| $H_I\succeq0$ is singular, or the exact conditions above otherwise fail | Add no heuristic closure | The pruning may still be mathematically valid with extra assumptions, but it can hide an opposite useful curvature certificate. |

Whether the endpoint satisfies the outside KKT inequalities does not enlarge this table. A full KKT point may yield a valid upward
Dickinson certificate, but that certificate can hide later curvature opportunities. A walk whose contract is “nothing gets hidden”
therefore uses KKT information to recognize its endpoint and exact value, not as an additional pruning permission.

Intuitively, this policy keeps only irreversible curvature information. Strict negativity can never be repaired by adding
coordinates; strict positivity can never be destroyed by removing coordinates. Flatness sits exactly on the boundary between those
two one-way facts, so the heuristic leaves it to the ordinary complete algorithm.

This gives the two pruning combinations to remember. Let $x$ lie in the relative interior of the face indexed by $I$, and let
$\lambda=x^TAx$.

> **FACE STATIONARITY + CONVEXITY + $\lambda\geq0$**
>
> **$\Longrightarrow$ DOWNWARD PRUNING.**

and

> **FULL-SIMPLEX KKT INEQUALITIES + $\lambda\geq0$**
>
> **$\Longrightarrow$ UPWARD DICKINSON PRUNING.**

Face stationarity checks only the coordinates in $I$. Convexity then makes the stationary point a minimizer throughout that face,
so a nonnegative value certifies every subface. Full-simplex KKT additionally checks every unused coordinate. Those inequalities make
the Dickinson upper endpoint equal to $[n]$, so the certificate reaches every support containing $I$.

If $\lambda<0$, pruning is unnecessary: $x$ itself is a nonnegative negative-value witness. If $\lambda=0$, the point
disproves strict copositivity, while its convex face is still safe for ordinary copositivity. If $\lambda>0$, the downward
certificate is valid for both classifications.

The two directions use different evidence. Curvature without outside KKT information does not automatically create a Dickinson
ceiling certificate. Full KKT information without convexity does not prove that the point minimizes its face. When both conditions
hold, the same support can prune both upward and downward.

## 1. The optimization problem

Let

$$
A\in\mathbb{R}^{n\times n}
$$

be a real symmetric matrix. Define the quadratic form

$$
q_A(x)=x^T A x,
$$

where $x\in\mathbb{R}^n$ is a column vector and $x^T$ is its transpose.

The **standard simplex** is

$$
\Delta_n=\left\{x\in\mathbb{R}^n:x_i\geq 0\text{ for every }i,
\quad \mathbf 1^T x=1\right\},
$$

where $\mathbf 1=(1,\ldots,1)^T$.

The standard quadratic program associated with $A$ is

$$
\mu(A)=\min_{x\in\Delta_n} q_A(x).
$$

Because $\Delta_n$ is compact and $q_A$ is continuous, this minimum is always attained.

By definition, $A$ is copositive when $q_A(y)\geq0$ for every $y\geq0$, and it is strictly copositive when $q_A(y)>0$ for every
nonzero $y\geq0$. Every such nonzero $y$ can be normalized to $x=y/(\mathbf1^Ty)\in\Delta_n$. Homogeneity then gives

$$
A\text{ is copositive}
\quad\Longleftrightarrow\quad
\mu(A)\geq 0,
$$

and

$$
A\text{ is strictly copositive}
\quad\Longleftrightarrow\quad
\mu(A)>0.
$$

Thus the three possible signs have exact copositivity meanings:

| Global minimum | Copositivity conclusion |
|---|---|
| $\mu(A)<0$ | $A$ is not copositive; a minimizing $x\geq0$ is a negative witness |
| $\mu(A)=0$ | $A$ is copositive but not strictly copositive |
| $\mu(A)>0$ | $A$ is strictly copositive |

The pruning rules below are useful only because they preserve this sign decision. Upward pruning keeps at least one support on which
the value $\mu(A)$ is attained. Downward pruning directly proves that an entire family of supports contains no negative value, and
sometimes no zero value either.

## 2. Supports and simplex faces

For any vector $x$, its ordinary support is the set of its nonzero coordinates:

$$
\operatorname{supp}(x)=\{i\in[n]:x_i\neq0\}.
$$

For $x\in\Delta_n$, all coordinates are nonnegative, so this is equivalently the set of indices where $x_i>0$.

For every nonempty index set $I\subseteq[n]$, define the simplex face

$$
\Delta_I=
\left\{x\in\Delta_n:x_j=0\text{ for every }j\notin I\right\}.
$$

Points in $\Delta_I$ may have zero coordinates inside $I$. The **relative interior** of that face is

$$
\operatorname{relint}(\Delta_I)=
\left\{x\in\Delta_I:x_i>0\text{ for every }i\in I\right\}.
$$

Therefore

$$
x\in\operatorname{relint}(\Delta_I)
\quad\Longleftrightarrow\quad
\operatorname{supp}(x)=I.
$$

Let $A_I$ denote the principal submatrix whose rows and columns are indexed by $I$. If $x$ is supported in $I$, then

$$
x^T A x=x_I^T A_I x_I,
$$

where $x_I$ is the vector of the coordinates indexed by $I$.

Every point of $\Delta_n$ has exactly one support, namely the indices of its positive coordinates. Searching the support lattice is
therefore not a separate combinatorial problem attached to copositivity: it is a decomposition of the original minimization problem
into simplex faces. To decide copositivity, it is enough to retain at least one support carrying a global minimizer and to prove the
required sign on every support that could still carry a smaller value.

## 3. Tangent directions and strict convexity on a face

Let $m=|I|$ be the number of indices in $I$. Inside the face $\Delta_I$, a feasible infinitesimal displacement must preserve the
sum of the coordinates. The tangent space is therefore

$$
\mathcal T_I=
\left\{v\in\mathbb{R}^{m}:\mathbf 1^T v=0\right\}.
$$

The quadratic form is **strictly convex on the face** $\Delta_I$ precisely when

$$
v^T A_I v>0
\qquad
\text{for every nonzero }v\in\mathcal T_I.
$$

This condition tests curvature only in directions that remain inside the affine hull of the simplex face. It does not test the
direction parallel to $\mathbf 1$.

This distinction is essential for copositivity. Tangent curvature does not say whether $q_A$ is positive or negative on the face.
Instead, it tells us whether that face can be the smallest support on which the copositivity-deciding global minimum is attained.

If the columns of a matrix $Z_I\in\mathbb{R}^{m\times(m-1)}$ form a basis of $\mathcal T_I$, then the **reduced Hessian** is

$$
H_I=Z_I^T A_I Z_I.
$$

Strict convexity on $\Delta_I$ is equivalent to positive definiteness of $H_I$:

$$
H_I\succ 0.
$$

This does not automatically mean that $A_I\succ0$. The exact relationship is developed in Section 7.

The particular basis $Z_I$ does not matter. A convenient choice, after ordering $I$, is

$$
Z_I=
\begin{bmatrix}
-1 & -1 & \cdots & -1\\
 1 &  0 & \cdots &  0\\
 0 &  1 & \cdots &  0\\
 \vdots & \vdots & \ddots & \vdots\\
 0 &  0 & \cdots &  1
\end{bmatrix}.
$$

Its columns are the directions $e_2-e_1,\ldots,e_m-e_1$.

### Singleton faces

If $|I|=1$, then $\mathcal T_I=\{0\}$. There is no nonzero tangent direction, so strict convexity is vacuously true. Consequently,
the curvature test alone never upward-prunes a singleton support.

## 4. The minimal-support minimizer theorem

The crucial fact is not merely that a global minimizer exists. Among all global minimizers, one can choose a minimizer using as few
coordinates as possible.

### Theorem

Let $x^*$ be a global minimizer of $q_A$ on $\Delta_n$ whose support

$$
I=\operatorname{supp}(x^*)
$$

has minimum cardinality among the supports of all global minimizers. Then $q_A$ is strictly convex on $\Delta_I$. Equivalently,

$$
v^T A_I v>0
\qquad
\text{for every nonzero }v\in\mathcal T_I.
$$

### Proof

Because $x^*$ has support $I$, it lies in $\operatorname{relint}(\Delta_I)$. For every $v\in\mathcal T_I$, both

$$
x^*+t v
\quad\text{and}\quad
x^*-t v
$$

remain in $\Delta_I$ for all sufficiently small positive $t$.

Consider the quadratic function along this line:

$$
q_A(x^*+t v)
=q_A(x^*)+2t\,v^T A_I x_I^*+t^2 v^T A_I v.
$$

Since $x^*$ is a minimizer and both signs of $t$ are locally feasible, the linear term must vanish:

$$
v^T A_I x_I^*=0.
$$

The quadratic coefficient cannot be negative, because then one sign of a sufficiently small $t$ would decrease the objective.
Hence

$$
v^T A_I v\geq0
$$

for every $v\in\mathcal T_I$.

Suppose strict convexity failed. Then there would be a nonzero tangent direction $v$ satisfying

$$
v^T A_I v=0.
$$

Along this direction both the linear and quadratic terms vanish, so

$$
q_A(x^*+t v)=q_A(x^*)
$$

for every feasible $t$. Move from $x^*$ along either $v$ or $-v$ until the first coordinate reaches zero. The endpoint is still a
global minimizer, but its support is a proper subset of $I$. This contradicts the minimal choice of $I$. Therefore the reduced
Hessian on $I$ must be positive definite. $\square$

*Literature.* This is the simplex specialization of Theorem 1 in Scozzari and Tardella,
[“A clique algorithm for standard quadratic programming”](https://doi.org/10.1016/j.dam.2007.09.020). Their theorem states that a
quadratic function bounded below on a pointed polyhedron attains its minimum in the relative interior of a face on which it is
strictly convex. The proof above gives the direct minimum-support argument for the simplex.

### Meaning of the theorem

A global minimum may occur on a flat face, but then the flat direction can be followed to a smaller face without increasing the
objective. Repeating this process eventually produces a global minimizer in the relative interior of a strictly convex face.

The theorem does **not** say that every global minimizer has a strictly convex support. It says that at least one global minimizer of
minimum support does.

### Why this theorem matters for copositivity

Passing to a minimum-support minimizer does not change its objective value. It still attains $\mu(A)$. Therefore the theorem applies
to every possible copositivity outcome:

- If $\mu(A)<0$, there is a negative witness whose support has strictly positive tangent curvature.
- If $\mu(A)=0$, there is a copositive zero whose support has strictly positive tangent curvature.
- If $\mu(A)>0$, there is a strictly positive global minimizer whose support has strictly positive tangent curvature.

Consequently, a complete copositivity algorithm does not have to retain every support on which a global minimizer might appear. It
only has to retain at least one minimum-support representative of the same global value. This is exactly what makes the upward rule
below safe: it removes supports that cannot be such representatives without changing whether the decisive value is negative, zero,
or positive.

## 5. Upward pruning

For a support $I$, define its upward closure in the Boolean lattice by

$$
\uparrow I=
\left\{J\subseteq[n]:I\subseteq J\right\}.
$$

These are all supports whose faces contain $\Delta_I$.

### Upward-pruning theorem

Assume strict convexity fails on $\Delta_I$. Thus there is a nonzero $v\in\mathcal T_I$ such that

$$
v^T A_I v\leq0.
$$

Then no support $J\in\uparrow I$ can be the support of a minimal-support global minimizer.

### Proof

Take any $J\supseteq I$. Extend $v$ by zeros in the coordinates of $J\setminus I$. The extended vector belongs to $\mathcal T_J$
and satisfies

$$
v^T A_J v=v^T A_I v\leq0.
$$

Therefore $q_A$ is not strictly convex on $\Delta_J$. By the minimal-support minimizer theorem, $J$ cannot be the support of a
minimal-support global minimizer. This holds for every $J\supseteq I$. $\square$

*Literature.* The existence of a strictly convex minimizing face is the Scozzari--Tardella result cited above. The upward-closure
statement is the direct hereditary corollary used here: extending the same tangent direction by zeros preserves its curvature on
every larger face.

### Geometric intuition

The obstruction is already present inside the smaller face:

- If $v^T A_Iv<0$, the face contains a descending curvature direction.
- If $v^T A_Iv=0$, the face contains a flat direction.

Adding more coordinates does not remove that direction. Every larger face still contains the same embedded line. Hence no larger
face can become strictly convex.

### What upward pruning does and does not prove

Upward pruning says:

> None of the supports containing $I$ is needed as the support of the chosen minimal-support global minimizer.

It does **not** say:

- that every principal matrix $A_J$ with $J\supseteq I$ is copositive;
- that $q_A$ has no stationary point on those faces;
- that $q_A$ has no global minimizer on those faces;
- or that every point on those faces has nonnegative value.

A larger face can contain a global minimizer, but if it does, an equally good minimizer exists on a smaller support.

### Why upward pruning is sound for copositivity

An upward-pruned face may still contain negative points or zeros. That is harmless because the rule is not claiming that the face is
nonnegative. It is claiming that this face cannot be the smallest carrier of the global minimum. If the true global minimum is
negative, a negative minimum-support witness remains outside the pruned upward cone. If it is zero, a minimum-support zero remains.
If it is positive, removing supports cannot manufacture a negative or zero value. Thus the pruning preserves the CP/SCP decision
even though it does not certify the pruned principal submatrices individually.

### Number of supports removed

If $|I|=m$, every index outside $I$ may be either absent or present. Therefore

$$
|\uparrow I|=2^{n-m}.
$$

Small upward certificates are potentially powerful because $2^{n-m}$ grows exponentially as $m$ decreases.

## 6. Downward pruning

For a nonempty support $I$, define its nonempty downward closure by

$$
\downarrow I=
\left\{J:\varnothing\neq J\subseteq I\right\}.
$$

### Positive-definite downward theorem

If the full principal matrix $A_I$ is positive definite, written

$$
A_I\succ0,
$$

then

$$
x^T A x>0
$$

for every nonzero nonnegative vector $x$ whose support is contained in $I$. Consequently every support in $\downarrow I$ is strictly
positive and can be downward-pruned from both the copositive and strictly copositive searches.

This is a direct copositivity certificate, not merely a statement about where a minimizer may occur. None of these supports can carry
a negative witness or a zero. If such certificates cover the entire support lattice, then $A$ is strictly copositive.

### Proof

Let $J$ be any nonempty subset of $I$, and let $x$ be a nonzero vector supported in $J$. Regard $x$ as a vector in
$\mathbb{R}^{|I|}$ by inserting zeros in the coordinates of $I\setminus J$. Positive definiteness gives

$$
x^T A_J x=x^T A_I x>0.
$$

Thus every nonzero vector supported in any subset of $I$ has strictly positive quadratic value. $\square$

*Literature.* This is the standard inheritance of positive definiteness by principal submatrices; see Horn and Johnson,
[*Matrix Analysis*, Chapter 7](https://www.cambridge.org/highereducation/books/matrix-analysis/FDA3627DC2B9F5C3DF2FD8C3CC136B48/positive-definite-and-semidefinite-matrices/0E9DB201B29FC39A7174E5C9B666DD62).
The displayed zero-extension argument is also the complete proof; it does not require a separate copositivity theorem.

### Positive semidefiniteness

If only

$$
A_I\succeq0
$$

is known, the same argument proves

$$
x^T A x\geq0
$$

on every support in $\downarrow I$. This certifies ordinary copositivity on those faces, but it does not certify strict copositivity:
a nonnegative vector in the kernel may have value zero.

Thus a positive-semidefinite downward certificate removes these supports from the search for a negative witness, but not from a
search whose purpose is to prove that no copositive zero exists. Covering every support by positive-semidefinite certificates proves
ordinary copositivity; proving strict copositivity requires strict positivity everywhere.

### Number of supports removed

If $|I|=m$, then

$$
|\downarrow I|=2^m-1.
$$

Large positive-definite principal supports are therefore the valuable downward certificates.

## 7. Relationship between $A_I$ and the reduced Hessian

The unrestricted Hessian of $x_I^TA_Ix_I$ is $2A_I$. On the simplex face, however, feasible displacements must satisfy
$\mathbf1^Tv=0$. The Hessian relevant to the constrained problem is therefore the **reduced Hessian**

$$
2H_I=2Z_I^TA_IZ_I.
$$

The harmless positive factor $2$ is omitted elsewhere in this note. The matrices $A_I$ and $H_I$ do not generally have the same
definiteness because $Z_I$ is rectangular: $H_I$ sees only the $(m-1)$-dimensional tangent space, whereas $A_I$ sees every direction
in $\mathbb R^m$.

Restriction preserves both positive and negative semidefiniteness. Thus there are four automatic one-way implications:

$$
A_I\succ0\quad\Longrightarrow\quad H_I\succ0,
$$

and

$$
A_I\succeq0\quad\Longrightarrow\quad H_I\succeq0.
$$

Likewise,

$$
A_I\prec0\quad\Longrightarrow\quad H_I\prec0,
$$

and

$$
A_I\preceq0\quad\Longrightarrow\quad H_I\preceq0.
$$

For a nontrivial face, where $m\geq2$, a negative-definite $A_I$ therefore gives a negative-definite reduced Hessian. It can never
give $H_I\succ0$, so it immediately satisfies the upward-pruning condition.

Indeed, for every nonzero $y\in\mathbb R^{m-1}$, the full-column-rank property of $Z_I$ gives $Z_Iy\neq0$, and

$$
y^TH_Iy=(Z_Iy)^TA_I(Z_Iy).
$$

Thus positivity of $A_I$ on all directions implies positivity on the smaller tangent space. Equivalently, if $H_I$ is not positive
definite, then $A_I$ cannot be positive definite.

*Literature.* Projecting a symmetric quadratic form onto a constraint subspace and relating the two inertias is standard constrained
inertia theory; see Han and Fujiwara,
[“An inertia theorem for symmetric matrices and its application to nonlinear programming”](https://doi.org/10.1016/0024-3795(85)90141-7).

The converse is false. Positive definiteness of $H_I$ does **not** imply positive definiteness of $A_I$. It only says that $A_I$ is
positive on the zero-sum directions. Because this tangent space has codimension one, $A_I$ can still have one negative or zero
eigenvalue that $H_I$ does not detect.

More precisely, let $p(A_I)$ and $r(A_I)$ be the numbers of positive and negative eigenvalues of $A_I$, and define $p(H_I)$ and
$r(H_I)$ similarly. Restriction to a codimension-one subspace gives

$$
\max\{0,p(A_I)-1\}\leq p(H_I)\leq p(A_I),
$$

and

$$
\max\{0,r(A_I)-1\}\leq r(H_I)\leq r(A_I).
$$

The tangent restriction can therefore lose at most one positive direction and at most one negative direction. In particular, if
$A_I$ has fewer than $m-1$ positive eigenvalues, then $H_I$ cannot be positive definite.

*Literature.* These codimension-one bounds are the hyperplane case of the projected-matrix inertia relation of Han and Fujiwara
cited above.

| Definiteness of $A_I$ | What follows for $H_I$ |
|---|---|
| $A_I\succ0$ | Necessarily $H_I\succ0$ |
| $A_I\succeq0$ but singular | $H_I\succeq0$; it is positive definite exactly when the nullity is one and the kernel is not tangent |
| $A_I\prec0$ | Necessarily $H_I\prec0$ for $m\geq2$ |
| $A_I\preceq0$ but singular | Necessarily $H_I\preceq0$ |
| $A_I$ indefinite with at least two nonpositive eigenvalues | $H_I$ cannot be positive definite |
| $A_I$ nonsingular with exactly one negative eigenvalue | $H_I$ may be positive definite; Section 11 gives the additional scalar test |

This leaves only three possible inertia patterns when $H_I\succ0$: $A_I$ is positive definite, $A_I$ is positive semidefinite with
nullity one, or $A_I$ is nonsingular with exactly one negative eigenvalue. All other patterns prove $H_I\not\succ0$ without forming
the reduced Hessian.

### Do we have to construct the reduced Hessian?

No. An exact factorization of $A_I$ already gives its inertia. That settles most cases immediately:

1. If $A_I\succ0$, then $H_I\succ0$ automatically, and the stronger downward certificate is available.
2. If $A_I$ has fewer than $m-1$ positive eigenvalues, then $H_I\not\succ0$, so the support can be upward-pruned.
3. If $A_I$ is nonsingular with exactly one negative eigenvalue, one additional scalar
   $\mathbf1^TA_I^{-1}\mathbf1$ decides the question.
4. If $A_I\succeq0$ has nullity one, one kernel vector and its coordinate sum decide the question.

Section 11 proves the last two tests. Thus an implementation can reuse the factorization of $A_I$ and need not build or factor
$Z_I^TA_IZ_I$ separately.

For example, take $A_I=B$, where $J_m=\mathbf 1\mathbf 1^T$ is the $m\times m$ all-ones matrix and

$$
B=I_m-2J_m.
$$

For every $v\in\mathcal T_I$, one has $J_mv=0$, so

$$
v^T Bv=\|v\|_2^2>0
$$

for every nonzero tangent vector. Thus the quadratic form is strictly convex on the simplex face.

Nevertheless, $B$ has eigenvalue $1-2m<0$ in the direction $\mathbf1$, so $B$ is indefinite. This gives the concrete combination

$$
H_I\succ0
\qquad\text{but}\qquad
A_I\not\succ0.
$$

However, for every $x$ on that simplex face,

$$
x^T Bx=\|x\|_2^2-2,
$$

which is negative. Strict face convexity therefore says nothing by itself about whether the attained value is positive, zero, or
negative.

The copositivity consequences are therefore different:

- **Strict tangent convexity** identifies where a minimal-support global minimizer carrying the decisive value $\mu(A)$ may live; it
  does not decide the sign of that value.
- **Positive definiteness of the principal matrix** proves positive values on all contained faces and therefore rules out both
  non-copositivity witnesses and copositive zeros there.

## 8. Setup and the indispensable assumption

Let $I\subseteq[n]$ be nonempty, let $m=|I|$, and write

$$
B=A_I\in\mathbb R^{m\times m}
$$

for the principal matrix indexed by $I$. Assume $B$ is symmetric and nonsingular. Solve

$$
Bw=\mathbf1,
\qquad
w=B^{-1}\mathbf1.
$$

All vector inequalities in this note are componentwise. Thus

$$
w\leq0
$$

means that every coordinate of $w$ is nonpositive, whereas

$$
w\not\leq0
$$

means that at least one coordinate is positive.

The Hadeler implication requires an induction hypothesis:

> Every proper principal submatrix of $B$ has already been proved copositive.

This is why Hadeler-style algorithms process supports in increasing cardinality. Without this assumption, the all-ones solution is
not a complete copositivity test: a negative witness may live entirely on an unchecked boundary face.

## 9. The correct all-ones copositivity criterion

Under the preceding assumptions,

$$
\boxed{
B\text{ is not copositive}
\quad\Longleftrightarrow\quad
B^{-1}\mathbf1\leq0.
}
$$

Negating this statement gives

$$
\boxed{
B\text{ is copositive}
\quad\Longleftrightarrow\quad
B^{-1}\mathbf1\not\leq0.
}
$$

The second box does **not** say

$$
B^{-1}\mathbf1\geq0.
$$

For copositivity, it is enough that the solution has at least one positive coordinate. It may have mixed signs.

### 9.1 Why a nonpositive solution proves failure

Suppose $w\leq0$ and define

$$
z=-w\geq0.
$$

Since $Bw=\mathbf1$,

$$
Bz=-\mathbf1.
$$

Therefore

$$
z^TBz=-\mathbf1^Tz<0.
$$

The inequality is strict because $z\neq0$. Hence $z$ is an explicit nonnegative witness proving that $B$, and therefore the full
matrix $A$, is not copositive.

This direction needs no induction hypothesis.

### 9.2 Why every minimal failure produces a negative solution

Now suppose $B$ is not copositive while every proper principal submatrix of $B$ is copositive. Minimize

$$
q_B(x)=x^TBx
$$

over the simplex

$$
\Delta_m=\{x\geq0:\mathbf1^Tx=1\}.
$$

Let $x^*$ be a global minimizer and write

$$
\mu=(x^*)^TBx^*<0.
$$

The minimizer cannot lie on the boundary of $\Delta_m$. If one coordinate of $x^*$ were zero, its negative value would already make
a proper principal submatrix non-copositive. Hence

$$
x^*>0.
$$

Because the minimizer is in the relative interior, its first-order stationarity equation is

$$
Bx^*=\mu\mathbf1.
$$

As $B$ is nonsingular,

$$
w=B^{-1}\mathbf1=\frac{x^*}{\mu}.
$$

Since $x^*>0$ and $\mu<0$, it follows that

$$
w<0.
$$

Thus a principal matrix that fails for the first time must return an entirely negative all-ones solution.

*Literature.* This is the all-ones specialization of Theorem 2 in K. P. Hadeler,
[“On copositive matrices”](https://doi.org/10.1016/0024-3795(83)90095-2). Dickinson obtains the equivalent alternative-system form
in Lemma 4.4 of [“A new certificate for copositivity”](https://doi.org/10.1016/j.laa.2018.12.025) using Hadeler's result and Farkas'
lemma.

## 10. What $B^{-1}\mathbf1$ means geometrically

Define

$$
\delta=\mathbf1^Tw.
$$

If $\delta\neq0$, normalize $w$ by setting

$$
\widehat x=\frac{w}{\delta}.
$$

Then

$$
\mathbf1^T\widehat x=1,
$$

and

$$
B\widehat x=\frac1\delta\mathbf1.
$$

Consequently,

$$
\widehat x^TB\widehat x=\frac1\delta.
$$

Thus $w=B^{-1}\mathbf1$ determines the unique stationary point of the quadratic form on the affine hyperplane
$\mathbf1^Tx=1$, provided $\delta\neq0$.

This stationary point belongs to the simplex only when $\widehat x\geq0$. It lies in the relative interior only when
$\widehat x>0$.

The important sign cases are:

- If $w<0$, then $\delta<0$, $\widehat x>0$, and its value $1/\delta$ is negative. This is the minimal-failure case.
- If $w>0$, then $\delta>0$, $\widehat x>0$, and its value is positive.
- If $w$ has mixed signs, then $\widehat x$ also has mixed signs and is outside the simplex.
- If $\delta=0$, $w$ cannot be normalized to the simplex; it is instead a zero-sum tangent direction. Moreover,
  $w^TBw=w^T\mathbf1=\delta=0$, so the reduced Hessian cannot be positive definite.

The all-ones solve therefore supplies a first-order stationary candidate when that candidate is feasible. It does not yet say
whether the stationary point is a minimum, maximum, or saddle point.

## 11. Exact tests from the inertia of a principal matrix

The reduced Hessian need not be constructed explicitly. Its definiteness can be recovered from an exact factorization of

$$
B=A_I.
$$

These inertia tests answer the support-location question needed by upward copositivity pruning: can $I$ still carry a
minimum-support global minimizer? Except when they prove $A_I\succeq0$ or $A_I\succ0$, they do not by themselves decide the sign of
$q_A$ on the face.

The **inertia** of a real symmetric matrix is the triple

$$
(n_+(B),n_-(B),n_0(B)),
$$

where the three numbers count its positive, negative, and zero eigenvalues.

### 11.1 Nonsingular case

Assume $B$ is nonsingular. Define

$$
w=B^{-1}\mathbf 1
$$

and

$$
\delta=\mathbf 1^T B^{-1}\mathbf 1=\mathbf 1^T w.
$$

Then the quadratic form is strictly convex on $\Delta_I$ exactly in either of the following cases:

1. $B$ is positive definite; or
2. $B$ has exactly one negative eigenvalue, no zero eigenvalue, and $\delta<0$.

The two cases have different copositivity meanings. In the first case, $B\succ0$ directly certifies strict positivity on the whole
face and all its subfaces. In the second case, strict tangent convexity only identifies a possible minimum-support minimizer face.
The normalized stationary candidate, when it lies in the relative interior, is

$$
x=\frac{w}{\delta},
\qquad
q_A(x)=\frac1\delta<0.
$$

It is then an immediate non-copositivity witness. If $w/\delta$ is not positive coordinatewise, this interior candidate is infeasible
and the sign of the minimum must be settled on smaller faces.

#### Why this criterion works

If $\delta\neq0$, every $y\in\mathbb{R}^m$ has a unique decomposition

$$
y=v+\alpha w,
\qquad v\in\mathcal T_I.
$$

The two parts are orthogonal with respect to the bilinear form induced by $B$ because

$$
v^T Bw=v^T\mathbf 1=0.
$$

Moreover,

$$
w^T Bw=w^T\mathbf 1=\delta.
$$

Thus the inertia of $B$ consists of the inertia on the tangent space plus the sign of $\delta$. If the tangent restriction is
positive definite, the only direction outside it is either positive, giving $B\succ0$, or negative, giving exactly one negative
eigenvalue and $\delta<0$.

If $\delta=0$, then $w$ itself is a nonzero tangent vector satisfying $w^TBw=0$, so strict face convexity fails.

*Literature.* This one-constraint inertia decomposition is a direct specialization of the projected-matrix inertia theorem of Han
and Fujiwara cited in Section 7.

### 11.2 Singular case

Assume $B$ is singular. The quadratic form is strictly convex on $\Delta_I$ exactly when all three conditions hold:

1. $B$ is positive semidefinite;
2. $B$ has nullity one; and
3. a nonzero kernel vector $z\in\ker(B)$ satisfies $\mathbf 1^Tz\neq0$.

The third condition says that the kernel direction is not tangent to the simplex face.

Because $B\succeq0$ in this case, the whole face and all its subfaces are already certified nonnegative. This is enough for ordinary
copositivity on those supports. Strict copositivity depends on whether $\ker(B)$ contains a nonzero nonnegative vector: such a vector
has value zero, while a mixed-sign kernel direction is not a copositivity zero.

#### Proof of the singular criterion

If the tangent restriction is positive definite, no nonzero kernel vector can lie in $\mathcal T_I$. Since $\mathcal T_I$ has
codimension one, the kernel can have dimension at most one. Because $B$ is singular, its nullity is exactly one, and its kernel
vector $z$ must satisfy $\mathbf 1^Tz\neq0$.

Every vector $y\in\mathbb{R}^m$ can then be written as

$$
y=v+\alpha z,
\qquad v\in\mathcal T_I.
$$

Since $Bz=0$,

$$
y^TBy=v^TBv\geq0.
$$

Therefore $B$ is positive semidefinite.

Conversely, suppose the three conditions hold. If a nonzero tangent vector $v$ had $v^TBv=0$, positive semidefiniteness would imply
$v\in\ker(B)$. But the one-dimensional kernel is spanned by $z$, which is not tangent. This is impossible. Hence every nonzero
tangent vector has strictly positive curvature.

*Literature.* This is the singular one-constraint case of the same projected-matrix inertia relation. The proof is included because
the position of the kernel relative to $\mathbf1^Tv=0$ is the part needed for copositivity pruning.

### A useful singular example

The matrix

$$
B=I_m-\frac1m J_m
$$

is positive semidefinite, has kernel $\operatorname{span}\{\mathbf 1\}$, and is positive definite on the tangent space. Its simplex
minimum is zero at the uniform vector. This shows again that strict face convexity does not imply strict copositivity.

## 12. KKT points and the curvature rule

Let $x\in\operatorname{relint}(\Delta_I)$. A stationary point of $q_A$ relative to the face satisfies

$$
A_Ix_I=\lambda\mathbf 1
$$

for some scalar $\lambda$. Because $\mathbf 1^Tx_I=1$,

$$
q_A(x)=x_I^TA_Ix_I=\lambda.
$$

The KKT multiplier is therefore the quadratic value itself. Its copositivity meaning is immediate once the candidate has been
verified:

- $\lambda<0$ gives an explicit nonnegative negative witness, so $A$ is not copositive.
- $\lambda=0$ gives a nonnegative zero-valued point and disproves strict copositivity, but ordinary copositivity remains unresolved
  until negative values elsewhere are excluded.
- $\lambda>0$ proves only that this candidate is positive; another face may still contain a zero or negative point.

For this point to satisfy the KKT conditions on the full simplex, every unused coordinate must satisfy

$$
(Ax)_j\geq\lambda
\qquad\text{for every }j\notin I.
$$

If the face is strictly convex, a stationary point on that face is its unique face minimizer. It is not necessarily the global
simplex minimizer, because another face may attain a smaller value.

The upward curvature rule is cheaper and more general than solving the KKT system: when strict convexity fails, the support can be
excluded before candidate probabilities or outside inequalities are calculated. When strict convexity holds, however, the KKT
value and the remaining faces are still needed to reach a global copositivity conclusion.

### Face stationarity versus a full-simplex KKT point

The word *KKT* is meaningful only after the optimization problem has been fixed. If the problem is restricted to $\Delta_I$, the
coordinates outside $I$ are no longer variables. A relative-interior KKT point of that restricted problem therefore checks only

$$
A_Ix_I=\lambda\mathbf1.
$$

This is best called a **face-stationary point**. It does not say what happens if mass is moved into a previously unused coordinate.

For the full simplex problem, the coordinates outside $I$ are still variables sitting at their lower bound $x_j=0$. The full KKT
conditions additionally require

$$
(Ax)_j\geq\lambda
\qquad(j\notin I).
$$

Intuitively, every unused coordinate is a closed door. The outside inequality checks whether opening that door by moving a little
mass into coordinate $j$ can improve the objective to first order. Thus a full-simplex KKT point does not look outside the
simplex, but it does inspect every direction from the current face into a larger face.

### Three different response sets: equality, KKT compatibility, and Dickinson

The game-theoretic extended support, the set of coordinates compatible with the KKT inequality, and Dickinson's upper endpoint are
three different objects. They coincide only in special cases.

Let $x\in\operatorname{relint}(\Delta_I)$ be face-stationary for the minimization problem, with

$$
A_Ix_I=\lambda\mathbf1,
\qquad
\lambda=x^TAx>0.
$$

The **equality set** is

$$
E_\lambda(x)=\{j:(Ax)_j=\lambda\}.
$$

It contains the used support $I$. When $x$ satisfies the full-simplex KKT conditions, this equality set is the genuine extended
support: its unused members are precisely the outside coordinates tied with the used coordinates at the same first-order value.

For minimization, the larger **KKT-compatible set** is

$$
K_\lambda(x)=\{j:(Ax)_j\geq\lambda\}.
$$

An outside coordinate in this set cannot improve the objective to first order. Equality means a tie; strict inequality means that
moving mass into that coordinate is locally worse. Full-simplex KKT requires every coordinate to belong to $K_\lambda(x)$.

Dickinson instead uses the absolute zero threshold

$$
N_A(x)=\{j:(Ax)_j\geq0\}.
$$

Because $\lambda>0$, these sets satisfy

$$
\boxed{
E_\lambda(x)
\subseteq
K_\lambda(x)
\subseteq
N_A(x).
}
$$

For the all-ones normalization, put $u=x/\lambda$. Then $A_Iu_I=\mathbf1$, and the same comparison becomes

$$
\underbrace{\{j:(Au)_j=1\}}_{\text{genuine extended support when full KKT holds}}
\subseteq
\underbrace{\{j:(Au)_j\geq1\}}_{\text{KKT-compatible for minimization}}
\subseteq
\underbrace{\{j:(Au)_j\geq0\}}_{\text{Dickinson upper endpoint}}.
$$

The intuition is that the three thresholds ask progressively weaker questions. Equality asks which coordinates are exact ties. The
KKT inequality asks which coordinates are no better than the current point. Dickinson asks only on which coordinates the product
remains nonnegative. In particular, the band

$$
0\leq(Au)_j<1
$$

is accepted by Dickinson even though it violates the minimization KKT inequality. Therefore Dickinson's upper endpoint should not
be called an extended support without qualification. It is a **nonnegative-response set**, and it can reach $[n]$ even when the
candidate is not a full-simplex KKT point. For maximization the outside KKT inequality reverses, but genuine extended support still
means equality with the common active value.

*Literature.* Equality-based extended support is standard in evolutionary game theory; see I. M. Bomze, “Detecting all
evolutionarily stable strategies,” Proposition 2.1 and the surrounding definitions. Dickinson's nonnegative-product set and its
Boolean-interval role come from Theorem 4.6 of his 2019 paper.

### Why full KKT reaches the Dickinson ceiling

For an embedded vector $u\in\mathbb R^n$, define its nonnegative-product set

$$
N_A(u)=\{j\in[n]:(Au)_j\geq0\}.
$$

This is Dickinson's upper endpoint, but it is not the ordinary support of a vector. Section 16 develops the complete interval theorem.
Here we need only show why the full KKT inequalities force $N_A(u)=[n]$.

Assume first that $\lambda>0$, and set

$$
u=\frac{x}{\lambda}.
$$

On the support $I$,

$$
(Au)_i=1
\qquad(i\in I),
$$

while the outside KKT inequalities give

$$
(Au)_j\geq1
\qquad(j\notin I).
$$

Hence every coordinate belongs to Dickinson's upper set:

$$
N_A(u)=[n].
$$

The resulting interval is the full upward cone $[I,[n]]$. If $\lambda=0$, the unscaled vector $x$ satisfies $Ax\geq0$, so
the same ceiling conclusion holds and $x$ is also a copositive zero. If $\lambda<0$, $x$ is already a negative witness.

The important distinction is therefore exact:

- **face stationarity plus convexity and nonnegative value** gives the downward block;
- **the outside inequalities of full KKT and nonnegative value** give the upward Dickinson block.

Neither statement is a substitute for the other.

## 13. The exact connection at a minimal failing support

At an arbitrary copositive support, the all-ones solution and the reduced Hessian need not point in the same direction. At a
**minimal non-copositive support**, however, they fit together exactly.

We already know that such a support has

$$
w<0,
\qquad
\delta<0,
\qquad
\widehat x=\frac{w}{\delta}>0,
\qquad
q_B(\widehat x)=\frac1\delta<0.
$$

Moreover,

$$
H\succ0.
$$

To see this, first-order optimality gives a global interior minimizer. Hence $H\succeq0$. If $H$ had a nonzero zero-curvature
direction, the quadratic value would remain constant along that direction. Moving until a coordinate vanished would produce the same
negative value on a proper face, contradicting the assumption that all proper principal submatrices are copositive. Therefore the
reduced Hessian is positive definite.

The complete structure of a minimal nonsingular failure is therefore:

$$
\boxed{
w<0,
\quad
\delta<0,
\quad
H\succ0,
\quad
B\text{ has exactly one negative eigenvalue}.
}
$$

The last conclusion follows from the codimension-one inertia relation: $H$ supplies $m-1$ positive directions, while the negative
value $1/\delta$ supplies the remaining negative direction.

*Literature.* Hadeler's Theorem 2 proves that a first non-copositive principal matrix is nonsingular with exactly one negative
eigenvalue. The strict-convex-face interpretation is also a simplex instance of Theorem 1 in Scozzari and Tardella,
[“A clique algorithm for standard quadratic programming”](https://doi.org/10.1016/j.dam.2007.09.020).

## 14. Copositivity does not imply convexity

The reverse conclusion is false. If $B$ is copositive, then under the induction hypothesis

$$
w\not\leq0,
$$

but this gives no fixed sign for the reduced Hessian.

For example, for $m\geq2$ let

$$
B=2\mathbf1\mathbf1^T-I_m.
$$

For every nonzero $x\geq0$,

$$
x^TBx=2(\mathbf1^Tx)^2-\|x\|_2^2>0,
$$

so $B$ is strictly copositive. Its all-ones solution is

$$
w=\frac1{2m-1}\mathbf1>0.
$$

But for every nonzero tangent vector $v$ with $\mathbf1^Tv=0$,

$$
v^TBv=-\|v\|_2^2<0.
$$

Hence

$$
H\prec0.
$$

The positive stationary point is the maximum of the face, not its minimum. The minima lie on smaller boundary faces, which are
copositive as well.

Therefore:

> The Hadeler all-ones criterion can certify copositivity even when the quadratic form is concave on the current face.

Copositivity controls the sign of the quadratic form only on the nonnegative orthant. Convexity controls how that value changes along
zero-sum tangent directions. Neither property implies the other.

## 15. Do we need the reduced Hessian for the Hadeler test?

No. Under the increasing-cardinality assumption, the all-ones sign test is already complete for the current nonsingular principal
matrix:

- $w\leq0$ gives a negative witness;
- $w\not\leq0$ rules out this support as a first copositivity failure.

The reduced Hessian answers a different question:

> Can this support be the smallest support of a global simplex minimizer?

If $H\not\succ0$, the support and all its supersets can be removed from that minimizer-support search. If $H\succ0$, curvature alone
does not decide the sign of the minimum, so the all-ones solution or another copositivity certificate remains necessary.

The same exact factorization of $B$ can support both tests. It gives the inertia of $B$ and solves $Bw=\mathbf1$. Section 11 shows
how the inertia and the scalar $\delta=\mathbf1^Tw$ decide whether $H\succ0$ without explicitly constructing $Z^TBZ$.

## 16. What Dickinson adds

Hadeler's criterion decides the current principal support. Dickinson reuses the same vector across many supports.

Embed $w$ into $\mathbb R^n$ by inserting zeros outside $I$; call the embedded vector $u$. When $w\not\leq0$, define

$$
L(u)=\operatorname{supp}(u)
$$

and the separately defined nonnegative-product set

$$
N_A(u)=\{j\in[n]:(Au)_j\geq0\}.
$$

Because

$$
(Au)_I=Bw=\mathbf1>0,
$$

we have $I\subseteq N_A(u)$. Every support $J$ satisfying

$$
L(u)\subseteq J\subseteq N_A(u)
$$

inherits a vector $u_J$ with

$$
u_J\not\leq0
\qquad\text{and}\qquad
A_Ju_J\geq0.
$$

By the Hadeler--Farkas alternative, such a $J$ cannot be a minimal support on which copositivity fails. Thus one all-ones solve
certifies the complete Boolean interval $[L(u),N_A(u)]$.

If these intervals cover every nonempty support, no minimal failing support exists and $A$ is copositive.

This is Dickinson's additional idea. It is not a convexity argument:

- the Dickinson interval comes from the sign pattern of $u$ and $Au$;
- the curvature closure comes from the definiteness of $Z^TA_IZ$;
- either certificate may exist without the other.

*Literature.* The interval certificate is Theorem 4.6 of Dickinson's 2019 paper. Its proof combines Hadeler's principal-submatrix
criterion with Farkas' lemma.

## 17. How curvature and Dickinson fit together—and where they do not

Sections 7 and 11 already gave the algebraic bridge: the tangent restriction and the all-ones direction are complementary parts of
the same quadratic form. This section concentrates on the more important conceptual question: why the two arguments often meet at
the same support while producing different pruning ranges.

### 17.1 Their common target

Let $M\subseteq[n]$ be inclusion-minimal among the non-copositive principal supports:

$$
A_M\notin\operatorname{COP},
\qquad
A_J\in\operatorname{COP}
\quad\text{for every }J\subsetneq M.
$$

Both pruning arguments rule out possible locations of $M$:

- If the reduced Hessian on $I$ is not positive definite, then neither $I$ nor any $J\supseteq I$ can equal $M$.
- If a Dickinson vector certifies $[L(u),N_A(u)]$, then no support in that Boolean interval can equal $M$.

Neither statement, by itself, says that every pruned principal matrix is copositive. Both preserve the decision because a
non-copositive matrix must possess at least one inclusion-minimal bad support.

### 17.2 Why their upward extensions differ

Although both arguments extend a vector by zeros when the support grows, the expressions that must remain valid are different.

#### Curvature extension

Suppose strict convexity fails on $I$. Then some nonzero $v\in\mathcal T_I$ satisfies

$$
v^TBv\leq0.
$$

For any $J\supseteq I$, extend $v$ by zeros on $J\setminus I$. The enlarged vector still has zero coordinate sum, and

$$
v^TA_Jv=v^TBv\leq0.
$$

The newly added rows and columns make no contribution because the corresponding coordinates of $v$ are zero on both sides of the
quadratic expression. Hence

$$
H_I\not\succ0
\quad\Longrightarrow\quad
H_J\not\succ0
\qquad\text{for every }J\supseteq I.
$$

One curvature direction therefore prunes the complete upward cone $[I,[n]]$.

#### Dickinson extension

Embed $w=B^{-1}\mathbf1$ into the full space by placing zeros outside $I$. On the original coordinates,

$$
(Aw)_i=1
\qquad(i\in I).
$$

On a newly added coordinate $j\notin I$, however,

$$
(Aw)_j=A_{jI}w.
$$

The value depends on the coupling between the new coordinate and the old support. Dickinson can extend the same certificate through
$j$ only if

$$
A_{jI}w\geq0.
$$

Consequently,

$$
N_A(w)=I\cup\{j\notin I:A_{jI}w\geq0\}.
$$

The essential distinction is therefore

$$
\boxed{
\begin{aligned}
\text{curvature:}&\quad v^TA_Jv=v^TA_Iv,\\
\text{Dickinson:}&\quad (Aw)_j=A_{jI}w\text{ must be tested for every new }j.
\end{aligned}
}
$$

Curvature is independent of the new rows. The reach of a particular Dickinson vector is not.

#### A two-dimensional example

Take

$$
B=
\begin{pmatrix}
1&2\\
2&1
\end{pmatrix},
\qquad
v=
\begin{pmatrix}
1\\-1
\end{pmatrix}.
$$

Then

$$
v^TBv=-2<0,
$$

so curvature prunes every superset of this two-index support. Meanwhile,

$$
w=B^{-1}\mathbf1
=
\frac13
\begin{pmatrix}
1\\1
\end{pmatrix}>0,
$$

so Dickinson is on its certificate side. If a third coordinate has coupling row $A_{3I}=(1,1)$, then

$$
A_{3I}w=\frac23>0
$$

and the Dickinson certificate includes that coordinate. If instead $A_{3I}=(-1,-1)$, then

$$
A_{3I}w=-\frac23<0
$$

and the same Dickinson vector stops before that coordinate. The principal matrix $B$, its curvature, and its all-ones solution are
unchanged. Curvature still prunes both enlargements because its tangent direction is unaffected by either outside coupling.

### 17.3 The precise implication under Hadeler induction

Assume, as in the maintained increasing-cardinality argument, that every proper principal submatrix of $B$ has already been proved
copositive. Then

$$
\boxed{
H\not\succ0
\quad\Longrightarrow\quad
B^{-1}\mathbf1\not\leq0.
}
$$

Indeed, suppose $w=B^{-1}\mathbf1\leq0$. Then $z=-w\geq0$ is nonzero and

$$
z^TBz=-\mathbf1^Tz<0.
$$

Thus $B$ is non-copositive. Since all proper principal submatrices are copositive, $B$ would be a minimal non-copositive support, and
Section 13 would force $H\succ0$, a contradiction. Hence $w\not\leq0$, so the root lies on Dickinson's certificate side.

The converse is false. For $B=I_m$,

$$
B^{-1}\mathbf1=\mathbf1>0
\qquad\text{and}\qquad
H\succ0.
$$

Dickinson certifies this root while curvature cannot prune it. Therefore, under the induction assumptions:

> Dickinson has the broader local trigger, but curvature has the stronger upward inheritance.

Curvature failure guarantees a Dickinson certificate for the root support. It does **not** guarantee that this particular Dickinson
vector has $N_A(w)=[n]$.

### 17.4 From the minimal bad minimizer to the Dickinson witness

Put $B=A_M$. Let $x$ minimize $x^TBx$ over $\Delta_M$, and let its value be $\mu<0$. Minimality forces $x>0$: otherwise the same
negative value would occur on a proper support. The KKT equation is

$$
Bx=\mu\mathbf1.
$$

Since $B$ is nonsingular,

$$
w=B^{-1}\mathbf1=\frac{x}{\mu}<0.
$$

Set $z=-w>0$. Then

$$
Bz=-\mathbf1
$$

and

$$
z^TBz=-\mathbf1^Tz<0.
$$

Thus the negative interior minimizer produces exactly the Hadeler--Dickinson non-copositivity witness:

$$
\boxed{
\text{minimal negative minimizer}
\Longrightarrow
B^{-1}\mathbf1<0
\Longrightarrow
\text{negative nonnegative witness}.
}
$$

### 17.5 From the negative all-ones solution back to a stationary point

Conversely, suppose

$$
w=B^{-1}\mathbf1<0.
$$

Define

$$
z=-w>0,
\qquad
s=\mathbf1^Tz>0,
\qquad
x=\frac{z}{s}.
$$

Then $x>0$ and $\mathbf1^Tx=1$. Moreover,

$$
Bx=\frac{Bz}{s}=-\frac1s\mathbf1.
$$

Therefore $x$ is an interior stationary point with multiplier and value

$$
\mu=-\frac1s<0,
\qquad
x^TBx=\mu<0.
$$

This proves non-copositivity immediately. It does **not**, on its own, prove that $x$ is a minimum or that the reduced Hessian is
positive definite.

If every proper principal submatrix of $B$ is copositive, the missing conclusion follows. A global negative minimizer cannot lie on
the boundary, so it must be this interior stationary point. Minimality then rules out both negative and zero tangent-curvature
directions, giving

$$
H_M\succ0.
$$

Hence minimality—not the algebraic identity $w=B^{-1}\mathbf1$—is the bridge from the Dickinson witness to strict face convexity.

### 17.6 What minimality forces below the bad support

Since $H_M\succ0$, every nontrivial subface $I\subseteq M$ also has positive-definite reduced Hessian: each tangent direction on
$I$ extends by zeros to a tangent direction on $M$. Consequently, curvature pruning cannot cut through the chain of supports leading
to a genuine minimal obstruction.

Dickinson certificates generated on proper supports $I\subsetneq M$ may cover other benign supports, but they cannot cover $M$.
Because $L(u)\subseteq I\subset M$, there must be at least one missing index $j\in M\setminus I$ for which

$$
(Au)_j<0.
$$

That index lies outside $N_A(u)$ and stops the Dickinson interval before it reaches $M$.

### 17.7 Positive curvature does not determine the all-ones solution

Strict face convexity describes shape, not whether the stationary value lies above or below zero. For $m\geq2$, consider

$$
B_c=I_m+c\mathbf1\mathbf1^T.
$$

For every tangent vector $v$ with $\mathbf1^Tv=0$,

$$
v^TB_cv=\|v\|_2^2.
$$

Thus the reduced Hessian is positive definite for every real $c$. Whenever $1+cm\neq0$, however,

$$
B_c^{-1}\mathbf1=\frac1{1+cm}\mathbf1.
$$

For $c=0$ this vector is positive. For $c<-1/m$ it is negative. In particular, choosing

$$
-\frac1{m-1}\leq c<-\frac1m
$$

makes the full support non-copositive while keeping every proper principal submatrix copositive. The tangent curvature is unchanged;
only the stationary height changes.

Therefore neither

$$
H\succ0\Longrightarrow B^{-1}\mathbf1<0
$$

nor

$$
H\succ0\Longrightarrow B^{-1}\mathbf1\not\leq0
$$

is valid in general.

### 17.8 A Dickinson certificate does not determine curvature

The converse implication also fails. Compare

$$
B_1=I_m
$$

with

$$
B_2=2\mathbf1\mathbf1^T-I_m.
$$

Both matrices are strictly copositive, and both have a positive all-ones solution:

$$
B_1^{-1}\mathbf1=\mathbf1,
\qquad
B_2^{-1}\mathbf1=\frac1{2m-1}\mathbf1.
$$

But $B_1$ has positive tangent curvature, whereas for every nonzero tangent vector $v$,

$$
v^TB_2v=-\|v\|_2^2<0.
$$

Thus the same kind of Dickinson certificate can occur on a strictly convex or a strictly concave face. Dickinson's certificate
condition therefore does not determine the reduced Hessian, even under the principal-submatrix induction assumptions.

### 17.9 Identical pruning sets do not make the proofs equivalent

If strict convexity fails on $I$, curvature pruning removes

$$
[I,[n]].
$$

If Dickinson independently produces a vector satisfying

$$
L(u)=I,
\qquad
N_A(u)=[n],
$$

its interval is the identical set

$$
[I,[n]].
$$

The resulting Boolean or SAT constraint may therefore be exactly the same. The evidence is still different:

- Curvature says that every superset inherits a nonpositive tangent-curvature direction and therefore cannot be a minimal bad
  support.
- Dickinson says that every support in the interval inherits the same first-order/Farkas vector and therefore cannot be a minimal
  bad support.

There is no mathematical conversion merely because the two arguments emit the same pruning clause.

For a superset outside the interval of the current Dickinson vector, curvature still proves that the support cannot be minimal bad.
If that superset were processed separately, either it would already contain a smaller bad support, or—once all its proper supports
were known copositive—it would produce its own, possibly different, Dickinson certificate. Curvature compresses these possible future
first-order exclusions into one hereditary second-order exclusion; it does not construct their individual Dickinson vectors.

### 17.10 How to picture the two arguments

Imagine the Boolean lattice as a collection of simplex faces. A support $I$ selects one face; passing to a superset adds new
coordinate directions and places the old face on the boundary of a larger one.

A minimal non-copositive support is the smallest face whose interior can hold a negative minimum. Such a face must satisfy two
independent requirements:

1. **Balance:** its negative interior point must satisfy the first-order equation $Bx=\mu\mathbf1$ with $\mu<0$.
2. **Bowl shape:** movement in every tangent direction must curve upward, so $H\succ0$.

Dickinson and curvature test these two requirements separately.

#### Curvature as a rail already lying in the face

A tangent vector $v$ with $v^TBv\leq0$ is a flat or downward-curving rail already contained in the smaller face. Enlarging the face
does not remove that rail: the same vector, padded with zeros, remains available. A larger face containing this rail cannot be the
smallest strictly convex bowl carrying the decisive minimum.

This explains both the strength and the limitation of curvature pruning:

- it reaches every superset without examining the new rows;
- it does not say whether the current face is positive, zero, or negative;
- it only says that the decisive minimum can be represented on some other, smaller-support face.

#### Dickinson as a response vector

Think of $w$ as sending a test signal through the matrix. On the current support, the response is fixed:

$$
Bw=\mathbf1.
$$

Every newly added coordinate $j$ produces an additional response

$$
A_{jI}w.
$$

If that response is nonnegative, the same first-order certificate remains valid after adding $j$. If it is negative, this particular
certificate no longer justifies crossing into that part of the lattice. Another vector may certify the enlarged support, but the
current one does not.

This is why a Dickinson certificate has a data-dependent ceiling $N_A(w)$, whereas a curvature obstruction has the fixed ceiling
$[n]$.

#### Why the same upward block can arise twice

Sometimes all outside responses are nonnegative, so Dickinson gives $N_A(w)=[n]$. If curvature also fails on the same support, both
arguments remove exactly $[I,[n]]$. One may picture two inspectors closing the same region for different reasons:

- the curvature inspector sees a permanent geometric rail incompatible with a minimal bowl;
- the Dickinson inspector carries one response vector that remains valid at every added coordinate.

The closed region is identical, but the evidence is not. Equal pruning output does not imply that one proof was algebraically
converted into the other.

#### The useful comparison

The right comparison is therefore not simply “which theorem is stronger?” It has two axes:

- **How often does it trigger?** Under Hadeler induction, Dickinson's certificate side includes every curvature-failure case and also
  some strictly convex cases.
- **How far does one certificate reach?** A curvature failure always reaches all supersets. A particular Dickinson vector reaches
  only those supersets whose newly added coordinates have nonnegative responses.

In short: Dickinson recognizes more roots; curvature, when available, has the cleaner hereditary geometry.

### 17.11 Implication map

| Starting information | What follows | What does not follow without more assumptions |
|---|---|---|
| Negative interior face minimizer | $B^{-1}\mathbf1<0$ and a negative witness | Nothing further is missing at a minimal bad support |
| $B^{-1}\mathbf1<0$ | A negative interior stationary point and non-copositivity | $H\succ0$ |
| $B^{-1}\mathbf1<0$ plus minimality | The stationary point is the minimal negative minimizer and $H\succ0$ | — |
| $H\succ0$ | Strict convexity on the face | The sign of $B^{-1}\mathbf1$ or of the face minimum |
| $H\not\succ0$ under induction | Curvature pruning of the upward cone and $B^{-1}\mathbf1\not\leq0$ at the root | $N_A(w)=[n]$ or an individual CP decision for every pruned support |
| Dickinson vector $u$ | Pruning of $[L(u),N_A(u)]$ | Any fixed sign of the reduced Hessian |

In short, the two tests are first-order and second-order projections of the same minimal-support KKT structure. Curvature determines
**where a minimal obstruction is geometrically capable of living**. Dickinson determines whether a first-order certificate excludes
a support, or whether the all-ones solve has instead exposed a negative witness. The curvature trigger is narrower, but its zero-
extension is automatically valid on every superset; the Dickinson trigger is broader, but the reach of one vector is limited by its
outside products.

## 18. The singular case

If $B$ is singular, $B^{-1}\mathbf1$ does not exist. The two copositivity questions then separate.

For ordinary copositivity, a principal matrix whose proper principal submatrices are copositive cannot be a first non-copositive
support while singular. Hadeler's Theorem 2 shows that a first non-copositive support must be nonsingular.

For strict copositivity, a singular matrix can fail only through a zero. Under the corresponding minimality assumptions, there is a
vector

$$
z>0,
\qquad
Bz=0,
$$

so

$$
z^TBz=0.
$$

This proves that $B$ is not strictly copositive, although it may remain ordinarily copositive. Hadeler's Theorem 3 is the strict
version of this statement.

Dickinson orients a nonzero kernel vector so that it has a positive component and uses $Bu=0\geq0$ in the same interval-certificate
construction. A nonnegative kernel vector is additionally an explicit zero witness for the strict question.

## 19. Examples

### 19.1 A flat face prunes upward

Let $B=J_m$. Every tangent vector satisfies

$$
v^TBv=(\mathbf 1^Tv)^2=0.
$$

The objective is constant on the simplex face. Every interior minimizer can be moved to the boundary with the same value, so the
full face is never needed as a minimal minimizer support.

Here the constant value is $1$, so $J_m$ is strictly copositive. Upward pruning does not establish that positivity; it merely shows
that the same global value is already represented on smaller supports.

### 19.2 Positive definiteness prunes downward

Let $B$ be a positive diagonal matrix:

$$
B=\operatorname{diag}(d_1,\ldots,d_m),
\qquad d_i>0.
$$

Then $B\succ0$, so every nonzero vector supported on $I$ or any subset of $I$ has positive quadratic value. The single definiteness
test certifies the entire downward cone and, if $I=[n]$, proves that the whole matrix is strictly copositive.

## 20. The two cones in one picture

For a tested support $I$:

$$
\boxed{\text{reduced Hessian not positive definite}}
\quad\Longrightarrow\quad
\boxed{\text{remove }\uparrow I}
$$

because no support in that upward cone can be the support of the chosen minimal-support global minimizer.

By contrast,

$$
\boxed{A_I\text{ positive definite}}
\quad\Longrightarrow\quad
\boxed{\text{remove }\downarrow I}
$$

because every nonzero vector supported in that downward cone has strictly positive value.

These conclusions are not interchangeable:

| Test on $I$ | What it proves | Copositivity consequence | Pruning direction |
|---|---|---|---|
| $Z_I^TA_IZ_I$ is not positive definite | $I$ and its supersets are unnecessary as minimal global-minimizer supports | The sign of $\mu(A)$ remains represented outside this cone | Upward |
| $A_I$ is positive definite | Every nonzero vector supported in $I$ or a subset of $I$ has positive value | No negative witness or zero occurs there | Downward for CP and SCP |
| $A_I$ is positive semidefinite | Every vector supported in $I$ or a subset has nonnegative value | No negative witness occurs there; zeros may remain | Downward for ordinary CP only |
| $Z_I^TA_IZ_I$ is positive definite | A face minimizer, if it exists, is unique on that face | Its sign must still be computed, and other faces may be lower | None from curvature alone |

## 21. What remains unresolved after these tests

If the reduced Hessian is positive definite while $A_I$ is indefinite, neither cone rule decides the support:

- the support remains a possible location of the minimal-support global minimizer;
- the quadratic value there may be positive, zero, or negative;
- and the KKT candidate and its outside inequalities may still need to be calculated.

This is exactly the nonsingular case with one negative eigenvalue and $\delta<0$. In the singular positive-semidefinite case with a
one-dimensional kernel transverse to the tangent space, downward pruning already certifies ordinary copositivity on the face, but
strict copositivity still requires checking whether that kernel contains a nonzero nonnegative vector.

Thus convexity pruning can remove enormous parts of the support lattice, but it cannot by itself replace the remaining copositivity
certificate machinery.

For an ordinary copositivity proof, every support that could still contain a negative value must eventually be eliminated by a
nonnegativity certificate or shown unnecessary as a carrier of $\mu(A)$. For a strict-copositivity proof, zeros must be excluded as
well. Conversely, finding one exact negative point settles non-copositivity immediately, while finding one exact zero settles only
the strict question.

## 22. Final mental model

The entire paper can be compressed into four objects and two directions:

| Object | Intuitive question | Exact role |
|---|---|---|
| The simplex face $\Delta_I$ | Which coordinates are currently allowed to carry mass? | It identifies one support in the Boolean lattice |
| The reduced Hessian $H_I$ | Is this face a bowl, flat, or curved downward in feasible directions? | It decides whether the support can be a minimum-support carrier |
| The all-ones solution $w=A_I^{-1}\mathbf1$ | Where is the affine stationary balance, and on which side of zero does it lie? | Under Hadeler induction it gives either a witness or a certificate |
| The Dickinson pair $[L(u),N_A(u)]$ | Across which other faces does the same first-order vector remain valid? | It removes one Boolean interval |

The two hereditary facts are:

$$
\boxed{\text{safe on a large support}\Longrightarrow\text{safe on every smaller support}}
$$

and

$$
\boxed{\text{not strictly convex on a small support}\Longrightarrow
       \text{not strictly convex on every larger support}.}
$$

The first is a value statement and prunes downward. The second is a location statement and prunes upward.

The two KKT statements are:

$$
\boxed{\text{face stationarity + convexity + nonnegative value}
       \Longrightarrow\text{downward pruning}}
$$

and

$$
\boxed{\text{full KKT inequalities + nonnegative value}
       \Longrightarrow\text{upward Dickinson pruning}.}
$$

The full-simplex outside inequalities are what distinguish a full KKT point from a merely face-stationary point. They are also what
make every unused coordinate enter $N_A(u)$.

Finally, Hadeler and Dickinson do not replace curvature, and curvature does not replace them:

- curvature tells us that a decisive minimum can be represented elsewhere;
- positive semidefiniteness tells us that no negative value exists below a support;
- Hadeler detects the first nonsingular negative support through the all-ones solve;
- Dickinson transports one valid first-order certificate across an interval of supports.

The algorithms become understandable once these roles are kept separate. The same SAT clause may occasionally arise from two
different proofs, but the reason it is sound—and therefore the situations in which it can be reused—depends on which proof produced
it.

## 23. References

- K. P. Hadeler, “On copositive matrices,” *Linear Algebra and its Applications* 49 (1983), 79–89.
  [DOI 10.1016/0024-3795(83)90095-2](https://doi.org/10.1016/0024-3795(83)90095-2)
- Peter J. C. Dickinson, “A new certificate for copositivity,” *Linear Algebra and its Applications* 569 (2019), 15–37.
  [DOI 10.1016/j.laa.2018.12.025](https://doi.org/10.1016/j.laa.2018.12.025)
- Immanuel M. Bomze, “Detecting all evolutionarily stable strategies,” *Journal of Optimization Theory and Applications* 75
  (1992), 313–329. [DOI 10.1007/BF00941470](https://doi.org/10.1007/BF00941470)
- Andrea Scozzari and Fabio Tardella, “A clique algorithm for standard quadratic programming,” *Discrete Applied Mathematics* 156
  (2008), 2439–2448. [DOI 10.1016/j.dam.2007.09.020](https://doi.org/10.1016/j.dam.2007.09.020)
- S.-P. Han and O. Fujiwara, “An inertia theorem for symmetric matrices and its application to nonlinear programming,” *Linear
  Algebra and its Applications* 72 (1985), 47–58.
  [DOI 10.1016/0024-3795(85)90141-7](https://doi.org/10.1016/0024-3795(85)90141-7)
- Roger A. Horn and Charles R. Johnson, *Matrix Analysis*, second edition, Chapter 7, Cambridge University Press, 2013.
  [Chapter page](https://www.cambridge.org/highereducation/books/matrix-analysis/FDA3627DC2B9F5C3DF2FD8C3CC136B48/positive-definite-and-semidefinite-matrices/0E9DB201B29FC39A7174E5C9B666DD62)
