# Slipstream native Linux perf and FlameGraph runbook

This is an internal operational runbook for the repository owner and coding
agents. It is not public-facing benchmark documentation. Follow it on the
native Linux benchmark machine instead of Docker Desktop. Do not use latency
numbers collected while `perf` is attached as the final TTO/RTT results.

## Objective

Produce a CPU profile and FlameGraph for the `slip-engine` thread while the
real Slipstream server, clients, queues, codec, and decision engine are
running. Profile TCP and gRPC separately.

The final profiling artifacts are:

```text
build-profile-native/perf_engine_tcp.data
build-profile-native/perf_engine_tcp.txt
build-profile-native/perf_engine_tcp.svg
build-profile-native/perf_engine_grpc.data
build-profile-native/perf_engine_grpc.txt
build-profile-native/perf_engine_grpc.svg
```

The final latency benchmark is a separate Release run without `perf`.

## Pinned dependencies

```text
gRPC tag:     v1.83.0
gRPC commit:  c876f4da50f7da2f331888b88b2a7243514139fe
Protobuf:     35.1 from the pinned gRPC dependency tree
```

## 1. Machine preflight

Run these commands before installing or building anything:

```bash
cat /etc/os-release
uname -m
uname -r
nproc
lscpu | grep -E 'Model name|Socket|Core|Thread|CPU\(s\)'
cat /proc/sys/kernel/perf_event_paranoid
```

Expected architecture is `x86_64`. Slipstream defaults require at least six
logical CPUs because the server and clients default to CPUs 0, 2, 3, 4, and 5.
Override the CPU arguments if the machine topology requires different CPUs.

Check the frequency governor without changing it:

```bash
grep . /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor 2>/dev/null | sort -u
```

Record the machine information with the benchmark results. Do not change the
governor, SMT, IRQ affinity, kernel settings, or isolated CPUs without the
machine owner's permission.

## 2. Install build and analysis tools

For Ubuntu/Debian:

```bash
sudo apt update
sudo apt install -y \
  build-essential \
  cmake \
  ninja-build \
  git \
  ca-certificates \
  autoconf \
  libtool \
  pkg-config \
  procps \
  python3 \
  python3-venv \
  python3-pip
```

On Ubuntu, install the matching perf package:

```bash
sudo apt install -y linux-tools-common "linux-tools-$(uname -r)"
```

On Debian, use:

```bash
sudo apt install -y linux-perf
```

Verify:

```bash
cmake --version
c++ --version
ninja --version
perf --version
python3 --version
```

## 3. Build and install the pinned gRPC version

Use a new temporary source and build directory. If these paths already exist,
inspect them and either reuse them or choose different explicit paths; do not
delete an unknown directory.

```bash
git clone \
  --branch v1.83.0 \
  --depth 1 \
  --recurse-submodules \
  --shallow-submodules \
  https://github.com/grpc/grpc.git \
  /tmp/slipstream-grpc-source
```

Verify the exact commit:

```bash
git -C /tmp/slipstream-grpc-source rev-parse HEAD
```

It must print:

```text
c876f4da50f7da2f331888b88b2a7243514139fe
```

Configure gRPC:

```bash
cmake \
  -S /tmp/slipstream-grpc-source \
  -B /tmp/slipstream-grpc-build \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/opt/grpc \
  -DgRPC_INSTALL=ON \
  -DgRPC_BUILD_TESTS=OFF \
  -DgRPC_BUILD_CODEGEN=ON \
  -DgRPC_BUILD_GRPC_CPP_PLUGIN=ON \
  -DgRPC_ABSL_PROVIDER=module \
  -DgRPC_CARES_PROVIDER=module \
  -DgRPC_PROTOBUF_PROVIDER=module \
  -DgRPC_RE2_PROVIDER=module \
  -DgRPC_SSL_PROVIDER=module \
  -DgRPC_ZLIB_PROVIDER=module
```

Build and install:

```bash
cmake --build /tmp/slipstream-grpc-build --parallel "$(nproc)"
sudo cmake --install /tmp/slipstream-grpc-build
```

Verify the installation:

```bash
/opt/grpc/bin/protoc --version
find /opt/grpc \
  -name gRPCConfig.cmake \
  -o -name ProtobufConfig.cmake
```

`protoc --version` should report `libprotoc 35.1`.

## 4. Install Python plotting dependencies and FlameGraph

Keep the Python environment outside the repository:

```bash
python3 -m venv /tmp/slipstream-benchmark-venv
/tmp/slipstream-benchmark-venv/bin/pip install --upgrade pip
/tmp/slipstream-benchmark-venv/bin/pip install numpy matplotlib
```

Install Brendan Gregg's FlameGraph scripts:

```bash
git clone --depth 1 \
  https://github.com/brendangregg/FlameGraph.git \
  /tmp/FlameGraph
```

## 5. Verify hardware performance counters

Run a short CPU workload:

```bash
sudo perf stat \
  -e cycles:u \
  -e instructions:u \
  -e branches:u \
  -e branch-misses:u \
  -- dd if=/dev/zero of=/dev/null bs=1M count=256 status=none
```

Do not continue with the final profile if `cycles:u` says `<not supported>`.
That means the PMU is unavailable. If the error says permission denied, use
`sudo` and inspect `/proc/sys/kernel/perf_event_paranoid`. Do not permanently
change the friend's machine configuration without permission.

## 6. Build the profiling configuration

From the Slipstream repository root, first verify the location:

```bash
test -f ./CMakeLists.txt
```

Configure and build:

```bash
cmake \
  -S . \
  -B build-profile-native \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_PREFIX_PATH=/opt/grpc \
  -DCMAKE_CXX_FLAGS_RELWITHDEBINFO="-O3 -g -DNDEBUG -fno-omit-frame-pointer"
```

```bash
cmake --build build-profile-native \
  --parallel "$(nproc)"
```

Run all tests before profiling:

```bash
ctest \
  --test-dir build-profile-native \
  --output-on-failure
```

## 7. Profile the real TCP Engine thread

Use two terminals in the same native Linux environment.

### Terminal A: start Slipstream and the clients

From the repository root:

```bash
SLIPSTREAM_START_DELAY_SECONDS=60 \
./scripts/run_replay.sh \
  --build-dir build-profile-native \
  --benchmark \
  --transport tcp \
  --symbol SYNTH4 \
  --band-bps 0
```

The 60-second start delay is only an attach window. Both clients still receive
the same synchronized absolute start timestamp.

### Terminal B: find only the Engine thread

From the repository root:

```bash
server_pid="$(pgrep -n -f '[b]uild-profile-native/slipstream/slipstream')"
test -n "${server_pid:?Slipstream server PID was not found}"
ps -T -p "${server_pid:?}" -o pid,tid,psr,comm
```

The process is named `slip-main`, not `slipstream`, after thread naming. That
is why PID discovery searches the command line instead of using
`pgrep -x slipstream`.

Find the Engine TID:

```bash
engine_tid="$(
  ps -T -p "${server_pid:?}" -o tid=,comm= |
  awk '$2 == "slip-engine" { print $1; exit }'
)"
test -n "${engine_tid:?slip-engine TID was not found}"
echo "server PID=${server_pid}, engine TID=${engine_tid}"
```

Attach perf only to that TID:

```bash
sudo perf record \
  -e cycles:u \
  -F 999 \
  -g \
  --call-graph fp \
  -t "${engine_tid:?}" \
  -o build-profile-native/perf_engine_tcp.data
```

Let the replay finish. If `perf` remains active after the Engine exits, stop
it with `Ctrl+C`. Do not use latency values from this profiled run as the
final latency benchmark.

## 8. Create the TCP textual report and FlameGraph

Inspect metadata and sample count first:

```bash
sudo perf report \
  -i build-profile-native/perf_engine_tcp.data \
  --header-only
```

Generate a text report:

```bash
sudo perf report \
  -i build-profile-native/perf_engine_tcp.data \
  --stdio \
  --sort dso,symbol \
  > build-profile-native/perf_engine_tcp.txt
```

Generate the FlameGraph:

```bash
sudo perf script \
  -i build-profile-native/perf_engine_tcp.data |
  /tmp/FlameGraph/stackcollapse-perf.pl \
  > build-profile-native/perf_engine_tcp.folded
```

```bash
/tmp/FlameGraph/flamegraph.pl \
  --title "Slipstream TCP Engine Thread" \
  --subtitle "cycles:u, O3, frame-pointer call graph" \
  build-profile-native/perf_engine_tcp.folded \
  > build-profile-native/perf_engine_tcp.svg
```

If root owns generated files, return ownership to the current user:

```bash
sudo chown "$(id -u):$(id -g)" \
  build-profile-native/perf_engine_tcp.data \
  build-profile-native/perf_engine_tcp.txt \
  build-profile-native/perf_engine_tcp.folded
```

## 9. Profile gRPC

Repeat the same workflow with this Terminal A command:

```bash
SLIPSTREAM_START_DELAY_SECONDS=60 \
./scripts/run_replay.sh \
  --build-dir build-profile-native \
  --benchmark \
  --transport grpc \
  --symbol SYNTH4 \
  --band-bps 0
```

Use the same PID/TID discovery commands, then record:

```bash
sudo perf record \
  -e cycles:u \
  -F 999 \
  -g \
  --call-graph fp \
  -t "${engine_tid:?}" \
  -o build-profile-native/perf_engine_grpc.data
```

Generate the text report:

```bash
sudo perf report \
  -i build-profile-native/perf_engine_grpc.data \
  --stdio \
  --sort dso,symbol \
  > build-profile-native/perf_engine_grpc.txt
```

Generate the FlameGraph:

```bash
sudo perf script \
  -i build-profile-native/perf_engine_grpc.data |
  /tmp/FlameGraph/stackcollapse-perf.pl \
  > build-profile-native/perf_engine_grpc.folded
```

```bash
/tmp/FlameGraph/flamegraph.pl \
  --title "Slipstream gRPC Engine Thread" \
  --subtitle "cycles:u, O3, frame-pointer call graph" \
  build-profile-native/perf_engine_grpc.folded \
  > build-profile-native/perf_engine_grpc.svg
```

## 10. Run authoritative latency benchmarks without perf

Use the Release build for final TTO and OE RTT data:

```bash
cmake \
  -S . \
  -B build-benchmark-native \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/opt/grpc
cmake --build build-benchmark-native --parallel "$(nproc)"
ctest --test-dir build-benchmark-native --output-on-failure
```

TCP:

```bash
./scripts/run_replay.sh \
  --build-dir build-benchmark-native \
  --benchmark \
  --transport tcp \
  --symbol SYNTH4 \
  --band-bps 0
```

gRPC:

```bash
./scripts/run_replay.sh \
  --build-dir build-benchmark-native \
  --benchmark \
  --transport grpc \
  --symbol SYNTH4 \
  --band-bps 0
```

Expected benchmark outputs:

```text
build-benchmark-native/execution_report_tcp.log
build-benchmark-native/execution_report_grpc.log
build-benchmark-native/tick_to_order_raw_tcp.csv
build-benchmark-native/tick_to_order_raw_grpc.csv
build-benchmark-native/oe_latency_tcp.csv
build-benchmark-native/oe_latency_grpc.csv
```

Plot existing histograms:

```bash
/tmp/slipstream-benchmark-venv/bin/python \
  scripts/plot_tick_to_order.py \
  --input build-benchmark-native/tick_to_order_raw_tcp.csv \
  --output build-benchmark-native/tick_to_order_tcp.png
```

```bash
/tmp/slipstream-benchmark-venv/bin/python \
  scripts/plot_tick_to_order.py \
  --input build-benchmark-native/tick_to_order_raw_grpc.csv \
  --output build-benchmark-native/tick_to_order_grpc.png
```

```bash
/tmp/slipstream-benchmark-venv/bin/python \
  scripts/plot_oe_latency.py \
  --tcp build-benchmark-native/oe_latency_tcp.csv \
  --grpc build-benchmark-native/oe_latency_grpc.csv \
  --output build-benchmark-native/oe_latency_tcp_vs_grpc.png
```

## 11. Interpret the profile carefully

The CPU FlameGraph answers where the Engine spends active CPU time. It does
not show how long the Engine remains blocked in `atomic::wait()`. Width means
sample share, not latency per call.

The real-time CSV replay is sparse. The Engine sleeps most of the time and may
accumulate too few CPU samples for a statistically useful FlameGraph. Check
the sample count before drawing conclusions. If only tens of samples exist,
do not present the graph as bottleneck evidence. The next step would be an
accelerated, semantically valid profiling workload that preserves the real
Engine, queues, timestamps, and decisions while providing many more events.

Use raw TTO and OE RTT from the unprofiled Release runs for latency claims.
Use `perf record` and FlameGraphs only to explain active CPU-cost distribution.
