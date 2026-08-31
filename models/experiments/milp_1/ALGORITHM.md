# MILP 1

## Idea

`milp_1` is an exact adaptation of Gondzio and Yildirim's second mixed-integer linear reformulation of the standard quadratic problem.
It computes the global simplex minimum

\[
\nu(A)=\min\{x^\top A x:x\geq0,\ \mathbf1^\top x=1\}
\]

with one MILP. The sign of this single exact number gives the complete copositivity classification:

\[
\nu(A)<0 \Rightarrow A\text{ is not copositive},\qquad
\nu(A)=0 \Rightarrow A\text{ is copositive but not strictly copositive},
\]

and

\[
\nu(A)>0 \Rightarrow A\text{ is strictly copositive}.
\]

The intuition is that scaling any nonzero nonnegative vector onto the simplex preserves the sign of the quadratic form. Copositivity is
therefore exactly the question whether the simplex minimum is nonnegative; strict copositivity asks whether it is positive.

The name records that this was coposit's first exact MILP experiment. The current formulation is a strict adaptation, not a faithful
baseline: the mathematical model is Gondzio--Yildirim MILP2, while coposit supplies its own exact rational branch-and-bound engine and
an integer shift that avoids free variables.

## Gondzio--Yildirim MILP2

For a support indicator $y\in\{0,1\}^n$, the continuous vector $x$ lies in the simplex and satisfies $x_j\leq y_j$. Thus every used
coordinate has $y_j=1$. Slack variables $z\geq0$ deactivate the payoff inequality only when a coordinate is unused:

\[
\begin{aligned}
\min\quad & \alpha,\\
\text{subject to}\quad
&(Ax)_j\leq\alpha+z_j &&(j=1,\ldots,n),\\
&\mathbf1^\top x=1,\\
&x_j\leq y_j &&(j=1,\ldots,n),\\
&z_j\leq U_j(1-y_j) &&(j=1,\ldots,n),\\
&x,z\geq0,\qquad y\in\{0,1\}^n.
\end{aligned}
\]

At an integral solution, $z_j=0$ on the support of $x$, so $\alpha$ bounds every used component of $Ax$. Gondzio and Yildirim prove
the minimax identity

\[
\nu(A)=\min_{x\in\Delta_n}\max_{j\in\operatorname{supp}(x)}(Ax)_j.
\]

Consequently, the MILP optimum is exactly the global quadratic minimum, even though the model contains no quadratic constraint. The
geometric intuition is that a global simplex minimizer equalizes the relevant first-order payoffs on its active face; the binaries let
the MILP choose that face.

This is formulation MILP2 in Jacek Gondzio and E. Alper Yildirim, *Global Solutions of Nonconvex Standard Quadratic Programs via Mixed
Integer Linear Programming Reformulations*, Technical Report ERGO-18-22, October 3, 2018. The preserved local source is
`research/papers/Gondzio_Yildirim_2021_StQP_MILP.txt`. MILP2 is a relaxation of their MILP1 formulation, but Proposition 3.2 proves
that it has the same optimal value $\nu(A)$.

## Exact bounds and the nonnegative shift

The shared Fractional MILP engine represents continuous variables as nonnegative. Define the exact integer bounds

\[
\ell=\min_{i,j}a_{ij},\qquad d=\min_i a_{ii}.
\]

For every simplex point, $x^\top A x$ is a convex combination of the entries of $A$, so $\nu(A)\geq\ell$. The simplex also contains
every unit vector, so $\nu(A)\leq d$. The implementation therefore writes

\[
t=\alpha-\ell,\qquad 0\leq t\leq d-\ell,
\]

and maximizes $-t$. It uses the exact slack bound

\[
U_j=\max_i a_{ji}-\ell.
\]

The implemented row is

\[
(Ax)_j-t-z_j\leq\ell,
\]

which is precisely $(Ax)_j\leq\alpha+z_j$ after substituting $\alpha=\ell+t$. If the exact maximized objective is $r=-t^*$, the
simplex minimum is recovered as

\[
\nu(A)=\ell-r.
\]

No rounding is used in this recovery.

## Complete decision flow

1. Compute $\ell$, $d$, and every $U_j$ exactly from the integer matrix.
2. Build one Gondzio--Yildirim MILP2 instance with variables $(x,y,z,t)$.
3. Solve it to proven optimality with the shared Fractional MILP engine.
4. Recover the exact rational value $\nu(A)=\ell-r$.
5. Return `(false, false)`, `(true, false)`, or `(true, true)` according to whether $\nu(A)$ is negative, zero, or positive.
6. An interrupted search remains unresolved. There is no second MILP and no silent fallback.

Floating-point LP relaxations only guide branching. Every incumbent, node bound, infeasibility decision, and final optimum used by the
classification is recomputed with FLINT rational arithmetic.

## Known Difficult Inputs

The formulation still has one binary variable per coordinate. Highly symmetric matrices can expose many equivalent supports, while
weak $U_j$ bounds can leave broad LP relaxations. Boundary-copositive instances are especially demanding because the solver must prove
the exact optimum is zero rather than merely find a witness near zero. Dense large-integer matrices can also cause rational tableau
growth. The shared solver currently has no presolve, cutting planes, warm starts, or reused LP bases, so these structures can lead to
large branch-and-bound trees or expensive exact LP certifications.
