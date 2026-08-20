# Hadeler and Dickinson: Copositivity, the All-Ones Solve, and Face Curvature

## Purpose

This note separates three ideas that are easy to mix together:

1. the Hadeler copositivity test obtained from solving a principal system with right-hand side $\mathbf1$;
2. the Dickinson reuse of one such solution as a certificate for many supports; and
3. the reduced Hessian, which describes convexity on a simplex face.

The all-ones solve is primarily a first-order and duality argument. The reduced Hessian is a second-order curvature object. They are
connected at a minimal failing support, but they are not the same test.

## 1. Setup and the indispensable assumption

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

## 2. The correct all-ones copositivity criterion

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

### 2.1 Why a nonpositive solution proves failure

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

### 2.2 Why every minimal failure produces a negative solution

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

## 3. What $B^{-1}\mathbf1$ means geometrically

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

## 4. The second-order object: the reduced Hessian

The unrestricted Hessian of $q_B(x)=x^TBx$ is

$$
2B.
$$

On the simplex face, feasible displacements satisfy

$$
\mathbf1^Tv=0.
$$

Let the columns of $Z\in\mathbb R^{m\times(m-1)}$ form a basis of this tangent space. The reduced Hessian, with the irrelevant factor
$2$ omitted, is

$$
H=Z^TBZ.
$$

It classifies the stationary point only in feasible tangent directions:

| Reduced Hessian | Meaning on the simplex face |
|---|---|
| $H\succ0$ | The face is strictly convex; a feasible stationary point is its unique minimizer |
| $H\succeq0$ but singular | The face is convex but has a flat direction |
| $H$ has a negative eigenvalue | A feasible interior stationary point is not a local minimum |
| $H\prec0$ | The face is strictly concave; a feasible stationary point is its unique maximizer |

This is a second-order statement. In contrast, the sign pattern of $w=B^{-1}\mathbf1$ is the Hadeler first-order/duality test.

## 5. The exact connection at a minimal failing support

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

## 6. Copositivity does not imply convexity

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

## 7. Do we need the reduced Hessian for the Hadeler test?

No. Under the increasing-cardinality assumption, the all-ones sign test is already complete for the current nonsingular principal
matrix:

- $w\leq0$ gives a negative witness;
- $w\not\leq0$ rules out this support as a first copositivity failure.

The reduced Hessian answers a different question:

> Can this support be the smallest support of a global simplex minimizer?

If $H\not\succ0$, the support and all its supersets can be removed from that minimizer-support search. If $H\succ0$, curvature alone
does not decide the sign of the minimum, so the all-ones solution or another copositivity certificate remains necessary.

The same exact factorization of $B$ can support both tests. It gives the inertia of $B$ and solves $Bw=\mathbf1$. As explained in the
companion convexity note, the inertia and the scalar $\delta=\mathbf1^Tw$ decide whether $H\succ0$ without explicitly constructing
$Z^TBZ$.

## 8. What Dickinson adds

Hadeler's criterion decides the current principal support. Dickinson reuses the same vector across many supports.

Embed $w$ into $\mathbb R^n$ by inserting zeros outside $I$; call the embedded vector $u$. When $w\not\leq0$, define

$$
L(u)=\operatorname{supp}(u)
$$

and

$$
U(u)=\{j\in[n]:(Au)_j\geq0\}.
$$

Because

$$
(Au)_I=Bw=\mathbf1>0,
$$

we have $I\subseteq U(u)$. Every support $J$ satisfying

$$
L(u)\subseteq J\subseteq U(u)
$$

inherits a vector $u_J$ with

$$
u_J\not\leq0
\qquad\text{and}\qquad
A_Ju_J\geq0.
$$

By the Hadeler--Farkas alternative, such a $J$ cannot be a minimal support on which copositivity fails. Thus one all-ones solve
certifies the complete Boolean interval $[L(u),U(u)]$.

If these intervals cover every nonempty support, no minimal failing support exists and $A$ is copositive.

This is Dickinson's additional idea. It is not a convexity argument:

- the Dickinson interval comes from the sign pattern of $u$ and $Au$;
- the curvature closure comes from the definiteness of $Z^TA_IZ$;
- either certificate may exist without the other.

*Literature.* The interval certificate is Theorem 4.6 of Dickinson's 2019 paper. Its proof combines Hadeler's principal-submatrix
criterion with Farkas' lemma.

## 9. Passing between curvature and Dickinson—and where this fails

This section restricts attention to a nonsingular principal matrix $B=A_I$. The singular case is treated separately in Section 10.
The curvature and Dickinson arguments are connected exactly: they examine complementary blocks of the same quadratic form. Their
different pruning ranges arise from what happens when the support is enlarged.

### 9.1 Their common target

Let $M\subseteq[n]$ be inclusion-minimal among the non-copositive principal supports:

$$
A_M\notin\operatorname{COP},
\qquad
A_J\in\operatorname{COP}
\quad\text{for every }J\subsetneq M.
$$

Both pruning arguments rule out possible locations of $M$:

- If the reduced Hessian on $I$ is not positive definite, then neither $I$ nor any $J\supseteq I$ can equal $M$.
- If a Dickinson vector certifies $[L(u),U(u)]$, then no support in that Boolean interval can equal $M$.

Neither statement, by itself, says that every pruned principal matrix is copositive. Both preserve the decision because a
non-copositive matrix must possess at least one inclusion-minimal bad support.

### 9.2 The exact algebraic connection

Solve

$$
Bw=\mathbf1
$$

and let

$$
\mathcal T_I=\{v\in\mathbb R^{|I|}:\mathbf1^Tv=0\}
$$

be the tangent space of the simplex face. For every $v\in\mathcal T_I$,

$$
v^TBw=v^T\mathbf1=0.
$$

Thus the all-ones direction $w=B^{-1}\mathbf1$ is $B$-orthogonal to every tangent direction. If the columns of $Z$ form a basis of
$\mathcal T_I$ and

$$
\delta=\mathbf1^Tw\neq0,
$$

then $Q=[Z,w]$ is nonsingular and

$$
Q^TBQ
=
\begin{pmatrix}
Z^TBZ & 0\\
0 & \delta
\end{pmatrix}
=
\begin{pmatrix}
H & 0\\
0 & \delta
\end{pmatrix}.
$$

The same matrix therefore splits into two complementary parts:

- $H$ describes second-order motion within the simplex face;
- $w$ and $\delta$ describe the stationary direction transverse to the tangent space.

If $\delta=0$, then $w$ itself lies in $\mathcal T_I$ and

$$
w^TBw=w^T\mathbf1=0.
$$

In that case the reduced Hessian is automatically not positive definite. This block decomposition is the fundamental reason that
the two tests repeatedly meet at the same supports; the connection is not accidental.

### 9.3 Why their upward extensions differ

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
U(w)=I\cup\{j\notin I:A_{jI}w\geq0\}.
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

### 9.4 The precise implication under Hadeler induction

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
Section 5 would force $H\succ0$, a contradiction. Hence $w\not\leq0$, so the root lies on Dickinson's certificate side.

The converse is false. For $B=I_m$,

$$
B^{-1}\mathbf1=\mathbf1>0
\qquad\text{and}\qquad
H\succ0.
$$

Dickinson certifies this root while curvature cannot prune it. Therefore, under the induction assumptions:

> Dickinson has the broader local trigger, but curvature has the stronger upward inheritance.

Curvature failure guarantees a Dickinson certificate for the root support. It does **not** guarantee that this particular Dickinson
vector has $U(w)=[n]$.

### 9.5 From the minimal bad minimizer to the Dickinson witness

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

### 9.6 From the negative all-ones solution back to a stationary point

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

### 9.7 What minimality forces below the bad support

Since $H_M\succ0$, every nontrivial subface $I\subseteq M$ also has positive-definite reduced Hessian: each tangent direction on
$I$ extends by zeros to a tangent direction on $M$. Consequently, curvature pruning cannot cut through the chain of supports leading
to a genuine minimal obstruction.

Dickinson certificates generated on proper supports $I\subsetneq M$ may cover other benign supports, but they cannot cover $M$.
Because $L(u)\subseteq I\subset M$, there must be at least one missing index $j\in M\setminus I$ for which

$$
(Au)_j<0.
$$

That index lies outside $U(u)$ and stops the Dickinson interval before it reaches $M$.

### 9.8 Positive curvature does not determine the all-ones solution

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

### 9.9 A Dickinson certificate does not determine curvature

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

### 9.10 Identical pruning sets do not make the proofs equivalent

If strict convexity fails on $I$, curvature pruning removes

$$
[I,[n]].
$$

If Dickinson independently produces a vector satisfying

$$
L(u)=I,
\qquad
U(u)=[n],
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

### 9.11 How to picture the two arguments

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

This is why a Dickinson certificate has a data-dependent ceiling $U(w)$, whereas a curvature obstruction has the fixed ceiling
$[n]$.

#### Why the same upward block can arise twice

Sometimes all outside responses are nonnegative, so Dickinson gives $U(w)=[n]$. If curvature also fails on the same support, both
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

### 9.12 Implication map

| Starting information | What follows | What does not follow without more assumptions |
|---|---|---|
| Negative interior face minimizer | $B^{-1}\mathbf1<0$ and a negative witness | Nothing further is missing at a minimal bad support |
| $B^{-1}\mathbf1<0$ | A negative interior stationary point and non-copositivity | $H\succ0$ |
| $B^{-1}\mathbf1<0$ plus minimality | The stationary point is the minimal negative minimizer and $H\succ0$ | — |
| $H\succ0$ | Strict convexity on the face | The sign of $B^{-1}\mathbf1$ or of the face minimum |
| $H\not\succ0$ under induction | Curvature pruning of the upward cone and $B^{-1}\mathbf1\not\leq0$ at the root | $U(w)=[n]$ or an individual CP decision for every pruned support |
| Dickinson vector $u$ | Pruning of $[L(u),U(u)]$ | Any fixed sign of the reduced Hessian |

In short, the two tests are first-order and second-order projections of the same minimal-support KKT structure. Curvature determines
**where a minimal obstruction is geometrically capable of living**. Dickinson determines whether a first-order certificate excludes
a support, or whether the all-ones solve has instead exposed a negative witness. The curvature trigger is narrower, but its zero-
extension is automatically valid on every superset; the Dickinson trigger is broader, but the reach of one vector is limited by its
outside products.

## 10. The singular case

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

## 11. Summary

The all-ones solve and the reduced Hessian answer different questions:

| Object | Question answered |
|---|---|
| $w=B^{-1}\mathbf1$ | Does the current support provide a negative witness, or a Hadeler--Farkas copositivity certificate? |
| $\delta=\mathbf1^Tw$ | What is the value $1/\delta$ of the normalized affine stationary point? |
| $H=Z^TBZ$ | Is the quadratic form convex along feasible zero-sum directions on this face? |
| Dickinson interval $[L(u),U(u)]$ | For how many other supports can the same first-order certificate be reused? |

The most important logical distinction is

$$
B\text{ copositive}
\quad\Longleftrightarrow\quad
B^{-1}\mathbf1\not\leq0,
$$

under the Hadeler principal-submatrix assumption. This means “at least one positive coordinate,” not “all coordinates are
nonnegative.” No convexity conclusion follows on the copositive branch. Convexity becomes forced only at a minimal failing support,
where the all-ones solution is negative, the normalized stationary point is an interior negative minimum, and the reduced Hessian is
positive definite.

## References

- K. P. Hadeler, “On copositive matrices,” *Linear Algebra and its Applications* 49 (1983), 79–89.
  [DOI 10.1016/0024-3795(83)90095-2](https://doi.org/10.1016/0024-3795(83)90095-2)
- Peter J. C. Dickinson, “A new certificate for copositivity,” *Linear Algebra and its Applications* 569 (2019), 15–37.
  [DOI 10.1016/j.laa.2018.12.025](https://doi.org/10.1016/j.laa.2018.12.025)
- Andrea Scozzari and Fabio Tardella, “A clique algorithm for standard quadratic programming,” *Discrete Applied Mathematics* 156
  (2008), 2439–2448. [DOI 10.1016/j.dam.2007.09.020](https://doi.org/10.1016/j.dam.2007.09.020)
