| Dataset | Adj. entries | Iter. | Config | Median time (ms) | ms / iter | CV (%) | Speedup vs seq | Efficiency | MEPS |
|---|---:|---:|---|---:|---:|---:|---:|---:|---:|
| ca-GrQc | 28,968 | 90 | sequential | 6.896 | 0.077 | 20.4 | 1.00 (baseline) | - | 378.1 |
| ca-GrQc | 28,968 | 90 | OpenMP static 1T | 7.678 | 0.085 | 24.4 | 0.90 | 0.90 | 339.6 |
| ca-GrQc | 28,968 | 90 | OpenMP static 2T | 3.183 | 0.035 | 3.6 | 2.17 | 1.08 | 819.2 |
| ca-GrQc | 28,968 | 90 | OpenMP static 4T | 2.216 | 0.025 | 53.1 | 3.11 | 0.78 | 1176.3 |
| ca-GrQc | 28,968 | 90 | OpenMP dynamic:64 1T | 6.062 | 0.067 | 11.8 | 1.14 | 1.14 | 430.1 |
| ca-GrQc | 28,968 | 90 | OpenMP dynamic:64 2T | 3.241 | 0.036 | 19.7 | 2.13 | 1.06 | 804.4 |
| ca-GrQc | 28,968 | 90 | OpenMP dynamic:64 4T | 3.825 | 0.042 | 117.6 | 1.80 | 0.45 | 681.6 |
| email-Enron | 367,662 | 86 | sequential | 56.523 | 0.657 | 1.6 | 1.00 (baseline) | - | 559.4 |
| email-Enron | 367,662 | 86 | OpenMP static 1T | 56.903 | 0.662 | 0.5 | 0.99 | 0.99 | 555.7 |
| email-Enron | 367,662 | 86 | OpenMP static 2T | 43.062 | 0.501 | 0.6 | 1.31 | 0.66 | 734.3 |
| email-Enron | 367,662 | 86 | OpenMP static 4T | 36.169 | 0.421 | 2.0 | 1.56 | 0.39 | 874.2 |
| email-Enron | 367,662 | 86 | OpenMP dynamic:64 1T | 58.926 | 0.685 | 2.0 | 0.96 | 0.96 | 536.6 |
| email-Enron | 367,662 | 86 | OpenMP dynamic:64 2T | 33.811 | 0.393 | 0.4 | 1.67 | 0.84 | 935.2 |
| email-Enron | 367,662 | 86 | OpenMP dynamic:64 4T | 18.518 | 0.215 | 2.4 | 3.05 | 0.76 | 1707.5 |
| rmat-s16 | 1,819,896 | 25 | sequential | 72.647 | 2.906 | 0.1 | 1.00 (baseline) | - | 626.3 |
| rmat-s16 | 1,819,896 | 25 | OpenMP static 1T | 73.505 | 2.940 | 0.2 | 0.99 | 0.99 | 619.0 |
| rmat-s16 | 1,819,896 | 25 | OpenMP static 2T | 38.450 | 1.538 | 0.9 | 1.89 | 0.94 | 1183.3 |
| rmat-s16 | 1,819,896 | 25 | OpenMP static 4T | 19.931 | 0.797 | 1.2 | 3.64 | 0.91 | 2282.7 |
| rmat-s16 | 1,819,896 | 25 | OpenMP dynamic:64 1T | 74.810 | 2.992 | 0.7 | 0.97 | 0.97 | 608.2 |
| rmat-s16 | 1,819,896 | 25 | OpenMP dynamic:64 2T | 39.639 | 1.586 | 0.3 | 1.83 | 0.92 | 1147.8 |
| rmat-s16 | 1,819,896 | 25 | OpenMP dynamic:64 4T | 20.787 | 0.831 | 0.2 | 3.49 | 0.87 | 2188.7 |
| web-Google | 5,105,039 | 90 | sequential | 1507.196 | 16.747 | 11.0 | 1.00 (baseline) | - | 304.8 |
| web-Google | 5,105,039 | 90 | OpenMP static 1T | 1558.339 | 17.315 | 4.1 | 0.97 | 0.97 | 294.8 |
| web-Google | 5,105,039 | 90 | OpenMP static 2T | 1189.462 | 13.216 | 18.9 | 1.27 | 0.63 | 386.3 |
| web-Google | 5,105,039 | 90 | OpenMP static 4T | 933.603 | 10.373 | 29.8 | 1.61 | 0.40 | 492.1 |
| web-Google | 5,105,039 | 90 | OpenMP dynamic:64 1T | 1600.554 | 17.784 | 13.1 | 0.94 | 0.94 | 287.1 |
| web-Google | 5,105,039 | 90 | OpenMP dynamic:64 2T | 948.904 | 10.543 | 22.2 | 1.59 | 0.79 | 484.2 |
| web-Google | 5,105,039 | 90 | OpenMP dynamic:64 4T | 992.111 | 11.023 | 21.5 | 1.52 | 0.38 | 463.1 |

`*` = more threads than logical cores (oversubscribed). Baseline = sequential pull PageRank, median over repetitions. MEPS = million CSR adjacency entries read per second.
