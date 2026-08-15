# Literature-Reported Solves

**Status:** current evidence audit, 2026-08-14  
**Database field:** `matrices.references_solved`

## Meaning

`references_solved` records papers that report a completed copositivity, strict-copositivity, or mathematically equivalent global
standard-quadratic-program decision for the identified matrix. It is a JSON array of objects:

```json
[
  {"source_id": 31},
  {"source_id": 40, "comment": "numerical Horn variational-index screening; not an exact certificate"}
]
```

`source_id` is required and refers to `sources.source_id`; `comment` is present only when the nature of the claim needs qualification.
The array is ordered by publication year and source ID. An empty array means that this audit found no identifiable completed claim. It
does **not** prove that the matrix has never been solved.

This field is deliberately separate from `source_id` and `additional_source_ids`: origin, mention, and reported successful solution are
different claims.

## Audit rule

A reference was entered when a paper's prose or result table identifies the matrix and reports a decision. The authors' claim is
recorded without independently rerunning their software. Exact global minimization of $x^T A x$ over the simplex is included because
the optimum's sign decides copositivity. Numerical, tolerance-based, or heuristic reports remain included when the paper presents them
as a result, but the object has a comment stating that qualification.

Positive scalar multiples inherit the same reference because they encode the same sign decision. References were not propagated merely
through a simultaneous row/column permutation: several papers explicitly report strong permutation-dependent runtime, and a family
mention alone does not identify a coefficient array.

The audit excludes:

- a timeout, question mark, or unresolved table entry;
- a relaxation bound that the paper does not claim is exact or decisive;
- a completely-positive-cone test that does not decide copositivity of the stored input;
- generated experiments whose random seed or exact coefficient array is unavailable; and
- aggregate success counts when the paper does not identify which individual instances failed.

## Populated evidence

After exact matrix deduplication, the 407 nonempty rows contain 619 unique paper–matrix-class claims from 23 sources. All 968
pre-deduplication occurrence claims were unioned into these survivor objects. The database is the authoritative matrix-level record. The main
audited groups are:

| Source IDs | Evidence recorded |
|---:|---|
| 23, 26, 27 | Bomze–de Klerk and Bundfuss/Dür/Žilinskas exact StQP and adaptive-copositive-program tables, including Q1–Q4 only where the reported bound or run completed |
| 30–36 | Sponsel; Bomze–Eichfelder; Deng; Tanaka–Yoshise; Brás–Eichfelder–Júdice; and Nie–Yang–Zhang direct copositivity tables |
| 39, 44, 60, 72, 76 | Completed global StQP solves by Liuzzi–Locatelli–Piccialli, Gondzio–Yıldırım, Scozzari–Tardella, Xia–Vera–Zuluaga, and Ahmadi–Hall–Papachristodoulou |
| 40 | Keys–Zhou–Lange Horn variational-index screening, explicitly marked as non-certificate numerical evidence |
| 43, 46–49 | Anstreicher; G.-Tóth–Hendrix–Casado; Safi–Nabavi–Caron; Ferreira et al.; and Júdice–Sessa–Fukushima result tables |
| 55, 67 | Hou–Tang–Toh's exact second-order result for the order-21 extended Horn matrix and Yang–Xu–Li's order-9 generalized Horn result |

Representative qualifications retained in the JSON objects include numerical tolerance, roundoff thresholds, heuristic positive
decisions, and equivalent global-StQP solves.

## Deliberate non-assignments

- Žilinskas's larger Table 3 reports no completed optimum, so those exact graph matrices were not marked.
- Sponsel's `penaCopos` run did not terminate, so it was not marked for that paper.
- Tanaka–Yoshise's unresolved G8 boundary at 3.0 and G12 boundary at 4.0 were excluded from their respective tables.
- Gondzio–Yıldırım's eight-member DIMACS1 group was assigned because every member completed; their 22-member DIMACS2 aggregate does
  not identify the single ILP timeout, so that aggregate was not guessed onto individual rows.
- Ferreira et al. say that “most” repository matrices were classified, but only the individually named table cases were assigned.
- Júdice–Sessa–Fukushima question-mark and numerical-trouble rows were excluded; their completed rows were retained.
- Xia–Vera–Zuluaga solve 13 of the 14 Scozzari–Tardella files in their table; the timed-out order-1000 file has their source absent.
- Papers that only construct exceptional matrices, prove hierarchy behavior, or test complete positivity were not treated as solver
  claims.

## Reproduction and checks

`testdata/archive/add_literature_solved_references_2026_08_14.py` is the guarded, idempotent migration. It accepts the guarded
3,490-row pre-deduplication or 3,157-row retained corpus, validates all 94 sources, propagates only exact positive scaling, preserves
merged survivor claims, validates every JSON object, and runs the foreign
key check. The completed database also passed `PRAGMA integrity_check`, has no duplicate `(matrix_id, source_id)` claims, and contains
only the keys `source_id` and optional `comment`.

A second independent selector audit corrected three omissions: the Hou–Tang–Toh result now points to the actual order-21 extended Horn
row (matrix 10064), Sponsel's two portfolio test matrices and the corresponding completed optimum evidence are included, and all eight
individually identifiable DIMACS1 instances solved in Gondzio–Yıldırım's Table 8 are included.
