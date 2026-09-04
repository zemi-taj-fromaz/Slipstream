#!/usr/bin/env python3

import argparse
import csv
from pathlib import Path

import matplotlib.pyplot as plt


def parse_arguments():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--input",
        type=Path,
        default=Path("build-linux/tick_to_order_histogram_grpc.csv"),
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("build-linux/tick_to_order_histogram_grpc.png"),
    )
    parser.add_argument("--show", action="store_true")
    return parser.parse_args()


def main():
    arguments = parse_arguments()
    latency_us = []
    counts = []
    overflow_count = 0

    with arguments.input.open(newline="") as histogram_file:
        for row in csv.DictReader(histogram_file):
            count = int(row["count"])
            if row["overflow"] == "true":
                overflow_count = count
            elif count != 0:
                latency_us.append(int(row["bucket_lower_us"]))
                counts.append(count)

    if overflow_count != 0:
        latency_us.append(5_000)
        counts.append(overflow_count)

    figure, axes = plt.subplots(figsize=(12, 6))
    axes.bar(latency_us, counts, width=1.0)
    axes.set_title("Slipstream tick-to-order latency")
    axes.set_xlabel("Latency (microseconds; 5000 includes overflow)")
    axes.set_ylabel("Order count")
    axes.grid(axis="y", alpha=0.25)
    figure.tight_layout()

    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(arguments.output, dpi=160)
    if arguments.show:
        plt.show()


if __name__ == "__main__":
    main()
