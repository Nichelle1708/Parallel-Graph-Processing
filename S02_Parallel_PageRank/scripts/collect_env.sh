#!/usr/bin/env bash
# Records the experimental environment (required by the case-study manual) into
# results/env.txt and a one-line hardware label into results/hw_string.txt.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p results

CPU=$(lscpu | awk -F: '/^Model name/{gsub(/^[ \t]+/,"",$2); print $2; exit}')
LOGICAL=$(nproc --all)
SOCKETS=$(lscpu | awk -F: '/^Socket\(s\)/{gsub(/ /,"",$2); print $2; exit}')
CPS=$(lscpu | awk -F: '/^Core\(s\) per socket/{gsub(/ /,"",$2); print $2; exit}')
THREADS_PER_CORE=$(lscpu | awk -F: '/^Thread\(s\) per core/{gsub(/ /,"",$2); print $2; exit}')
RAM_GB=$(awk '/MemTotal/{printf "%.1f", $2/1024/1024}' /proc/meminfo)
OS=$(. /etc/os-release 2>/dev/null && echo "${PRETTY_NAME:-unknown}")
KERNEL=$(uname -r)
CXX_BIN=${CXX:-g++}
CXX_VER=$($CXX_BIN --version | head -n1)

echo "${CPU}; ${LOGICAL} logical cores; ${RAM_GB} GB RAM; ${OS}" | tr -d '\n' > results/hw_string.txt

{
  echo "date_utc:            $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "cpu_model:           ${CPU}"
  echo "sockets:             ${SOCKETS:-?}"
  echo "cores_per_socket:    ${CPS:-?}"
  echo "threads_per_core:    ${THREADS_PER_CORE:-?}"
  echo "logical_cores:       ${LOGICAL}"
  echo "ram_gb:              ${RAM_GB}"
  echo "gpu:                 not used (CPU-only study)"
  echo "os:                  ${OS}"
  echo "kernel:              ${KERNEL}"
  echo "compiler:            ${CXX_VER}"
  echo "compile_flags:       $(make -s -n -B all 2>/dev/null | grep -m1 'src/bfs_bench.cpp' | sed 's/ -o .*//' || true)"
  echo "OMP_PROC_BIND:       ${OMP_PROC_BIND:-unset}"
  echo "OMP_PLACES:          ${OMP_PLACES:-unset}"
  echo "OMP_SCHEDULE:        ${OMP_SCHEDULE:-unset (loop uses explicit schedule(dynamic,chunk))}"
  echo "OMP_NUM_THREADS:     ${OMP_NUM_THREADS:-unset (thread count set explicitly per run)}"
  echo "cpu_governor:        $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || echo unavailable)"
  echo "python:              $(python3 --version 2>&1)"
  echo "pandas/matplotlib:   $(python3 -c 'import pandas,matplotlib;print(pandas.__version__, matplotlib.__version__)' 2>&1)"
} > results/env.txt

cat results/env.txt
