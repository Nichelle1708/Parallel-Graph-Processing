#!/usr/bin/env bash
# Runs the complete S01 experiment set and appends raw rows to results/bfs_raw.csv.
#
# Overridable environment variables (defaults in brackets):
#   THREADS        thread counts, comma separated  [powers of two up to the logical core count]
#   REPS           timed repetitions per source & configuration  [10]
#   SOURCES        BFS source vertices per graph (sampled from the largest component)  [5]
#   PROFILE_REPS   instrumented repetitions per source & thread count  [3]
#   RMAT_SCALES    synthetic R-MAT scales  [16 18 20]   (edge factor 16)
#   SNAP_FILES     space separated SNAP/MTX files  [data/ca-GrQc.txt data/email-Enron.txt data/roadNet-CA.txt]
#   STUDENT_ID     [S1]      BATCH_ID  [CS4]
#   OUT            raw CSV path  [results/bfs_raw.csv]
set -euo pipefail
cd "$(dirname "$0")/.."

export OMP_PROC_BIND=${OMP_PROC_BIND:-close}
export OMP_PLACES=${OMP_PLACES:-cores}

LOGICAL=$(nproc --all)
if [ -z "${THREADS:-}" ]; then
  T=1; THREADS="1"
  while [ $((T * 2)) -le "$LOGICAL" ]; do T=$((T * 2)); THREADS="${THREADS},${T}"; done
fi
REPS=${REPS:-10}
SOURCES=${SOURCES:-5}
PROFILE_REPS=${PROFILE_REPS:-3}
RMAT_SCALES=${RMAT_SCALES:-"16 18 20"}
SNAP_FILES=${SNAP_FILES:-"data/ca-GrQc.txt data/email-Enron.txt data/roadNet-CA.txt"}
STUDENT_ID=${STUDENT_ID:-S1}
BATCH_ID=${BATCH_ID:-CS4}
OUT=${OUT:-results/bfs_raw.csv}
STATS_OUT=${STATS_OUT:-results/graph_stats.csv}

make -s all
scripts/collect_env.sh > /dev/null
HW=$(cat results/hw_string.txt)
mkdir -p "$(dirname "$OUT")"

echo "threads=${THREADS} reps=${REPS} sources=${SOURCES} profile_reps=${PROFILE_REPS}"
echo "OMP_PROC_BIND=${OMP_PROC_BIND} OMP_PLACES=${OMP_PLACES}"

run() {  # run <graph spec> <name>
  ./bin/bfs_bench --graph "$1" --name "$2" --threads "$THREADS" --reps "$REPS" --sources "$SOURCES" \
      --profile-reps "$PROFILE_REPS" --out "$OUT" --stats-out "$STATS_OUT" \
      --student-id "$STUDENT_ID" --batch-id "$BATCH_ID" --hardware "$HW"
}

for f in $SNAP_FILES; do
  if [ -f "$f" ]; then
    b=$(basename "$f"); run "$f" "${b%.*}"
  else
    echo "WARNING: $f not found - run scripts/download_data.sh first (skipping)" >&2
  fi
done
for s in $RMAT_SCALES; do run "rmat:${s}:16:1" "rmat-s${s}"; done

echo "done. next: python3 scripts/analyze.py"
