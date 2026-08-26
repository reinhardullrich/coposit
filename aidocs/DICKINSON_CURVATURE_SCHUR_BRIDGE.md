# Dickinson Intervals And Curvature: An Exact Schur-Complement Bridge

## Status and purpose

This is a mathematical research note about the connection between Dickinson intervals and curvature pruning on simplex faces. It
answers three questions:

1. What does a Dickinson interval imply about positive definiteness and simplex-tangent curvature?
2. What does curvature information imply about the next Dickinson calculation?
3. Which quantities can be reused algorithmically instead of treating the two methods as unrelated tests?

The central answer is:

> A Dickinson interval by itself contains too little information to determine curvature. It records only signs of matrix-vector
> products. If the factorization and numerical response that generated the interval are retained, however, ordinary definiteness,
> simplex-tangent curvature, and the next all-ones Dickinson solve are all controlled by the same Schur-complement data.

The cleanest theorem concerns the nonsingular all-ones branch

$$
A_Iw=\mathbf 1.
$$

This is the traditional Dickinson construction. A later section explains what remains valid for a general Dickinson vector created
by Halfspace--Rays widening. Singular roots require separate kernel and generalized-Schur formulas and are deliberately not hidden
inside the nonsingular notation.

The underlying certificate theorem is Peter J. C. Dickinson,
[*A New Certificate for Copositivity*](../research/papers/Dickinson_2019_new_certificate_for_copositivity.pdf), *Linear Algebra and
its Applications* 569 (2019), 15--37. The Schur-complement identities below are standard linear algebra; their use as a two-way
bridge between Dickinson intervals and coposit's curvature frontiers is the subject of this note.

## Part I. Formal mathematics

## 1. Basic objects

### 1.1 Matrix, supports, and principal matrices

Let

$$
A\in\mathbb R^{n\times n}
$$

be symmetric. Write

$$
[n]=\{1,\ldots,n\}.
$$

A **support** is a nonempty set $I\subseteq[n]$. Its cardinality is denoted by $|I|$. The principal matrix selected by $I$ is

$$
A_I=A[I,I].
$$

If $I$ and $R$ are disjoint, then

$$
A_{I,R}=A[I,R]
$$

denotes the rectangular block with rows in $I$ and columns in $R$.

The simplex face belonging to $I$ is

$$
\Delta_I=
\left\{
x\in\mathbb R^{|I|}:x\geq0,
\ \mathbf1_I^Tx=1
\right\},
$$

where $\mathbf1_I$ is the all-ones vector of length $|I|$.

### 1.2 Tangent space and reduced Hessian

The linear tangent space of $\Delta_I$ is

$$
\mathcal T_I=
\left\{
d\in\mathbb R^{|I|}:\mathbf1_I^Td=0
\right\}.
$$

Choose any full-column-rank matrix

$$
Z_I\in\mathbb R^{|I|\times(|I|-1)}
$$

whose columns span $\mathcal T_I$. The **reduced Hessian** on the face is

$$
H_I=Z_I^TA_IZ_I.
$$

The particular basis $Z_I$ does not matter for definiteness. A different tangent basis changes $H_I$ by congruence and therefore
preserves its inertia.

The face has strictly positive curvature precisely when

$$
H_I\succ0.
$$

If $H_I\not\succ0$, there is a nonzero tangent direction $d\in\mathcal T_I$ with

$$
d^TA_Id\leq0.
$$

Padding $d$ by zeros preserves this direction in every larger face. Therefore $H_I\not\succ0$ gives **upward curvature pruning**:
every support containing $I$ can be excluded as the first support carrying a negative minimum.

If instead

$$
A_I\succ0,
$$

then every principal submatrix inside $I$ is positive definite. This gives **downward pruning** of every nonempty subset of $I$.

### 1.3 The three nonsingular curvature regions

For this note, a nonsingular support $I$ is placed in one of three regions:

1. **Downward-pruning region:** $A_I\succ0$.
2. **Middle region:** $H_I\succ0$ but $A_I\not\succ0$.
3. **Upward-pruning region:** $H_I\not\succ0$.

The middle region exists because strict convexity on the simplex tangent space is weaker than positive definiteness in the complete
coordinate space. A matrix may curve upward in every direction whose coordinates sum to zero while having one negative direction
transverse to the simplex.

This three-way division describes the nonsingular strict-curvature core. Singular positive-semidefinite downward certificates need
additional consistency conditions and are discussed separately in the existing curvature note.

## 2. Dickinson certificates and their intervals

For $u\in\mathbb R^n$, define its lower support

$$
L(u)=\operatorname{supp}(u)=\{i\in[n]:u_i\neq0\}
$$

and its nonnegative-response set

$$
N_A(u)=\{j\in[n]:(Au)_j\geq0\}.
$$

Suppose

$$
u\notin-\mathbb R_+^n
\qquad\text{and}\qquad
L(u)\subseteq N_A(u).
$$

The first condition says that $u$ has at least one positive coordinate. Under these conditions, Dickinson's theorem certifies every
support in the Boolean interval

$$
[L(u),N_A(u)]
=
\{J:L(u)\subseteq J\subseteq N_A(u)\}.
$$

The interval records two kinds of information only:

- whether a coordinate of $u$ is zero; and
- whether a coordinate of $Au$ is negative or nonnegative.

It does **not** record the magnitudes of $u$ or $Au$, and it does not record a matrix factorization. This loss of numerical
information is why the bare set-theoretic interval cannot determine curvature.

### 2.1 The traditional all-ones vector

Let $I\subseteq[n]$ be the support currently processed and assume that

$$
B=A_I
$$

is nonsingular. The traditional vector is the unique solution of

$$
Bw=\mathbf1_I.
$$

Embed $w$ into $\mathbb R^n$ by putting zeros outside $I$. Then

$$
(Aw)_i=1
\qquad(i\in I),
$$

so $I\subseteq N_A(w)$. If $w$ has full support, then $L(w)=I$, and the complete interval is

$$
[I,N_A(w)].
$$

If some coordinates of $w$ are zero, then $L(w)\subsetneq I$. The main theorem below describes the upper part of the interval,
namely supports of the form $I\cup R$. The additional faces between $L(w)$ and $I$ require either contraction to the true lower
support or the general tangent-residual construction in Section 8.

## 3. All quantities used by the Schur bridge

Fix a nonsingular processed support $I$. Let

$$
F\subseteq[n]\setminus I
$$

be a set of optional indices. The most important choice is

$$
F=N_A(w)\setminus I,
$$

the indices that the traditional Dickinson vector allows us to add. The theorem is valid for any $F$ disjoint from $I$.

Write

$$
k=|I|,
\qquad
m=|F|.
$$

Partition the principal matrix on $I\cup F$ as

$$
A_{I\cup F}
=
\begin{pmatrix}
B&E\\
E^T&C
\end{pmatrix},
$$

where

$$
B=A_I\in\mathbb R^{k\times k},
\qquad
E=A_{I,F}\in\mathbb R^{k\times m},
\qquad
C=A_F\in\mathbb R^{m\times m}.
$$

The following objects are computed from the retained factorization of $B$.

### 3.1 The root all-ones solution

Define

$$
w=B^{-1}\mathbf1_I\in\mathbb R^k
$$

and

$$
\delta=\mathbf1_I^Tw\in\mathbb R.
$$

The scalar $\delta$ is the sum of the root Dickinson vector. When $H_I\succ0$ and $B$ is nonsingular, $\delta\neq0$.

### 3.2 Responses of the optional indices

Define

$$
Y=B^{-1}E\in\mathbb R^{k\times m}.
$$

Column $j$ of $Y$ is the solution of the coordinate right-hand-side system associated with optional index $j$.

The Dickinson responses of all optional indices are

$$
\rho=E^Tw\in\mathbb R^m.
$$

Because $B$ is symmetric,

$$
\rho=Y^T\mathbf1_I.
$$

For an optional index $j\in F$, the scalar $\rho_j$ is exactly

$$
\rho_j=(Aw)_j=A_{j,I}w.
$$

Therefore $j$ lies in the traditional Dickinson upper endpoint exactly when

$$
\rho_j\geq0.
$$

### 3.3 Deficit from the simplex-normal value

Define

$$
g=\mathbf1_F-\rho\in\mathbb R^m.
$$

Thus

$$
g_j=1-\rho_j.
$$

Dickinson interval membership compares $\rho_j$ with zero. Curvature depends on the discrepancy between $\rho_j$ and one. This
difference between the thresholds $0$ and $1$ is one reason interval membership alone cannot decide curvature.

### 3.4 Ordinary Schur matrix

Define

$$
S=C-E^TY
=
C-E^TB^{-1}E
\in\mathbb R^{m\times m}.
$$

This is the ordinary Schur complement of $B$ in $A_{I\cup F}$. It describes the quadratic form on the optional coordinates after the
unconstrained old coordinates in $I$ have been eliminated.

### 3.5 Tangent-curvature Schur matrix

Assume from now on that

$$
H_I\succ0.
$$

Then $\delta\neq0$, and we define

$$
K=S+\frac{gg^T}{\delta}
\in\mathbb R^{m\times m}.
$$

The matrix $K$ is the ordinary Schur matrix plus one rank-one correction. This correction enforces the simplex tangent condition:
every direction must have coordinate sum zero.

### 3.6 What $S_R$ and $K_R$ mean

Let

$$
R\subseteq F
$$

be any selection of optional indices, and define the enlarged support

$$
J_R=I\cup R.
$$

Then

$$
S_R=S[R,R]
$$

means the principal submatrix of the already constructed matrix $S$ whose rows and columns are indexed by $R$. Similarly,

$$
K_R=K[R,R].
$$

The notation does not mean a new unrelated matrix. It means: construct $S$ and $K$ once on the complete optional set $F$, and then
select their principal submatrices.

Equivalently, with

$$
E_R=A_{I,R},
\qquad
C_R=A_R,
\qquad
Y_R=B^{-1}E_R,
\qquad
\rho_R=E_R^Tw,
\qquad
g_R=\mathbf1_R-\rho_R,
$$

we have

$$
S_R=C_R-E_R^TB^{-1}E_R
$$

and

$$
K_R=S_R+\frac{g_Rg_R^T}{\delta}.
$$

Thus every support $J_R$ inside the optional cube is represented by two smaller principal matrices indexed only by $R$.

### 3.7 Notation summary

| Symbol | Size or type | Meaning |
|---|---|---|
| $I$ | subset of $[n]$ | Processed root support. |
| $F$ | subset of $[n]\setminus I$ | Complete set of optional indices being inspected. |
| $R$ | subset of $F$ | One particular selection of optional indices. |
| $J_R=I\cup R$ | subset of $[n]$ | Enlarged support represented by $R$. |
| $B=A_I$ | $k\times k$ | Root principal matrix. |
| $E=A_{I,F}$ | $k\times m$ | Couplings between root and optional indices. |
| $C=A_F$ | $m\times m$ | Principal matrix on all optional indices. |
| $w=B^{-1}\mathbf1_I$ | $k$-vector | Traditional root Dickinson vector. |
| $\delta=\mathbf1_I^Tw$ | scalar | Sum of the root Dickinson vector. |
| $Y=B^{-1}E$ | $k\times m$ | Root solves for every optional coordinate block. |
| $\rho=E^Tw$ | $m$-vector | Dickinson responses on the optional indices. |
| $g=\mathbf1_F-\rho$ | $m$-vector | Deficit between those responses and the all-ones target. |
| $S=C-E^TB^{-1}E$ | $m\times m$ | Ordinary Schur matrix controlling principal-matrix definiteness. |
| $K=S+gg^T/\delta$ | $m\times m$ | Tangent Schur matrix controlling simplex curvature. |
| $S_R=S[R,R]$ | $|R|\times|R|$ | Principal submatrix of $S$ for the chosen extension $R$. |
| $K_R=K[R,R]$ | $|R|\times|R|$ | Principal submatrix of $K$ for the chosen extension $R$. |

### Lemma 3.8: the sign of $\delta$ identifies the root region

Assume that $B$ is nonsingular and $H_I\succ0$. Then $\delta\neq0$, and

$$
B\cong H_I\oplus[\delta].
$$

Consequently,

$$
\delta>0
\quad\Longleftrightarrow\quad
B\succ0,
$$

whereas $\delta<0$ means that $B$ has exactly one negative direction and lies in the middle region.

#### Proof

The all-ones solution satisfies

$$
Bw=\mathbf1_I.
$$

For every tangent-basis column of $Z_I$,

$$
Z_I^TBw=Z_I^T\mathbf1_I=0,
$$

while

$$
w^TBw=w^T\mathbf1_I=\delta.
$$

If $\delta=0$, then $w$ would be a nonzero tangent vector with $w^TBw=0$, contradicting $H_I\succ0$. Hence $\delta\neq0$, the
columns of $[Z_I,w]$ form a basis of $\mathbb R^{|I|}$, and

$$
\begin{pmatrix}
Z_I^T\\
w^T
\end{pmatrix}
B
\begin{pmatrix}
Z_I&w
\end{pmatrix}
=
\begin{pmatrix}
H_I&0\\
0&\delta
\end{pmatrix}.
$$

The inertia conclusions follow immediately. $\square$

## 4. The two exact congruences

### Theorem 4.1: ordinary definiteness is governed by $S_R$

For every $R\subseteq F$,

$$
A_{J_R}
\cong
B\oplus S_R,
$$

where $\cong$ denotes congruence by an invertible change of coordinates and $\oplus$ denotes a block direct sum.

Consequently,

$$
\operatorname{inertia}(A_{J_R})
=
\operatorname{inertia}(B)
+
\operatorname{inertia}(S_R).
$$

In particular,

$$
A_{J_R}\succ0
\quad\Longleftrightarrow\quad
B\succ0
\text{ and }
S_R\succ0.
$$

#### Proof

The block elimination matrix

$$
T_R=
\begin{pmatrix}
I_k&-B^{-1}E_R\\
0&I_{|R|}
\end{pmatrix}
$$

is invertible. Direct multiplication gives

$$
T_R^T
A_{J_R}
T_R
=
\begin{pmatrix}
B&0\\
0&S_R
\end{pmatrix}.
$$

Congruence preserves inertia, which proves the result. $\square$

### Theorem 4.2: simplex-tangent curvature is governed by $K_R$

Assume $H_I\succ0$. For every $R\subseteq F$, the reduced Hessian on the enlarged face satisfies

$$
H_{J_R}
\cong
H_I\oplus K_R.
$$

Consequently,

$$
H_{J_R}\succ0
\quad\Longleftrightarrow\quad
K_R\succ0.
$$

#### Proof

Let $Z_I$ be a basis matrix for $\mathcal T_I$. Put

$$
P_R=Y_R+w\frac{g_R^T}{\delta}
\in\mathbb R^{k\times|R|}.
$$

Because

$$
\mathbf1_I^TY_R=\rho_R^T
$$

and $\mathbf1_I^Tw=\delta$, we obtain

$$
\mathbf1_I^TP_R
=
\rho_R^T+g_R^T
=
\mathbf1_R^T.
$$

Therefore every column of

$$
Q_R=
\begin{pmatrix}
-P_R\\
I_{|R|}
\end{pmatrix}
$$

has coordinate sum zero. The columns of

$$
\widehat Z_R=
\begin{pmatrix}
Z_I&-P_R\\
0&I_{|R|}
\end{pmatrix}
$$

form a basis of the tangent space $\mathcal T_{J_R}$. The first block contains the old tangent directions, while the second block
adds one balanced direction for every new coordinate.

The old and new tangent directions are orthogonal with respect to the quadratic form. Indeed,

$$
B P_R
=
E_R+\mathbf1_I\frac{g_R^T}{\delta},
$$

and hence

$$
Z_I^T(-BP_R+E_R)
=
-Z_I^T\mathbf1_I\frac{g_R^T}{\delta}
=0.
$$

The quadratic form on the new balanced directions is

$$
Q_R^TA_{J_R}Q_R
=
S_R+\frac{g_Rg_R^T}{\delta}
=K_R.
$$

Thus

$$
\widehat Z_R^TA_{J_R}\widehat Z_R
=
\begin{pmatrix}
H_I&0\\
0&K_R
\end{pmatrix}.
$$

This matrix is a reduced Hessian on $J_R$, so the claimed congruence follows. $\square$

### Corollary 4.3: exact classification when the root is positive definite

Assume

$$
B\succ0.
$$

Then $H_I\succ0$ and $\delta>0$. Every enlarged support $J_R=I\cup R$ falls into exactly one of the following cases:

| Exact conditions | Region of $J_R$ | Available curvature action |
|---|---|---|
| $S_R\succ0$ | Downward-pruning region | $A_{J_R}\succ0$, so every nonempty subset of $J_R$ may be removed. |
| $S_R\not\succ0$ and $K_R\succ0$ | Middle region | Neither strict-curvature closure applies. |
| $K_R\not\succ0$ | Upward-pruning region | Every superset of $J_R$ may be removed. |

Because $\delta>0$,

$$
K-S=\frac{gg^T}{\delta}\succeq0.
$$

Thus $S_R\succ0$ automatically implies $K_R\succ0$. The rank-one correction can convert an indefinite $S_R$ into a
positive-definite $K_R$; this is precisely how the middle region arises.

### Corollary 4.4: exact classification when the root is already in the middle region

Assume

$$
H_I\succ0,
\qquad
B\not\succ0,
\qquad
B\text{ nonsingular}.
$$

Then $B$ has exactly one negative direction and

$$
\delta<0.
$$

No superset of $I$ can become positive definite, because every $A_{J_R}$ contains the non-positive-definite principal matrix $B$.
For every $R\subseteq F$:

| Exact condition | Region of $J_R$ |
|---|---|
| $K_R\succ0$ | Middle region |
| $K_R\not\succ0$ | Upward-pruning region |

Now

$$
K-S=\frac{gg^T}{\delta}\preceq0.
$$

Therefore $S_R\not\succ0$ implies $K_R\not\succ0$. Once the root is in the middle region, failure of ordinary Schur definiteness is
already enough to guarantee upward curvature failure.

## 5. The curvature structure inside one Dickinson interval

Assume that $w$ has full support and let

$$
F=N_A(w)\setminus I.
$$

Then every support of the form

$$
J_R=I\cup R,
\qquad R\subseteq F,
$$

lies in the Dickinson interval $[I,N_A(w)]$.

Construct $S$ and $K$ once on $F$. The complete curvature structure of this upper interval is then represented on the smaller
Boolean lattice $2^F$:

- $R$ lies in the positive-definite region exactly when $B\succ0$ and $S_R\succ0$;
- $R$ has strictly positive tangent curvature exactly when $K_R\succ0$;
- $R$ is in the middle region exactly when $K_R\succ0$ but $A_{J_R}\not\succ0$.

When $B\succ0$, the middle region is therefore

$$
\boxed{
\left\{R\subseteq F:K_R\succ0\right\}
\setminus
\left\{R\subseteq F:S_R\succ0\right\}.
}
$$

This formula gives a precise answer to the question “what can be known about curvature inside a Dickinson interval?” The bare
interval does not answer it, but the retained factorization turns it into two principal-submatrix definiteness problems on $F$.

Both properties have the expected monotonicity:

- $S_R\succ0$ is inherited when indices are removed from $R$;
- $K_R\not\succ0$ is inherited when indices are added to $R$.

Hence maximal $S$-positive-definite sets describe the downward frontier, while minimal $K$-non-positive-definite sets describe the
upward frontier inside the Dickinson interval.

## 6. Transforming curvature information back into a Dickinson solve

The same Schur matrix $S_R$ gives the all-ones solution on the enlarged support. No new factorization of the complete matrix
$A_{J_R}$ is mathematically necessary.

### Theorem 6.1: child all-ones solution

Assume that $S_R$ is nonsingular. Let

$$
z=A_{J_R}^{-1}\mathbf1_{J_R}
$$

and partition it as

$$
z=
\begin{pmatrix}
z_I\\
z_R
\end{pmatrix}.
$$

Then

$$
\boxed{
z_R=S_R^{-1}g_R
}
$$

and

$$
\boxed{
z_I=w-Y_Rz_R.
}
$$

#### Proof

The block equations are

$$
Bz_I+E_Rz_R=\mathbf1_I
$$

and

$$
E_R^Tz_I+C_Rz_R=\mathbf1_R.
$$

The first equation gives

$$
z_I=w-Y_Rz_R.
$$

Substitution into the second equation gives

$$
E_R^Tw+(C_R-E_R^TY_R)z_R=\mathbf1_R.
$$

Using $E_R^Tw=\rho_R$ and $C_R-E_R^TY_R=S_R$ yields

$$
S_Rz_R=\mathbf1_R-\rho_R=g_R.
$$

This proves both formulas. $\square$

### Corollary 6.2: child stationary scalar

Define

$$
\delta_{J_R}=\mathbf1_{J_R}^Tz.
$$

Then

$$
\boxed{
\delta_{J_R}
=
\delta+g_R^TS_R^{-1}g_R.
}
$$

#### Proof

Since $\mathbf1_I^TY_R=\rho_R^T$,

$$
\begin{aligned}
\mathbf1_{J_R}^Tz
&=\mathbf1_I^T(w-Y_Rz_R)+\mathbf1_R^Tz_R\\
&=\delta+(\mathbf1_R-\rho_R)^Tz_R\\
&=\delta+g_R^TS_R^{-1}g_R.
\end{aligned}
$$

$\square$

### Corollary 6.3: responses outside the child support

Let $h\notin J_R$. Define the old response

$$
\eta_h=A_{h,I}w
$$

and the residual coupling row

$$
c_{h,R}=A_{h,R}-A_{h,I}Y_R.
$$

Then the response of the child Dickinson vector at coordinate $h$ is

$$
\boxed{
(Az)_h
=
\eta_h+c_{h,R}S_R^{-1}g_R.
}
$$

Thus the next Dickinson upper endpoint can be obtained from the root solution, the residual couplings, and the reduced solve with
$S_R$. A full child factorization is not needed merely to construct the child all-ones certificate.

### Theorem 6.4: inertia relation between $S_R$, $K_R$, and the child solve

Assume that $S_R$ is nonsingular. For a symmetric matrix $M$, let

$$
\operatorname{inertia}(M)=(n_+(M),n_-(M),n_0(M))
$$

denote its numbers of positive, negative, and zero eigenvalues. Then

$$
\boxed{
\operatorname{inertia}(K_R)
=
\operatorname{inertia}(S_R)
+
\operatorname{inertia}(-\delta_{J_R})
-
\operatorname{inertia}(-\delta).
}
$$

Addition and subtraction are componentwise. A nonzero scalar is regarded as a one-by-one symmetric matrix when its inertia is
taken.

#### Proof

Consider the bordered matrix

$$
M_R=
\begin{pmatrix}
S_R&g_R\\
g_R^T&-\delta
\end{pmatrix}.
$$

Eliminating the last scalar block gives the Schur complement

$$
S_R+\frac{g_Rg_R^T}{\delta}=K_R.
$$

Therefore

$$
\operatorname{inertia}(M_R)
=
\operatorname{inertia}(-\delta)
+
\operatorname{inertia}(K_R).
$$

Eliminating $S_R$ instead gives the scalar Schur complement

$$
-\delta-g_R^TS_R^{-1}g_R
=
-\delta_{J_R}.
$$

Therefore

$$
\operatorname{inertia}(M_R)
=
\operatorname{inertia}(S_R)
+
\operatorname{inertia}(-\delta_{J_R}).
$$

Equating the two expressions proves the identity. $\square$

This identity says that the curvature change from $S_R$ to $K_R$ is controlled by the signs of the root and child all-ones sums.
It is not a separate phenomenon.

## 7. Adding one index: the complete scalar bridge

The one-index case makes the connection especially transparent. Let

$$
R=\{j\}
$$

and write

$$
c=A_{I,j}\in\mathbb R^k,
\qquad
d=A_{jj}\in\mathbb R.
$$

Define:

$$
\rho_j=c^Tw,
$$

the Dickinson response;

$$
s_j=d-c^TB^{-1}c,
$$

the ordinary Schur pivot;

$$
g_j=1-\rho_j,
$$

the response deficit; and

$$
\kappa_j=s_j+\frac{g_j^2}{\delta},
$$

the tangent-curvature pivot.

Then

$$
A_{I\cup\{j\}}\cong B\oplus[s_j]
$$

and

$$
H_{I\cup\{j\}}\cong H_I\oplus[\kappa_j].
$$

If $s_j\neq0$, the child all-ones sum is

$$
\delta_j
=
\delta+\frac{g_j^2}{s_j}.
$$

The four quantities satisfy the exact identity

$$
\boxed{
\kappa_j=\frac{s_j\delta_j}{\delta}.
}
$$

Indeed,

$$
\frac{s_j\delta_j}{\delta}
=
\frac{s_j}{\delta}
\left(
\delta+\frac{g_j^2}{s_j}
\right)
=
s_j+\frac{g_j^2}{\delta}
=\kappa_j.
$$

If the root matrix $B$ is positive definite, then $\delta>0$ and:

| Scalar conditions | Meaning for $I\cup\{j\}$ |
|---|---|
| $s_j>0$ | The enlarged principal matrix is positive definite; downward pruning is available. |
| $s_j<0$ and $\kappa_j>0$ | The enlarged face is in the middle region. |
| $\kappa_j\leq0$ | The enlarged face has nonpositive tangent curvature; upward pruning is available. |
| $s_j=0$ | The enlarged principal matrix is singular; the singular branch is required. |

Traditional Dickinson interval membership asks only whether

$$
\rho_j\geq0.
$$

The curvature decision depends on

$$
s_j,
\qquad
\delta,
\qquad
\text{and}
\qquad
(1-\rho_j)^2.
$$

Therefore the sign stored in the interval cannot determine the curvature pivot.

### 7.1 Two-dimensional sanity check

Let

$$
A=
\begin{pmatrix}
a&b\\
b&c
\end{pmatrix},
\qquad
I=\{1\},
\qquad
a\neq0.
$$

Then

$$
w=\frac1a,
\qquad
\delta=\frac1a,
\qquad
\rho_2=\frac ba,
$$

and

$$
s_2=c-\frac{b^2}{a}.
$$

The curvature pivot becomes

$$
\begin{aligned}
\kappa_2
&=c-\frac{b^2}{a}
+a\left(1-\frac ba\right)^2\\
&=a+c-2b.
\end{aligned}
$$

This is exactly the curvature of the tangent direction $(1,-1)^T$:

$$
(1,-1)
A
\begin{pmatrix}
1\\-1
\end{pmatrix}
=a+c-2b.
$$

Thus even in dimension two:

- Dickinson asks whether $b/a\geq0$;
- ordinary definiteness asks whether $c-b^2/a>0$ when $a>0$;
- tangent curvature asks whether $a+c-2b>0$.

These tests are connected through the same numbers, but none is merely a renaming of another.

## 8. General Dickinson vectors from Halfspace--Rays widening

The preceding paired matrices $S$ and $K$ use the canonical vector

$$
w=B^{-1}\mathbf1_I.
$$

Halfspace--Rays widening may instead produce an arbitrary exact Dickinson vector $u\in\mathbb R^n$. Let

$$
L=L(u),
\qquad
U=N_A(u),
\qquad
p=Au,
\qquad
\sigma=\mathbf1^Tu,
\qquad
q=u^TAu.
$$

The following direct curvature implication remains valid without assuming that $u$ came from an all-ones solve.

### Theorem 8.1: one optional coordinate gives an exact tangent direction

Assume $\sigma\neq0$. For $j\in U\setminus L$, define

$$
v_j=e_j-\frac{u}{\sigma}.
$$

Then $v_j$ is supported inside $L\cup\{j\}$ and

$$
\mathbf1^Tv_j=0.
$$

Its exact curvature is

$$
\boxed{
v_j^TAv_j
=
A_{jj}
-\frac{2p_j}{\sigma}
+\frac{q}{\sigma^2}.
}
$$

Therefore

$$
A_{jj}\sigma^2-2p_j\sigma+q\leq0
$$

proves that $H_{L\cup\{j\}}\not\succ0$. This gives upward curvature pruning from $L\cup\{j\}$ to the full support $[n]$, possibly
far beyond the upper endpoint $U$ of the original Dickinson interval.

The test is only one-sided. A positive value examines one tangent direction and does not prove that the entire reduced Hessian is
positive definite.

If $\sigma=0$, then $u$ itself is tangent on $L$. In that case

$$
q=u^TAu\leq0
$$

already proves upward curvature failure at $L$.

### Theorem 8.2: exact tangent residual for a complete widened interval

Let

$$
F=U\setminus L
$$

be the free indices of a general Dickinson interval. Choose one anchor index $a\in L$. Inside the coordinates $U$, let

$$
Z_L=
\left[e_i-e_a:i\in L\setminus\{a\}\right]
$$

be a basis of the tangent space on $L$, and let

$$
W_F=
\left[e_j-e_a:j\in F\right]
$$

contain the optional balanced directions. In the combined tangent basis $[Z_L,W_F]$, write

$$
\begin{pmatrix}
Z_L^T\\
W_F^T
\end{pmatrix}
A_U
\begin{pmatrix}
Z_L&W_F
\end{pmatrix}
=
\begin{pmatrix}
B_T&C_T\\
C_T^T&D_T
\end{pmatrix}.
$$

Assume that the lower face has strictly positive curvature, so

$$
B_T=Z_L^TA_LZ_L\succ0.
$$

If $|L|=1$, then $Z_L$ and $B_T$ have zero columns and zero rows. In that case the elimination below is empty and one simply sets
$\mathcal R=D_T$.

Define the exact tangent residual

$$
\mathcal R
=
D_T-C_T^TB_T^{-1}C_T.
$$

For every $Q\subseteq F$,

$$
\boxed{
H_{L\cup Q}\succ0
\quad\Longleftrightarrow\quad
\mathcal R_Q\succ0,
}
$$

where

$$
\mathcal R_Q=\mathcal R[Q,Q].
$$

#### Proof

The tangent basis on $L\cup Q$ consists of the columns of $Z_L$ together with the columns of $W_F$ indexed by $Q$. Its reduced
Hessian is therefore

$$
\begin{pmatrix}
B_T&(C_T)_Q\\
(C_T)_Q^T&(D_T)_Q
\end{pmatrix}.
$$

Block elimination of the positive-definite matrix $B_T$ makes this reduced Hessian congruent to

$$
B_T\oplus\mathcal R_Q.
$$

It is positive definite exactly when $\mathcal R_Q$ is positive definite. $\square$

This theorem applies to any Dickinson interval, including intervals produced by arbitrary positive right-hand sides and ray
combinations. It also handles the case $L\subsetneq I$ directly.

The price is that $\mathcal R$ describes only tangent curvature. Unlike the paired canonical matrices $S$ and $K$, it does not by
itself describe ordinary positive definiteness or construct the next all-ones Dickinson vector.

## 9. Singular roots

If $B=A_I$ is singular, then $B^{-1}$, $Y$, $S$, and the formulas based on them are undefined. This is not a removable notation
problem: a singular root has additional kernel directions and the system

$$
Bx=\mathbf1_I
$$

may be inconsistent or may have an affine family of solutions.

The correct singular analysis uses:

- an exact basis of $\ker B$;
- projected outside rows acting on that kernel;
- a generalized Schur complement on a complementary nonsingular subspace; and
- an affine analysis when $Bx=\mathbf1_I$ is consistent.

These formulas are developed in [`SINGULAR_LIFT_DICKINSON_RESEARCH.md`](SINGULAR_LIFT_DICKINSON_RESEARCH.md). An implementation
must branch explicitly on singularity. It must not apply the inverse formulas above through a numerical pseudoinverse and treat the
result as an exact certificate.

## Part II. Intuition

## 10. The same elimination seen in two coordinate systems

The matrix $S$ and the matrix $K$ are not two unrelated constructions.

The ordinary Schur matrix

$$
S=C-E^TB^{-1}E
$$

asks:

> After the old coordinates in $I$ have adjusted themselves optimally, what quadratic form remains on the newly added coordinates?

This is an unconstrained question in the complete coordinate space. It is the correct question for positive definiteness of the
principal matrix.

Curvature on the simplex asks a slightly different question. A movement in the optional coordinates changes their coordinate sum.
The old coordinates must compensate so that the total sum remains zero. The correction

$$
\frac{gg^T}{\delta}
$$

is exactly the cost of enforcing that balance. Therefore

$$
K=S+\frac{gg^T}{\delta}
$$

is the Schur matrix after the simplex tangent constraint has been imposed.

In short:

- $S$ eliminates the old coordinates;
- $K$ eliminates the old coordinates **and** preserves the simplex balance.

## 11. Why Dickinson signs alone are insufficient

For an optional coordinate $j$, Dickinson records only

$$
\rho_j\geq0
\quad\text{or}\quad
\rho_j<0.
$$

This decides whether the existing certificate survives when $j$ is admitted. Curvature instead uses

$$
g_j=1-\rho_j.
$$

The value $1$ appears because the old vector solves

$$
Bw=\mathbf1_I.
$$

For a new all-ones solution, the new coordinate must also reach response $1$, not merely a nonnegative response. The deficit $g_j$
is the amount still missing after the old vector is reused.

Two coordinates may both satisfy $\rho_j\geq0$ and hence both belong to the Dickinson endpoint, while having very different deficits
$1-\rho_j$ and very different Schur pivots. Their curvature can therefore be opposite.

The lost connection is not absent from the mathematics; it was discarded when the numerical response was reduced to one sign bit.

## 12. The middle region as a rank-one phenomenon

Suppose the root $B$ is positive definite. Then $\delta>0$, so

$$
K=S+\text{a positive-semidefinite rank-one matrix}.
$$

The unrestricted principal matrix may possess one negative direction, represented by $S_R\not\succ0$. If that direction changes
the coordinate sum, it may be unavailable on the simplex tangent space. The rank-one correction can remove that one bad direction,
leaving

$$
K_R\succ0.
$$

That is exactly the middle region: the principal matrix is not positive definite, but the simplex face is still strictly convex.

Once $K_R$ also ceases to be positive definite, the bad direction can be chosen tangent to the simplex. From that support onward,
upward curvature pruning is available.

## 13. A Dickinson interval as a reduced search problem

A Dickinson interval with free set

$$
F=U\setminus I
$$

contains $2^{|F|}$ supports above $I$. Ordinarily, the interval is entered into the support generator and forgotten: every support
inside it is treated as already covered.

The Schur bridge gives a second interpretation. The interval is a smaller Boolean lattice on the optional coordinates $F$, equipped
with two matrices:

- $S$, which describes its downward-pruning frontier;
- $K$, which describes its upward-pruning frontier.

A curvature-bad support found inside the interval is especially valuable. The Dickinson interval reaches only to $U$, whereas an
upward curvature certificate starting at $I\cup R$ reaches every superset up to $[n]$. It can therefore prune supports outside the
interval that would otherwise remain available.

This is the precise reason to inspect selected Dickinson intervals rather than viewing every such inspection as duplicated work.

## 14. Why the transformation works in both directions

The root solve supplies $w$ and the factorization of $B$. The same factorization produces $Y=B^{-1}E$. From these quantities:

1. $\rho=E^Tw$ gives the current Dickinson endpoint;
2. $S=C-E^TY$ gives ordinary definiteness after extension;
3. $K=S+gg^T/\delta$ gives tangent curvature after extension;
4. $S_R^{-1}g_R$ gives the new coordinates of the child all-ones solution;
5. $w-Y_RS_R^{-1}g_R$ gives its old coordinates; and
6. one residual matrix-vector product gives the child's outside Dickinson responses.

Thus there is no conceptual gap between “perform a Dickinson step” and “perform a curvature step.” They become separate only if the
factorization and response values are discarded after the interval endpoints have been constructed.

## Part III. Algorithmic consequences

## 15. Information that should be retained temporarily

For a nonsingular root $I$ with $H_I\succ0$, the useful temporary state is:

- the exact factorization of $B=A_I$;
- the all-ones solution $w$;
- its sum $\delta=\mathbf1^Tw$;
- the exact outside responses $\rho$; and
- the lower and upper endpoints of the chosen Dickinson interval.

The matrices $Y$, $S$, and $K$ need not be constructed for every root. They can be built only when the interval is selected for
curvature inspection.

## 16. Three levels of increasing work

The connection can be used at three different costs.

### Level 1: one-direction tests from a general certificate

For every free index $j\in U\setminus L$, evaluate

$$
A_{jj}\sigma^2-2(Au)_j\sigma+u^TAu.
$$

All vector quantities already exist after constructing the Dickinson certificate. A nonpositive result is an exact upward-curvature
certificate on $L\cup\{j\}$. A positive result is inconclusive.

This is the smallest useful bridge for Halfspace--Rays certificates.

### Level 2: endpoint curvature

Test the reduced Hessian only at the Dickinson upper endpoint $U$. If it is not positive definite, shrink $U$ by deleting optional
indices while non-positive curvature remains. The result is a smaller upward-pruning root whose closure reaches beyond $U$.

This asks only whether the interval hides at least one useful curvature obstruction near its top.

### Level 3: complete local frontier search

Construct the paired matrices $S$ and $K$ for a canonical interval, or the tangent residual $\mathcal R$ for a general widened
interval. Search the local Boolean lattice on the free indices:

- grow $S$-positive-definite sets to find maximal downward roots;
- shrink $K$-non-positive-definite sets to find minimal upward roots; and
- leave the remaining middle sets available for a new Dickinson calculation when needed.

This exposes the full curvature boundary hidden inside the interval, but its worst case remains exponential in $|F|$.

## 17. Proposed exact workflow for a canonical interval

The following is the smallest complete experiment that uses the two-way bridge rather than merely checking one endpoint.

1. Process an uncovered support $I$ and factor $B=A_I$ exactly.
2. If $B$ is singular, enter the explicit singular branch and do not use the formulas below.
3. Compute $w=B^{-1}\mathbf1_I$, $\delta=\mathbf1_I^Tw$, and the traditional or widened Dickinson interval.
4. If the interval is not selected for local inspection, store its ordinary Dickinson certificate and continue the existing
   traversal.
5. Let $F$ be the optional indices to inspect. Reuse the factorization of $B$ to solve $Y=B^{-1}E$.
6. Construct $\rho$, $g$, $S$, and $K$ exactly.
7. Search principal submatrices of $S$ and $K$ on the smaller support space $2^F$:
   - every exactly verified minimal $K_R\not\succ0$ gives the global upward closure $[I\cup R,[n]]$;
   - if $B\succ0$, every exactly verified maximal $S_R\succ0$ gives the downward closure of $I\cup R$;
   - every $K_R\succ0$ with $A_{I\cup R}\not\succ0$ lies in the middle region.
8. At a selected middle support with nonsingular $S_R$, compute its all-ones vector through
   $z_R=S_R^{-1}g_R$ and $z_I=w-Y_Rz_R$.
9. Compute the new outside responses using Corollary 6.3 and construct the child's Dickinson interval.

Every pruning decision must be verified with exact arithmetic. Floating point may nominate an interval, endpoint, or local subset for
inspection, but it may not establish definiteness, non-positive curvature, or a Dickinson certificate.

## 18. Incremental alternative for a frontier walk

Constructing dense $m\times m$ matrices $S$ and $K$ may be unnecessary when only one path through the optional indices is wanted.
For a current support $I$, evaluate each possible one-index extension $j$ through

$$
\rho_j,
\qquad
s_j,
\qquad
\kappa_j.
$$

These three values tell us:

- whether the current Dickinson vector reaches $j$;
- whether adding $j$ preserves positive definiteness; and
- whether adding $j$ crosses the tangent-curvature frontier.

A walk can choose an uncovered extension according to its exact or floating-point-screened Schur pivot, verify the chosen step
exactly, and update the factorization incrementally. This avoids constructing the complete local residual, but it explores only one
path and does not characterize all incomparable frontier points.

The full local search and the incremental walk therefore answer different questions:

- the full search tries to expose the antichain of frontier roots inside one interval;
- the walk tries to reach one useful frontier root cheaply.

## 19. What the bridge guarantees and what it does not

### Guaranteed

1. Given the retained canonical factorization, $S_R$ exactly determines the contribution of the new coordinates to ordinary
   definiteness.
2. Given $H_I\succ0$, $K_R$ exactly determines the contribution of the new coordinates to simplex-tangent curvature.
3. Given nonsingular $S_R$, the same reduced system exactly constructs the child all-ones Dickinson vector.
4. For a general Dickinson vector, Theorem 8.1 supplies an exact cheap curvature direction, and Theorem 8.2 reduces complete local
   curvature to principal submatrices of one residual matrix.

### Not guaranteed

1. Membership in a Dickinson interval alone does not determine curvature.
2. Curvature alone does not determine a Dickinson upper endpoint; the outside matrix-vector responses are still required.
3. A positive one-direction test in Theorem 8.1 does not prove positive-definite reduced Hessian.
4. The local frontier may contain many incomparable supports. The Schur reduction lowers the matrix dimension but does not remove
   the exponential worst case of Boolean-lattice search.
5. The inverse formulas do not apply to singular roots.

## 20. Practical research questions

The identities reduce the open algorithmic problem to measurable choices:

1. Which Dickinson intervals are wide enough, or otherwise structured enough, to justify constructing $S$ and $K$?
2. Is the one-direction test already sufficient to recover most hidden upward curvature roots?
3. Is endpoint shrinking sufficient, or does complete local frontier search add materially more coverage?
4. Can $S_R^{-1}g_R$ generate wider child Dickinson intervals cheaply enough to compensate for the local search?
5. On hard BPQY and Peng instances, how large is $|F|$, how many minimal $K$-bad sets occur, and how much coverage extends beyond the
   original Dickinson endpoint?

These are empirical questions. The mathematical bridge proves that the transformations are exact; it does not by itself prove that
their bookkeeping cost is profitable.

## 21. Relation to existing coposit experiments

Parts of this bridge have already appeared separately in the experimental models:

- [`sat_c1`](../models/hadeler-based/sat_c1/ALGORITHM.md) implements the one-direction certificate test and an exact tangent residual
  search inside selected Dickinson intervals;
- [`sat_c4`](../models/hadeler-based/sat_c4/ALGORITHM.md) checks curvature at staged Dickinson upper endpoints; and
- the dual-frontier experiments use one-index ordinary and tangent Schur pivots to guide local walks.

The paired $S/K$ formulation in this note makes the wider relation explicit. It shows that one retained root factorization can
simultaneously provide:

- the Dickinson response;
- ordinary-definiteness frontiers;
- tangent-curvature frontiers;
- child all-ones vectors; and
- child outside responses.

No benchmark claim is made here. A new implementation should first compare the three work levels in Section 16 using existing stored
diagnostics before adding another large local search to the current model.
