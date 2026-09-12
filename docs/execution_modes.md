# Execution modes

`--execution-mode` selects the server policy. Default: `engine_wait`.

| Mode | Engine when ingress is empty | Network loop |
| --- | --- | --- |
| `engine_wait` | Atomic generation wait; notified after enqueue | TCP/UDP: poll + eventfd; gRPC: blocking CQ wait with timeout |
| `engine_spin` | Busy spin with CpuRelax | Repeated nonblocking receives / immediate CQ checks |
| `engine_probe` | Existing Probe using the last user trade and a synthetic feed timestamp | Same as engine_spin |

These modes affect the server's engine and network threads. Clients retain their
scheduled replay. gRPC internal worker threads are managed by gRPC.

Probe evicts expired window entries but does not insert the synthetic trade.
Consequently it can change decisions compared with eviction only on real events.
Separate TCP streams also do not guarantee a single combined arrival order.

## Build and compare

Inside the Linux workspace:

```bash
cmake --build build-profile --parallel
ctest --test-dir build-profile --output-on-failure

./scripts/run_replay.sh --build-dir build-profile --benchmark --transport tcp --symbol SYNTH4 --band-bps 0 --execution-mode engine_wait
./scripts/run_replay.sh --build-dir build-profile --benchmark --transport tcp --symbol SYNTH4 --band-bps 0 --execution-mode engine_spin
./scripts/run_replay.sh --build-dir build-profile --benchmark --transport tcp --symbol SYNTH4 --band-bps 0 --execution-mode engine_probe
```

Run these sequentially with the same symbol, parameters, CPU assignments and input.
Replay timing is unchanged; each command performs a full replay.

For each mode, outputs in the build directory are:

```text
execution_report_tcp_engine_wait.log
tick_to_order_raw_tcp_engine_wait.csv
oe_latency_tcp_engine_wait.csv
```

The suffix changes to `engine_spin` or `engine_probe`. The report and CSV rows also
identify the mode. A repeat of the same transport and mode overwrites its previous
files. TTO raw CSV is written in benchmark mode and retains nanosecond samples.
gRPC uses `grpc` instead of `tcp`; UDP multicast uses `tcp` because OE is TCP,
so it overwrites the corresponding TCP/mode filenames. The report includes the
full feed transport for identification.

```bash
python3 scripts/plot_tick_to_order.py --input \
  build-profile/tick_to_order_raw_tcp_engine_wait.csv \
  build-profile/tick_to_order_raw_tcp_engine_spin.csv \
  build-profile/tick_to_order_raw_tcp_engine_probe.csv \
  --output build-profile/tto_policies.png --bucket-us 1
```

The overlay uses shared histogram bins, separate colors, sample counts and median
lines. `--max-us` crops the displayed samples; reported medians still use all samples.
