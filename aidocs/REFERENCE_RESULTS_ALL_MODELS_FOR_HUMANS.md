# All-Model Core/Stress Results for Humans

Last updated: 2026-08-17

This comparison uses 512 Core/Stress matrices. We admitted only matrices for which the shared preprocessing pipeline had already
reported that it could not complete the classification. When a normal solver call is made, preprocessing still runs first and only
the unresolved matrix components are passed to the selected algorithm. The table therefore compares the algorithms on the work
that preprocessing leaves behind, instead of rewarding every algorithm equally for cases already solved before its own code runs.

Preprocessing finds negative witnesses particularly well. It consequently removes almost all known non-copositive matrices from
this comparison; only 19 known non-copositive cases remain among the 512 matrices. Most of the retained set is copositive or strictly
copositive and therefore requires an algorithm to establish a positive result rather than merely find one counterexample.

Earlier comparisons did not apply this filter and still contained many matrices that the current preprocessing now solves by
itself. Whether these mostly easy non-copositive cases are included shifts the ranking of the literature baseline algorithms quite
drastically. Results and rankings from the older test composition should therefore not be transferred directly to this table.

Every matrix has a five-second timeout.

Hadeler-based models were run in `both` mode, where `solved` means a complete copositivity and strict-copositivity classification.
The completed baseline and other experimental models were run for ordinary copositivity only. Their solved counts therefore answer
a narrower question and are not directly equivalent to the Hadeler-based counts.

Color marks the algorithm family, while a thin black rectangle marks a literature baseline:

- <span style="background:#dff4df; padding:0.1em 0.3em;">light green</span>: cone-based;
- <span style="background:#e7eef8; padding:0.1em 0.3em;">light blue</span>: Hadeler-based;
- <span style="border:1px solid #222; padding:0.1em 0.3em;">thin black rectangle</span>: literature baseline.

<table style="border-collapse:collapse;">
  <thead>
    <tr><th>Approach</th><th>Solved instances</th><th>Models</th></tr>
  </thead>
  <tbody style="border:2px solid #777;">
    <tr><td rowspan="3"><strong>Dickinson-based</strong><br><small>avoid enumerating every support individually</small></td><td align="right">457</td><td><code style="background:#e7eef8;">bdd_dickinson</code>, <code style="background:#e7eef8;">cbdd_dickinson</code>, <code style="background:#e7eef8;">cbdd_dickinson_improved_1</code>, <code style="background:#e7eef8;">clingo_dickinson</code>, <code style="background:#e7eef8;">czdd_dickinson</code>, <code style="background:#e7eef8;">sat_halfspace_dickinson</code>, <code style="background:#e7eef8;">zdd_dickinson</code></td></tr>
    <tr><td align="right">456</td><td><code style="background:#e7eef8;">sat_dickinson</code>, <code style="background:#e7eef8;">upper_endpoint_cbdd_dickinson</code></td></tr>
    <tr><td align="right">452</td><td><code style="background:#e7eef8;">affine_companion_dickinson</code>, <code style="background:#e7eef8;">breadth_first_singular_lift_dickinson</code>, <code style="background:#e7eef8;">ceiling_pruned_dickinson</code>, <code style="background:#e7eef8;">kernel_cone_dickinson</code>, <code style="background:#e7eef8;">layered_singular_lift_dickinson</code>, <code style="background:#e7eef8;">wide_certificate_cbdd_dickinson</code>, <code style="background:#e7eef8;">wide_certificate_sat_dickinson</code></td></tr>
  </tbody>
  <tbody style="border:2px solid #777;">
    <tr><td rowspan="2"><strong>Best-performing cone algorithms</strong><br><small>in this test</small></td><td align="right">405</td><td><code style="background:#dff4df;">adaptive_zischg_sponsel_copomatrix</code></td></tr>
    <tr><td align="right">403</td><td><code style="background:#dff4df;">adaptive_sponsel_copomatrix</code></td></tr>
  </tbody>
  <tbody style="border:2px solid #777;">
    <tr><td rowspan="8"><strong>Explicit-support Hadeler/Dickinson</strong></td><td align="right">381</td><td><code style="background:#e7eef8;">support_polished_frank_wolfe_dickinson</code></td></tr>
    <tr><td align="right">380</td><td><code style="background:#e7eef8;border:1px solid #222;padding:0 0.15em;">dickinson_2019</code>, <code style="background:#e7eef8;">frank_wolfe_dickinson</code>, <code style="background:#e7eef8;">nullity_support_pruned_dickinson</code>, <code style="background:#e7eef8;">one_step_frank_wolfe_dickinson</code>, <code style="background:#e7eef8;">pairwise_frank_wolfe_dickinson</code></td></tr>
    <tr><td align="right">379</td><td><code style="background:#e7eef8;">dense_bitset_dickinson</code></td></tr>
    <tr><td align="right">379</td><td><code style="background:#e7eef8;">interval_recursive_dickinson</code>, <code style="background:#e7eef8;">support_pruned_dickinson</code>, <code style="background:#e7eef8;">zischg_dickinson</code></td></tr>
    <tr><td align="right">375</td><td><code style="background:#e7eef8;">zischg_fracessa</code></td></tr>
    <tr><td align="right">374</td><td><code style="background:#e7eef8;">rhs_dickinson</code>, <code style="background:#e7eef8;">zischg_hadeler</code></td></tr>
    <tr><td align="right">373</td><td><code style="background:#e7eef8;">fracessa</code></td></tr>
    <tr><td align="right">372</td><td><code style="background:#e7eef8;border:1px solid #222;padding:0 0.15em;">hadeler_1983</code></td></tr>
  </tbody>
  <tbody style="border:2px solid #777;">
    <tr><td rowspan="2"><strong>Cone baselines</strong><br><small>reduce matrix order</small></td><td align="right">349</td><td><code style="background:#dff4df;border:1px solid #222;padding:0 0.15em;">copomatrix_2011</code></td></tr>
    <tr><td align="right">344</td><td><code style="background:#dff4df;border:1px solid #222;padding:0 0.15em;">danninger_1990</code></td></tr>
  </tbody>
  <tbody style="border:2px solid #777;">
    <tr><td rowspan="6"><strong>Same-order cone baselines</strong><br><small>plus adaptive hybrids</small></td><td align="right">318</td><td><code style="background:#dff4df;border:1px solid #222;padding:0 0.15em;">safi_2021</code></td></tr>
    <tr><td align="right">296</td><td><code style="background:#dff4df;">adaptive_dutour_copomatrix</code></td></tr>
    <tr><td align="right">254</td><td><code style="background:#dff4df;">adaptive_dutour_danninger</code></td></tr>
    <tr><td align="right">249</td><td><code style="background:#dff4df;border:1px solid #222;padding:0 0.15em;">dutour_2018</code></td></tr>
    <tr><td align="right">230</td><td><code style="background:#dff4df;border:1px solid #222;padding:0 0.15em;">sponsel_2012</code>, <code style="background:#dff4df;">frank_wolfe_sponsel</code></td></tr>
    <tr><td align="right">221</td><td><code style="background:#dff4df;border:1px solid #222;padding:0 0.15em;">bundfuss_2008</code></td></tr>
  </tbody>
</table>

## What Matters In This Table

- The main point is not the small ordering differences between individual algorithms. The results form tight groups rather than a
  random spread: many related experiments finish in almost exactly the same performance category. A modification therefore often
  changes little until it changes the underlying way the support family or cone is searched.
- The development history is not balanced across the groups. The cone experiments came first. Once the Dickinson variants that
  avoid enumerating every support individually proved much stronger, most subsequent work moved to that approach; the cone
  experiments are therefore generally older.
- `adaptive_sponsel_copomatrix` combines a same-order algorithm with an order-reducing one. It lets Sponsel subdivide while that is
  productive, but after 1,000 consecutive same-order splits on one branch it forces a COPOMATRIX step. This simple cutoff hybrid
  worked surprisingly well here, solving 403 of 512 matrices.
- Among the literature baselines, `dickinson_2019` is clearly strongest in this test, solving 380 matrices. The maintained
  `hadeler_1983` implementation is also remarkably competitive at 372 and outperforms every cone baseline; the strongest cone
  baseline, `copomatrix_2011`, solves 349.
- Matrix order means the $n$ in an $n\times n$ matrix. The minimum order is 4 and the maximum is 3,361. The cumulative distribution
  is 75.8% at order 25 or below, 80.7% at order 50 or below, and 90.4% at order 100 or below.
