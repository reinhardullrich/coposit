# Singular-Lift Dickinson: Theory, Counterexamples, And Search Consequences

This note develops the mathematics behind the singular-lifting Dickinson experiments. Its purpose is to answer three practical
questions:

1. When must lifting expose a useful one-dimensional kernel direction?
2. When can lifting never succeed?
3. Can the useful direction be found without traversing a large graph of principal supports?

The central result is an exact dichotomy. If a useful ceiling direction already lies in the kernel of the root principal matrix, then
either it can be isolated after the minimum possible number of lifts, or a persistent kernel of dimension at least two makes every
nullity-one lift impossible. This distinction is visible by linear algebra at the root.

The note also proves what breadth-first traversal minimizes, gives counterexamples to unrestricted completeness and accessibility,
and relates the search to polyhedral cones, oriented matroids, delta-matroids, and kernel circuits.

## 1. Dickinson Certificates And The Singular-Lift Experiment

Let

$$
A\in\mathbb Z^{n\times n}
$$

be symmetric, and write

$$
[n]=\{1,\ldots,n\}.
$$

For an index set $I\subseteq[n]$, let $A_I$ denote the principal submatrix with rows and columns indexed by $I$.

For a vector $u\in\mathbb R^n$, define its lower and upper support sets by

$$
L(u)=\operatorname{supp}(u)=\{i:u_i\neq0\},
$$

and

$$
U(u)=N_A(u)=\{j:(Au)_j\geq0\}.
$$

Dickinson's certificate theorem says that an admissible vector $u$, meaning

$$
u\notin-\mathbb R_+^n,
$$

certifies every support in the Boolean interval

$$
[L(u),U(u)]
=
\{J\subseteq[n]:L(u)\subseteq J\subseteq U(u)\}.
$$

The singular-lift experiments retain only **ceiling certificates**, for which

$$
U(u)=[n].
$$

Such a vector certifies every later support containing $L(u)$:

$$
[L(u),[n]]
=
\{J\subseteq[n]:L(u)\subseteq J\}.
$$

This is a specialization of Dickinson's Theorem 4.6 and Algorithms 1–2 in
[A New Certificate for Copositivity](https://ris.utwente.nl/ws/files/87825628/cop_cert.pdf).

### The cardinalities

Four quantities must be kept separate.

1. The **root support** $I$ is where ordinary Dickinson processing starts the lift. Its cardinality is

   $$
   k=|I|.
   $$

2. A **lifted support** $T\supseteq I$ is reached by adding indices. Its cardinality is

   $$
   t=|T|.
   $$

3. A candidate vector obtained from $\ker A_T$ may have zero coordinates. Its true lower support has cardinality

   $$
   \ell=|L(u)|.
   $$

   It is possible that $\ell<t$.

4. The upper cardinality is $|U(u)|$. Every retained singular-lift certificate has

   $$
   |U(u)|=n.
   $$

The complete diagnostic tuple is therefore

$$
(\text{root }k,\text{lifted }k,|U|,|L|,\text{count}).
$$

## 2. The Current Lifting Rule

Let

$$
\nu(T)=\dim\ker A_T=|T|-\operatorname{rank}(A_T)
$$

denote the nullity of the principal matrix on $T$.

When a root $I$ has $\nu(I)>1$, a factorization can return an arbitrary one-dimensional direction from a multidimensional kernel.
That direction may fail the ceiling test even though another direction in the same kernel would pass it.

The experiment therefore adds indices. From a support $T$ with $\nu(T)>1$, it considers every child

$$
T'=T\cup\{j\},
\qquad j\notin T.
$$

The current stopping rule is:

- if $\nu(T')>1$, continue lifting from $T'$;
- if $\nu(T')=1$, recover the unique kernel ray, test the relevant orientation or both orientations, and stop that route;
- if the nullity-one ray fails the ceiling test, do not continue through it.

Different orders of adding indices can reach the same principal support. A call-wide seen set factors and expands each lifted support
only once. This is mathematically safe because the principal matrix, its kernel, and its children depend only on the support, not on
the path by which it was reached.

Certificates discovered while processing ordinary roots of cardinality $k$ remain pending until every root of that cardinality has
finished. This prevents one root from suppressing another root in the same layer before the second root has had a chance to produce a
stronger lower support.

## 3. Exact Effect Of Adding One Row And Column

The first theorem gives the complete nullity transition for one symmetric border.

### Theorem 3.1: symmetric border trichotomy

Let $B\in\mathbb R^{m\times m}$ be symmetric with kernel

$$
K=\ker B,
\qquad
q=\dim K.
$$

Consider the bordered matrix

$$
C=
\begin{pmatrix}
B&b\\
b^T&a
\end{pmatrix}.
$$

Exactly one of the following occurs.

1. If $b$ is not orthogonal to $K$, then

   $$
   \dim\ker C=q-1,
   $$

   and

   $$
   \ker C
   =
   \{(x,0):x\in K,\ b^Tx=0\}.
   $$

2. If $b$ is orthogonal to $K$, then $b\in\operatorname{range}B$. Choose any $y$ satisfying $By=b$ and define

   $$
   s=a-b^Ty.
   $$

   Then

   $$
   \dim\ker C=
   \begin{cases}
   q,&s\neq0,\\
   q+1,&s=0.
   \end{cases}
   $$

Consequently,

$$
\dim\ker C\in\{q-1,q,q+1\}.
$$

#### Proof

Suppose first that $b$ is not orthogonal to $K$. Then there is a $z\in K$ with $b^Tz\neq0$. If $(x,\tau)\in\ker C$, the first block
equation is

$$
Bx+b\tau=0.
$$

Multiplying by $z^T$ gives

$$
\tau z^Tb=0.
$$

Hence $\tau=0$. The remaining equations are

$$
Bx=0,
\qquad
b^Tx=0.
$$

The functional $x\mapsto b^Tx$ is nonzero on the $q$-dimensional space $K$, so its kernel inside $K$ has dimension $q-1$.

Now suppose that $b\perp K$. Because $B$ is symmetric,

$$
\operatorname{range}B=K^\perp,
$$

so $b=By$ for some $y$. A block congruence transformation reduces $C$ to

$$
\begin{pmatrix}
B&0\\
0&a-b^Ty
\end{pmatrix}.
$$

Congruence preserves nullity. The last scalar therefore either contributes no new kernel direction or one new kernel direction,
which proves the two remaining cases. $\square$

### Consequences

A symmetric border can lower nullity by at most one. Therefore a root of nullity $q>1$ needs at least

$$
q-1
$$

lifts before it can reach nullity one:

$$
t\geq k+q-1.
$$

There is also a stronger implementation consequence:

> A child of a lifted state with nullity greater than one can never be nonsingular.

The nonsingular-child case is mathematically unreachable inside the current lift, although it remains possible when extending a
nullity-one state that the algorithm has already chosen to stop at.

The theorem also identifies exactly when nullity falls. It falls by one precisely when the new border is not orthogonal to the
current kernel. Conditional on a fixed singular parent and continuously varying otherwise-generic border data, this is the generic
case. Structured matrices can instead make many borders orthogonal to the kernel, causing nullity to remain unchanged or increase.

## 4. The Root Ceiling Cone

The useful directions already present at a root can be described without constructing any principal supersets.

Fix a root $I$ and define

$$
K_I=\ker A_I,
\qquad
q=\dim K_I>1,
\qquad
R=[n]\setminus I.
$$

For $x\in K_I$, the products outside the root are

$$
P_Ix=A_{R,I}x.
$$

The **root ceiling cone** is

$$
C_I
=
\{x\in K_I:P_Ix\geq0\}.
$$

Every nonzero vector

$$
x\in C_I\setminus(-\mathbb R_+^I)
$$

is an admissible Dickinson ceiling direction supported inside $I$.

The lineality space of this cone is

$$
H_I
=
C_I\cap(-C_I)
=
\{x\in K_I:P_Ix=0\}.
$$

Equivalently,

$$
H_I=\ker A_{[:,I]},
$$

where $A_{[:,I]}$ is the full matrix restricted to the columns indexed by $I$. Every $h\in H_I$, extended by zero outside $I$,
satisfies

$$
Ah=0.
$$

For this reason, $H_I$ is called the **persistent kernel**: it remains in the kernel of every principal superset of $I$.

## 5. Exact Completeness Criterion

The next theorem answers the main existence question.

### Theorem 5.1: root-cone lifting dichotomy

Let $I\subseteq[n]$ satisfy

$$
q=\dim\ker A_I>1.
$$

Assume that the root ceiling cone contains an admissible direction:

$$
C_I\setminus(-\mathbb R_+^I)\neq\varnothing.
$$

Then a principal superset $T\supseteq I$ is reachable through the current singular-lifting rule, has nullity one, and has a ceiling
kernel ray if and only if

$$
\dim H_I\leq1.
$$

When such a superset exists, one exists after exactly $q-1$ lifts:

$$
|T|=|I|+q-1.
$$

#### Proof: persistent kernel of dimension at least two

Suppose

$$
\dim H_I\geq2.
$$

Every $h\in H_I$, extended by zero on $T\setminus I$, lies in $\ker A_T$ for every $T\supseteq I$. Therefore

$$
\dim\ker A_T\geq\dim H_I\geq2.
$$

No nullity-one principal superset exists.

Notice that lifting is unnecessary in this case. Any nonzero $h\in H_I$ satisfies $Ah=0$. One of $h$ and $-h$ has a positive
coordinate, so it is already an admissible ceiling certificate.

#### Proof: one-dimensional persistent kernel

Suppose

$$
\dim H_I=1.
$$

The map $P_I$ restricted to $K_I$ has rank $q-1$. Choose $q-1$ linearly independent rows of this restriction. Add the corresponding
outside indices one at a time.

At each step the new row is a nonzero functional on the current kernel. By Theorem 3.1, nullity falls by exactly one and the new
coordinate is zero. After $q-1$ steps, the remaining kernel is exactly $H_I$, which is one-dimensional. Its generator satisfies
$Ah=0$ and is therefore a ceiling direction.

#### Proof: no persistent kernel

Suppose

$$
H_I=\{0\}.
$$

Then $C_I$ is a pointed polyhedral cone. It contains an admissible nonzero vector by assumption. A pointed polyhedral cone is generated
by its extreme rays, so at least one extreme ray is admissible; otherwise every vector in the cone would lie in
$-\mathbb R_+^I$.

An extreme ray in the $q$-dimensional space $K_I$ is determined by $q-1$ linearly independent active inequalities. Choose the
corresponding $q-1$ outside indices. Their rows vanish on the extreme ray and are independent on $K_I$.

Adding these indices one at a time lowers nullity by exactly one at every step. All added coordinates remain zero. The final kernel
is the selected extreme ray, which passes the ceiling test by construction. The lower bound from Theorem 3.1 shows that no
nullity-one descendant could have been reached in fewer than $q-1$ lifts. $\square$

The active-inequality characterization used here is the standard characterization of extreme rays of pointed polyhedral cones; see
[Nemirovski, Mathematical Essentials, Theorem II.9.6](https://www2.isye.gatech.edu/~nemirovs/CUPProjectIni.pdf).

### Interpretation

Under the theorem's hypothesis, there are only two possibilities:

- the useful direction is exposed at the theoretically minimum lifted order;
- a persistent kernel of dimension at least two makes nullity one impossible everywhere above the root.

There is no intermediate case where the first useful nullity-one direction lies deeper than $q-1$ while the useful direction was
already present in the root kernel.

## 6. Counterexample To Unrestricted Completeness

The persistent-kernel condition is necessary.

Consider

$$
A=\operatorname{diag}(0,0,1),
\qquad
I=\{1,2\}.
$$

Then

$$
\ker A_I=\mathbb R^2.
$$

The vector $e_1$ satisfies

$$
Ae_1=0,
\qquad
U(e_1)=[3].
$$

Thus the root kernel contains a perfect ceiling direction. The only strict principal superset is the full support, but

$$
\dim\ker A=2.
$$

No nullity-one descendant exists. Here

$$
H_I=\ker A_I
$$

has dimension two, exactly as Theorem 5.1 predicts.

The absolute smallest example is the order-two zero matrix with the full support as root. The order-three example is more informative
because an actual lift is available.

## 7. A Useful Nullity-One Superset Need Not Be Accessible

Theorem 5.1 concerns directions already present in the root kernel. A useful larger kernel direction can involve nonzero newly added
coordinates. Such a target need not be reachable under the first-nullity-one stopping rule.

Consider

$$
A=
\begin{pmatrix}
0&0&1&0&-1\\
0&0&0&1&-1\\
1&0&0&0&0\\
0&1&0&0&0\\
-1&-1&0&0&0
\end{pmatrix},
\qquad
I=\{1,2\}.
$$

The root has nullity two. Each one-index child has nullity one:

- $I\cup\{3\}$ has kernel ray $e_2$;
- $I\cup\{4\}$ has kernel ray $e_1$;
- $I\cup\{5\}$ has kernel ray $e_1-e_2$.

Each ray fails the ceiling test in both orientations:

- for $e_2$, the products at indices 4 and 5 have signs $+$ and $-$;
- for $e_1$, the products at indices 3 and 5 have signs $+$ and $-$;
- for $e_1-e_2$, the products at indices 3 and 4 have signs $+$ and $-$.

Every principal enlargement using two of the indices $\{3,4,5\}$ is nonsingular. Nevertheless, the full matrix has nullity one with

$$
w=(0,0,1,1,1)^T,
\qquad
Aw=0.
$$

The full support therefore gives a useful nonnegative ceiling direction.

Every ordering from $I$ to the full support follows the nullity pattern

$$
2\longrightarrow1\longrightarrow0\longrightarrow1.
$$

The current rule stops at the first nullity-one matrix, whose ray fails the ceiling test, and can never reach the useful full-support
ray. This proves:

> A useful nullity-one principal superset can exist without an ordering whose proper intermediate supports all have nullity greater
> than one.

The same example proves that an empty root ceiling cone does not justify pruning every deeper lift. New coordinates can create a
useful direction that did not exist in the original root kernel.

## 8. Delta-Matroid Interpretation Of Accessibility

For a symmetric matrix $A$, define the family

$$
\mathcal F_A
=
\{S\subseteq[n]:A_S\text{ is nonsingular}\}.
$$

This family is a representable delta-matroid. Brijder and Hoogeboom prove that principal nullity is exactly symmetric-difference
distance to this family:

$$
\nu(S)
=
\min_{F\in\mathcal F_A}|S\mathbin{\triangle}F|.
$$

See [Nullity and Loop Complementation for Delta-Matroids, Theorem 6.2](https://documentserver.uhasselt.be/bitstream/1942/15199/1/nullity_loopc_dmatroid.pdf).

The lifted-support graph can therefore be viewed as follows:

- nullity greater than one means delta-matroid distance greater than one;
- nullity one means distance one;
- nonsingular means distance zero.

Accessibility asks for an inclusion-monotone path in the Boolean lattice that remains at distance greater than one until its terminal
vertex. The delta-matroid exchange axiom does not guarantee such a path. The order-five counterexample follows the distance sequence

$$
2\longrightarrow1\longrightarrow0\longrightarrow1.
$$

Delta-matroids describe the principal-nullity geometry exactly, but they do not contain the outside sign information needed for the
ceiling test.

## 9. What Breadth First And Depth First Preserve

For one fixed root $I$, define the stopped lifted graph:

- its expandable vertices are the principal supersets $T\supseteq I$ with $\nu(T)>1$;
- an edge adds one index;
- a nullity-one child is a terminal and is not expanded;
- only terminals whose ray passes the ceiling test are accepted.

### Theorem 9.1: per-root breadth-first minimality

Assume no timeout or memory limit is reached and every first-visited state is eventually expanded. FIFO breadth-first traversal
discovers accepted terminals in nondecreasing lifted cardinality. Its first accepted terminal therefore has minimum $|T|$ among all
accepted terminals reachable from that root under the stopping rule.

#### Reason

Every edge increases cardinality by one. A FIFO queue processes all expandable vertices at distance $d$ before any vertex at
distance $d+1$. Terminals are generated from those vertices in the same order. Global deduplication does not remove descendants:
the first copy of a state expands the same child family that every duplicate would have expanded.

### Limits of the theorem

The current breadth-first implementation drains one root completely before processing the next root of the same outer cardinality.
It therefore does not guarantee that the first certificate found across all roots has globally minimum lifted cardinality.

A state first reached from an earlier root belongs to that root in the diagnostics. A later root may encounter it as a duplicate.
This changes ownership but not the descendant certificate set, provided the first visit was fully expanded.

Certificates from earlier outer cardinalities can suppress already-certified lifted supports. The minimum theorem applies to the
remaining unsuppressed graph. The pending barrier prevents new certificates from suppressing other ordinary roots in their own
outer layer.

### DFS and BFS without resource limits

Subject to the same stopping rule, active certificates, and state deduplication, exhaustive DFS and BFS visit the same finite stopped
graph and admit the same terminal certificate tests. They differ only in order and storage:

- DFS keeps a path-sized recursion stack and may reach very deep supports immediately;
- BFS finds the shallowest terminals first but may retain an enormous frontier.

With a timeout or memory cutoff, the order becomes part of the practical result.

## 10. A Principled Best-First Order

Plain BFS uses only current depth. It does not distinguish a shallow state whose nullity has stalled from a slightly deeper state
that is already close to nullity one.

For a state $T$ below root $I$, define

$$
g(T)=|T|-|I|
$$

and

$$
h(T)=\nu(T)-1.
$$

The value $h(T)$ is a lower bound on the number of additional lifts needed to reach nullity one. The border theorem gives

$$
h(T)\leq1+h(T')
$$

for every child $T'$, so the bound is consistent.

An A*-style priority

$$
f(T)=g(T)+h(T)
$$

therefore has a precise guarantee:

> If terminals are accepted when removed from the priority queue, the first accepted ceiling terminal has minimum lifted
> cardinality among all reachable accepted terminals.

This order postpones states whose nullity has remained unchanged or increased. It is the strongest simple traversal priority found
here with an admissibility proof.

Priorities based on predicted $|L|$, current outside-sign violations, or apparent certificate width may still be useful heuristics,
but no safe dominance theorem is known for them. Added coordinates can change the kernel and the outside signs discontinuously.

## 11. Full-Support Nullity Greater Than One

At the full support,

$$
T=[n],
$$

the ceiling condition is automatic for every kernel vector. If

$$
0\neq w\in\ker A,
$$

then

$$
Aw=0
$$

and therefore

$$
U(w)=[n].
$$

No linear program or extreme-ray search is required. Orient $w$ so that it has a positive coordinate and store

$$
L(w)=\operatorname{supp}(w).
$$

This produces a valid CP coverage certificate at any positive nullity.

Its implication for strict copositivity depends on its sign:

- if $w\geq0$, it is a nonnegative zero and disproves strict copositivity;
- if $w$ has mixed signs, it provides CP coverage but does not by itself decide strict copositivity.

The current lifted rule obtains a candidate only at nullity one. A full-support lifted state of higher nullity consequently ends
without using a certificate that is already available from any one nullspace vector.

## 12. The Exact Cone Problem Below Full Support

At a non-full support $T$, choose a basis matrix $Z$ for

$$
K_T=\ker A_T.
$$

Every kernel vector is $w=Zy$. Let

$$
G=A_{[n]\setminus T,T}Z.
$$

Finding a ceiling direction is exactly the homogeneous feasibility problem

$$
Gy\geq0,
\qquad
Zy\notin-\mathbb R_+^T.
$$

The second condition means that at least one coordinate of $Zy$ is positive. Homogeneous scaling allows it to be handled by solving,
for each $i\in T$, the exact feasibility problem

$$
Gy\geq0,
\qquad
(Zy)_i\geq1.
$$

At most $|T|$ such problems give a complete answer.

If the cone has no lineality, it is pointed and has finitely many extreme rays. If it has lineality, a complete finite description
consists of a basis of the lineality space and the extreme rays of the pointed quotient. This is the standard polyhedral-cone
setting; a classical algorithmic reference is
[McRae and Davidson, An Algorithm for the Extreme Rays of a Pointed Convex Polyhedral Cone](https://epubs.siam.org/doi/10.1137/0202023).

An exact linear-programming implementation would solve the mathematical problem directly, but it would introduce machinery that
coposit currently avoids. The next section gives a smaller exact alternative when nullity is small.

## 13. Direct Search Inside A Small Root Kernel

Suppose the root kernel has dimension $q$ and no persistent kernel. Every extreme ray of the pointed ceiling cone is determined by
$q-1$ independent active outside equations.

This gives a finite exact search:

- when $q=2$, inspect individual projected outside rows;
- when $q=3$, inspect pairs of projected outside rows;
- when $q=4$, inspect triples;
- in general, inspect independent sets of $q-1$ projected outside rows.

For each selected set:

1. intersect its equations with the root kernel;
2. recover the resulting one-dimensional ray;
3. orient it toward a positive coordinate;
4. test every outside product exactly.

This enumerates the same root-confined extreme rays that zero-coordinate lifting can expose, without building or factoring the
corresponding principal supersets.

For a root of cardinality six and nullity three in an order-45 matrix, there are 39 outside indices. The crude pair bound is only

$$
\binom{39}{2}=741.
$$

The direct search is complete for ceiling directions already contained in the original root kernel. It is not complete for useful
directions requiring nonzero newly added coordinates, as the order-five accessibility counterexample demonstrates.

The sign patterns of the projected rows form the covectors of a realizable oriented matroid. One-dimensional cells correspond to
cocircuits. This is the established combinatorial language closest to the root-kernel extreme-ray search; see
[Björner, Las Vergnas, Sturmfels, White, and Ziegler, Oriented Matroids](https://assets.cambridge.org/97805217/77506/excerpt/9780521777506_excerpt.pdf).

## 14. What Zero Coordinates In A Lifted Ray Mean

Let $A_T$ have nullity one, let $w$ span its kernel, and define

$$
L=\operatorname{supp}(w)\subseteq T.
$$

Restricting to the nonzero coordinates gives

$$
A_Lw_L=0.
$$

Embedding $w_L$ back into the full space gives exactly the original vector, so the full product $Aw$ and the upper support $U(w)$
are unchanged.

There is a stronger circuit statement.

### Theorem 14.1: the true support is a rectangular column circuit

The columns indexed by $L$ form a circuit of the rectangular matrix $A_{T,L}$: they are dependent, but every proper subset is
independent.

#### Proof

The vector $w_L$ has no zero coordinate and satisfies

$$
A_{T,L}w_L=0.
$$

Suppose a proper subset of these columns were dependent. Extending a dependence on that proper subset by zeros would produce a
kernel vector of $A_T$ with support strictly contained in $L$. It could not be proportional to $w$, contradicting

$$
\dim\ker A_T=1.
$$

Therefore $L$ is a minimal dependent column set. $\square$

The same claim need not hold for the principal matrix $A_L$. That matrix uses fewer row equations and may have a kernel of dimension
greater than one. Ordinary Dickinson processing at $L$ can consequently choose a different null direction and miss the useful ray.

The indices in

$$
T\setminus L
$$

have zero coordinates. Their role is not to enlarge the vector's true support. Their row equations isolate the desired ray inside a
higher-dimensional kernel.

This provides the clearest interpretation of successful zero-coordinate lifting:

> The added indices act as selected active equations, not as participating support coordinates.

If $A$ is copositive and the accepted ray is nonnegative, it is a zero of $A$. It is a minimal zero only when no copositive zero has
strictly smaller support. Mixed-sign ceiling rays are Dickinson certificates but are not copositive zeros. Dickinson's Lemma 5.2 and
Corollary 5.3 describe how minimal zeros appear in a complete copositivity certificate.

## 15. Sound Pruning And Early Decisions

The theory gives several exact rules.

### Persistent-kernel decision

For a lifted state $T$, compute

$$
h(T)=\dim\ker A_{[:,T]}.
$$

If $h(T)\geq2$, no principal superset of $T$ can have nullity one. The current rule accepts lifted candidates only at nullity one,
so this branch can be discarded as incapable of reaching a terminal.

A stronger action is available: any nonzero vector in this persistent kernel satisfies $Aw=0$ and is already a ceiling certificate.

### Remaining-depth bound

If

$$
\nu(T)-1>n-|T|,
$$

there are too few unused indices to lower nullity to one. The branch cannot reach a nullity-one terminal.

### Exact border decision

Theorem 3.1 decides whether a border lowers, preserves, or raises nullity by testing its restriction to the current kernel and, when
that restriction vanishes, one exact Schur scalar. This can avoid treating every child factorization as unrelated work.

### A* lower bound

The value

$$
\nu(T)-1
$$

is an admissible and consistent lower bound on the remaining lift depth.

### Conditions that are not safe

The following observations do not justify pruning the complete lifted branch:

- the current root ceiling cone contains only zero;
- the currently chosen nullspace vector has many negative outside products;
- a nullity-one child fails the ceiling test;
- the current principal support has no useful candidate with all newly added coordinates fixed to zero.

The order-five counterexample shows why. A later useful direction may require nonzero new coordinates and may appear only after
passing through a nullity-one or nonsingular principal set that the current algorithm refuses to traverse.

Inertia and principal-minor identities provide additional nullity information but do not supply the missing outside sign
information. No stronger general sign-safe branch prune was established from them.

## 16. Evidence From Matrix 9647

Matrix 9647 is an order-45 MANN/Steiner instance.

### Full matrix

Exact fraction-free LDLT factorization gives

$$
\operatorname{rank}(A)=29,
\qquad
\nu(A)=45-29=16.
$$

Every nonzero full-matrix kernel vector is automatically a ceiling direction. A lifted visit to the full support nevertheless emits
no certificate under the current rule because it extracts lifted candidates only at nullity one, while the full matrix has
nullity 16.

### Depth-first run from root cardinality three

The depth-first search reached lifted cardinality 45 during its first second. This meant only that one singular route had reached the
full support; it did not mean that a certificate had been retained.

The first retained certificate appeared at about 72 seconds and had

$$
(\text{root }k,\text{lifted }k,|U|,|L|)
=
(3,25,45,6).
$$

A 120-second run processed approximately 1.86 million distinct lifted principal supports and skipped approximately 17.27 million
duplicate routes. It retained one ceiling certificate during that period.

### Breadth-first probe from root cardinality three

The breadth-first variant found ceiling certificates at lifted cardinality ten:

$$
(\text{root }k,\text{lifted }k,|U|,|L|)
=
(3,10,45,6).
$$

After roughly 30 seconds, it had accepted six certificates with this tuple. It had reached only lifted cardinality ten, but its seen
cache contained about 15.07 million supports and its FIFO frontier about 10.52 million.

This shows the traversal tradeoff directly. Breadth first exposed shallower certificates much earlier, while the unresolved frontier
grew to millions of supports.

### Nullity-three root and the minimum-depth theorem

A separate useful root has cardinality six and nullity three. A successful lift reaches a principal support of cardinality eight and
nullity one. The final vector has true lower support of cardinality six and upper support $[45]$.

The border theorem requires at least

$$
3-1=2
$$

lifts. The successful cardinality-eight support therefore occurs at the minimum possible depth. Its zero coordinates show that some
lifted indices are acting as constraint equations rather than as members of the final vector support.

## 17. Literature Connections And Limits

The singular-superset construction is not part of Dickinson's paper. Dickinson specifies how a suitable vector certifies a Boolean
interval and says to choose a nullspace vector when a principal matrix is singular. He does not optimize over a multidimensional
kernel or add principal indices until the kernel becomes one-dimensional.

The closest established mathematical structures are:

- **polyhedral cones:** the ceiling directions inside a fixed kernel form a homogeneous polyhedral cone;
- **oriented matroids:** projected outside-row sign patterns are covectors, and root-kernel extreme rays correspond to
  one-dimensional cells or cocircuits;
- **symmetric delta-matroids:** principal nullity is distance to the family of nonsingular principal sets;
- **column matroid circuits:** the actual support of a nullity-one lifted ray is a minimal dependent set of columns in
  $A_{T,L}$;
- **active-set methods:** zero-coordinate lifted indices impose active equations that isolate a ray.

Facial reduction is a useful analogy but not an exact identification. Graph minimum-rank theory usually varies matrix entries within
a prescribed graph pattern, whereas the matrix in this problem is fixed exactly. Principal-nullity sequences describe possible
nullity patterns but do not contain the sign information required by the ceiling test.

No published construction was found that exactly matches:

> add principal indices, continue only while nullity is greater than one, test the first nullity-one ray, and stop that route.

This is not a claim of novelty. It records only the result of the targeted search through Dickinson's paper and the relevant
principal-nullity, delta-matroid, oriented-matroid, and polyhedral-cone literature.

## 18. Established Results, Counterexamples, And Conjectures

### Proved here

1. A symmetric one-index border changes nullity by exactly $-1$, $0$, or $+1$, with the cases characterized by the border's action
   on the current kernel.
2. A lifted child of a nullity-greater-than-one state cannot be nonsingular.
3. If a useful ceiling direction exists in the root kernel, an accessible useful nullity-one lift exists exactly when the persistent
   kernel has dimension at most one.
4. When that lift exists, it exists at the minimum possible depth $q-1$.
5. A persistent kernel of dimension at least two prevents every nullity-one principal superset but directly supplies a ceiling
   certificate.
6. Per-root BFS finds the minimum lifted cardinality admitted by the stopping rule.
7. The A* heuristic $h(T)=\nu(T)-1$ is admissible and consistent.
8. At full support, every nonzero kernel vector is a ceiling certificate regardless of nullity.
9. The true support of a nullity-one lifted vector is a circuit of the rectangular column matrix $A_{T,L}$.

### Counterexamples established here

1. A root kernel can contain a ceiling direction while no nullity-one principal superset exists.
2. A useful nullity-one superset can exist but be inaccessible under the first-nullity-one stopping rule.
3. An empty root ceiling cone does not imply that no useful larger direction exists.
4. A failed nullity-one child does not imply that every larger principal superset is useless.

### Plausible algorithmic conjectures

1. For small root nullity, direct enumeration of active outside equations should be substantially cheaper than traversing singular
   principal supersets.
2. A* priority $|T|-|I|+\nu(T)-1$ should reach useful terminals with less work than plain BFS on instances where nullity frequently
   stalls or increases.
3. Persistent-kernel detection should be especially valuable on structured matrices with many globally supported kernel
   dependencies.

These are performance conjectures, not correctness claims. They require isolated models and benchmarks before adoption.

## 19. Recommended Next Mathematical Experiment

The theory suggests separating two problems that singular lifting currently mixes together.

For every high-nullity root:

1. compute one exact basis of $\ker A_I$;
2. project every outside row into that small kernel space;
3. compute the persistent kernel $H_I$;
4. if $H_I\neq\{0\}$, extract a global-kernel circuit and store its ceiling certificate;
5. if $H_I=\{0\}$ and the nullity is small, enumerate independent sets of $q-1$ projected outside rows and test the resulting rays;
6. use genuine coordinate-adding singular lifting only when the root-kernel search produces no ceiling direction.

This approach searches the mathematical object of interest directly. It does not walk a large principal-support graph merely to
rediscover active equations that could have been selected inside the root kernel.

The direct root-kernel search is exact for every ceiling direction already present at the root. It must remain distinct from the
more general search for directions that require nonzero newly added coordinates.

## 20. The Free-Index Identity And Its Singularity Content

The earlier analysis in
[`FULL_MATRIX_FACTORIZATION_AND_RHS_DICKINSON_CERTIFICATES.md`](../research/FULL_MATRIX_FACTORIZATION_AND_RHS_DICKINSON_CERTIFICATES.md)
proved the exact identity behind the frequently observed inequality $d\leq n-k$.

Let $I$ be the processed support, $k=|I|$, and let

$$
d=|U|-|L|.
$$

For an ordinary candidate supported inside $I$, define

$$
z=k-|L|,
\qquad
r=n-|U|.
$$

Here $z$ counts zero coordinates inside the processed support and $r$ counts indices excluded from the upper set. Then

$$
\boxed{d=(n-k)+z-r.}
$$

Therefore

$$
\boxed{d>n-k\quad\Longleftrightarrow\quad z>r.}
$$

A zero coordinate is necessary for $d>n-k$, but it is not sufficient: an excluded upper index offsets one zero. If the certificate is
a ceiling certificate, then $r=0$ and

$$
d-(n-k)=z.
$$

The excess over $n-k$ then counts the vanished coordinates exactly.

This zero does **not** imply that the processed principal matrix is singular. The earlier note gives an explicit positive-definite
integer matrix for which $A_I^{-1}\mathbf1$ contains a zero and $d>n-k$. The zero is an algebraic degeneracy of the chosen right-hand
side: one Cramer numerator vanishes while $\det A_I\neq0$.

The singular branch is different. If

$$
A_Iu_I=0
$$

and $L=\operatorname{supp}(u_I)\subsetneq I$, then

$$
A_Lu_L=0.
$$

Thus $A_L$ is singular. For a nullity-one lifted ray, every vanished coordinate exposes a smaller singular principal support, while
the indices in $I\setminus L$ serve only as extra row equations that isolate the ray.

### Theorem 20.1: surviving ceiling violations force a singular principal support

Assume supports are processed in increasing cardinality, the nonsingular candidate at $I$ is $A_I^{-1}\mathbf1$, and traversal
pruning retains only ceiling certificates. If a newly emitted ceiling certificate at $I$ satisfies $d>n-k$, then either $A_I$ is
singular or its smaller true-support matrix $A_L$ is singular.

#### Proof

The inequality $d>n-k$ forces $L\subsetneq I$. If $A_I$ is singular, the claim is immediate. Suppose instead that $A_I$ is
nonsingular. Restricting $A_Iu_I=\mathbf1$ to the rows in $L$ gives

$$
A_Lu_L=\mathbf1.
$$

Assume for contradiction that $A_L$ is also nonsingular. The earlier solve at $L$ then returns exactly $u_L$. Embedding it into the
full space gives the same vector, hence the same ceiling upper set $[n]$.

If $L$ was processed, it stored $[L,[n]]$ before cardinality $|I|$ and therefore suppresses $I$. If $L$ was itself suppressed, the
certificate that suppressed it was another ceiling interval $[F,[n]]$ with $F\subseteq L\subseteq I$, and that interval suppresses
$I$ as well. In either case, no new certificate can be emitted at $I$, a contradiction. Therefore $A_L$ must be singular. $\square$

Consequently, in the ceiling-only increasing-cardinality traversal, every actually emitted certificate satisfying

$$
d>n-k
$$

is tied to singularity even if the processed matrix $A_I$ is nonsingular: either the current branch is singular or the contracted
support $L$ is singular. The raw positive-definite counterexample from the earlier research note remains valid, but it is not an
emitted-traversal counterexample because every principal submatrix is nonsingular and its smaller support produces the same ceiling
certificate first.

The conclusion need not hold for ordinary Dickinson pruning with bounded upper sets, because a bounded interval can suppress $L$
without suppressing $I$.

### The affine companion problem at a singular support

The proof suggests a second search at a singular principal support that is different from the homogeneous kernel-cone search.

Suppose a later nonsingular support $I$ produces

$$
A_Iu_I=\mathbf1
$$

with true support $L\subsetneq I$. Restriction gives

$$
A_Lu_L=\mathbf1.
$$

If $A_L$ is singular, the system is nevertheless consistent. Symmetry gives the equivalent orthogonality condition

$$
\mathbf1\perp\ker A_L.
$$

Equivalently,

$$
\mathbf1^Tz=0
\qquad\text{for every }z\in\ker A_L.
$$

Here **consistent** means that the equations have at least one solution. Singularity means only that a solution cannot be unique; it
does not mean that no solution exists. For a square system $Bx=b$, the possibilities are:

1. $B$ is nonsingular, so there is exactly one solution;
2. $B$ is singular and $b\notin\operatorname{range}(B)$, so there is no solution; or
3. $B$ is singular and $b\in\operatorname{range}(B)$, so there are infinitely many solutions.

The usual exact consistency test is

$$
\operatorname{rank}(B)=\operatorname{rank}([B\mid b]).
$$

Equivalently, $b$ must be orthogonal to every vector in $\ker B^T$. Because $A_L$ is symmetric, this becomes precisely
$\mathbf1\perp\ker A_L$. In the present argument consistency is not a conjecture: the later vector $u_L$ is already an explicit
solution of $A_Lu_L=\mathbf1$.

Every nonzero null direction therefore has mixed signs: a nonzero nonnegative or nonpositive vector cannot have coordinate sum zero.
This explains why the ordinary singular branch can produce a weak mixed-sign kernel certificate while missing the useful affine
candidate.

Every solution is then

$$
x=x_0+Zy,
$$

where $Z$ spans $\ker A_L$. The ceiling conditions become the affine exact inequalities

$$
A_{[n]\setminus L,L}(x_0+Zy)\geq0.
$$

Consistency alone therefore does not establish a ceiling certificate. It supplies the affine family in which one must solve these
outside inequalities. If the hypothesized later candidate is a ceiling candidate, then this feasibility problem is guaranteed to
contain $u_L$, although an arbitrary particular solution $x_0$ need not itself satisfy the inequalities. The search is an exact
polyhedral feasibility problem in $\dim\ker A_L$ variables rather than a traversal through principal supersets of $L$.

All solutions have the same coordinate sum because $\mathbf1^TZy=0$. They also satisfy

$$
x^TA_Lx=x^T\mathbf1,
$$

so movement through the affine kernel changes support and outside products without changing the principal stationary value after the
usual simplex normalization. This is the same non-isolated stationary-family geometry that appears in the FracESSA formulation of the
standard quadratic problem.

Thus a singular support contains two distinct certificate searches:

1. the homogeneous problem $A_Lx=0$, which is the kernel-cone model; and
2. the affine problem $A_Lx=\mathbf1$, when consistent.

Testing only an arbitrary homogeneous null vector can miss the affine certificate that a larger nonsingular support later rediscovers.
An exact consistency test and one particular solution use the same singular factorization. Testing the particular solution first is
cheap; optimizing over the affine kernel is again a low-dimensional exact polyhedral problem.

The phrase **smallest support** requires care. The construction recovers the later candidate on its true support $L$, which is
strictly smaller than the processed support $I$. It does not prove that $L$ is globally minimum. Moving through the affine family can
make further coordinates vanish. If a feasible solution has true support $L'\subsetneq L$, its ceiling interval $[L',[n]]$ strictly
contains $[L,[n]]$ and is therefore a stronger certificate. One may search for such a solution by imposing coordinate equalities
$x_i=0$ while preserving affine feasibility. Greedy deletion yields an inclusion-minimal support in the searched family; finding a
minimum-cardinality support is a separate combinatorial optimization problem.

#### A three-dimensional example

Consider

$$
A=
\begin{pmatrix}
1&1&0\\
1&1&2\\
0&2&0
\end{pmatrix}.
$$

The full matrix is nonsingular, with determinant $-4$, and its unique all-ones solution is

$$
A^{-1}\mathbf1=
\begin{pmatrix}
1/2\\[2pt]1/2\\[2pt]0
\end{pmatrix}.
$$

Thus $I=\{1,2,3\}$ but the true support is $L=\{1,2\}$. The contracted matrix

$$
A_L=
\begin{pmatrix}
1&1\\
1&1
\end{pmatrix}
$$

is singular, while $A_Lx=\mathbf1$ is consistent. Its complete affine solution family is

$$
x(t)=
\begin{pmatrix}
t\\[2pt]1-t
\end{pmatrix}.
$$

The outside product is

$$
A_{\{3\},L}x(t)=2(1-t),
$$

so every $t\leq1$ satisfies the ceiling inequality. The later full-matrix solve selects $t=1/2$, but the earlier singular support can
already find it directly. It can even select $t=1$, whose true support is $\{1\}$ and whose embedded product is

$$
A
\begin{pmatrix}
1\\0\\0
\end{pmatrix}
=
\begin{pmatrix}
1\\1\\0
\end{pmatrix}\geq0.
$$

This produces the stronger ceiling certificate $[\{1\},[3]]$. The example separates the two roles cleanly: the homogeneous kernel
describes directions along the affine family, while the affine right-hand side locates the stationary candidates themselves.

#### Consequence for certificate activation

If a homogeneous certificate found at singular $L$ is activated immediately, it may suppress a larger support $I$ before $I$ exposes
a better affine candidate. Searching the consistent affine family at $L$ first can expose that candidate before any such suppression.
This does not require singular lifting: the candidate is recovered where it already lives. A sound experimental order is therefore:

1. factor $A_L$ and inspect the homogeneous kernel as usual;
2. use the same factorization to test $A_Lx=\mathbf1$ for consistency;
3. if consistent, search $x_0+\ker A_L$ for the outside ceiling inequalities and, optionally, remove feasible coordinates; and
4. only then activate weaker certificates capable of hiding larger supports.

The classification remains exact if this affine search is omitted or bounded, provided ordinary Dickinson traversal remains the
fallback. The shortcut changes when a certificate is discovered, not the mathematical validity of any certificate.

This is a separate experimental idea, not part of the homogeneous kernel-cone model. It is especially targeted at surviving
$d>n-k$ certificates whose processed parent is nonsingular but whose contracted support is singular.

### Relative to an outer lifting root

For a lifted candidate, compare its true support with the outer root $I$. Define

$$
a=|L\setminus I|,
\qquad
z=|I\setminus L|,
\qquad
r=n-|U|.
$$

Then $|L|=k-z+a$ and

$$
\boxed{d-(n-k)=z-a-r.}
$$

For a ceiling certificate,

$$
\boxed{d-(n-k)=z-a.}
$$

This classifies the certificate geometrically:

- $d<n-k$ proves that genuinely new nonzero coordinates outnumber vanished root coordinates;
- $d=n-k$ means that the true support has no net size change;
- $d>n-k$ means that more root coordinates vanished than were added.

The first case cannot be produced by a root-kernel-only search, because every such candidate remains supported inside the root. It is
therefore an exact diagnostic of behavior that requires genuine coordinate-adding lifting.

### Coverage interpretation

The interval contains $2^d$ supports. Relative to the nominal ceiling interval above a full-support size-$k$ candidate, its raw
coverage ratio is

$$
2^{d-(n-k)}=2^{z-r}
$$

for an ordinary candidate. Each internal zero doubles the interval; each excluded upper index halves it.

Dickinson traverses by cardinality, so the more relevant local count is often the number of covered supports in layer $s$:

$$
\left|[L,U]\cap\binom{[n]}s\right|
=
\binom{d}{s-|L|}.
$$

This gives an exact next-layer pruning measure without enumerating the interval.

## 21. When An Early Certificate Can Hide A Better One

Boolean intervals satisfy

$$
[L_2,U_2]\subseteq[L_1,U_1]
$$

exactly when

$$
L_1\subseteq L_2
\qquad\text{and}\qquad
U_2\subseteq U_1.
$$

This gives a precise answer to the concern that using a weak certificate now may suppress a support that would have produced a much
stronger certificate later.

### Theorem 21.1: ceiling dominance

Suppose the active certificate is $[L,[n]]$ and it suppresses a later support $S\supseteq L$. Any certificate $[L',U']$ generated at
$S$ is already dominated whenever $L\subseteq L'$:

$$
[L',U']\subseteq[L,[n]].
$$

Therefore the suppressed support can create new coverage only if its candidate drops at least one index of the old lower set:

$$
L\nsubseteq L'.
$$

In particular, a later full-support candidate with $L'=S$ is always redundant. Under a ceiling certificate, the feared opportunity
loss is entirely a zero-coordinate or support-cancellation phenomenon. This is the direct connection to the $d-(n-k)$ identity.

For a non-ceiling certificate $[L,U]$, even a full-support later candidate can obtain an upper set that extends outside $U$. The same
dominance conclusion does not hold.

### Practical consequence

Certificate validity and certificate activation can be separated. Every valid certificate may be retained for the proof while only
selected certificates are used to prune the traversal. Declining to use a valid certificate cannot make the classification wrong; it
only causes additional supports to be processed.

This permits exact experiments that:

1. activate ceiling certificates immediately;
2. retain only intervals not subsumed by another retained interval;
3. delay weak finite-upper certificates for a bounded number of layers; and
4. spend any look-ahead budget on singular or otherwise degenerate covered supports, because generic full-support descendants of a
   ceiling certificate are provably redundant.

Only the runtime effect of the last two choices is heuristic.

## 22. The Polar Cone And The Right Nullity Split

Let $Z$ be a basis of the $q$-dimensional root kernel and let

$$
G=A_{[n]\setminus I,I}Z.
$$

Write the rows of $G$ as $g_j^T$ and define

$$
Q_I=\operatorname{cone}\{g_j:j\notin I\}.
$$

The root ceiling cone is the polar cone

$$
C_I=Q_I^*
=
\{y:g_j^Ty\geq0\text{ for every }j\notin I\}.
$$

Consequently:

1. its lineality space is

   $$
   \operatorname{lin}(C_I)=\ker G;
   $$

2. ignoring only the final admissibility orientation, it contains a nonzero direction exactly when

   $$
   Q_I\neq\mathbb R^q;
   $$

3. when $G$ has full column rank, $C_I$ is pointed and its extreme rays are polar to the facets of $Q_I$.

The brute-force enumeration of every $(q-1)$-tuple of projected rows therefore tests many duplicate nonfacets. The mathematical
problem is facet enumeration for $Q_I$, or equivalently extreme-ray enumeration for $C_I$. Reverse search and double description are
the standard output-sensitive approaches.

### Persistent and ephemeral nullity

Define

$$
h(I)=\dim\ker A_{[:,I]}.
$$

If $I\subseteq J$, zero extension embeds $\ker A_{[:,I]}$ into $\ker A_{[:,J]}$. Hence

$$
h(I)\leq h(J).
$$

Adding one column changes $h$ by zero or one. Persistent nullity can never disappear. As soon as $h>0$, a global-kernel ceiling
certificate already exists and further lifting is unnecessary for coverage.

The complementary quantity

$$
e(I)=\nu(I)-h(I)
$$

has the exact rank descriptions

$$
e(I)
=
\operatorname{rank}A_{[:,I]}-\operatorname{rank}A_I
=
\operatorname{rank}G.
$$

It counts the principal-kernel directions killed by equations outside the root. The pair $(h,e)$ is more informative than the total
nullity $\nu=h+e$: $h$ supplies immediate global certificates, while $e$ is the dimension on which outside-row cone geometry acts.

### Theorem 22.1: persistent nullity guarantees a sparse ceiling certificate

Let $H\subseteq\mathbb R^k$ be an $h$-dimensional persistent kernel. Then $H$ contains a nonzero vector with at least $h-1$ zero
coordinates. Equivalently, it contains a vector with support size at most

$$
k-h+1.
$$

#### Proof

Represent a basis of $H$ by a full-column-rank matrix $Z_H\in\mathbb R^{k\times h}$. It has $h$ linearly independent rows. Select
$h-1$ of them and impose that the corresponding coordinates of $Z_Hy$ vanish. These are $h-1$ independent homogeneous equations in
$h$ unknowns, so they leave a nonzero solution $y$. The resulting vector $Z_Hy$ has the claimed zeros. $\square$

Every vector in a persistent kernel satisfies $Au=0$, so its upper set is $[n]$. The theorem therefore guarantees a ceiling
certificate with

$$
d\geq n-k+h-1.
$$

At full support this becomes

$$
d\geq h-1.
$$

An arbitrary kernel basis vector need not attain this bound. A fundamental circuit or a greedy exact column-dependence reduction does.
For CP coverage, the implementation should therefore extract a circuit rather than accept the first dense kernel vector. The circuit
may have mixed signs; finding a nonnegative global-kernel vector for the SCP decision is a separate cone-feasibility problem.

### Theorem 22.2: failure has a small positive-spanning witness

If the root ceiling cone is trivial before the admissibility test,

$$
C_I=\{0\},
$$

then the projected rows positively span $\mathbb R^q$. Choose an inclusion-minimal positive-spanning subset. This is a positive basis,
and every positive basis in $\mathbb R^q$ contains at most $2q$ vectors. Therefore at most $2q$ outside rows suffice to prove that no
nonzero root-confined ceiling direction exists.

This does not find a certificate, but it gives a compact exact obstruction that can be cached or reused. A failed brute-force scan
need not be represented by all $n-k$ outside inequalities.

### Small-nullity algorithms

For $q=2$, the nonzero projected rows lie in a plane. The ceiling cone is nonzero exactly when their directions lie in a closed
semicircle. Its boundary rays can be found with an exact angular order using half-plane classification and cross products. This gives
an $O(m\log m)$ exact special case for $m=n-k$ outside rows instead of testing all pairs.

For fixed small $q$, exact low-dimensional linear programming is another option when the algorithm needs only one feasible direction.
Randomized fixed-dimensional methods have expected arithmetic work linear in $m$ for fixed $q$. Enumerating every extreme ray solves a
strictly harder problem and is justified only when several rays are needed to obtain a stronger lower support.

## 23. Flats For Root-Confined Lifts, Circuits For Genuine Lifts

The same projected row configuration $G$ controls two dual kinds of lifting.

### Root-confined lifting

If every added coordinate is fixed to zero, adding outside index $j$ imposes only

$$
g_j^Ty=0
$$

on the root-kernel coordinate $y$. A selected set $J$ produces

$$
F_J=\bigcap_{j\in J}\ker g_j^T.
$$

This subspace depends only on the span of the selected projected rows, not on their order or on which redundant rows were selected.
The zero-coordinate part of the Boolean support graph is therefore a redundant representation of the lattice of flats of the row
matroid of $G$.

One-dimensional root-confined terminals are rank-$(q-1)$ flats. The useful ones are the correctly oriented extreme rays of $C_I$.
Flat, cocircuit, or facet enumeration can replace this whole part of the lifted-support graph exactly.

### Genuine coordinate-adding lifting

Let $J$ be a set of added indices and write a lifted kernel vector as

$$
w=(x,y),
$$

where $x$ is indexed by $I$ and $y$ by $J$. Its first block equation is

$$
A_Ix+A_{I,J}y=0.
$$

Multiplying by $Z^T$ and using symmetry gives

$$
Z^TA_{I,J}y
=
G_J^Ty
=0.
$$

### Theorem 23.1: projected-row dependence is necessary

If the projected rows indexed by $J$ are linearly independent, every kernel vector of $A_{I\cup J}$ has $y=0$. That lifted support can
expose only root-confined directions already represented by $C_I$.

Every genuinely new-coordinate direction requires a dependent set of projected rows. The support of $y$ contains a matroid circuit;
if $y$ is support-minimal as a dependency of $G_J^T$, its support is itself a circuit and has at most $q+1$ elements.

This condition is necessary but not sufficient. The second block equation

$$
A_{J,I}x+A_Jy=0
$$

may reject a projected-row circuit. Circuit enumeration can therefore guide exact lifted solves, but a circuit alone is not a
Dickinson certificate.

### Theorem 23.2: the circuit Schur scalar

Let the projected rows indexed by $J$ form a circuit. Then $G_J$ has row rank $|J|-1$, and its left kernel is spanned by one vector
$y$ with no zero coordinate:

$$
G_J^Ty=0.
$$

Write

$$
B=A_{I,J},
\qquad
C=A_J.
$$

Because $Z^TBy=G_J^Ty=0$, the vector $By$ lies in $\operatorname{range}A_I$. Reuse the root factorization to choose any solution of

$$
A_Ix_0=-By.
$$

Define

$$
\sigma_J
=
y^T(B^Tx_0+Cy).
$$

Then a kernel vector of $A_{I\cup J}$ whose new-coordinate part is proportional to $y$ exists exactly when

$$
\boxed{\sigma_J=0.}
$$

#### Proof

Every solution of the first block equation with new part $y$ is

$$
x=x_0+Z\alpha.
$$

The second block equation becomes

$$
G_J\alpha=-(B^Tx_0+Cy).
$$

Because the left kernel of $G_J$ is $\operatorname{span}\{y\}$, this system is consistent exactly when its right-hand side is
orthogonal to $y$, which is $\sigma_J=0$. The scalar is independent of the chosen $x_0$: changing $x_0$ by $Z\beta$ changes the
parenthesized vector by $G_J\beta$, whose product with $y$ is zero. $\square$

The first block equation also gives

$$
y^TB^Tx_0=-x_0^TA_Ix_0,
$$

so the scalar has the Schur-complement form

$$
\boxed{\sigma_J=y^TCy-x_0^TA_Ix_0.}
$$

This is the multi-index analogue of the one-border Schur scalar in Theorem 3.1. A circuit can therefore be tested with one solve using
the existing root factorization, one exact quadratic scalar, and—only when the scalar vanishes—a solve with the small projected matrix
$G_J$. Factoring the enlarged principal matrix is unnecessary.

The same calculation gives the complete enlarged nullity. Let $c=|J|$. The root-confined solutions have dimension

$$
\dim\ker G_J=q-c+1.
$$

When $\sigma_J\neq0$, the circuit coefficient must be zero and these are all kernel vectors. When $\sigma_J=0$, the circuit coefficient
is one additional free parameter. Hence

$$
\boxed{
\nu(I\cup J)
=
\begin{cases}
q-c+1,&\sigma_J\neq0,\\
q-c+2,&\sigma_J=0.
\end{cases}}
$$

For a generic circuit of size $q+1$, the enlarged principal matrix is nonsingular when $\sigma_J\neq0$ and has nullity one when
$\sigma_J=0$. The order-five counterexample follows the second case: its useful direction appears only when a size-$(q+1)$ projected
circuit satisfies this exact scalar equality.

After denominators are cleared, $\sigma_J=0$ is an algebraic equality in the matrix entries. It is nongeneric under continuous
perturbation but can be enforced repeatedly by combinatorial designs, symmetries, and exact constructions. Deep singular lifting is
therefore principally a structured-matrix technique; generic matrices normally fail the scalar test.

### Theorem 23.3: exact reduced lifting for an arbitrary added set

The circuit scalar generalizes to every added set $J$. Let

$$
r=\operatorname{rank}G_J
$$

and let the columns of $N$ form a basis of

$$
\ker G_J^T.
$$

Because $G_J^TN=0$, every column of $BN$ is orthogonal to $\ker A_I$ and therefore lies in $\operatorname{range}A_I$. Reuse the root
factorization to solve

$$
A_IX=-BN.
$$

Define the reduced symmetric matrix

$$
S_J
=
N^T(B^TX+CN)
=
N^TCN-X^TA_IX.
$$

Then

$$
\boxed{
\nu(A_{I\cup J})
=
q-r+\nu(S_J).}
$$

#### Proof

The first block equation forces the new part to have the form $y=N\beta$. Every corresponding old part is

$$
x=X\beta+Z\alpha.
$$

The second block equation becomes

$$
G_J\alpha=-(B^TX+CN)\beta.
$$

It is solvable exactly when the right-hand side is orthogonal to $\ker G_J^T$, that is, exactly when

$$
S_J\beta=0.
$$

For each $\beta\in\ker S_J$, the solutions for $\alpha$ form an affine space parallel to $\ker G_J$, whose dimension is $q-r$. The
two independent parameter spaces give the stated nullity. Symmetry follows from $A_IX=-BN$, which gives

$$
N^TB^TX=-X^TA_IX.
$$

Changing $X$ by a root-kernel term $ZK$ does not change $S_J$ because $N^TG_J=0$. $\square$

The decomposition has a direct interpretation:

- $q-r$ is the root-confined kernel left after the selected outside equations;
- $\ker S_J$ is exactly the source of genuinely new-coordinate kernel directions.

When $G_J$ has independent rows, $N$ and $S_J$ are empty and only the first term remains. When $J$ is a circuit, $S_J$ is the one-by-
one matrix $[\sigma_J]$. Theorem 23.2 is therefore the smallest nontrivial case of this general reduction.

Computationally, this permits an enlarged support to reuse the root factorization and factor only $S_J$, whose order is

$$
|J|-\operatorname{rank}G_J,
$$

the number of projected-row dependencies. It does not remove the combinatorial choice of $J$, but it can make every chosen lift much
cheaper and exposes the exact part of the lift that creates new coordinates.

### Corollary 23.4: inertia of the enlarged principal matrix

Let the inertia of $A_I$ be

$$
(p_I,n_I,q),
$$

let $r=\operatorname{rank}G_J$, and let the inertia of $S_J$ be

$$
(p_S,n_S,z_S).
$$

Then

$$
\boxed{
\operatorname{inertia}(A_{I\cup J})
=
(p_I+r+p_S,\ n_I+r+n_S,\ q-r+z_S).}
$$

After the nonsingular range of $A_I$ is eliminated by congruence, every independent projected coupling contributes one hyperbolic
pair: one positive and one negative direction. The dependency subspace is governed by $S_J$, while $q-r$ root-kernel directions
remain uncoupled zeros. This proves the formula and recovers Theorem 23.3's nullity component.

The inertia formula does not itself decide copositivity, because a negative eigendirection need not be nonnegative. It does determine
exact factorization behavior and can prevent repeated large rank and nullity calculations.

### Generic depth gap

If the projected rows are in general linear position in $\mathbb R^q$, every set of at most $q$ rows is independent. A genuinely
coordinate-adding direction then needs at least $q+1$ nonzero new coordinates. A root-confined ray, by contrast, can be isolated by
$q-1$ independent active equations.

This creates the structural gap

$$
q-1
\quad\text{versus}\quad
q+1.
$$

The first-nullity-one stopping rule terminates at the first quantity and cannot cross the intervening nullity-one or nonsingular
supports to reach the second. The order-five accessibility counterexample has $q=2$ and first obtains its useful genuine direction
after three added coordinates, exactly $q+1$.

The two search objects are therefore:

- cocircuits or facets for root-confined ceiling directions; and
- circuits as necessary seeds for genuinely coordinate-adding directions.

This projected rank-$q$ configuration is a more natural search space than all principal supersets.

If a circuit has cardinality $c$, every coefficient of its dependency vector is nonzero. A ceiling direction produced from it has at
most $k+c$ nonzero coordinates, so its free-index count satisfies

$$
d\geq n-k-c.
$$

Searching circuits by increasing cardinality therefore maximizes a rigorous lower bound on the interval width of the resulting
ceiling certificate. Under general position $c=q+1$, so the worst-case width loss relative to a root ceiling certificate is only
$q+1$ free indices.

### Theorem 23.5: greedy ceiling support polishing is exact

Start with any admissible ceiling direction in a fixed root kernel. Maintain every coordinate already fixed to zero. For each nonzero
coordinate $i$, ask exactly whether an admissible ceiling direction still exists after also imposing $u_i=0$. When the answer is yes,
replace the current direction and retain all newly obtained zeros. Continue until every remaining coordinate has failed this test.

The final support is inclusion-minimal among admissible ceiling directions satisfying the accumulated constraints.

#### Proof

Suppose a feasible direction with strictly smaller support remained. It would omit at least one coordinate $i$ of the final support.
When $i$ was tested, the algorithm imposed only a subset of the zero constraints present at termination. The hypothetical smaller
direction would therefore have been feasible at that earlier test, contradicting its failure. $\square$

Every successful removal replaces $[L,[n]]$ by a strict superset interval $[L',[n]]$ with $L'\subsetneq L$. The procedure is
order-dependent and need not minimize $|L|$ globally, but it can never weaken the certificate.

## 24. Stronger Practical Conclusions

The theory now suggests the following additions to the original practical list.

1. **Deduplicate flats, not support paths, for root-confined search.** Different lifted supports imposing the same projected-row span
   are the same mathematical state.
2. **Enumerate facets rather than all row combinations.** Extreme rays of the ceiling cone are polar to facets of the projected-row
   cone.
3. **Use a dedicated exact $q=2$ path.** The closed-semicircle test is complete and much smaller than general cone machinery.
4. **Use fixed-dimensional exact feasibility when one direction is enough.** Do not enumerate every extreme ray merely to answer an
   existence question.
5. **Normalize projected rows.** Zero rows impose no inequality; positive proportional rows duplicate an inequality; opposite
   proportional rows together impose an equality. Structured matrices may shrink substantially under this exact reduction.
6. **Stop at persistent nullity.** Once $h>0$, extract a global-kernel circuit and use its ceiling certificate immediately.
7. **Polish a found ceiling support.** Repeatedly impose $u_i=0$ whenever the ceiling cone remains admissibly feasible. Every successful
   removal strictly enlarges the ceiling interval. When no coordinate can be removed, the result is inclusion-minimal within the
   searched cone, although not necessarily minimum-cardinality.
8. **Keep an interval antichain.** Discard $[L_2,U_2]$ whenever another retained interval has $L_1\subseteq L_2$ and $U_2\subseteq U_1$.
9. **Separate proof storage from pruning activation.** Store every valid certificate, but test bounded delayed activation for weak
   finite-upper intervals.
10. **Probe covered supports only where improvement is possible.** Under a ceiling certificate, target singular supports capable of
    dropping an old lower-set index; generic full-support descendants are dominated.
11. **Use projected-row independence as a no-new-coordinate result.** An independent $G_J$ proves that support $I\cup J$ cannot add
    genuinely participating coordinates.
12. **Try circuit-guided jumps before arbitrary deep lifting.** Small projected-row circuits are the first supports on which genuine
    new coordinates can occur. Test each with the circuit Schur scalar while reusing the root factorization; factor an enlarged
    principal matrix only when the reduced test leaves additional work. Retain ordinary Dickinson as the correctness fallback.
13. **Use the reduced lift matrix for larger added sets.** Factor $S_J$, whose order is the dependency dimension
    $|J|-\operatorname{rank}G_J$, instead of refactoring $A_{I\cup J}$ from scratch.
14. **Use modular rejection before large exact zero tests.** A nonzero modular determinant or circuit Schur scalar proves the exact
    integer quantity nonzero. Only modular zeros need arbitrary-precision confirmation. This is exact-safe and especially relevant
    when matrix entries have hundreds of digits.
15. **Record structural diagnostics.** Useful quantities are $(q,h,e)$, distinct projected rays, facet counts, circuit sizes,
    $d-(n-k)$, and the next-layer binomial coverage. These separate arithmetic cost from degeneracy and pruning power.
16. **Search the affine companion before activating a singular certificate.** When $A_L$ is singular, use its existing exact
    factorization to test $A_Lx=\mathbf1$ for consistency. If it is consistent, search $x_0+\ker A_L$ for the outside ceiling
    inequalities and remove feasible coordinates before allowing a weaker homogeneous certificate to suppress supersets. This can
    recover at $L$ a certificate that ordinary traversal would expose only at a later nonsingular support.

Items 1–8, 11, 13, 14, and the consistency reduction in item 16 are exact transformations or deductions. Items 9, 10, 12, and the
activation order in item 16 are search policies: they preserve exactness because they only change which valid certificates are
activated early or which extra supports are probed before the unchanged fallback.

## 25. Additional Results And Remaining Limits

The following statements are now proved.

1. The inequality $d\leq n-k$ is an exact balance between internal zeros and upper-set exclusions, not a universal certificate law.
2. For a ceiling certificate, $d-(n-k)$ counts vanished coordinates relative to the processed support.
3. Relative to an outer root, the same difference counts vanished root coordinates minus genuinely added coordinates.
4. A raw zero in a nonsingular principal solve need not imply singularity, but a surviving ceiling violation forces singularity either
   at the processed support or at its contracted true support.
5. A consistent singular affine system $A_Lx=\mathbf1$ can reproduce a certificate otherwise found only at a larger nonsingular
   support.
6. A ceiling certificate can hide a non-dominated later certificate only if the later lower support drops an old lower-set index.
7. Persistent nullity is monotone under support enlargement and guarantees a ceiling vector with at least $h-1$ zeros.
8. Ephemeral nullity is the rank of the projected outside-row system.
9. A trivial root ceiling cone has a positive-spanning obstruction using at most $2q$ projected rows.
10. Zero-coordinate root lifting factors through the flat lattice of the projected-row matroid.
11. A genuinely new-coordinate direction requires a projected-row dependency.
12. A projected-row circuit extends to a true lifted kernel direction exactly when its circuit Schur scalar vanishes.
13. For an arbitrary added set, the genuinely new part of the enlarged kernel is exactly $\ker S_J$, and the full enlarged nullity is
    $q-\operatorname{rank}G_J+\nu(S_J)$.
14. The same reduction gives the complete enlarged inertia by adding one positive and one negative direction for every independent
    projected coupling and then adding the inertia of $S_J$.
15. Under general position, a genuinely new-coordinate direction requires at least $q+1$ nonzero new coordinates.
16. Greedy exact coordinate elimination produces an inclusion-minimal ceiling support in the searched cone.

Two limits remain important.

1. A projected-row circuit need not satisfy the second block equation, so circuit-guided jumps are not a complete replacement for all
   genuine coordinate-adding lifts.
2. Globally minimizing $|L|$ is a sparse feasible-vector problem. Greedy support polishing produces an inclusion-minimal support, not
   necessarily one of minimum cardinality.

## 26. Ceiling Certificates As A Hypergraph

Let $\mathcal F$ be the family of retained ceiling lower supports. A future support $S$ remains uncovered exactly when

$$
L\nsubseteq S
\qquad\text{for every }L\in\mathcal F.
$$

Thus $\mathcal F$ is a hypergraph and the uncovered supports are its independent sets. Equivalently, the complement $[n]\setminus S$
must intersect every $L\in\mathcal F$, so complements of uncovered supports are hypergraph transversals or hitting sets.

This reformulation has three consequences.

1. Inclusion-minimal retained lower supports form a hypergraph clutter. A lower support containing another retained lower support is
   exactly redundant.
2. High-cardinality uncovered supports correspond to small hitting sets. A complement-based generator may therefore be preferable in
   the top Boolean layers.
3. Certificate width alone cannot predict runtime. The overlap pattern of the hyperedges determines marginal coverage and decision-
   diagram size.

### Rigorous remaining-work bounds

At cardinality $s$, a ceiling certificate with lower support $L$ covers

$$
\binom{n-|L|}{s-|L|}
$$

supports. Therefore the number $R_s$ of supports not covered by the current certificates satisfies the union-bound lower estimate

$$
R_s
\geq
\binom ns
-
\sum_{L\in\mathcal F}
\binom{n-|L|}{s-|L|},
$$

clipped at zero.

This is a rigorous lower bound on future emitted supports if no additional certificate is found. It can prove that a quick finish is
impossible under the current coverage, independently of processor speed.

Pairwise overlap gives the next Bonferroni correction. Two ceiling intervals intersect in

$$
[L_1\cup L_2,[n]],
$$

which covers

$$
\binom{n-|L_1\cup L_2|}{s-|L_1\cup L_2|}
$$

supports in layer $s$. Using the single and pair counts bounds the covered union from both sides. Computing all pairs may itself be
expensive, but the largest or newest certificates can provide a cheap partial bound.

For a general Dickinson interval $[L,U]$, the same layer count is

$$
\binom{|U|-|L|}{s-|L|}.
$$

Two general intervals intersect exactly when

$$
L_1\cup L_2\subseteq U_1\cap U_2,
$$

and the intersection is then

$$
[L_1\cup L_2,U_1\cap U_2].
$$

These formulas turn the stored certificate distribution into exact combinatorial work bounds rather than a purely empirical runtime
correlate.

## References

1. Peter J. C. Dickinson, “A New Certificate for Copositivity,” *Linear Algebra and its Applications* 569 (2019), 15–37.
   [Accepted manuscript](https://ris.utwente.nl/ws/files/87825628/cop_cert.pdf).
2. Robert Brijder and Hendrik Jan Hoogeboom, “Nullity and Loop Complementation for Delta-Matroids,” *SIAM Journal on Discrete
   Mathematics* 27 (2013), 492–506.
   [Author manuscript](https://documentserver.uhasselt.be/bitstream/1942/15199/1/nullity_loopc_dmatroid.pdf).
3. Anders Björner, Michel Las Vergnas, Bernd Sturmfels, Neil White, and Günter M. Ziegler, *Oriented Matroids*, second edition,
   Cambridge University Press.
   [Publisher excerpt](https://assets.cambridge.org/97805217/77506/excerpt/9780521777506_excerpt.pdf).
4. Arkadi Nemirovski, *Mathematical Essentials*, Theorem II.9.6 on extreme directions of polyhedral cones.
   [Manuscript](https://www2.isye.gatech.edu/~nemirovs/CUPProjectIni.pdf).
5. Walter B. McRae and Ernest R. Davidson, “An Algorithm for the Extreme Rays of a Pointed Convex Polyhedral Cone,”
   *SIAM Journal on Computing*.
   [DOI page](https://epubs.siam.org/doi/10.1137/0202023).
6. Raimund Seidel, “Small-Dimensional Linear Programming and Convex Hulls Made Easy,” *Discrete & Computational Geometry* 6
   (1991), 423–434.
   [EuDML record and full text](https://eudml.org/doc/131168).
7. David Avis and Komei Fukuda, “A Pivoting Algorithm for Convex Hulls and Vertex Enumeration of Arrangements and Polyhedra,”
   *Discrete & Computational Geometry* 8 (1992), 295–313.
8. Leonid Khachiyan, Endre Boros, Khaled Elbassioni, Vladimir Gurvich, and Kazuhisa Makino, “On the Complexity of Some Enumeration
   Problems for Matroids,” *SIAM Journal on Discrete Mathematics* 19 (2005), 966–984.
   [DOI record](https://doi.org/10.1137/S0895480103428338).
9. Charles Audet, “A Short Proof on the Cardinality of Maximal Positive Bases,” *Optimization Letters* 5 (2011), 191–194.
   [DOI record](https://doi.org/10.1007/s11590-010-0229-3).
