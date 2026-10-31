# CS4 / S02 — Parallel PageRank
**Course:** BCSE412L — Parallel Computing

This case study compares a sequential pull-style PageRank implementation with an OpenMP implementation using static and dynamic loop scheduling. The benchmark driver verifies parallel ranks against the sequential baseline within a configured numerical tolerance.

## Contents

| Path | Purpose |
|---|---|
| `src/graph.hpp` | CSR graph representation, graph loaders and synthetic graph generators |
| `src/pagerank.hpp` | Sequential and OpenMP pull-style PageRank kernels |
| `src/pagerank_bench.cpp` | Benchmark driver, verification, timing, CSV output and graph statistics |
| `tests/test_pagerank.cpp` | Small deterministic correctness checks for PageRank |
| `scripts/download_data.sh` | Downloads the supported SNAP input graphs |
| `scripts/run_pagerank.sh` | Runs PageRank experiments over datasets, thread counts and schedules |
| `scripts/analyze_pagerank.py` | Produces summary tables and figures from recorded CSVs |
| `results/` | Archived raw/processed results, environment details and figures |

## Requirements

Linux or WSL2 is recommended. You need `g++` with C++17 and OpenMP support, GNU Make, `curl`, and Python 3 with `pandas`, `numpy`, and `matplotlib` for analysis. The environment script uses Linux utilities, so native macOS support is not guaranteed.

## Correctness tests

```bash
make clean
make test
```

The test uses a small graph with a known uniform PageRank solution and compares the sequential and OpenMP results. It is a smoke test, not a substitute for verifying all large datasets and parameter combinations.

## Reproduce the experiments

```bash
scripts/download_data.sh
scripts/run_pagerank.sh
python3 scripts/analyze_pagerank.py
```

The runner supports these environment variables (see the script header for the full list):

```bash
THREADS=1,2,4 REPS=10 PROFILE_REPS=3 SCHEDULES=static,dynamic \
  scripts/run_pagerank.sh
```

The script may append to existing result files. Move or back up `results/pr_raw.csv` and related outputs before a fresh run if you want the output to contain only new measurements. It skips missing dataset files with warnings; review the log and `pr_graph_stats.csv` to confirm the workload set.

## Method

- **Algorithm:** pull-style PageRank over the transposed CSR graph, with damping factor 0.85 by default.
- **Dangling nodes:** their rank mass is redistributed uniformly across all vertices each iteration.
- **Convergence:** L1 change below `1e-8`, with a maximum of 100 iterations by default.
- **Parallelism:** OpenMP parallel loops; the benchmark compares `static` and `dynamic` scheduling, using chunk size 64 for the dynamic schedule by default.
- **Verification:** parallel output is compared with the sequential rank vector using a numerical tolerance. Rank sum and convergence metadata are recorded by the benchmark.
- **Timing:** total iterative runtime includes OpenMP parallel-region overhead. Graph loading and transpose construction are outside the timed iterative loop.

## Interpretation notes

PageRank total runtime depends on both work per iteration and the number of iterations required to converge. Report iteration counts alongside elapsed time. The graph suite includes undirected/symmetrised graphs and the directed `web-Google` graph, so results reflect multiple differences in structure and preprocessing—not only graph size or sparsity.

The archived outputs are historical measurements from the recorded environment. Re-run on the target machine before making claims about reproducibility or general performance.
