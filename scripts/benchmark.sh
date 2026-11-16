#!/usr/bin/env bash
set -euo pipefail
# Run after local modbus-sim and bms-collector are available.
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$ROOT/benchmarks/results"
STAMP="$(date -u +%Y%m%dT%H%M%SZ)"
"$ROOT/build/bms-edge" --host "${SIM_HOST:-127.0.0.1}" --port "${SIM_PORT:-15020}" \
  --collector "${COLLECTOR_HOST:-127.0.0.1}" --collector-port "${COLLECTOR_PORT:-17001}" \
  --rounds "${ROUNDS:-1000}" --poll-ms "${POLL_MS:-10}" \
  --metrics-file "$ROOT/benchmarks/results/${STAMP}.json" \
  > "$ROOT/benchmarks/results/${STAMP}.jsonl"
printf 'Evidence: %s\n' "$ROOT/benchmarks/results/${STAMP}.json"
