# Parallel Graph Processing: BFS and PageRank
**Course:** BCSE412L — Parallel Computing  
**Case studies:** CS4–S01 (Parallel Breadth-First Search) and CS4–S02 (Parallel PageRank)

This repository-style package brings together both case-study implementations, their experiment scripts, recorded CSV outputs, plots, environment notes, and the editable report. It is intended to make the work easier to inspect and reproduce—not to imply that every benchmark has already been rerun on a clean machine.

## Package map

- `S01_Parallel_BFS/` — sequential and OpenMP BFS, test suite, benchmark driver, analysis scripts, recorded results and figures.
- `S02_Parallel_PageRank/` — sequential and OpenMP PageRank, test suite, benchmark driver, schedule comparisons, recorded results and figures.
- `report/` — editable combined case-study report.
- `data/README.md` — dataset download and attribution notes. Large input datasets are not bundled.
- `docs/` — commands and reproducibility checklist.

## Requirements

- Linux or WSL2 recommended; the environment collection scripts use Linux utilities such as `nproc` and `/proc`.
- A C++17 compiler with OpenMP support (for example, GCC/g++) and GNU Make.
- Python 3 with `pandas`, `numpy`, and `matplotlib` for analysis/plotting.
- `curl` and network access if downloading datasets.

## Quick start: correctness tests

Run from the extracted project directory:

```bash
cd S01_Parallel_BFS
make clean && make test

cd ../S02_Parallel_PageRank
make clean && make test
```

The test commands are intended as the first check before timing. A successful compile or test run does not guarantee that long benchmark runs will reproduce the archived timing values.

## Re-run experiments

### S01 — BFS

```bash
cd S01_Parallel_BFS
scripts/download_data.sh
scripts/run_experiments.sh
python3 scripts/analyze.py
```

### S02 — PageRank

```bash
cd S02_Parallel_PageRank
scripts/download_data.sh
scripts/run_pagerank.sh
python3 scripts/analyze_pagerank.py
```

Review each script's header and README before launching a full run. The scripts may append rows to existing CSVs; back up or move the archived outputs first if you want a clean new experiment set. Some datasets are large and a full run can take considerable time and disk space. The PageRank runner skips missing input files with a warning, so check the log and graph statistics to confirm which workloads actually ran.

## Understanding the recorded results

The `results/` folders contain historical experiment outputs. Reported timing is machine- and load-dependent; treat the archived values as observations from the recorded environment, not universal performance claims. The environment notes report an ARM64 Linux system with four logical CPUs and approximately 3.8 GB RAM. The specific CPU model is not identified, which limits hardware-level generalization.

For the final submission, verify that the report tables match the included summary CSVs and that the dataset versions, commands, compiler flags, and run identifiers are documented. Do not combine pilot and final runs without clearly labelling them.

## Known scope and limitations

- The two workloads perform different operations; compare scaling patterns rather than raw BFS-versus-PageRank elapsed times.
- The PageRank datasets vary in graph family and structure. They do not constitute a controlled experiment that varies only edge density.
- Some recorded configurations have high timing variability. The report flags these rather than treating small differences as definitive.
- Energy use and OpenMP runtime scheduling cost were not directly measured.
- Dataset downloads and licenses/attribution should be checked before redistribution.

## Packaging

To recreate a source archive from the parent directory:

```bash
zip -r CS4_Parallel_Graph_Processing.zip CS4_Parallel_Graph_Processing \
  -x "*/.DS_Store" "*/__pycache__/*" "*/bin/*"
unzip -t CS4_Parallel_Graph_Processing.zip
```

Keep raw CSVs and the analysis scripts together. Avoid including machine-specific binaries unless the instructor explicitly requests them.
