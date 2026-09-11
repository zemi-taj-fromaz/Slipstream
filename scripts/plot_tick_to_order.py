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
        default=Path("build-linux/tick_to_order_raw_grpc.csv"),
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("build-linux/tick_to_order_grpc.png"),
    )
    parser.add_argument("--bucket-us", type=float, default=1.0)
    parser.add_argument("--max-us", type=float)
    parser.add_argument("--show", action="store_true")
    return parser.parse_args()


def main():
    arguments = parse_arguments()
    if arguments.bucket_us <= 0:
        raise ValueError("--bucket-us must be greater than zero")

    with arguments.input.open(newline="") as raw_file:
        latency_us = np.asarray([
            int(row["tto_ns"]) / 1_000.0
            for row in csv.DictReader(raw_file)
        ])

    if latency_us.size == 0:
        raise ValueError(f"no TTO samples in {arguments.input}")

    max_us = arguments.max_us
    if max_us is None:
        max_us = math.ceil(float(np.max(latency_us)) / arguments.bucket_us) \
            * arguments.bucket_us
    if max_us <= 0:
        max_us = arguments.bucket_us

    visible = latency_us[latency_us <= max_us]
    edges = np.arange(0.0, max_us + arguments.bucket_us, arguments.bucket_us)

    figure, axes = plt.subplots(figsize=(12, 6))
    axes.hist(visible, bins=edges, color="#4EA8DE", alpha=0.75)
    axes.set_title("Slipstream tick-to-order latency")
    axes.set_xlabel("Latency (microseconds)")
    axes.set_ylabel("Order count")
    axes.grid(axis="y", alpha=0.25)
    figure.tight_layout()

    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(arguments.output, dpi=160)
    if arguments.show:
        plt.show()


if __name__ == "__main__":
    main()
