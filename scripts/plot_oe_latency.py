#!/usr/bin/env python3

import argparse
import csv
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np


def parse_arguments():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--tcp",
        type=Path,
        default=Path("build-linux/oe_latency_tcp.csv"),
    )
    parser.add_argument(
        "--grpc",
        type=Path,
        default=Path("build-linux/oe_latency_grpc.csv"),
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("build-linux/oe_latency_tcp_vs_grpc.png"),
    )
    parser.add_argument("--bins", type=int, default=50)
    parser.add_argument("--max-us", type=float)
    parser.add_argument("--show", action="store_true")
    return parser.parse_args()


def read_latency_us(path):
    with path.open(newline="") as latency_file:
        values = [
            int(row["latency_ns"]) / 1_000.0
            for row in csv.DictReader(latency_file)
        ]

    if not values:
        raise ValueError(f"no latency samples in {path}")
    return np.asarray(values)


def percentile_text(values):
    return (
        f"p50={np.percentile(values, 50):.1f} us, "
        f"p99={np.percentile(values, 99):.1f} us, "
        f"p99.9={np.percentile(values, 99.9):.1f} us"
    )


def main():
    arguments = parse_arguments()
    if arguments.bins <= 0:
        raise ValueError("--bins must be greater than zero")

    tcp = read_latency_us(arguments.tcp)
    grpc = read_latency_us(arguments.grpc)
    combined = np.concatenate((tcp, grpc))
    max_us = arguments.max_us
    if max_us is None:
        max_us = float(np.max(combined)) * 1.01
    if max_us <= 0:
        raise ValueError("--max-us must be greater than zero")

    tcp_visible = tcp[tcp <= max_us]
    grpc_visible = grpc[grpc <= max_us]
    edges = np.linspace(0.0, max_us, arguments.bins + 1)

    figure, axes = plt.subplots(figsize=(12, 7))
    axes.hist(
        tcp_visible,
        bins=edges,
        color="#F4D35E",
        alpha=0.65,
        label=f"TCP ({percentile_text(tcp)})",
    )
    axes.hist(
        grpc_visible,
        bins=edges,
        color="#4EA8DE",
        alpha=0.58,
        label=f"gRPC ({percentile_text(grpc)})",
    )

    tcp_median = float(np.median(tcp))
    grpc_median = float(np.median(grpc))
    axes.axvline(tcp_median, color="#D4A900", linestyle="--", linewidth=1.8)
    axes.axvline(grpc_median, color="#1976A3", linestyle="--", linewidth=1.8)

    axes.set_title("Order-entry trade-to-confirmation latency")
    axes.set_xlabel("Round-trip latency (microseconds)")
    axes.set_ylabel("Frequency")
    axes.set_xlim(0.0, max_us)
    axes.grid(axis="y", alpha=0.22)
    axes.legend()
    figure.tight_layout()

    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(arguments.output, dpi=180)
    if arguments.show:
        plt.show()


if __name__ == "__main__":
    main()
