import argparse
import csv
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", nargs="+", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--bucket-us", type=float, default=0.1)
    args = parser.parse_args()
    if not np.isfinite(args.bucket_us) or args.bucket_us <= 0:
        parser.error("--bucket-us must be positive and finite")

    series = []
    for filename in args.input:
        path = Path(filename)
        with path.open(newline="") as stream:
            values = np.array([
                float(row["tto_ns"]) / 1000.0
                for row in csv.DictReader(stream)
            ])
        if not len(values):
            raise ValueError("No samples in {}".format(path))
        if not np.all(np.isfinite(values)) or np.any(values < 0):
            raise ValueError("Invalid latency values in {}".format(path))

        prefix = "tick_to_order_raw_"
        label = path.stem
        if label.startswith(prefix):
            label = label[len(prefix):]
        series.append((label, np.sort(values)))

    colors = plt.get_cmap("tab10").colors
    fig, axes = plt.subplots(1, 3, figsize=(19, 6))
    fig.subplots_adjust(left=0.065, right=0.98, bottom=0.29,
                        top=0.85, wspace=0.28)

    for ax, limit, title in [
        (axes[0], 25.0, "Fast modes: 0–25 µs"),
        (axes[1], 160.0, "Main distributions: 0–160 µs"),
    ]:
        bins = np.arange(0, limit + args.bucket_us, args.bucket_us)
        largest_count = 1
        notes = []
        for index, (label, values) in enumerate(series):
            color = colors[index % len(colors)]
            visible = values[values <= limit]
            counts, _ = np.histogram(visible, bins=bins)
            largest_count = max(largest_count, counts.max())
            ax.hist(visible, bins=bins, histtype="step",
                    linewidth=1.4, color=color)
            median = np.median(values)
            if median <= limit:
                ax.axvline(median, color=color, linestyle="--",
                           linewidth=1, alpha=0.8)
            notes.append("{}: {} above view".format(
                label, np.count_nonzero(values > limit)))

        ax.set_yscale("log")
        ax.set_ylim(0.8, max(3, largest_count * 1.5))
        ax.set_xlim(0, limit)
        ax.set_title(title)
        ax.set_xlabel("Latency (µs)")
        ax.set_ylabel("Samples per {:g} ns bin · log scale".format(
            args.bucket_us * 1000))
        ax.grid(True, which="major", alpha=0.2)
        ax.text(0, -0.23, "\n".join(notes), transform=ax.transAxes,
                fontsize=8, va="top")

    ax = axes[2]
    for index, (label, values) in enumerate(series):
        unique, counts = np.unique(values, return_counts=True)
        percent_at_or_above = (
            100.0 * (len(values) - np.r_[0, np.cumsum(counts)[:-1]])
            / len(values)
        )
        ax.step(unique, percent_at_or_above, where="pre",
                color=colors[index % len(colors)], linewidth=1.5,
                label=label)
        p50 = values[int(np.ceil(0.50 * len(values))) - 1]
        p99 = values[int(np.ceil(0.99 * len(values))) - 1]
        print("{}: n={}, p50={:.3f}, p99={:.3f}, max={:.3f} µs".format(
            label, len(values), p50, p99, values[-1]))

    ax.set_xscale("symlog", linthresh=1)
    ax.set_yscale("log")
    ax.set_title("Full distribution — all samples")
    ax.set_xlabel("Latency (µs; logarithmic above 1 µs)")
    ax.set_ylabel("Samples at or above latency (%)")
    ax.grid(True, which="major", alpha=0.2)
    ax.legend(fontsize=8, loc="best")

    fig.suptitle("Slipstream TCP tick-to-order latency", fontsize=18)
    fig.text(0.065, 0.045,
             "Dashed lines: full-data medians. Histogram views are cropped; "
             "the right panel includes every sample.",
             fontsize=10)
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(str(output), dpi=180)
    plt.close(fig)
    print("Saved {}".format(output))


if __name__ == "__main__":
    main()
