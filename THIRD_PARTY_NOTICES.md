# Third-Party Notices

coposit is licensed under GPL-3.0-or-later.

## Maintained Build

| Component | Use | License | Source |
|---|---|---|---|
| FLINT | Exact integer arithmetic and matrices | LGPL-3.0-or-later | <https://github.com/flintlib/flint> |
| GMP | FLINT dependency | LGPL-3.0-only OR GPL-2.0-only | <https://gmplib.org/> |
| MPFR | FLINT dependency | LGPL-3.0-or-later | <https://www.mpfr.org/> |
| pybind11 3.0.4 | Python extension binding fetched by CMake | BSD-3-Clause | <https://github.com/pybind/pybind11/tree/v3.0.4> |
| GoogleTest 1.14.0 | Test-only dependency fetched by CMake | BSD-3-Clause | <https://github.com/google/googletest/tree/v1.14.0> |

The immediate-integer Bareiss update in `cpp/include/coposit/fraction_free_ldlt.hpp` is adapted from Fredrik Johansson's
`fmpz_mat_fflu` implementation in FLINT and retains its LGPL-3.0-or-later notice in the header.

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
