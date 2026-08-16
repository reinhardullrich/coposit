# Exact One-Step Frank–Wolfe Sponsel

Classification: coposit-created CP/SCP predicate variant. It adds one exact centre-to-vertex Frank–Wolfe line minimization to the
maintained Sponsel 2012 node flow and otherwise retains Sponsel's `H` certificate, Bundfuss split, traversal, and resource outcomes
unchanged.

Public mode boundary: `solve(A, copositivity_mode::copositive)` tests copositivity (CP), while
`solve(A, copositivity_mode::strictly_copositive)` tests strict copositivity (SCP). The model does not implement combined `both`
classification; callers that need both predicates must select them separately.

## Idea In Plain Language

Sponsel's model partitions the standard simplex into smaller simplices. At each node it can reject a bad vertex or edge, accept the
whole node with an exact positive certificate, or split the node and continue.

This variant adds one more rejection attempt between the positive certificate and the split. It starts at the centre of the current
simplex, chooses the vertex with the smallest first-order value, and minimizes the quadratic exactly along that single line segment.
For SCP, a nonpositive exact value rejects; for CP, only a negative value rejects. Otherwise the model performs the same split and
traversal as `sponsel_2012`.

There is no floating-point proposal, tolerance, iteration loop, restart policy, or approximate reconstruction. The complete
Frank–Wolfe addition is one rational line minimization evaluated with FLINT integers.

## Name, Sources, And Classification

The identifier is `frank_wolfe_sponsel`:

- **Frank–Wolfe** names the centre-to-vertex conditional-gradient step;
- **Sponsel** names the exact certificate-and-partition fallback copied from `sponsel_2012`.

Primary sources are:

- Marguerite Frank and Philip Wolfe, “An Algorithm for Quadratic Programming,” *Naval Research Logistics Quarterly* 3(1–2),
  95–110 (1956), [DOI 10.1002/nav.3800030109](https://doi.org/10.1002/nav.3800030109);
- Julia Sponsel, Stefan Bundfuss, and Mirjam Dür, “An Improved Algorithm to Test Copositivity,” *Journal of Global Optimization*
  52(3), 537–551 (2012), [DOI 10.1007/s10898-011-9766-2](https://doi.org/10.1007/s10898-011-9766-2);
- Stefan Bundfuss and Mirjam Dür, “Algorithmic Copositivity Detection by Simplicial Partition,” *Linear Algebra and its
  Applications* 428(7), 1511–1523 (2008), [DOI 10.1016/j.laa.2007.09.035](https://doi.org/10.1016/j.laa.2007.09.035).

This combination is not a historical algorithm from those papers. The single exact Frank–Wolfe step and its placement after a
failed `H` certificate are coposit choices. The fallback was copied from
[`sponsel_2012`](../../baselines/sponsel_2012/ALGORITHM.md). The authoritative implementation is [`solver.cpp`](solver.cpp).

## Decision Problem And Simplex Nodes

For a nonempty square symmetric integer matrix $A$, the selected mode decides either

$$
x^TAx\geq0\quad\text{for every }x\geq0 \qquad\text{(CP)},
$$

or

$$
x^TAx>0\quad\text{for every nonzero }x\geq0 \qquad\text{(SCP)}.
$$

Homogeneity permits restriction to the standard simplex. A search node is

$$
\Delta=\operatorname{conv}\{v_1,\ldots,v_n\}
$$

with vertex matrix $V=(v_1,\ldots,v_n)$ and exact Gram matrix

$$
G=V^TAV,
\qquad
g_{ij}=v_i^TAv_j.
$$

Every point in the node is $Vy$ for some barycentric vector $y\geq0$ with $\mathbf1^Ty=1$, and

$$
(Vy)^TA(Vy)=y^TGy.
$$

The initial node has $V=I$ and $G=A$. Child Gram matrices are stored as positive integer multiples of their rational mathematical
values. Positive common scaling changes none of the tests below.

## Retained Sponsel Tests

Each node first performs the maintained Sponsel decisions.

### Vertex rejection

For CP, a vertex rejects exactly when $g_{ii}<0$. For SCP, it rejects when $g_{ii}\leq0$. Thus a zero diagonal is retained in CP
mode and rejected in SCP mode.

### Minimum negative edge and two-generator rejection

The model scans off-diagonal entries in lexicographic order and retains the first numerically smallest negative entry $g_{ij}$.
If no negative off-diagonal exists, an entrywise-nonnegative node is CP; it is SCP when its diagonal has already passed the strict
vertex test.

For the selected edge, write

$$
\alpha=g_{ii}>0,
\qquad
\beta=g_{jj}>0,
\qquad
\gamma=g_{ij}<0.
$$

The two-generator restriction has a nonpositive direction exactly when

$$
\gamma^2\geq\alpha\beta.
$$

The strict reverse inequality rejects both modes. Equality rejects SCP but is a valid CP boundary case.

### Mode-aware `H` certificate

Define $S(G)$ by retaining the diagonal and every negative off-diagonal entry of $G$ while replacing each positive off-diagonal
entry by zero. Then

$$
G=S(G)+N^+(G),
$$

where $N^+(G)$ is entrywise nonnegative. Exact fraction-free LDLT accepts the node when

$$
S(G)\succeq0\quad\text{in CP mode},
\qquad
S(G)\succ0\quad\text{in SCP mode}.
$$

The entrywise-nonnegative remainder preserves the corresponding inequality. Failure of this sufficient certificate is unresolved
and sends the node to Frank–Wolfe and then subdivision.

Only a node that survives the vertex and edge tests and fails this positive certificate receives the Frank–Wolfe step.

## The Exact Frank–Wolfe Step

### Centre and linear oracle

Let the current Gram matrix have order $n$ and begin at the simplex centre

$$
y_0=\frac1n\mathbf1.
$$

Define the row sums and total sum

$$
r_i=\sum_k g_{ik},
\qquad
T=\sum_i r_i=\mathbf1^TG\mathbf1.
$$

The centre value is $T/n^2$. The all-ones barycentric vector rejects when $T<0$ in CP mode or $T\leq0$ in SCP mode. A CP zero is not
a negative witness, so CP mode continues.

For $f(y)=y^TGy$, the gradient is $2Gy$. A linear function reaches its minimum on the simplex at a vertex, so at $y_0$ choose

$$
j\in\arg\min_i r_i.
$$

Ties use the first index. The Frank–Wolfe direction is

$$
d=e_j-y_0.
$$

### Exact descent and curvature

Along $y(\lambda)=y_0+\lambda d$, $0\leq\lambda\leq1$,

$$
f(y(\lambda))=f(y_0)+2\lambda g+\lambda^2h,
$$

where

$$
g=d^TGy_0=\frac{nr_j-T}{n^2}
$$

and

$$
h=d^TGd=\frac{n^2g_{jj}-2nr_j+T}{n^2}.
$$

The implementation names the positive numerator of $-g$

$$
p=T-nr_j
$$

and the curvature numerator

$$
q=n^2g_{jj}-2nr_j+T.
$$

If $p\leq0$, no simplex vertex supplies first-order descent from the centre and the Frank–Wolfe phase stops. If $q\leq0$, the line
is linear or concave; its minimum is at an endpoint, and both endpoints are already known positive. If $p\geq q$, the constrained
minimum is the already-checked vertex $e_j$. None of these cases rejects.

The remaining case has $0<p<q$ and the exact interior minimizer

$$
\lambda=\frac pq.
$$

### Integer witness without rational storage

The rational point is

$$
y(\lambda)=\frac{q-p}{nq}\mathbf1+\frac pq e_j.
$$

Multiplying by the positive denominator $nq$ gives the nonzero nonnegative integer vector

$$
z=(q-p)\mathbf1+np\,e_j.
$$

Homogeneity gives

$$
y(\lambda)^TGy(\lambda)\mathrel{\diamond}0
\quad\Longleftrightarrow\quad
z^TGz\mathrel{\diamond}0,
$$

for either exact comparison $\diamond\in\{<,\leq\}$. The implementation evaluates this without allocating $z$:

$$
z^TGz
=(q-p)^2T
+2(q-p)(np)r_j
+(np)^2g_{jj}.
$$

Every operation is an arbitrary-precision integer addition or multiplication. A negative result rejects CP and SCP; zero rejects
only SCP. A value that does not reject the selected mode proves nothing and transfers control to the unchanged Sponsel split.

## Retained Bundfuss Split

For the minimum negative edge $(i,j)$ retained before the `H` and Frank–Wolfe tests, Sponsel's inherited split computes

$$
\lambda_1=\frac{-\gamma}{\alpha-\gamma},
\qquad
\lambda_2=\frac{\beta-\gamma}{\alpha-2\gamma+\beta},
\qquad
\lambda_3=\frac{\beta}{\beta-\gamma},
$$

and selects

$$
\lambda=\max\bigl(\lambda_1,\min(\lambda_2,\lambda_3)\bigr).
$$

The new edge point is

$$
w=\lambda v_i+(1-\lambda)v_j.
$$

One child replaces $v_i$ by $w$ and the other replaces $v_j$ by $w$. Their union is the parent simplex. Fraction comparisons use
cross multiplication, rational child Gram matrices are cleared to positive integer scale, and common integer content is removed.

Both children are inspected immediately in the inherited order. Unresolved children enter a last-in-first-out work list. The second
unresolved child is therefore expanded next. Rejection of any child rejects the input; acceptance requires every generated child to
be certified.

## Complete Decision Flow

```text
receive a parser-guaranteed nonempty square symmetric integer matrix A
inspect the initial Gram matrix A

inspect(G, mode):
    reject if some diagonal fails mode: < 0 for CP, <= 0 for SCP
    choose the first minimum negative off-diagonal entry
    accept if no negative off-diagonal exists
    reject if the selected two-generator restriction is negative, or is zero in SCP mode
    accept if exact LDLT proves S(G) PSD for CP or PD for SCP
    calculate the centre row sums and total exactly
    reject if the centre or its one exact Frank–Wolfe line minimum is negative, or is zero in SCP mode
    otherwise retain the Sponsel/Bundfuss split pair

while an unresolved node remains:
    split it by the inherited exact lambda rule
    inspect both children in the inherited order
    reject on the first failed child

accept when no unresolved child remains
```

## Correctness Boundary

The Frank–Wolfe phase adds rejection only. Its centre and line point are nonzero nonnegative barycentric vectors, and every rejection
uses an exact negative value for CP or an exact nonpositive value for SCP. Such a vector maps through the node's nonnegative vertex
matrix to a valid witness for the original input.

Failure to find a witness is not acceptance. The unchanged exact Sponsel certificate and complete retained partition decide every
subsequent result. Consequently:

- Frank–Wolfe rounding cannot affect the result because no floating arithmetic exists;
- one-step stationarity is not treated as a certificate;
- equality is rejected exactly in SCP mode and retained in CP mode;
- a timeout or open-node limit remains unresolved rather than `false`.

## Source Behavior And coposit Changes

Retained unchanged from `sponsel_2012`:

- node Gram representation;
- mode-aware vertex and two-generator rejection;
- first minimum negative edge selection;
- entrywise-nonnegative and mode-aware PSD/PD `H` acceptance;
- three-lambda Bundfuss split and exact child construction;
- child inspection, last-in-first-out traversal, content reduction, timeout checkpoints, and open-node limit.

Added by this coposit variant:

- one centre point per node that fails the `H` certificate;
- the minimum-row-sum Frank–Wolfe vertex;
- one exact rational line minimization;
- rejection only after the exact integer quadratic value fails the selected CP/SCP predicate.

No implementation is shared with another model. The solver was copied so the Sponsel baseline remains mathematically unchanged.

## Termination And Resource Outcomes

The Frank–Wolfe addition has no loop and always terminates after one triangular matrix scan and a fixed number of integer operations.
It can only remove a node by rejection; otherwise it leaves the Sponsel tree unchanged.

The inherited same-dimensional partition need not expose a boundary zero quickly. coposit stops unresolved if a split would exceed
50,000 simultaneously unfinished nodes. Timed model companions also observe cooperative timeout checkpoints. Neither condition is
reported as a negative classification.

## Known Difficult Inputs

Positive nodes cannot benefit from a negative or zero witness search. A node that fails the selected `H` certificate pays one
additional exact row-sum scan before it splits. Exceptional matrices for which `H` rarely succeeds may therefore accumulate pure
overhead.

One line from the centre explores only points whose non-selected coordinates have equal barycentric weights. A negative region or
boundary zero on another face can remain invisible. In particular, sparse or uneven high-support zeros need not lie on any
centre-to-vertex segment.

Boundary matrices remain the main termination risk. If the exact zero is not the centre, the one line minimum, a generated vertex,
or a selected two-generator direction, the inherited partition may continue refining toward it without producing it exactly.

Large integer Gram entries make the row sums and exact line evaluation more expensive, although the step adds no rational matrix and
no iterative coefficient growth of its own. When no witness is found, every original Sponsel weakness remains, including raw
minimum-edge selection, same-dimensional refinement, exact LDLT cost, and split-induced coefficient growth.
