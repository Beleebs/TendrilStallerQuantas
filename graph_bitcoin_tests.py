#!/usr/bin/env python3
"""
Graph QUANTAS/Bitcoin experiment JSON output.

Creates line graphs and histograms for each test in the JSON file.
Default metrics:
  - block_received_delay
  - blocks_mined_per_round
  - msgs_sent_per_round
  - txs_made_per_round

Note: some output files use txs_sent_per_round instead of txs_made_per_round.
This script automatically falls back to txs_sent_per_round when txs_made_per_round is absent.

Usage:
  python graph_bitcoin_tests.py path/to/output.json
  python graph_bitcoin_tests.py path/to/output.json --out-dir graphs
"""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
from typing import Any, Iterable

import matplotlib.pyplot as plt
from matplotlib.ticker import MaxNLocator


DEFAULT_METRICS = [
    "block_received_delay",
    "blocks_mined_per_round",
    "msgs_sent_per_round",
    "txs_made_per_round",
]

# Allows the script to support slightly different metric names between versions.
METRIC_ALIASES = {
    "txs_made_per_round": ["txs_made_per_round", "txs_created_per_round", "txs_sent_per_round"],
    "txs_created_per_round": ["txs_created_per_round", "txs_made_per_round", "txs_sent_per_round"],
}


def load_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def as_numeric_list(values: Any) -> list[float]:
    """Return only numeric values from a JSON list."""
    if not isinstance(values, list):
        return []

    nums: list[float] = []
    for value in values:
        if isinstance(value, (int, float)) and not isinstance(value, bool):
            nums.append(float(value))
    return nums


def resolve_metric(test: dict[str, Any], requested_metric: str) -> tuple[str | None, list[float]]:
    """Find the actual metric key in the test, respecting aliases."""
    candidate_keys = METRIC_ALIASES.get(requested_metric, [requested_metric])

    for key in candidate_keys:
        values = as_numeric_list(test.get(key))
        if values:
            return key, values

    return None, []


def safe_filename(name: str) -> str:
    return "".join(c if c.isalnum() or c in ("-", "_") else "_" for c in name)


def plot_line(values: list[float], metric_label: str, test_index: int, out_dir: Path) -> Path:
    x = list(range(len(values)))

    fig, ax = plt.subplots(figsize=(12, 5))
    ax.plot(x, values)
    ax.set_title(f"Test {test_index}: {metric_label} over time")
    ax.set_xlabel("Round / sample index")
    ax.set_ylabel(metric_label)
    ax.grid(True, alpha=0.3)
    
    ax.xaxis.set_major_locator(MaxNLocator(integer=True))
    ax.yaxis.set_major_locator(MaxNLocator(integer=True))

    output_path = out_dir / f"test_{test_index}_{safe_filename(metric_label)}_line.png"
    fig.tight_layout()
    fig.savefig(output_path, dpi=150)
    plt.close(fig)
    return output_path


def plot_histogram(values: list[float], metric_label: str, test_index: int, out_dir: Path) -> Path:
    # Integer-like metrics get integer bin edges; otherwise use a reasonable automatic bin count.
    all_integer_like = all(float(v).is_integer() for v in values)
    min_value = min(values)
    max_value = max(values)

    if all_integer_like and max_value - min_value <= 100:
        bins = [x - 0.5 for x in range(int(min_value), int(max_value) + 2)]
    else:
        bins = min(50, max(10, int(math.sqrt(len(values)))))

    fig, ax = plt.subplots(figsize=(9, 5))
    ax.hist(values, bins=bins)
    ax.set_title(f"Test {test_index}: {metric_label} distribution")
    ax.set_xlabel(metric_label)
    ax.set_ylabel("Frequency")
    ax.grid(True, alpha=0.3)
    
    ax.xaxis.set_major_locator(MaxNLocator(integer=True))
    ax.yaxis.set_major_locator(MaxNLocator(integer=True))

    output_path = out_dir / f"test_{test_index}_{safe_filename(metric_label)}_hist.png"
    fig.tight_layout()
    fig.savefig(output_path, dpi=150)
    plt.close(fig)
    return output_path


def summarize(values: list[float]) -> str:
    return (
        f"n={len(values)}, "
        f"min={min(values):g}, "
        f"max={max(values):g}, "
        f"mean={sum(values) / len(values):.3f}, "
        f"sum={sum(values):g}"
    )


def main() -> None:
    parser = argparse.ArgumentParser(description="Graph metrics from Bitcoin simulation JSON output.")
    parser.add_argument("json_file", type=Path, help="Path to the JSON output file.")
    parser.add_argument("--out-dir", type=Path, default=Path("graphs"), help="Directory for generated PNG graphs.")
    parser.add_argument(
        "--metrics",
        nargs="+",
        default=DEFAULT_METRICS,
        help="Metric keys to graph. Defaults to the common Bitcoin simulation metrics.",
    )
    args = parser.parse_args()

    data = load_json(args.json_file)
    tests = data.get("tests")

    if not isinstance(tests, list):
        raise ValueError("Expected top-level JSON key 'tests' to be a list.")

    args.out_dir.mkdir(parents=True, exist_ok=True)

    generated: list[Path] = []

    for test_index, test in enumerate(tests):
        if not isinstance(test, dict):
            print(f"Skipping test {test_index}: expected object, got {type(test).__name__}")
            continue

        seed = test.get("seed", data.get("testSeeds", [None] * (test_index + 1))[test_index])
        print(f"\nTest {test_index}" + (f" | seed={seed}" if seed is not None else ""))

        for requested_metric in args.metrics:
            actual_key, values = resolve_metric(test, requested_metric)

            if not values:
                print(f"  - {requested_metric}: missing or not numeric; skipped")
                continue

            label = actual_key if actual_key == requested_metric else f"{requested_metric} ({actual_key})"
            print(f"  - {label}: {summarize(values)}")

            generated.append(plot_line(values, label, test_index, args.out_dir))
            generated.append(plot_histogram(values, label, test_index, args.out_dir))

    print("\nGenerated graph files:")
    for path in generated:
        print(f"  {path}")


if __name__ == "__main__":
    main()
