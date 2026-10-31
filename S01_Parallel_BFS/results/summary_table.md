| Dataset | Adj. entries | Levels | Config | Median time (ms) | CV (%) | Speedup vs seq | Efficiency | MTEPS |
|---|---:|---:|---|---:|---:|---:|---:|---:|
| ca-GrQc | 28,968 | 12 | sequential | 0.083 | 21.1 | 1.00 (baseline) | - | 323.9 |
| ca-GrQc | 28,968 | 12 | OpenMP 1T | 0.100 | 9.6 | 0.83 | 0.83 | 269.5 |
| ca-GrQc | 28,968 | 12 | OpenMP 2T | 0.128 | 15.6 | 0.65 | 0.32 | 209.8 |
| ca-GrQc | 28,968 | 12 | OpenMP 4T | 0.121 | 52.2 | 0.68 | 0.17 | 221.4 |
| email-Enron | 367,662 | 9 | sequential | 0.607 | 30.7 | 1.00 (baseline) | - | 596.2 |
| email-Enron | 367,662 | 9 | OpenMP 1T | 0.690 | 33.8 | 0.88 | 0.88 | 524.4 |
| email-Enron | 367,662 | 9 | OpenMP 2T | 0.479 | 62.2 | 1.27 | 0.63 | 755.2 |
| email-Enron | 367,662 | 9 | OpenMP 4T | 0.328 | 104.0 | 1.85 | 0.46 | 1101.4 |
| rmat-s16 | 1,819,896 | 6 | sequential | 2.314 | 28.5 | 1.00 (baseline) | - | 786.6 |
| rmat-s16 | 1,819,896 | 6 | OpenMP 1T | 2.215 | 27.8 | 1.04 | 1.04 | 821.7 |
| rmat-s16 | 1,819,896 | 6 | OpenMP 2T | 1.462 | 30.6 | 1.58 | 0.79 | 1244.5 |
| rmat-s16 | 1,819,896 | 6 | OpenMP 4T | 0.964 | 34.5 | 2.40 | 0.60 | 1888.0 |
| roadNet-CA | 5,533,214 | 690 | sequential | 35.518 | 1.0 | 1.00 (baseline) | - | 155.4 |
| roadNet-CA | 5,533,214 | 690 | OpenMP 1T | 43.964 | 1.0 | 0.81 | 0.81 | 125.6 |
| roadNet-CA | 5,533,214 | 690 | OpenMP 2T | 25.327 | 0.8 | 1.40 | 0.70 | 218.0 |
| roadNet-CA | 5,533,214 | 690 | OpenMP 4T | 14.605 | 1.2 | 2.43 | 0.61 | 378.0 |
| rmat-s18 | 7,610,360 | 6 | sequential | 14.399 | 7.9 | 1.00 (baseline) | - | 528.5 |
| rmat-s18 | 7,610,360 | 6 | OpenMP 1T | 14.603 | 11.2 | 0.99 | 0.99 | 521.1 |
| rmat-s18 | 7,610,360 | 6 | OpenMP 2T | 8.300 | 12.0 | 1.73 | 0.87 | 916.9 |
| rmat-s18 | 7,610,360 | 6 | OpenMP 4T | 4.932 | 40.8 | 2.92 | 0.73 | 1543.1 |
| rmat-s20 | 31,400,212 | 7 | sequential | 84.276 | 5.5 | 1.00 (baseline) | - | 372.6 |
| rmat-s20 | 31,400,212 | 7 | OpenMP 1T | 88.019 | 6.5 | 0.96 | 0.96 | 356.7 |
| rmat-s20 | 31,400,212 | 7 | OpenMP 2T | 46.987 | 5.9 | 1.79 | 0.90 | 668.3 |
| rmat-s20 | 31,400,212 | 7 | OpenMP 4T | 27.095 | 9.5 | 3.11 | 0.78 | 1158.9 |

`*` = more threads than logical cores (oversubscribed). Baseline = sequential queue-based BFS, median over all sources and repetitions.
