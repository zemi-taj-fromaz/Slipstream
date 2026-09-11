# TCP profiling results — 2026-09-11

Runbook: ../../perf-flamegraph-runbook.md

Configuration: TCP, SYNTH4, band-bps 0.
Build: RelWithDebInfo, -O3 -g -DNDEBUG -fno-omit-frame-pointer.
Recording: cycles:u, 999 Hz target, frame-pointer call graphs, engine TID 31109.

## Findings
75 CPU samples; zero reported lost samples.
First-to-last sample span: approximately 56 minutes.
Engine::Run, TradeManager::Push and VwapWindow appear in the recording.
Sample count is insufficient for reliable hotspot rankings.
Some unresolved addresses and suspicious outer frames require investigation.

## Execution report
Market quantity: 1000300
Executed quantity: 136130
Average fill price: 33.8019
Session VWAP: 33.8141
Slippage: -6.14 bps
Participation: 13.61%
Tick-to-order p50 / p99 / p99.9: 112.977 / 188.465 / 239.393 us
Latency samples: 890; samples >= 5000 us: 0.

These profiled latency values are not final Release benchmark results.

## Next steps
Validate a denser workload and inspect a short recording before another long run.
Text and SVG are portable. Raw perf re-symbolization needs the exact executable
and matching libraries/debug symbols. Preserve the original profiling build.
