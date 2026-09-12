import csv
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

root = Path("docs/profiling/rtt-tcp-vs-grpc-engine-probe")
series = []

for transport, color in [("tcp", "tab:blue"), ("grpc", "tab:orange")]:
    path = root / ("oe_latency_" + transport + "_engine_probe.csv")
    with path.open(newline="") as f:
        rows = list(csv.DictReader(f))
    if not rows:
        raise ValueError("Empty CSV: " + str(path))
    if any(r["execution_mode"] != "engine_probe" or
           r["transport"] != transport for r in rows):
        raise ValueError("Unexpected mode or transport: " + str(path))

    values = np.sort(np.array(
        [int(r["latency_ns"]) for r in rows], dtype=np.float64
    ) / 1000.0)
    if np.any(values < 0):
        raise ValueError("Negative RTT: " + str(path))
    series.append((transport.upper(), color, values))

def percentile(values, p):
    return values[int(np.ceil(p * len(values))) - 1]

fig, axes = plt.subplots(2, 2, figsize=(14, 9), constrained_layout=True)
fig.suptitle("TCP vs gRPC — order-entry RTT, engine_probe", fontsize=17)

# Shared 100 ns bins; show through the larger p99.
limit = max(percentile(v, .99) for _, _, v in series) * 1.05
bins = np.arange(0, limit + 0.1, 0.1)

for ax, (label, color, values) in zip(axes[0], series):
    visible = values[values <= limit]
    counts, _ = np.histogram(visible, bins=bins)
    ax.hist(visible, bins=bins, histtype="step",
            linewidth=1.2, color=color)
    ax.axvline(percentile(values, .50), color=color, linestyle="--")
    ax.set_yscale("log")
    ax.set_ylim(.8, max(3, counts.max() * 1.5))
    ax.set_xlim(0, limit)
    ax.set_title("{}: {} samples; {} above view".format(
        label, len(values), np.count_nonzero(values > limit)))
    ax.set_xlabel("RTT (µs)")
    ax.set_ylabel("Samples per 100 ns bin · log scale")
    ax.grid(True, alpha=.2)

for label, color, values in series:
    x, counts = np.unique(values, return_counts=True)
    y = 100.0 * np.cumsum(counts) / len(values)
    # Extend the empirical CDF from zero latency.
    x = np.r_[0.0, x]
    y = np.r_[0.0, y]
    for ax in axes[1]:
        ax.step(x, y, where="post", color=color, label=label)
        ax.set_xlabel("RTT (µs)")
        ax.set_ylabel("Samples with RTT ≤ x (%)")
        ax.grid(True, which="major", alpha=.2)
        ax.legend()

axes[1, 0].set_title("CDF — full distribution")
axes[1, 0].set_ylim(0, 101)
axes[1, 1].set_title("CDF — slowest 5%")
axes[1, 1].set_ylim(95, 100.1)

for ax in axes[1]:
    ax.set_xscale("symlog", linthresh=1)
    ax.set_xlabel("RTT (µs; logarithmic above 1 µs)")

output = root / "rtt_tcp_vs_grpc_engine_probe.png"
fig.savefig(str(output), dpi=180)
plt.close(fig)

summary = root / "rtt_summary.csv"
with summary.open("w", newline="") as f:
    writer = csv.writer(f)
    writer.writerow(["transport", "samples", "p50_us", "p99_us",
                     "p999_us", "max_us"])
    for label, _, values in series:
        stats = [percentile(values, p) for p in (.50, .99, .999)]
        writer.writerow([label, len(values)] + stats + [values[-1]])
        print("{}: n={}, p50={:.3f}, p99={:.3f}, "
              "p99.9={:.3f}, max={:.3f} µs".format(
                  label, len(values), *stats, values[-1]))

print("Saved", output)
print("Saved", summary)
