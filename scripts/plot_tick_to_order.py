#!/usr/bin/env python3

import argparse
import csv
import math
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np


def parse_arguments():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--input",
        type=Path,
        nargs="+",
        default=[Path("build-linux/tick_to_order_raw_tcp_engine_wait.csv")],
    )
    parser.add_argument(
        "--output",
        type=Path,
    )
    parser.add_argument("--bucket-us", type=float, default=1.0)
    parser.add_argument("--max-us", type=float)
    parser.add_argument("--show", action="store_true")
    return parser.parse_args()


def main():
    arguments = parse_arguments()
    if arguments.bucket_us <= 0:
        raise ValueError("--bucket-us must be greater than zero")

    datasets = []
    for path in arguments.input:
        with path.open(newline="") as raw_file:
            rows = list(csv.DictReader(raw_file))
        if not rows:
            raise ValueError(f"no TTO samples in {path}")
        latency_us = np.asarray([int(row["tto_ns"]) / 1_000.0 for row in rows])
        label = path.stem.removeprefix("tick_to_order_raw_")
        datasets.append((label, latency_us))

    if arguments.output is None:
        arguments.output = arguments.input[0].with_name(
            "tick_to_order_" + "_vs_".join(label for label, _ in datasets) + ".png")

    max_us = arguments.max_us
    if max_us is None:
        max_us = math.ceil(max(float(np.max(values)) for _, values in datasets) / arguments.bucket_us) \
            * arguments.bucket_us
    if max_us <= 0:
        max_us = arguments.bucket_us

    edges = np.arange(0.0, max_us + arguments.bucket_us, arguments.bucket_us)

    figure, axes = plt.subplots(figsize=(12, 6))
    colors = ["#4EA8DE", "#E9B949", "#47A878"]
    for index, (label, values) in enumerate(datasets):
        color = colors[index % len(colors)]
        median = float(np.median(values))
        axes.hist(values[values <= max_us], bins=edges, color=color, alpha=0.5,
                  label=f"{label}: n={values.size}, median={median:.3f} us")
        axes.axvline(median, color=color, linestyle="--")
    axes.set_title("Slipstream tick-to-order latency")
    axes.set_xlabel("Latency (microseconds)")
    axes.set_ylabel("Order count")
    axes.grid(axis="y", alpha=0.25)
    axes.legend()
    figure.tight_layout()

    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(arguments.output, dpi=160)
    if arguments.show:
        plt.show()


if __name__ == "__main__":
    main()
