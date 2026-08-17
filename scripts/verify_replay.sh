#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(cd -- "${script_dir}/.." && pwd)"
build_dir="${1:-${project_dir}/build-linux}"
verification_dir="${build_dir}/verification"

status=0

verify_stream() {
    local stream="$1"
    local expected="${verification_dir}/expected_${stream}.csv"
    local received="${verification_dir}/received_${stream}.csv"

    for file in "${expected}" "${received}"; do
        if [[ ! -f "${file}" ]]; then
            echo "[verify] missing file: ${file}" >&2
            status=1
            return
        fi
    done

    local expected_count
    local received_count
    expected_count=$(($(wc -l < "${expected}") - 1))
    received_count=$(($(wc -l < "${received}") - 1))

    if cmp -s "${expected}" "${received}"; then
        echo "[verify] PASS ${stream}: ${received_count} events, exact order and fields match"
        return
    fi

    echo "[verify] FAIL ${stream}: expected ${expected_count} events, received ${received_count}" >&2
    echo "[verify] first differences:" >&2
    diff -u "${expected}" "${received}" | sed -n '1,120p' >&2 || true
    status=1
}

verify_stream quotes
verify_stream trades

if ((status == 0)); then
    echo "[verify] SUCCESS: both TCP streams match their CSV-derived sequences"
else
    echo "[verify] FAILURE: replay output does not match" >&2
fi

exit "${status}"
