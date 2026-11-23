#!/bin/bash
# Runs the full-vs-incremental SPF experiment sweep for the paper.
# Every configuration writes raw per-event data to results/<timestamp>/.
#
# Usage: ./scripts/run_benchmark.sh [reps] [trials]
#   Run from the project root after building (cmake --build build).
#   Close other applications first — background load adds timing noise.

set -e
REPS="${1:-10}"
TRIALS="${2:-1000}"
BENCH=./build/benchmark
OUT="results/$(date +%Y%m%d_%H%M%S)"
mkdir -p "$OUT"

if [ ! -x "$BENCH" ]; then
  echo "Build first: cmake -B build && cmake --build build"; exit 1
fi

echo "Output: $OUT  (reps=$REPS, trials=$TRIALS)"
{ echo "date: $(date)"; echo "cpu: $(nproc) cores"; uname -a; } > "$OUT/machine.txt"

# Headline numbers on the 20-router campus topology
$BENCH --topology campus --reps "$REPS" --trials "$TRIALS" \
       --csv "$OUT/campus.csv" | tee "$OUT/campus.txt"

# Scaling with topology size, single failures
for N in 10 20 50 100; do
  $BENCH --nodes "$N" --reps "$REPS" --trials "$TRIALS" \
         --csv "$OUT/size_${N}.csv" > "$OUT/size_${N}.txt"
  echo "size $N done"
done

# Crossover: burst size at a fixed topology size
for B in 1 2 4 8 16 32; do
  $BENCH --nodes 50 --burst "$B" --reps "$REPS" --trials "$TRIALS" \
         --csv "$OUT/burst_${B}.csv" > "$OUT/burst_${B}.txt"
  echo "burst $B done"
done

# Concurrent read load
for K in 0 1 2 4 8; do
  $BENCH --nodes 50 --readers "$K" --reps "$REPS" --trials "$TRIALS" \
         --csv "$OUT/readers_${K}.csv" > "$OUT/readers_${K}.txt"
  echo "readers $K done"
done

echo "All results in $OUT"
