#!/usr/bin/env python3
"""Render one model's chronological support history as a bounded-size JPEG."""

from __future__ import annotations

import argparse
import itertools
import math
import re
import signal
import sqlite3
import subprocess
import tempfile
from dataclasses import dataclass
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_RESULTS_DATABASE = ROOT / "experiments/diagnostics.sqlite3"
DEFAULT_CORPUS_DATABASE = ROOT / "testdata/copos_testdata.sqlite3"
DEFAULT_COPOSIT = ROOT / "cpp/build/coposit"
TOKEN = re.compile(r"([a-z_]+)=(\[[^]]*\]|\S+)")

COLORS = {
    "upward": "#D00000",
    "dickinson": "#0066FF",
    "downward": "#FFD000",
    "uncovered": "#4FB878",
}
BACKGROUND = "#F7F7F4"
INK = "#1B1B1B"
FLOATING_ONLY = "#8A8A8A"
MUTED = "#696969"
GRID = "#B8B8B2"
CANVAS_WIDTH = 1920
CANVAS_HEIGHT = 1080
CHART_LEFT = 130
CHART_RIGHT_MARGIN = 230
CHART_TOP = 205
CHART_BOTTOM_MARGIN = 100


@dataclass(frozen=True)
class Event:
    sequence: int
    event: str
    model: str
    n: int
    frontier: str
    kind: str
    source: int
    lower: int
    upper: int
    coverage: str
    floating_checked: bool
    exact_checked: bool

    @property
    def source_size(self) -> int:
        return self.source.bit_count()

    @property
    def lower_size(self) -> int:
        return max(1, self.lower.bit_count()) if self.coverage == "downward" else self.lower.bit_count()

    @property
    def upper_size(self) -> int:
        return self.upper.bit_count()

    @property
    def category(self) -> str:
        if self.coverage == "upward":
            return "upward"
        if self.coverage == "downward":
            return "downward"
        return "dickinson"

    def covers(self, support: int) -> bool:
        return self.event == "certificate" and self.lower & support == self.lower and support & ~self.upper == 0


def parse_set(value: str, n: int) -> int:
    if value == "all":
        return (1 << n) - 1
    if not value.startswith("[") or not value.endswith("]"):
        raise ValueError(f"invalid support: {value}")
    result = 0
    for item in value[1:-1].split(","):
        if not item:
            continue
        index = int(item)
        if not 1 <= index <= n:
            raise ValueError(f"support index {index} is outside 1..{n}")
        result |= 1 << (index - 1)
    return result


def parse_boolean(value: str | None, name: str) -> bool:
    if value not in {"yes", "no"}:
        raise ValueError(f"{name} must be yes or no; rerun diagnostics recorded before this field existed")
    return value == "yes"


def diagnostics_from_text(text: str) -> str:
    for line in text.splitlines():
        if line.startswith("diagnostics_hex="):
            return bytes.fromhex(line.removeprefix("diagnostics_hex=")).decode()
    return text


def parse_events(text: str) -> list[Event]:
    events: list[Event] = []
    dimensions: set[int] = set()
    models: set[str] = set()
    previous_sequence = 0
    for line in diagnostics_from_text(text).splitlines():
        fields = dict(TOKEN.findall(line))
        if fields.get("event") not in {"certificate", "visited_support"}:
            continue
        n = int(fields["n"])
        dimensions.add(n)
        models.add(fields["model"])
        sequence = int(fields["sequence"])
        if sequence <= previous_sequence:
            raise ValueError("diagnostics events are not in strictly increasing sequence order")
        previous_sequence = sequence
        source = parse_set(fields["source"], n)
        coverage = fields.get("coverage", "none")
        lower = parse_set(fields.get("lower", fields["source"]), n)
        upper = parse_set(fields.get("upper", fields["source"]), n)
        floating_checked = parse_boolean(fields.get("floating_checked"), "floating_checked")
        exact_checked = parse_boolean(fields.get("exact_checked"), "exact_checked")
        if not floating_checked and not exact_checked:
            raise ValueError("a support-history event must record at least one performed check")
        if fields["event"] == "certificate" and not exact_checked:
            raise ValueError("a mathematical certificate must be exact-checked")
        events.append(
            Event(
                sequence,
                fields["event"],
                fields["model"],
                n,
                fields.get("frontier", "unknown"),
                fields.get("kind", "none"),
                source,
                lower,
                upper,
                coverage,
                floating_checked,
                exact_checked,
            )
        )
    if not events:
        raise ValueError("no chronological support-history events found")
    if len(dimensions) != 1:
        raise ValueError("one image cannot combine diagnostics from delegated matrices of different dimensions")
    if len(models) != 1:
        raise ValueError("one image cannot combine support histories from different models")
    return events


def result_mode(mode: str) -> str:
    return {"strict": "strictly_copositive", "non-strict": "copositive", "both": "both"}[mode]


def load_stored_diagnostics(
    matrix_id: int,
    model: str,
    model_parameter: str | None,
    mode: str,
    preprocessing: str,
    database: Path,
) -> str:
    result_model = model if model_parameter is None else f"{model}@{model_parameter}"
    with sqlite3.connect(f"file:{database}?mode=ro", uri=True) as connection:
        row = connection.execute(
            """
            SELECT diagnostics
              FROM results
             WHERE matrix_id = ? AND model_id = ? AND mode = ? AND preprocessing = ?
               AND diagnostics LIKE '%event=%'
             ORDER BY recorded_at DESC
             LIMIT 1
            """,
            (matrix_id, result_model, result_mode(mode), "both" if preprocessing == "on" else "none"),
        ).fetchone()
    if row is None:
        raise ValueError(
            f"matrix {matrix_id} has no stored {result_model} history for mode={mode}, preprocessing={preprocessing}; "
            "use --run to execute it"
        )
    return row[0]


def corpus_matrix(matrix_id: int, database: Path) -> tuple[str, str | None]:
    with sqlite3.connect(f"file:{database}?mode=ro", uri=True) as connection:
        row = connection.execute("SELECT dimension, matrix FROM matrices WHERE matrix_id = ?", (matrix_id,)).fetchone()
    if row is None:
        raise ValueError(f"matrix {matrix_id} is not in {database}")
    dimension, storage = row
    if storage.startswith("file:"):
        path = database.parent / storage.removeprefix("file:")
        if not path.is_file():
            raise FileNotFoundError(path)
        return str(path), None
    return "-", f"{dimension}#{storage}"


def run_model(
    matrix_id: int,
    model: str,
    model_parameter: str | None,
    mode: str,
    preprocessing: str,
    timeout: float,
    corpus_database: Path,
    coposit: Path,
    allow_timeout: bool,
) -> tuple[str, bool]:
    matrix_argument, standard_input = corpus_matrix(matrix_id, corpus_database)
    command = [
        str(coposit),
        "--model",
        model,
        "--mode",
        mode,
        "--preprocessing",
        preprocessing,
        "--machine",
        "--collect-diagnostics",
    ]
    if model_parameter is not None:
        command.extend(("--model-parameter", model_parameter))
    command.append(matrix_argument)
    process = subprocess.Popen(
        command,
        text=True,
        stdin=subprocess.PIPE if standard_input is not None else subprocess.DEVNULL,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    timeout_requested = False
    try:
        stdout, stderr = process.communicate(input=standard_input, timeout=timeout)
    except subprocess.TimeoutExpired:
        if not hasattr(signal, "SIGUSR1"):
            process.kill()
            process.communicate()
            raise TimeoutError("partial diagnostics at timeout require SIGUSR1")
        timeout_requested = True
        process.send_signal(signal.SIGUSR1)
        try:
            stdout, stderr = process.communicate(timeout=10)
        except subprocess.TimeoutExpired:
            process.kill()
            process.communicate()
            raise TimeoutError(f"{model} did not stop cooperatively after {timeout:g} seconds")
    if process.returncode != 0:
        message = stderr.strip() or f"coposit exited with status {process.returncode}"
        raise RuntimeError(message)
    fields = dict(line.partition("=")[::2] for line in stdout.splitlines() if "=" in line)
    if fields.get("coposit_result") != "1":
        raise RuntimeError("coposit returned malformed machine output")
    timed_out = fields.get("status") == "5"
    if timed_out and timeout_requested and allow_timeout:
        return diagnostics_from_text(stdout), True
    if fields.get("status") != "0":
        message = bytes.fromhex(fields.get("error_message_hex", "")).decode() or f"coposit status {fields.get('status')}"
        raise TimeoutError(message) if timed_out else RuntimeError(message)
    return diagnostics_from_text(stdout), False


def combinadic_rank(indices: tuple[int, ...]) -> int:
    return sum(math.comb(index, position) for position, index in enumerate(indices, 1))


def combinadic_unrank(rank: int, n: int, k: int) -> tuple[int, ...]:
    if not 0 <= rank < math.comb(n, k):
        raise ValueError("combinadic rank outside layer")
    result = [0] * k
    maximum = n - 1
    for position in range(k, 0, -1):
        low, high = position - 1, maximum
        while low < high:
            middle = (low + high + 1) // 2
            if math.comb(middle, position) <= rank:
                low = middle
            else:
                high = middle - 1
        result[position - 1] = low
        rank -= math.comb(low, position)
        maximum = low - 1
    return tuple(result)


def mask_for(indices: tuple[int, ...]) -> int:
    result = 0
    for index in indices:
        result |= 1 << index
    return result


def classify(support: int, events: list[Event]) -> str:
    if support == 0:
        return "uncovered"
    for event in events:
        if event.covers(support):
            return event.category
    return "uncovered"


def font(size: int, bold: bool = False) -> ImageFont.FreeTypeFont | ImageFont.ImageFont:
    name = "DejaVuSans-Bold.ttf" if bold else "DejaVuSans.ttf"
    try:
        return ImageFont.truetype(name, size)
    except OSError:
        return ImageFont.load_default()


def total_count(value: int) -> str:
    if value <= 1_000_000:
        return f"{value:,}"
    exponent = len(str(value)) - 1
    return f"{value / 10**exponent:.1f}×10^{exponent}"


def layer_samples(n: int, k: int, bins: int, exact: bool, samples_per_bin: int):
    total = math.comb(n, k)
    if exact:
        for indices in itertools.combinations(range(n), k):
            rank = combinadic_rank(indices)
            yield rank * bins // total, mask_for(indices), 1.0
        return
    for pixel in range(bins):
        start = pixel * total // bins
        stop = (pixel + 1) * total // bins
        count = min(samples_per_bin, stop - start)
        if count == 0:
            continue
        weight = (stop - start) / count
        for sample in range(count):
            rank = start + ((2 * sample + 1) * (stop - start)) // (2 * count)
            yield pixel, mask_for(combinadic_unrank(rank, n, k)), weight


def draw_legend(draw: ImageDraw.ImageDraw, x: int, y: int) -> None:
    start_x = x
    labels = [
        ("upward", "upward pruning"),
        ("dickinson", "Dickinson pruning"),
        ("downward", "downward pruning"),
        ("uncovered", "not covered"),
    ]
    label_font = font(20)
    for category, label in labels:
        draw.rectangle((x, y + 3, x + 24, y + 24), fill=COLORS[category])
        draw.text((x + 34, y), label, fill=INK, font=label_font)
        x += 54 + round(draw.textlength(label, font=label_font))
    x, y = start_x, y + 30
    for color, label in ((INK, "exact check"), (FLOATING_ONLY, "floating-only check")):
        draw.ellipse((x + 5, y + 7, x + 19, y + 21), fill=color, outline=BACKGROUND, width=2)
        draw.text((x + 34, y), label, fill=INK, font=label_font)
        x += 54 + round(draw.textlength(label, font=label_font))


def draw_coverage_summary(draw: ImageDraw.ImageDraw, width: int, percentages: dict[str, float]) -> None:
    summary_x, summary_y = width - 480, 20
    draw.rectangle((summary_x - 10, summary_y - 6, width - 48, summary_y + 132), fill=BACKGROUND)
    draw.text((summary_x, summary_y), "estimated lattice coverage", fill=INK, font=font(18, True))
    for row, (category, label_text) in enumerate(
        (
            ("upward", "upward"),
            ("dickinson", "Dickinson"),
            ("downward", "downward"),
            ("uncovered", "not covered"),
        )
    ):
        y = summary_y + 26 + row * 21
        draw.rectangle((summary_x, y + 2, summary_x + 16, y + 18), fill=COLORS[category])
        draw.text((summary_x + 25, y), label_text, fill=INK, font=font(16))
        draw.text((width - 58, y), f"{percentages[category]:.2f}%", fill=INK, font=font(16), anchor="ra")


def estimate_existing_coverage(image: Image.Image, n: int) -> dict[str, float]:
    if image.size != (CANVAS_WIDTH, CANVAS_HEIGHT) or n < 1:
        raise ValueError(f"existing-image estimation requires a {CANVAS_WIDTH}x{CANVAS_HEIGHT} support-history image and positive dimension")
    chart_left, chart_right = CHART_LEFT, image.width - CHART_RIGHT_MARGIN
    chart_width = chart_right - chart_left
    chart_height = CANVAS_HEIGHT - CHART_TOP - CHART_BOTTOM_MARGIN
    bottom = CHART_TOP + chart_height
    if image.height < bottom:
        raise ValueError("image is too short for the requested support-history dimension")
    max_log = math.log2(math.comb(n, n // 2)) if n > 1 else 1
    row_gap = chart_height / max(1, n)
    band_height = max(2, min(18, round(row_gap * 0.62)))
    palette = {category: tuple(bytes.fromhex(color[1:])) for category, color in COLORS.items()}
    pixels = image.convert("RGB").load()
    totals = {category: 0.0 for category in COLORS}

    # ponytail: this recovers the already-rendered deterministic sample; persist raw event streams if exact reconstruction is later required.
    for k in range(1, n + 1):
        y = bottom - k * row_gap
        layer_width = 18 + (chart_width - 18) * math.log2(math.comb(n, k)) / max_log
        left = (image.width - layer_width) / 2
        colored = {category: 0 for category in COLORS}
        for pixel_y in range(round(y - band_height / 2), round(y + band_height / 2) + 1):
            for pixel_x in range(math.ceil(left), math.floor(left + layer_width) + 1):
                color = pixels[pixel_x, pixel_y]
                category, distance = min(
                    ((category, sum((channel - target) ** 2 for channel, target in zip(color, rgb))) for category, rgb in palette.items()),
                    key=lambda item: item[1],
                )
                if distance <= 100**2:
                    colored[category] += 1
        colored_total = sum(colored.values())
        if not colored_total:
            raise ValueError(f"could not recover colored coverage band for k={k}")
        layer_total = math.comb(n, k)
        for category, count in colored.items():
            totals[category] += layer_total * count / colored_total
    total = sum(totals.values())
    return {category: 100 * count / total for category, count in totals.items()}


def render(events: list[Event], label: str, output: Path, exact_limit: int, samples_per_bin: int) -> dict[str, object]:
    n = events[0].n
    model = events[0].model
    certificates = [event for event in events if event.event == "certificate"]
    by_cardinality = {
        k: [event for event in certificates if event.lower_size <= k <= event.upper_size] for k in range(n + 1)
    }
    source_checks: dict[int, bool] = {}
    for event in events:
        source_checks[event.source] = source_checks.get(event.source, False) or event.exact_checked
    visited_counts = [0] * (n + 1)
    for source in source_checks:
        visited_counts[source.bit_count()] += 1
    low_max = max((event.source_size for event in events if event.frontier in {"initial", "low"}), default=0)
    high_min = min((event.source_size for event in events if event.frontier == "high"), default=n + 1)

    width = CANVAS_WIDTH
    chart_left, chart_right = CHART_LEFT, width - CHART_RIGHT_MARGIN
    chart_width = chart_right - chart_left
    chart_height = CANVAS_HEIGHT - CHART_TOP - CHART_BOTTOM_MARGIN
    top = CHART_TOP
    bottom = top + chart_height
    image = Image.new("RGB", (width, CANVAS_HEIGHT), BACKGROUND)
    draw = ImageDraw.Draw(image)
    draw.text((64, 30), f"{label} — {model}", fill=INK, font=font(34, True))
    draw.text(
        (64, 75),
        f"n={n}   events={len(events):,}   reached low frontier k≤{low_max}   reached high frontier k≥{high_min if high_min <= n else '—'}",
        fill=MUTED,
        font=font(21),
    )
    draw_legend(draw, 64, 118)

    max_log = math.log2(math.comb(n, n // 2)) if n > 1 else 1
    row_gap = chart_height / max(1, n)
    band_height = max(2, min(18, round(row_gap * 0.62)))
    geometry: dict[int, tuple[float, float, float]] = {}
    exact_layers: list[int] = []
    sampled_layers: list[int] = []
    coverage_counts = {category: 0.0 for category in COLORS}

    for k in range(n + 1):
        y = bottom - k * row_gap
        layer_width = 18 + (chart_width - 18) * math.log2(math.comb(n, k)) / max_log
        left = (width - layer_width) / 2
        total = math.comb(n, k)
        bins = max(1, min(total, round(layer_width)))
        exact = total <= exact_limit
        (exact_layers if exact else sampled_layers).append(k)
        counts = [{category: 0.0 for category in COLORS} for _ in range(bins)]
        totals = [0.0] * bins
        for pixel, support, weight in layer_samples(n, k, bins, exact, samples_per_bin):
            category = classify(support, by_cardinality[k])
            counts[pixel][category] += weight
            totals[pixel] += weight
            if k:
                coverage_counts[category] += weight
        pixel_width = layer_width / bins
        for pixel, values in enumerate(counts):
            x0 = left + pixel * pixel_width
            x1 = left + (pixel + 1) * pixel_width + 0.5
            y0 = y - band_height / 2
            for category in ("upward", "dickinson", "downward", "uncovered"):
                share = 0 if totals[pixel] == 0 else values[category] / totals[pixel]
                y1 = y0 + band_height * share
                if y1 > y0:
                    draw.rectangle((x0, y0, x1, y1), fill=COLORS[category])
                y0 = y1
        draw.line((left, y + band_height / 2 + 1, left + layer_width, y + band_height / 2 + 1), fill=GRID, width=1)
        layer_font_size = max(11, min(18, round(row_gap * 0.8)))
        draw.text((32, y - layer_font_size / 2), f"k={k}", fill=INK, font=font(layer_font_size))
        count_font = font(max(10, layer_font_size - 1))
        visited = f"{visited_counts[k]:,}"
        draw.text((left - 12 - draw.textlength(visited, font=count_font), y - 11), visited, fill=MUTED, font=count_font)
        draw.text((left + layer_width + 12, y - 11), total_count(total), fill=MUTED, font=count_font)
        geometry[k] = (left, layer_width, y)

    coverage_total = sum(coverage_counts.values())
    coverage_percentages = {category: 100 * count / coverage_total for category, count in coverage_counts.items()}
    draw_coverage_summary(draw, width, coverage_percentages)

    caption_font = font(22)
    draw.text((width / 4, bottom + 55), "number of checked supports", fill=MUTED, font=caption_font, anchor="mm")
    draw.text((3 * width / 4, bottom + 55), f"C({n}, k)", fill=MUTED, font=caption_font, anchor="mm")

    for source, exact_checked in sorted(source_checks.items(), key=lambda item: item[1]):
        k = source.bit_count()
        if k == 0:
            continue
        indices = tuple(index for index in range(n) if source >> index & 1)
        rank = combinadic_rank(indices)
        total = math.comb(n, k)
        left, layer_width, y = geometry[k]
        x = left + (rank + 0.5) * layer_width / total
        radius = 3
        color = INK if exact_checked else FLOATING_ONLY
        draw.ellipse((x - radius, y - radius, x + radius, y + radius), fill=color, outline=BACKGROUND, width=1)

    output.parent.mkdir(parents=True, exist_ok=True)
    image.save(output, "JPEG", quality=92, optimize=True, progressive=True)
    return {
        "dimension": n,
        "events": len(events),
        "low_max": low_max,
        "high_min": high_min if high_min <= n else None,
        "exact_layers": exact_layers,
        "sampled_layers": sampled_layers,
        "coverage_percentages": coverage_percentages,
        "output": output,
    }


def self_test() -> None:
    sample = (
        "event=certificate sequence=1 model=test_model n=4 frontier=low kind=dickinson source=[1] "
        "coverage=interval lower=[1] upper=[1,2,3] exclude_empty=no floating_checked=no exact_checked=yes\n"
        "event=certificate sequence=2 model=test_model n=4 frontier=high kind=positive_definite source=[1,2,3] "
        "coverage=downward lower=[] upper=[1,2,3] exclude_empty=yes floating_checked=yes exact_checked=yes\n"
        "event=certificate sequence=3 model=test_model n=4 frontier=walk kind=walk_negative_curvature source=[4] "
        "coverage=upward lower=[4] upper=all exclude_empty=no floating_checked=yes exact_checked=yes\n"
        "event=visited_support sequence=4 model=test_model n=4 frontier=high source=[2,3,4] "
        "floating_checked=yes exact_checked=no\n"
    )
    events = parse_events(sample)
    assert len(events) == 4 and events[0].model == "test_model" and events[0].covers(1) and not events[0].covers(8)
    assert classify(0, events) == "uncovered"
    assert events[0].exact_checked and not events[0].floating_checked
    assert events[1].exact_checked and events[1].floating_checked
    assert events[2].floating_checked and events[2].exact_checked
    assert events[3].floating_checked and not events[3].exact_checked
    assert events[2].category == "upward" and classify(8, events) == "upward"
    assert total_count(1_000_000) == "1,000,000" and total_count(5_200_300) == "5.2×10^6"
    for n in range(1, 8):
        for k in range(n + 1):
            for rank in range(math.comb(n, k)):
                assert combinadic_rank(combinadic_unrank(rank, n, k)) == rank
    with tempfile.TemporaryDirectory() as directory:
        database = Path(directory) / "diagnostics.sqlite3"
        with sqlite3.connect(database) as connection:
            connection.execute(
                "CREATE TABLE results (matrix_id INTEGER, model_id TEXT, mode TEXT, preprocessing TEXT, "
                "diagnostics TEXT, recorded_at TEXT)"
            )
            connection.execute(
                "INSERT INTO results VALUES (7, 'test_model', 'both', 'both', ?, '2026-08-23')",
                (sample,),
            )
        assert load_stored_diagnostics(7, "test_model", None, "both", "on", database) == sample
        output = Path(directory) / "test.jpg"
        result = render(events, "self-test", output, exact_limit=1_000, samples_per_bin=3)
        assert result["coverage_percentages"]["upward"] > 0
        with Image.open(output) as image:
            assert image.format == "JPEG" and image.size == (CANVAS_WIDTH, CANVAS_HEIGHT)
        percentages = result["coverage_percentages"]
        assert math.isclose(sum(percentages.values()), 100) and math.isclose(percentages["dickinson"], 400 / 15)
        with Image.open(output) as image:
            estimated = estimate_existing_coverage(image, 4)
        assert math.isclose(sum(estimated.values()), 100) and estimated["dickinson"] > 0 and estimated["downward"] > 0


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", nargs="?", help="corpus matrix ID, or a diagnostics text file")
    parser.add_argument("output", nargs="?", type=Path)
    parser.add_argument("--model", help="model identifier; required with a matrix ID")
    parser.add_argument("--label", help="human-readable matrix label used in the image title")
    parser.add_argument("--model-parameter")
    parser.add_argument("--mode", choices=("strict", "non-strict", "both"), default="both")
    parser.add_argument("--preprocessing", choices=("on", "off"), default="on")
    parser.add_argument("--run", action="store_true", help="run the selected model instead of reading a stored history")
    parser.add_argument("--allow-timeout", action="store_true", help="render partial diagnostics after a cooperative timeout")
    parser.add_argument("--timeout", type=float, default=300)
    parser.add_argument("--results-database", type=Path, default=DEFAULT_RESULTS_DATABASE)
    parser.add_argument("--corpus-database", type=Path, default=DEFAULT_CORPUS_DATABASE)
    parser.add_argument("--coposit", type=Path, default=DEFAULT_COPOSIT)
    parser.add_argument("--exact-limit", type=int, default=1_000_000, help="largest layer enumerated exactly")
    parser.add_argument("--samples-per-pixel", type=int, default=5)
    parser.add_argument("--annotate-existing-dimension", type=int, metavar="N", help="add an estimated summary to an existing JPEG without rerunning a model")
    parser.add_argument("--self-test", action="store_true")
    arguments = parser.parse_args()
    if arguments.self_test:
        self_test()
        print("self-test passed")
        return
    if arguments.source is None:
        parser.error("source is required unless --self-test is used")
    if arguments.annotate_existing_dimension is not None:
        if arguments.run or arguments.output is not None:
            parser.error("--annotate-existing-dimension updates the source JPEG in place and cannot be combined with --run or OUTPUT")
        path = Path(arguments.source)
        with Image.open(path) as source_image:
            image = source_image.convert("RGB")
        percentages = estimate_existing_coverage(image, arguments.annotate_existing_dimension)
        draw_coverage_summary(ImageDraw.Draw(image), image.width, percentages)
        image.save(path, "JPEG", quality=92, optimize=True, progressive=True)
        print(f"updated {path}  estimated_coverage={percentages}")
        return
    if arguments.exact_limit < 1 or arguments.samples_per_pixel < 1 or arguments.timeout <= 0:
        parser.error("limits must be positive")
    if arguments.allow_timeout and not arguments.run:
        parser.error("--allow-timeout requires --run")
    if arguments.source.isdecimal():
        if arguments.model is None:
            parser.error("--model is required with a matrix ID")
        matrix_id = int(arguments.source)
        if arguments.run:
            text, timed_out = run_model(
                matrix_id,
                arguments.model,
                arguments.model_parameter,
                arguments.mode,
                arguments.preprocessing,
                arguments.timeout,
                arguments.corpus_database,
                arguments.coposit,
                arguments.allow_timeout,
            )
        else:
            text = load_stored_diagnostics(
                matrix_id,
                arguments.model,
                arguments.model_parameter,
                arguments.mode,
                arguments.preprocessing,
                arguments.results_database,
            )
            timed_out = False
        label = arguments.label or f"matrix {matrix_id}" + (" — timed out" if timed_out else "")
        output = arguments.output or Path(__file__).with_name(f"{arguments.model}-matrix-{matrix_id}.jpg")
    else:
        if arguments.run:
            parser.error("--run requires a corpus matrix ID")
        path = Path(arguments.source)
        text = path.read_text()
        label = arguments.label or path.stem
        output = arguments.output or path.with_suffix(".jpg")
    events = parse_events(text)
    if arguments.model is not None and events[0].model != arguments.model:
        raise ValueError(f"diagnostics belongs to model {events[0].model}, not {arguments.model}")
    result = render(events, label, output, arguments.exact_limit, arguments.samples_per_pixel)
    print(
        f"wrote {result['output']}  n={result['dimension']}  events={result['events']}  "
        f"exact_layers={result['exact_layers']}  sampled_layers={result['sampled_layers']}"
    )


if __name__ == "__main__":
    main()
