# Support-History Image

`render.py` turns any model's chronological support-history diagnostics into one JPEG. It is not tied to SAT-B3. Models that do not
emit the shared `event=certificate` and `event=visited_support` contract cannot be visualized this way.

Render a stored history:

```bash
python3 visualizations/support-history/render.py MATRIX_ID OUTPUT.jpg --model MODEL
```

Run the selected model with preprocessing and combined CP/SCP classification, then render the returned history directly:

```bash
python3 visualizations/support-history/render.py MATRIX_ID OUTPUT.jpg --model MODEL --run --timeout 300
```

Add `--allow-timeout` to stop the model cooperatively at that cutoff and render its incomplete history. The image title marks the
timeout; unresolved regions remain green. Use `--label TEXT` to replace the matrix identifier in the title.

The run uses the existing `coposit` launcher and does not insert a benchmark result. `--mode`, `--preprocessing`, and
`--model-parameter` select a different supported model call when needed.

The default corpus is `testdata/copos_testdata.sqlite3`; stored histories come from `experiments/diagnostics.sqlite3`. A diagnostics
text file may be given instead of a matrix ID. Such a file may contain raw `event=...` lines or machine output containing
`diagnostics_hex=...`; the model name is then read from the events.

The vertical axis is support cardinality. The decorative $k=0$ layer shows the empty support in gray even though the algorithm
searches only nonempty supports. Horizontal position is colexicographic combinadic rank, so small strategy indices remain on the left
and large indices on the right. Band width is proportional to `log2(binomial(n,k))`, above a small fixed minimum; therefore the
one-set layers $k=0$ and $k=n$ have the same narrow width.

The exact number of distinct visited supports is printed to the left of each band. The total layer size $\binom nk$ is printed on
the right; totals above one million use scientific notation. Horizontal captions below the lattice identify the two columns, with
the current matrix dimension substituted into `C(n, k)`.

The renderer determines the reached low and high frontiers from the recorded events; it has no fixed cardinality cutoff. Every layer
containing at most one million supports is enumerated exactly. Larger layers are represented by
deterministic samples in image-pixel bins. This changes generation cost, not the JPEG dimensions. Every analyzed source support is a
uniform small dot: black when exact arithmetic checked it, and gray when only the floating-point high-frontier filter checked it. If
both checks reached the same support, black takes precedence.
Every JPEG uses the same screen-sized 1920-by-1080 canvas; higher-dimensional lattices use tighter vertical spacing.
The fill distinguishes upward pruning, Dickinson pruning, and downward pruning; uncovered supports are green. Pair curvature remains
ordinary upward pruning because it is only that rule's cardinality-two special case. `--exact-limit` changes the threshold for an
intentionally expensive one-off render.

The upper-right summary estimates each class's share of all $2^n-1$ nonempty supports. Exact layers contribute exact counts; sampled
layers contribute their deterministic sampling weights, so the four displayed percentages always cover the complete nonempty lattice.
An existing JPEG whose raw event stream is unavailable can be updated without rerunning its model using
`--annotate-existing-dimension N`; this reconstructs the estimate from the colored bands already stored in that image.

Run the built-in parser, combinadic, and JPEG smoke check with:

```bash
python3 visualizations/support-history/render.py --self-test
```
