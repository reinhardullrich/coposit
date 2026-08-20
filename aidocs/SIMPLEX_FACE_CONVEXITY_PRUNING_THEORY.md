# Convexity Pruning for Copositivity on Faces of the Simplex

## Purpose

This note explains two exact pruning rules for deciding copositivity by minimizing a quadratic form on the standard simplex:

1. **Upward pruning:** if the quadratic form is not strictly convex on a face, that face and every larger face containing it can be
   excluded as the support of a minimal-support global minimizer.
2. **Downward pruning:** if a principal matrix is positive definite, that face and every smaller nonempty face contained in it have
   strictly positive quadratic value.

The sign of the global simplex minimum decides whether the matrix is copositive or strictly copositive. The two rules preserve, or
directly certify, that sign while removing supports from the search. They point in opposite directions in the Boolean lattice of
supports, but they have different hypotheses and different meanings. Strict convexity on a simplex face is not the same as positive
definiteness of the corresponding principal matrix.

The proofs use standard quadratic-optimization language, but the purpose here is specifically copositivity. The presentation is
independent of SAT, Dickinson certificates, or any particular implementation.

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

Write

$$
[n]=\{1,2,\ldots,n\}.
$$

For a vector $x$, its support is

$$
\operatorname{supp}(x)=\{i\in[n]:x_i>0\}.
$$

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
| $A_I$ nonsingular with exactly one negative eigenvalue | $H_I$ may be positive definite; Section 8 gives the additional scalar test |

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

Section 8 proves the last two tests. Thus an implementation can reuse the factorization of $A_I$ and need not build or factor
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

## 8. Exact tests from the inertia of a principal matrix

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

### 8.1 Nonsingular case

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

### 8.2 Singular case

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

## 9. KKT points and the curvature rule

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

## 10. Examples

### 10.1 A flat face prunes upward

Let $B=J_m$. Every tangent vector satisfies

$$
v^TBv=(\mathbf 1^Tv)^2=0.
$$

The objective is constant on the simplex face. Every interior minimizer can be moved to the boundary with the same value, so the
full face is never needed as a minimal minimizer support.

Here the constant value is $1$, so $J_m$ is strictly copositive. Upward pruning does not establish that positivity; it merely shows
that the same global value is already represented on smaller supports.

### 10.2 Positive definiteness prunes downward

Let $B$ be a positive diagonal matrix:

$$
B=\operatorname{diag}(d_1,\ldots,d_m),
\qquad d_i>0.
$$

Then $B\succ0$, so every nonzero vector supported on $I$ or any subset of $I$ has positive quadratic value. The single definiteness
test certifies the entire downward cone and, if $I=[n]$, proves that the whole matrix is strictly copositive.

## 11. The two cones in one picture

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

## 12. What remains unresolved after these tests

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

## 13. Literature and provenance

The key literature result is that a standard quadratic program has a global minimizer in the relative interior of a face on which
the objective is strictly convex:

- Andrea Scozzari and Fabio Tardella, “A clique algorithm for standard quadratic programming,” *Discrete Applied Mathematics* 156
  (2008), 2439–2448, Theorem 1. [DOI 10.1016/j.dam.2007.09.020](https://doi.org/10.1016/j.dam.2007.09.020)

The inertia characterization of a symmetric matrix restricted to a linear constraint space is a standard constrained-inertia
result; a directly relevant reference is:

- S.-P. Han and O. Fujiwara, “An inertia theorem for symmetric matrices and its application to nonlinear programming,” *Linear
  Algebra and its Applications* 72 (1985), 47–58.
  [DOI 10.1016/0024-3795(85)90141-7](https://doi.org/10.1016/0024-3795(85)90141-7)

For positive-definite and positive-semidefinite matrices and their principal submatrices, see:

- Roger A. Horn and Charles R. Johnson, *Matrix Analysis*, second edition, Chapter 7, Cambridge University Press, 2013.
  [Chapter page](https://www.cambridge.org/highereducation/books/matrix-analysis/FDA3627DC2B9F5C3DF2FD8C3CC136B48/positive-definite-and-semidefinite-matrices/0E9DB201B29FC39A7174E5C9B666DD62)

The upward Boolean-lattice rule is the immediate hereditary corollary proved in Section 5: a non-positive tangent direction on a
face remains present on every larger face. The downward rule is the classical inheritance of positive definiteness by principal
submatrices.
