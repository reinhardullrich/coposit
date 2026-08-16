# Literature-Reported Unsolved Cases

**Status:** current evidence audit, 2026-08-16
**Database field:** `matrices.references_unsolved`

## Meaning

`references_unsolved` records papers that explicitly report a failed solve for an identified matrix. It is a JSON array of objects:

```json
[
  {"source_id": 30, "comment": "M=N did not terminate within 30 minutes; M=H and M=S++N completed"}
]
```

`source_id` refers to `sources.source_id`. The required comment names the failing method and failure mode. An empty array means only
that this audit found no identifiable failure claim; it does not prove that every published method solves the matrix.

This field is deliberately separate from origin, mention, and `references_solved`. The same paper may occur in both solved and
unsolved arrays when one method or variant completed and another timed out, exhausted memory, had numerical trouble, or returned a
wrong result. In the current corpus, 149 of the 173 matrices with an unsolved claim also have at least one solved claim.

## Audit rule

A claim was entered only when the paper identifies the matrix or an all-member group and explicitly reports one of these outcomes:

- a time or memory limit without the required result;
- numerical failure or an inconclusive result;
- a nonnegative heuristic outcome that the paper explicitly calls a guess or undecidable;
- failure to prove the claimed global optimum, even when a negative upper bound still happened to prove non-copositivity; or
- a result that the paper's known answer shows is wrong.

No claim was inferred from omission, a blank table cell with no defined meaning, or the mere fact that another method was faster.
Aggregate failures were assigned to individual matrices only when the paper says that every member of the group failed. Unlike
`references_solved`, failure claims are not propagated to other retained positive scalar multiples: numerical solver behavior can
change under scaling. Claims already attached to occurrences merged by corpus deduplication remain on their common survivor.

## Populated evidence

The 197 nonempty rows contain 256 matrix–paper failure claims from 14 sources:

| Source | Matrices | Explicit evidence |
|---:|---:|---|
| 27 | 29 | Žilinskas's named DIMACS copositive-program runs reached their limits without a guaranteed optimum |
| 30 | 4 | Sponsel inner-cone variants timed out on graph12, Peña, and hamming6-2 cases |
| 33, 34 | 13, 22 | Tanaka–Yoshise variants did not terminate within six hours on named graph thresholds |
| 35 | 25 | Brás et al. LCP/MIP/BARON limits, including inconclusive classifications and global-optimum timeouts |
| 39 | 6 | Liuzzi et al. binary-branching variants ran out of memory on all six order-500 density-0.25 instances |
| 44 | 76 | Gondzio–Yıldırım all-member failures in ST100, ST200, ST500, ST1000, DIMACS1, and BSU orders 10–24 |
| 47 | 12 | Safi et al. report that BD failed where SNC completed |
| 48 | 5 | Ferreira et al. obtained only nonnegative randomized heuristic outputs, which the paper says permit a guess but not a decision |
| 49 | 24 | Júdice et al. time, memory, and numerical failures plus three wrong B&B boundary classifications |
| 71 | 4 | Dobre–Vera SDP hierarchy levels hit memory or 8,000-second limits |
| 72 | 17 | Xia et al. StableQP and Scozzari–Tardella solver timeouts |
| 97 | 17 | Aragón-Artacho et al. reached critical points on the copositive boundary, an outcome their test explicitly declares undecidable |

The three explicitly wrong outcomes are Júdice et al.'s benchmark B&B results for the boundary matrices derived from
`c-fat200-1`, `c-fat200-2`, and `c-fat200-5`. The paper's proposed Algorithm 4 correctly solved those same matrices, so source 49 is
present in both arrays with different comments.

## Deliberate non-assignments

- Bundfuss–Dür say that “all other” tested DIMACS instances produced poor bounds, but do not give a complete attempted-instance list.
- Gondzio–Yıldırım report partial failures for BLST and DIMACS2 without identifying every failed member; those aggregates were not
  guessed onto individual rows.
- Tanaka–Yoshise's random-instance tables report averages for unseeded generated matrices, so no exact stored coefficient array can
  be linked.
- Júdice et al.'s named Morgen and Jagota failures were retained as paper evidence but cannot be attached because those exact matrices
  are not in the current corpus.
- Hou et al.'s large random failures have no reproducible seed or matching stored array. Nishijima et al.'s failures concern complete
  positivity rather than copositivity of the stored input.
- General statements such as “most matrices were classified” and single-failure aggregate counts with no named row were excluded.
- Keys–Zhou–Lange's positive Horn table is an approximation experiment on matrices already known to be copositive, not an explicitly
  failed decision, so it is neither solved nor unsolved.

## Reproduction and checks

`testdata/archive/add_literature_unsolved_references_2026_08_14.py` is the guarded, idempotent migration. It requires the retained
3,157-matrix, 94-source corpus, validates every object and source, and runs the foreign-key check. A second dry run reports zero
pending rows. The completed database has no duplicate `(matrix_id, source_id)` pair, no malformed object, and passes
`PRAGMA integrity_check`.

`testdata/archive/correct_literature_reference_semantics_2026_08_16.py` adds the explicitly inconclusive Ferreira and
Aragón-Artacho rows while leaving screens, mentions, and unattempted cases out of both arrays.
