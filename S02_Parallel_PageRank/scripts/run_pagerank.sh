#!/usr/bin/env bash
# Runs the complete S02 (PageRank) experiment set and appends raw rows to results/pr_raw.csv.
# Same thread list / pinning / hardware label logic as run_experiments.sh so the batch can be merged.
#
# Overridable environment variables (defaults in brackets):
#   THREADS          thread counts, comma separated [powers of two up to the logical core count]
#   REPS             timed repetitions per configuration [10]
#   PROFILE_REPS     instrumented repetitions per configuration [3]
#   SCHEDULES        OpenMP loop schedules, comma separated [static,dynamic]
#   CHUNK            chunk size for dynamic [64]
#   RMAT_SCALES      symmetrised R-MAT scales (same graphs as S1) [16 18 20]
#   RMAT_DIR_SCALES  directed R-MAT scales (these have dangling nodes) []   e.g. "18"
#   UNDIRECTED_FILES SNAP files treated as undirected [data/ca-GrQc.txt data/email-Enron.txt data/roadNet-CA.txt]
#   DIRECTED_FILES   SNAP files kept directed [data/web-Google.txt]  (scripts/download_data.sh web-Google)
#   TOL / DAMPING / MAX_ITER   convergence tolerance (L1) / damping / iteration cap [1e-8 / 0.85 / 100]
#   STUDENT_ID [S2]   BATCH_ID [CS4]   OUT [results/pr_raw.csv]   STATS_OUT [results/pr_graph_stats.csv]
#   CXX [g++]   PR_FLAGS  compiler flags [-O3 -march=native -std=c++17 -fopenmp]
#   HARDWARE_LABEL   override the hardware label (must be identical across the batch)
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
PROFILE_REPS=${PROFILE_REPS:-3}
SCHEDULES=${SCHEDULES:-static,dynamic}
CHUNK=${CHUNK:-64}
RMAT_SCALES=${RMAT_SCALES:-"16 18 20"}
RMAT_DIR_SCALES=${RMAT_DIR_SCALES:-""}
UNDIRECTED_FILES=${UNDIRECTED_FILES:-"data/ca-GrQc.txt data/email-Enron.txt data/roadNet-CA.txt"}
DIRECTED_FILES=${DIRECTED_FILES:-"data/web-Google.txt"}
TOL=${TOL:-1e-8}; DAMPING=${DAMPING:-0.85}; MAX_ITER=${MAX_ITER:-100}
STUDENT_ID=${STUDENT_ID:-S2}
BATCH_ID=${BATCH_ID:-CS4}
OUT=${OUT:-results/pr_raw.csv}
STATS_OUT=${STATS_OUT:-results/pr_graph_stats.csv}
CXX=${CXX:-g++}
PR_FLAGS=${PR_FLAGS:-"-O3 -march=native -std=c++17 -fopenmp"}

mkdir -p bin results "$(dirname "$OUT")"
$CXX $PR_FLAGS -Isrc src/pagerank_bench.cpp -o bin/pagerank_bench
scripts/collect_env.sh > /dev/null
HW=${HARDWARE_LABEL:-$(cat results/hw_string.txt)}

echo "threads=${THREADS} schedules=${SCHEDULES} reps=${REPS} profile_reps=${PROFILE_REPS} tol=${TOL} damping=${DAMPING} max_iter=${MAX_ITER}"
echo "OMP_PROC_BIND=${OMP_PROC_BIND} OMP_PLACES=${OMP_PLACES}"
echo "hardware: ${HW}"

run() {  # run <graph spec> <name> [extra args]
  local spec=$1 name=$2; shift 2
  ./bin/pagerank_bench --graph "$spec" --name "$name" --threads "$THREADS" --schedules "$SCHEDULES" --chunk "$CHUNK" \
      --reps "$REPS" --profile-reps "$PROFILE_REPS" --tol "$TOL" --damping "$DAMPING" --max-iter "$MAX_ITER" \
      --out "$OUT" --stats-out "$STATS_OUT" --student-id "$STUDENT_ID" --batch-id "$BATCH_ID" --hardware "$HW" "$@"
}

for f in $UNDIRECTED_FILES; do
  if [ -f "$f" ]; then b=$(basename "$f"); run "$f" "${b%.*}"
  else echo "WARNING: $f not found - run scripts/download_data.sh first (skipping)" >&2; fi
done
for f in $DIRECTED_FILES; do
  if [ -f "$f" ]; then b=$(basename "$f"); run "$f" "${b%.*}" --directed
  else echo "WARNING: $f not found - run scripts/download_data.sh web-Google first (skipping)" >&2; fi
done
for s in $RMAT_SCALES; do run "rmat:${s}:16:1" "rmat-s${s}"; done
for s in $RMAT_DIR_SCALES; do run "rmat:${s}:16:1" "rmat-s${s}-dir" --directed; done

echo "done. next: python3 scripts/analyze_pagerank.py"
