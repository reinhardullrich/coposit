# Hadeler-Based Models

This directory is the canonical source location for all models that inherit the principal-support approach of Hadeler, Dickinson,
or FracESSA. Model identifiers, command-line names, Python names, and stored benchmark identities do not depend on this directory
grouping and remain unchanged.

Each model owns its implementation, focused test, and authoritative `ALGORITHM.md`.

| Lineage | Models |
|---|---|
| Literature roots and direct support traversal | `hadeler_1983`, `dickinson_2019`, `dense_bitset_dickinson`, `interval_recursive_dickinson`, `support_pruned_dickinson`, `nullity_support_pruned_dickinson`, `rhs_dickinson` |
| Boolean-lattice representations | `bdd_dickinson`, `zdd_dickinson`, `cbdd_dickinson`, `cbdd_halfspace_dickinson`, `upper_endpoint_cbdd_dickinson`, `cbdd_dickinson_improved_1`, `wide_certificate_cbdd_dickinson`, `multithreaded_cbdd_dickinson`, `czdd_dickinson`, `sat_dickinson`, `sat_halfspace_dickinson`, `sat_halfspace_rays_dickinson`, `sat_b1`, `sat_b2`, `sat_a1`, `sat_a2`, `sat_a3`, `sat_a4`, `sat_a5`, `sat_halfspace_lp_dickinson`, `sat_halfspace_milp_dickinson`, `sat_halfspace_rays_lookahead_dickinson`, `sat_halfspace_rays_wide_dickinson`, `wide_certificate_sat_dickinson`, `xxx`, `xxx_two`, `clingo_dickinson`, `clingo_halfspace_dickinson` |
| Ceiling and singular-support searches | `ceiling_pruned_dickinson`, `kernel_cone_dickinson`, `affine_companion_dickinson`, `layered_singular_lift_dickinson`, `breadth_first_singular_lift_dickinson` |
| Frank–Wolfe proposals before Dickinson | `frank_wolfe_dickinson`, `one_step_frank_wolfe_dickinson`, `pairwise_frank_wolfe_dickinson`, `support_polished_frank_wolfe_dickinson` |
| FracESSA and Zischg descendants | `fracessa`, `zischg_hadeler`, `zischg_dickinson`, `zischg_fracessa` |
