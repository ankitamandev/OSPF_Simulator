#!/bin/bash
# Runs the reconvergence benchmark and saves results with a timestamp.
# Usage: ./scripts/run_benchmark.sh [num_nodes] [num_trials]

NODES="${1:-50}"
TRIALS="${2:-1000}"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
OUTFILE="benchmark_results_${TIMESTAMP}.txt"

echo "Running benchmark: ${NODES} nodes, ${TRIALS} trials..."
./build/benchmark "${NODES}" "${TRIALS}" | tee "${OUTFILE}"

echo ""
echo "Results saved to ${OUTFILE}"