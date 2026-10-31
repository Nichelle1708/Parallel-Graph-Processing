# Reproducibility checklist

Use this checklist before calling a new run final.

## Environment
- [ ] Record OS, kernel, architecture, CPU model, physical/logical cores, RAM, compiler version, compiler flags, and OpenMP settings.
- [ ] Record a source commit or archive checksum.
- [ ] Keep the exact command line and environment-variable overrides.

## Inputs and algorithm settings
- [ ] Record graph source/version, checksum, vertex and edge counts, and preprocessing.
- [ ] For BFS, record source vertices, thread counts, schedule/chunk settings, and repetitions.
- [ ] For PageRank, record directedness, damping factor, L1 tolerance, maximum iterations, dangling-node handling, schedules, and chunk size.

## Measurement
- [ ] Run correctness tests before performance measurements.
- [ ] Separate warm-up, timed, and profiled runs.
- [ ] Preserve raw CSVs; regenerate summaries and figures from those raw files.
- [ ] Report median and variability, and flag unstable configurations.
- [ ] Avoid merging pilot and final run sets without an explicit label.

## Submission
- [ ] Confirm report tables match the included CSV summaries.
- [ ] Confirm figure captions, units, and dataset names.
- [ ] Add the actual student names/IDs, batch, faculty, semester, and submission date.
- [ ] Test the archive from a fresh extraction.
- [ ] Verify dataset attribution and license terms.
