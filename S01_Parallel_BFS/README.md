# CS4 / S01 — Parallel Breadth-First Search (BCSE412L Parallel Computing)

Sequential vs OpenMP parallel BFS on graphs of increasing size and different structure.
This is the **baseline** part of the CS4 batch: Student 2 (PageRank) and Student 3 (graph
structure vs performance) reuse `src/graph.hpp`, the CSV schema and the dataset list.

## Contents

| Path | Purpose |
|---|---|
| `src/graph.hpp` | CSR graph, SNAP/MatrixMarket loader, R-MAT / Erdős–Rényi / grid generators, transpose, components, degree statistics (shared by the whole batch) |
| `src/bfs.hpp` | Sequential queue-based BFS (baseline) and OpenMP level-synchronous BFS |
| `src/bfs_bench.cpp` | Benchmark driver: sources, repetitions, thread counts, verification, CSV output |
| `tests/test_bfs.cpp` | Correctness tests (`make test`) |
| `scripts/download_data.sh` | Downloads the SNAP graphs |
| `scripts/run_experiments.sh` | Runs the full experiment set |
| `scripts/collect_env.sh` | Records hardware / compiler / runtime configuration to `results/env.txt` |
| `scripts/analyze.py` | Computes speedup / efficiency from raw data; writes tables and figures |

## Requirements

Linux (or WSL2 on Windows), `g++` ≥ 9 with OpenMP, `make`, `curl`, Python 3 with `pandas`, `numpy`, `matplotlib`.
Memory measurement reads `/proc`, so macOS/native Windows are not supported.

## Reproduce the main results

```bash
make test                      # must print ALL TESTS PASSED
scripts/download_data.sh       # ca-GrQc, email-Enron, roadNet-CA into data/
scripts/run_experiments.sh     # appends raw rows to results/bfs_raw.csv
python3 scripts/analyze.py     # tables + figures in results/
```

Defaults: threads = powers of two up to the logical core count, 5 source vertices per graph,
10 timed repetitions per source and configuration, 3 profiled repetitions, graphs =
3 SNAP graphs + R-MAT scales 16, 18, 20. Change them with environment variables, e.g.
`THREADS=1,2,4,8 REPS=15 scripts/run_experiments.sh` (see the header of the script).
Thread pinning is set to `OMP_PROC_BIND=close OMP_PLACES=cores`.

Use the thread counts **up to your physical core count**. Rows with more threads than logical
cores are flagged `Oversubscribed` in the outputs. Close other programs and keep the laptop
plugged in during runs.

## Method

* **Graphs** are treated as undirected: edges symmetrised, self-loops and duplicate edges removed,
  adjacency lists sorted. Vertex ids are compacted to `0..n-1`.
* **Baseline**: standard queue-based sequential BFS.
* **Parallel BFS**: level-synchronous top-down. Threads take dynamically scheduled chunks
  (`schedule(dynamic, 64)`) of the current frontier, claim unvisited neighbours with a relaxed
  atomic compare-and-swap on the distance array, collect them in thread-local buffers, and the
  buffers are concatenated into the next frontier after a barrier.
* **Sources**: sampled (fixed seed 12345) from the largest connected component, so every BFS
  reaches the same set of vertices. Same sources for every configuration of a graph.
* **Timing**: `omp_get_wtime()` around the traversal only. Distance-array reset, graph loading
  and CSR construction are excluded (load time is in `graph_stats.csv`). OpenMP region start-up
  is included in the parallel time. One untimed warm-up per source and configuration; configurations
  are interleaved inside each repetition.
* **Verification**: every parallel run (including warm-ups and profiled runs) is compared with the
  sequential distances; the program aborts on any mismatch. Column `Verified` is therefore always 1.
* **Profiled runs** (`Profiled = 1`) add timestamps to measure barrier wait and frontier-merge time.
  They are stored as separate rows and are **not** used for speedup/efficiency.

## Metric definitions (derived in `analyze.py` from raw data)

| Metric | Definition |
|---|---|
| Speedup | median sequential time of the same dataset / parallel time |
| Efficiency | Speedup / Threads |
| MTEPS | `Edges_Scanned` / time / 10⁶; `Edges_Scanned` = sum of degrees of reached vertices (adjacency entries scanned). On a symmetrised graph this counts each undirected edge twice, so it differs from the Graph500 TEPS convention by a factor ≈ 2 |
| Synchronization_Time | (profiled runs) per-thread idle time waiting at the end-of-level barrier, summed over levels, averaged over threads |
| Merge_Time | (profiled runs) wall time of the frontier-concatenation phase, summed over levels |
| Memory_Use | resident set size (MB) of the process after the run; `Peak_RSS_MB` = peak over the process lifetime (includes graph construction) |

**Not measured, written as `NA`:** `Communication_Time` (shared-memory study, no communication),
`Scheduling_Time` (OpenMP runtime scheduling cost is not separately observable here),
`Energy_if_available` (no energy counters used), and `Speedup`/`Efficiency` in `bfs_raw.csv`
(filled in `bfs_master_rows.csv`).

## CSV schema (one row per run)

The 19 columns required by the manual, in order: `Student_ID, Batch_ID, Workload, Dataset,
Problem_Size, Algorithm, Parallel_Model, Hardware, Resource_Count, Threads, Execution_Time,
Speedup, Efficiency, Communication_Time, Synchronization_Time, Scheduling_Time, Memory_Use,
Energy_if_available, Run_ID`, followed by extra columns: `Vertices, Source_Vertex, Rep, Levels,
Reached, Edges_Scanned, Schedule, Profiled, Merge_Time, Peak_RSS_MB, Logical_Cores, Verified`.

**Batch convention:** `Problem_Size` = number of CSR adjacency entries (stored directed edges after
preprocessing; = 2 × undirected edges for the symmetrised graphs). `Execution_Time` is in seconds.
`Student_ID` defaults to `S1` and `Batch_ID` to `CS4`; override with `STUDENT_ID` / `BATCH_ID`.

## Outputs

`results/bfs_raw.csv` (raw), `bfs_master_rows.csv` (with Speedup/Efficiency), `bfs_summary.csv`,
`bfs_overhead.csv`, `summary_table.md`, `graph_stats.csv`, `env.txt`, and `results/figures/`:
time vs size, speedup vs threads, efficiency vs threads, throughput vs threads, overhead breakdown,
speedup vs number of BFS levels (PNG + SVG; `analyze.py` is the editable plot source).

## Datasets (SNAP, facts from the SNAP dataset pages)

| Graph | Nodes | Edges | Type | Diameter |
|---|---:|---:|---|---:|
| ca-GrQc | 5,242 | 14,496 | undirected | 17 |
| email-Enron | 36,692 | 183,831 | undirected | 11 |
| roadNet-CA | 1,965,206 | 2,766,607 | undirected | 849 |
| web-Google (optional) | 875,713 | 5,105,039 | directed | 21 |

R-MAT graphs: Graph500 initiator probabilities (0.57/0.19/0.19/0.05), 16 edge samples per vertex,
random vertex relabelling, seed 1. This is an R-MAT-style generator, not the Graph500 reference code.

## Limitations to state in the report

* Only top-down BFS; no direction-optimising (bottom-up) variant. The static frontier chunking is not
  edge-balanced, so very high-degree vertices can cause imbalance.
* Results are from one machine; thread counts above the physical core count (or hyper-threads) are
  not equivalent to extra cores. Laptop frequency scaling / thermal throttling add noise.
* `Synchronization_Time` and `Merge_Time` come from instrumented runs, which perturb timing slightly.
* The `Memory_Use` column is process-level RSS, not per-data-structure memory.
