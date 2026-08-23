# How SAT-B3 Runs

This document explains the **workflow** of SAT-B3, the current incumbent in the curvature-based SAT line. It focuses on where each
mathematical test is used, what the result changes, and how the algorithm moves to the next support.

It does not repeat the full proofs. The underlying mathematics is developed in:

- [Copositivity on the Simplex: Curvature, KKT Points, Hadeler, and Dickinson](../aidocs/COPOSITIVITY_CURVATURE_HADELER_DICKINSON.md)
- [Dickinson's Copositivity Algorithm, Step By Step](DICKINSON_ALGORITHM_STEP_BY_STEP.md)

SAT itself is assumed to be known. Here, SAT is simply the mechanism that answers:

> Give me one support of this cardinality that has not already been covered.

## 1. What the algorithm stores

A nonempty support $I\subseteq[n]$ selects the principal matrix

$$
A_I.
$$

SAT-B3 can finish supports in four ways.

### Upward pruning

If $I$ receives an upward certificate, SAT-B3 removes

$$
\{J:I\subseteq J\subseteq[n]\}.
$$

Thus $I$ and every support containing $I$ are finished.

### Downward pruning

If $I$ receives a downward certificate, SAT-B3 removes

$$
\{J:\varnothing\neq J\subseteq I\}.
$$

Thus $I$ and every nonempty support contained in $I$ are finished.

### Dickinson interval

For a Dickinson vector $u$, define

$$
L(u)=\operatorname{supp}(u),
\qquad
U(u)=\{j:(Au)_j\geq0\}.
$$

The certificate removes every support in

$$
[L(u),U(u)]
=
\{J:L(u)\subseteq J\subseteq U(u)\}.
$$

### Exact-support removal

Sometimes SAT-B3 has finished analyzing $I$, but has obtained no safe conclusion about its neighbors. It then removes only $I$.

## 2. The cheap pair prepass

Before the main loop, SAT-B3 checks every two-index support $\{i,j\}$. Its one-dimensional tangent curvature is

$$
c_{ij}=a_{ii}+a_{jj}-2a_{ij}.
$$

If $c_{ij}\leq0$, the pair cannot be the support of the smallest-support global minimizer needed by the copositivity argument.
SAT-B3 therefore upward-prunes the pair and every support containing it.

This prepass is performed first because it needs no matrix factorization.

## 3. The two frontiers

SAT-B3 starts with

$$
\text{low}=1,
\qquad
\text{high}=n.
$$

The two frontiers have different purposes.

- The **low frontier** moves from small supports toward large supports. It uses exact arithmetic and eventually completes the proof.
- The **high frontier** moves from large supports toward small supports. It looks opportunistically for principal matrices that are
  strictly copositive for a reason that is strong enough to certify every smaller principal support at once.

The algorithm alternates individual supports:

1. process one low support;
2. process one high support;
3. process another low support;
4. process another high support;
5. continue in this order.

It does **not** finish an entire low layer before trying the high side.

If the current low layer contains no unresolved support, low increases until it finds one. If the current high layer contains no
remaining high candidate, high decreases in the same way.

When high becomes smaller than low, the high search stops. The low frontier continues alone through all remaining cardinalities up
to $n$.

## 4. What happens at one low support

Suppose SAT returns an unresolved support $I$ at the current low cardinality.

### 4.1 Exact factorization

SAT-B3 extracts $A_I$ and computes an exact fraction-free $LDL^T$ factorization.

This one factorization supplies:

- rank and singularity;
- inertia and definiteness information;
- the reduced-curvature decision;
- the solve needed for the Dickinson candidate;
- additional solves used by the Halfspace-Rays optimization.

### 4.2 Immediate negative or zero witnesses

If $A_I$ is nonsingular, SAT-B3 solves

$$
A_Ix=\mathbf1.
$$

If the resulting vector gives an exact nonnegative negative witness after the appropriate sign change, the full matrix is not
copositive. The algorithm stops immediately.

If $A_I$ is singular, SAT-B3 obtains an exact kernel vector

$$
A_Iz=0.
$$

If one orientation of $z$ is nonnegative, SAT-B3 has found a copositive zero. Strict copositivity is therefore false, but the
ordinary-copositivity search continues.

### 4.3 Apply the curvature test

Let $Z$ span the directions whose coordinate sum is zero on the selected face. The reduced Hessian is

$$
H_I=Z^TA_IZ.
$$

SAT-B3 does not construct $Z$ explicitly. It obtains the sign of this reduced curvature from the exact factorization of $A_I$.

The result controls the next action:

- If $H_I$ is **not positive definite**, SAT-B3 upward-prunes $I$.
- If $H_I$ **is positive definite**, curvature alone cannot remove $I$, so SAT-B3 creates a Dickinson certificate instead.

This is the main low-side choice:

> First try the direct curvature prune. Only if that is unavailable, pay for the additional Halfspace-Rays Dickinson optimization.

### 4.4 Create the Dickinson interval

In the nonsingular case, the starting vector comes from $A_Ix=\mathbf1$. The retained factorization also supplies the coordinate
directions used by the Halfspace-Rays sweeps.

The optimization prefers:

1. a larger upper set $|U|$;
2. when $|U|$ is tied, a larger interval width $|U|-|L|$.

It may also test at most two combined rays after the ordinary coordinate sweeps stop improving the certificate.

In the singular case, SAT-B3 uses the kernel direction and chooses the sign producing the larger upper set.

The final exact vector supplies one interval $[L,U]$, and SAT removes that interval.

### 4.5 Full-support shortcut

If the full support $[n]$ remains eligible, it is the first high-frontier candidate. SAT-B3 finishes immediately if the full matrix
is positive definite, or if it is positive semidefinite and the exact system $Ax=\mathbf1$ has a solution: either downward
certificate covers every nonempty support. A floating-point rejection does not lose this shortcut, because the exact low frontier
can still select $[n]$ later.

## 5. What happens at one high support

Suppose SAT returns a high candidate $I$ at the current high cardinality.

The high side looks for either of two exact downward certificates:

1. $A_I$ is positive definite; or
2. $A_I$ is positive semidefinite and $A_Ix=\mathbf1$ is consistent, meaning that at least one solution $x$ exists.

The second condition is the improvement from SAT-B2 to SAT-B3. It matters when $A_I$ is singular: positive definiteness is then
impossible, but the consistency test can still prove strict copositivity on every nonzero nonnegative vector supported in $I$.

It does not run the Halfspace-Rays Dickinson optimization.

### 5.1 Floating-point screening

SAT-B3 first applies a fast floating-point $LDL^T$ positive-semidefiniteness filter.

- If $A_I$ does **not** look positive semidefinite, $I$ is retired only from the high search.
- If $A_I$ **does** look positive semidefinite, SAT-B3 verifies it exactly.

A floating-point rejection is not a mathematical decision. The support remains globally unresolved and can later be selected by the
exact low frontier.

### 5.2 Exact verification

If exact arithmetic confirms

$$
A_I\succ0,
$$

then every nonempty principal support contained in $I$ is positive definite. SAT-B3 downward-prunes all of them.

If $A_I$ is singular, SAT-B3 instead asks whether

$$
A_I\succeq0
\qquad\text{and}\qquad
A_Ix=\mathbf1\text{ has a solution}.
$$

When both statements are true, no nonzero nonnegative vector can lie in the kernel of $A_I$. Therefore every such vector has a
strictly positive quadratic value, and every nonempty principal support inside $I$ is strictly copositive. SAT-B3 downward-prunes
all of them.

The intuition is simple: a positive-semidefinite matrix can vanish only on its kernel. Consistency of $A_Ix=\mathbf1$ forces every
kernel vector to have coordinate sum zero, whereas a nonzero nonnegative vector has positive coordinate sum. The nonnegative cone
and the kernel therefore meet only at zero.

If neither downward certificate is valid, SAT-B3 checks whether the exact calculation exposed a negative witness or a nonnegative
zero.

- A negative witness stops the complete algorithm with **not copositive**.
- A zero records **not strictly copositive**, while ordinary classification continues.
- Otherwise the high side removes only $I$.

The high side deliberately does no further certificate optimization. Its purpose is to find cheap, large downward closures, not to
replace the exact low traversal.

## 6. The complete loop

~~~text
APPLY the pair-curvature prepass
low <- 1
high <- n
WHILE low <= n:
    SKIP low-cardinality layers with no globally unresolved support
    IF no low layer remains:
        BREAK
    I <- one unresolved support at low
    PROCESS I with the exact low rules (full upward pruning when strict convexity fails; otherwise advanced Halfspace-Rays Dickinson pruning)
    IF a negative witness was found:
        RETURN not copositive
    IF no globally unresolved support remains:
        RETURN the accumulated classification
    IF low <= high:
        SKIP high-cardinality layers with no remaining high candidate
        IF low <= high:
            J <- one high candidate at high
            SCREEN J in floating point
            IF J looks positive semidefinite:
                FACTORIZE A_J exactly
                IF A_J is positive definite:
                    DOWNWARD-PRUNE J and every nonempty support contained in J
                ELSE IF A_J is positive semidefinite AND A_J x = 1 has a solution:
                    DOWNWARD-PRUNE J and every nonempty support contained in J
                ELSE IF the exact calculation produces a negative witness:
                    RETURN not copositive
                ELSE:
                    RECORD any exact zero and REMOVE only J
            ELSE:
                RETIRE J only from the high search
            IF no globally unresolved support remains:
                RETURN the accumulated classification
RETURN the accumulated classification
~~~

## 7. Why the low frontier completes the proof

The high frontier is optional acceleration. A floating-point rejection leaves the support globally open, so the high side alone can
never certify that all work is finished.

The low frontier supplies completeness. Every support selected there is handled by:

- an upward curvature prune;
- a Dickinson interval containing the selected support;
- the full-support downward shortcut; or
- immediate termination with a negative witness.

The support lattice is finite. Therefore the low frontier eventually finds a negative witness or covers every remaining support.

## 8. How the final answer is accumulated

SAT-B3 keeps ordinary and strict copositivity separate.

- An exact $u\geq0$ with $u^TAu<0$ proves **not copositive** and stops the run.
- An exact nonzero $u\geq0$ with $u^TAu=0$ proves **not strictly copositive**, but the ordinary search continues.
- Covering all supports without a negative witness proves **copositive**.
- Covering all supports without a negative witness or a zero proves **strictly copositive**.

## 9. The shortest mental model

SAT-B3 repeatedly does this:

> Try to prune upward from a small support. Then try to prune downward from a large support. Use a Dickinson interval whenever the
> low-side curvature test cannot prune upward.

The algorithm is favorable when small supports often permit upward pruning, or when large supports satisfy either exact downward
condition. It is slow when neither happens and the low frontier must cover most of the support lattice with narrow Dickinson
intervals.
