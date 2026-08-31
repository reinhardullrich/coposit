# Fractional MILP

Fractional MILP is a reusable mixed-integer linear-programming tool. It keeps the problem data as FLINT arbitrary-precision integers,
uses floating-point LP relaxations to guide branch-and-bound, and accepts mathematical decisions only after recomputing them with
FLINT rational arithmetic.

It is shared infrastructure under `coposit::fractional_milp`; it is not a copositivity model.

## The safety rule

> **A floating-point result may choose where to search, but it may never prove feasibility, infeasibility, unboundedness, optimality,
> or a node bound used for pruning. Every such decision is made again with exact rational arithmetic.**

This separation is the point of the solver. A wrong floating-point estimate may cause extra branching or an unnecessary exact LP
solve, but it cannot remove a valid solution or create a false proof.

## Supported problems

The first implementation maximizes an integer linear objective subject to:

- integer linear constraints with `<=`, `=`, or `>=` senses;
- nonnegative continuous variables;
- binary variables;
- optional nonnegative integer upper bounds.

Rational coefficients do not require a rational input matrix. Multiply each constraint by a positive common denominator, and multiply
the objective by a positive common denominator if necessary. The resulting integer MILP has the same feasible set and optimizers;
only the objective scale may change.

## Search loop

At each branch-and-bound node, the solver:

1. fixes the binary variables selected by the path to that node;
2. solves the resulting LP relaxation in `double` arithmetic;
3. uses a fractional binary value only to select the next branch;
4. invokes the FLINT rational simplex whenever the floating LP suggests a prune, appears integral, reports infeasibility, or reports
   unboundedness;
5. prunes or accepts a solution only from that exact result.

If a floating solution is fractional and its estimated bound appears capable of improving the incumbent, the solver may branch
without first running the exact LP. This is safe because both binary children are retained. The floating result changes only their
order, never the covered search space.

The solver returns one of five outcomes:

- `optimal`: the incumbent and every pruning bound were established exactly;
- `positive_objective_found`: the optional early-stop rule found an exactly feasible integral point with an exactly positive
  objective; this certifies existence, not optimality;
- `infeasible`: exact arithmetic eliminated the complete tree without finding a feasible integer point;
- `unbounded`: an exactly unbounded node remained after all binary variables relevant to that node were fixed;
- `interrupted`: a node limit, deadline, or global timeout stopped the search before a proof was complete.

`interrupted` is deliberately distinct from every mathematical answer.

The `stop_on_positive_objective` option enables the second outcome. A floating-point objective can never trigger it.

## Exact arithmetic

The public model stores coefficients as `coposit::integer`, backed by FLINT `fmpz`. Exact LP tableaux use canonical FLINT `fmpq`
fractions internally. Returned points and objective values contain reduced numerators and positive denominators.

Each constraint is independently scaled into a moderate floating-point range for the advisory LP. The unscaled integer constraint is
preserved for the exact LP.

## Current limits

This is the smallest complete solver needed for the current experiments. It does not yet implement general integer variables,
negative variable lower bounds, cutting planes, presolve, warm starts, or reuse of a floating LP basis for exact certification. The
exact simplex currently resolves a node from its integer model when certification is required.

Those are performance extensions, not correctness gaps. Add them only after measurements show where Fractional MILP spends its time;
every future cut, presolve reduction, and cached bound must obey the same exact-verification rule.
