#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(cd -- "${script_dir}/.." && pwd)"
build_dir="${project_dir}/build-linux"
start_delay_seconds="${SLIPSTREAM_START_DELAY_SECONDS:-3}"
md_host="127.0.0.1"
md_port="9001"
oe_host="127.0.0.1"
oe_port="9002"
transport="tcp"
benchmark="false"
md_a_group="239.255.0.1"
md_a_port="14200"
md_b_group="239.255.0.2"
md_b_port="14201"
md_multicast_interface="0.0.0.0"
main_cpu="0"
network_cpu="2"
engine_cpu="3"
md_client_cpu="4"
oe_client_cpu="5"
server_args=()

while (($# > 0)); do
    case "$1" in
        --build-dir)
            build_dir="$2"
            shift 2
            ;;
        --md-host)
            md_host="$2"
            shift 2
            ;;
        --md-port)
            md_port="$2"
            shift 2
            ;;
        --oe-host)
            oe_host="$2"
            shift 2
            ;;
        --oe-port)
            oe_port="$2"
            shift 2
            ;;
        --transport)
            transport="$2"
            shift 2
            ;;
        --benchmark)
            benchmark="true"
            shift
            ;;
        --md-a-group)
            md_a_group="$2"
            shift 2
            ;;
        --md-a-port)
            md_a_port="$2"
            shift 2
            ;;
        --md-b-group)
            md_b_group="$2"
            shift 2
            ;;
        --md-b-port)
            md_b_port="$2"
            shift 2
            ;;
        --md-multicast-interface)
            md_multicast_interface="$2"
            shift 2
            ;;
        --main-cpu)
            main_cpu="$2"
            shift 2
            ;;
        --network-cpu)
            network_cpu="$2"
            shift 2
            ;;
        --engine-cpu)
            engine_cpu="$2"
            shift 2
            ;;
        --md-client-cpu)
            md_client_cpu="$2"
            shift 2
            ;;
        --oe-client-cpu)
            oe_client_cpu="$2"
            shift 2
            ;;
        *)
            server_args+=("$1")
            shift
            ;;
    esac
done

server_args+=(
    --md-host "${md_host}"
    --md-port "${md_port}"
    --oe-host "${oe_host}"
    --oe-port "${oe_port}"
    --transport "${transport}"
    --md-a-group "${md_a_group}"
    --md-a-port "${md_a_port}"
    --md-b-group "${md_b_group}"
    --md-b-port "${md_b_port}"
    --md-multicast-interface "${md_multicast_interface}"
    --main-cpu "${main_cpu}"
    --network-cpu "${network_cpu}"
    --engine-cpu "${engine_cpu}"
)

client_benchmark_args=()
if [[ "${benchmark}" == "true" ]]; then
    server_args+=(--benchmark)
    client_benchmark_args+=(--benchmark)
fi

server="${build_dir}/slipstream/slipstream"
md_client="${build_dir}/market_data_client/market_data_client"
oe_client="${build_dir}/order_entry_client/order_entry_client"

for executable in "${server}" "${md_client}" "${oe_client}"; do
    if [[ ! -x "${executable}" ]]; then
        echo "[launcher] missing executable: ${executable}" >&2
        exit 1
    fi
done

if [[ ! "${start_delay_seconds}" =~ ^[0-9]+$ ]]; then
    echo "[launcher] SLIPSTREAM_START_DELAY_SECONDS must be a non-negative integer" >&2
    exit 1
fi

pids=()

cleanup() {
    for pid in "${pids[@]}"; do
        if kill -0 "${pid}" 2>/dev/null; then
            kill "${pid}" 2>/dev/null || true
        fi
    done
}

trap cleanup EXIT INT TERM

"${server}" "${server_args[@]}" &
pids+=("$!")

sleep 0.2

if ! kill -0 "${pids[0]}" 2>/dev/null; then
    echo "[launcher] slipstream exited before the clients could connect" >&2
    exit 1
fi

now_ns="$(date +%s%N)"
start_at_ns=$((now_ns + start_delay_seconds * 1000000000))

if [[ "${benchmark}" != "true" ]]; then
    echo "[launcher] Replay starts at Unix nanoseconds: ${start_at_ns}"
fi

"${md_client}" \
    --host "${md_host}" \
    --port "${md_port}" \
    --transport "${transport}" \
    --md-a-group "${md_a_group}" \
    --md-a-port "${md_a_port}" \
    --md-b-group "${md_b_group}" \
    --md-b-port "${md_b_port}" \
    --md-multicast-interface "${md_multicast_interface}" \
    --cpu "${md_client_cpu}" \
    "${client_benchmark_args[@]}" \
    --start-at-ns "${start_at_ns}" &
pids+=("$!")

oe_transport="${transport}"
if [[ "${transport}" == "udp-multicast" ]]; then
    oe_transport="tcp"
fi

"${oe_client}" \
    --host "${oe_host}" \
    --port "${oe_port}" \
    --transport "${oe_transport}" \
    --cpu "${oe_client_cpu}" \
    "${client_benchmark_args[@]}" \
    --start-at-ns "${start_at_ns}" &
pids+=("$!")

status=0
for pid in "${pids[@]}"; do
    if ! wait "${pid}"; then
        status=1
    fi
done

trap - EXIT INT TERM
exit "${status}"
