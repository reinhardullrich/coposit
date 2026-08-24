# Third-Party Notices

coposit is licensed under GPL-3.0-or-later.

## Maintained Build

| Component | Use | License | Source |
|---|---|---|---|
| FLINT | Exact integer arithmetic and matrices | LGPL-3.0-or-later | <https://github.com/flintlib/flint> |
| GMP | FLINT dependency | LGPL-3.0-only OR GPL-2.0-only | <https://gmplib.org/> |
| MPFR | FLINT dependency | LGPL-3.0-or-later | <https://www.mpfr.org/> |
| GoogleTest 1.14.0 | Test-only dependency fetched by CMake | BSD-3-Clause | <https://github.com/google/googletest/tree/v1.14.0> |
| CaDiCaL 2.2.1 | Incremental SAT engine for the `sat_dickinson` experiment | MIT | <https://github.com/arminbiere/cadical/tree/rel-2.2.1> |
| NBC MiniSat All 1.0.2 | Boolean support enumeration for `nbc_b6` and `nbc_b7` | MIT | <https://www.sd.is.uec.ac.jp/toda/code/nbc_minisat_all.html> |
| Improved NBC MiniSat All derivative | Resumable Boolean support enumeration for `improved_nbc_b7`, `improved_nbc_b8`, and `improved_nbc_b9` | MIT | Derived locally from NBC MiniSat All 1.0.2 |
| clingo 5.8.2 with clasp 3.4.1 | Backtracking support enumeration for `clasp_b3`, `clingo_dickinson`, and `clingo_halfspace_dickinson` | MIT | <https://github.com/potassco/clingo/tree/v5.8.2> |
| Open MCS | MCS maximum-clique search adapted for the Motzkin--Straus pre-check | GPL-3.0-or-later | <https://github.com/darrenstrash/open-mcs/tree/735788af066fc8589f577036af521f22f45c2731> |

The immediate-integer Bareiss update in `cpp/include/coposit/fraction_free_ldlt.hpp` is adapted from Fredrik Johansson's
`fmpz_mat_fflu` implementation in FLINT and retains its LGPL-3.0-or-later notice in the header.

`cpp/third_party/nbc_minisat_all/` adapts Takahisa Toda's NBC MiniSat All 1.0.2, based on MiniSat-C 1.14.1. The local changes remove
the command-line interface and expose model and termination callbacks for exact-cardinality enumeration. The original notices are
preserved as `LICENSE.nbc_minisat_all` and `LICENSE.MiniSat` in that directory.

`cpp/third_party/improved_nbc_minisat_all/` is a separate copy of that adaptation. It adds a defined repeated-call reset boundary,
retains logically valid learned clauses, and latches only permanent root-level inconsistency. The same MIT notices are preserved in
that directory.

`cpp/include/coposit/open_mcs.hpp` adapts Darren Strash's Open MCS implementation at commit
`735788af066fc8589f577036af521f22f45c2731`. It retains the MCR initial ordering, greedy coloring bound, static-order traversal, and
MCS recoloring rule. The coposit integration replaces the executable and container machinery with its packed adjacency, cooperative
timeout, diagnostics, unrestricted-size index type, and exact decision-threshold stop.

The `dutour_2018` model is an independent FLINT-integer adaptation of the mathematics in Mathieu Dutour Sikirić's
`PairDecomposition` and `TestStrictCopositivity` in Polyhedral Common. It was verified against commit
`d2252bc89d991fa6df9750ac9647e19b6a9aca02`; no Polyhedral Common source code is copied into the maintained model.

The `bundfuss_2008` model is an independent exact implementation of the Bundfuss simplicial-partition mathematics. Its formulas and
control flow were checked against J. M. G. Salmerón's 2018 public implementation, commit
`5537fd94768efbce85b3225b05bf39db8d81a332`; no source from that repository is copied into the maintained model.

`research/data/mckay_connected_graphs/` preserves Brendan McKay's graph6 catalogs of non-isomorphic connected graphs used to
generate the Hoffman-Pereira corpus rows. The source page at <https://users.cecs.anu.edu.au/~bdm/data/graphs.html> licenses these
graph data under Creative Commons Attribution 4.0.

## Preserved Experiments

`experiments/copositivity_bundfuss_flint_2026-08-07/src/` contains a copied upstream research implementation. No license file was
captured with that local snapshot; its redistribution terms remain unresolved. The independent FLINT adaptation is under `flint/`.
