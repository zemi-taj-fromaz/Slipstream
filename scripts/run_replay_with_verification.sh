#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(cd -- "${script_dir}/.." && pwd)"
build_dir="${project_dir}/build-linux"

arguments=("$@")
for ((index = 0; index < ${#arguments[@]}; ++index)); do
    if [[ "${arguments[index]}" == "--build-dir" ]]; then
        if ((index + 1 >= ${#arguments[@]})); then
            echo "[launcher] --build-dir is missing its value" >&2
            exit 1
        fi
        build_dir="${arguments[index + 1]}"
        break
    fi
done

verification_dir="${build_dir}/verification"

mkdir -p "${verification_dir}"
rm -f \
    "${verification_dir}/expected_quotes.csv" \
    "${verification_dir}/received_quotes.csv" \
    "${verification_dir}/expected_trades.csv" \
    "${verification_dir}/received_trades.csv"

echo "[launcher] Replay verification is enabled"

SLIPSTREAM_VERIFY_REPLAY=1 \
    "${script_dir}/run_replay.sh" "$@"

"${script_dir}/verify_replay.sh" "${build_dir}"
