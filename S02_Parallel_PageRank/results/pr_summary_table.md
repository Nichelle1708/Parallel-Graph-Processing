| Dataset | Adj. entries | Iter. | Config | Median time (ms) | ms / iter | CV (%) | Speedup vs seq | Efficiency | MEPS |
|---|---:|---:|---|---:|---:|---:|---:|---:|---:|
| ca-GrQc | 28,968 | 90 | sequential | 5.719 | 0.064 | 12.8 | 1.00 (baseline) | - | 455.9 |
| ca-GrQc | 28,968 | 90 | OpenMP static 1T | 5.104 | 0.057 | 34.8 | 1.12 | 1.12 | 510.8 |
| ca-GrQc | 28,968 | 90 | OpenMP static 2T | 3.290 | 0.037 | 9.0 | 1.74 | 0.87 | 792.4 |
| ca-GrQc | 28,968 | 90 | OpenMP static 4T | 2.054 | 0.023 | 34.5 | 2.78 | 0.70 | 1269.6 |
| ca-GrQc | 28,968 | 90 | OpenMP dynamic:64 1T | 5.571 | 0.062 | 11.7 | 1.03 | 1.03 | 468.0 |
| ca-GrQc | 28,968 | 90 | OpenMP dynamic:64 2T | 3.220 | 0.036 | 9.9 | 1.78 | 0.89 | 809.6 |
| ca-GrQc | 28,968 | 90 | OpenMP dynamic:64 4T | 2.881 | 0.032 | 140.0 | 1.99 | 0.50 | 905.0 |
| email-Enron | 367,662 | 86 | sequential | 57.863 | 0.673 | 2.3 | 1.00 (baseline) | - | 546.4 |
| email-Enron | 367,662 | 86 | OpenMP static 1T | 57.302 | 0.666 | 1.8 | 1.01 | 1.01 | 551.8 |
| email-Enron | 367,662 | 86 | OpenMP static 2T | 43.270 | 0.503 | 5.4 | 1.34 | 0.67 | 730.7 |
| email-Enron | 367,662 | 86 | OpenMP static 4T | 35.707 | 0.415 | 2.2 | 1.62 | 0.41 | 885.5 |
| email-Enron | 367,662 | 86 | OpenMP dynamic:64 1T | 59.502 | 0.692 | 3.1 | 0.97 | 0.97 | 531.4 |
| email-Enron | 367,662 | 86 | OpenMP dynamic:64 2T | 33.924 | 0.394 | 1.7 | 1.71 | 0.85 | 932.1 |
| email-Enron | 367,662 | 86 | OpenMP dynamic:64 4T | 18.590 | 0.216 | 6.8 | 3.11 | 0.78 | 1700.9 |
| rmat-s16 | 1,819,896 | 25 | sequential | 72.778 | 2.911 | 12.4 | 1.00 (baseline) | - | 625.2 |
| rmat-s16 | 1,819,896 | 25 | OpenMP static 1T | 73.497 | 2.940 | 3.0 | 0.99 | 0.99 | 619.0 |
| rmat-s16 | 1,819,896 | 25 | OpenMP static 2T | 38.636 | 1.545 | 1.8 | 1.88 | 0.94 | 1177.6 |
| rmat-s16 | 1,819,896 | 25 | OpenMP static 4T | 19.937 | 0.797 | 2.3 | 3.65 | 0.91 | 2282.1 |
| rmat-s16 | 1,819,896 | 25 | OpenMP dynamic:64 1T | 76.594 | 3.064 | 2.2 | 0.95 | 0.95 | 594.0 |
| rmat-s16 | 1,819,896 | 25 | OpenMP dynamic:64 2T | 40.015 | 1.601 | 2.1 | 1.82 | 0.91 | 1137.0 |
| rmat-s16 | 1,819,896 | 25 | OpenMP dynamic:64 4T | 20.882 | 0.835 | 3.2 | 3.49 | 0.87 | 2178.8 |
| web-Google | 5,105,039 | 90 | sequential | 1502.129 | 16.690 | 7.8 | 1.00 (baseline) | - | 305.9 |
| web-Google | 5,105,039 | 90 | OpenMP static 1T | 1525.629 | 16.951 | 4.7 | 0.98 | 0.98 | 301.2 |
| web-Google | 5,105,039 | 90 | OpenMP static 2T | 845.771 | 9.397 | 16.1 | 1.78 | 0.89 | 543.2 |
| web-Google | 5,105,039 | 90 | OpenMP static 4T | 787.993 | 8.755 | 21.9 | 1.91 | 0.48 | 583.1 |
| web-Google | 5,105,039 | 90 | OpenMP dynamic:64 1T | 1615.141 | 17.946 | 7.0 | 0.93 | 0.93 | 284.5 |
| web-Google | 5,105,039 | 90 | OpenMP dynamic:64 2T | 948.904 | 10.543 | 13.7 | 1.58 | 0.79 | 484.2 |
| web-Google | 5,105,039 | 90 | OpenMP dynamic:64 4T | 903.606 | 10.040 | 15.0 | 1.66 | 0.42 | 508.5 |
| roadNet-CA | 5,533,214 | 82 | sequential | 1529.891 | 18.657 | 14.0 | 1.00 (baseline) | - | 296.6 |
| roadNet-CA | 5,533,214 | 82 | OpenMP static 1T | 1549.559 | 18.897 | 10.3 | 0.99 | 0.99 | 292.8 |
| roadNet-CA | 5,533,214 | 82 | OpenMP static 2T | 839.100 | 10.233 | 19.7 | 1.82 | 0.91 | 540.8 |
| roadNet-CA | 5,533,214 | 82 | OpenMP static 4T | 848.158 | 10.343 | 14.1 | 1.80 | 0.45 | 535.0 |
| roadNet-CA | 5,533,214 | 82 | OpenMP dynamic:64 1T | 1813.461 | 22.115 | 19.0 | 0.84 | 0.84 | 250.2 |
| roadNet-CA | 5,533,214 | 82 | OpenMP dynamic:64 2T | 1315.945 | 16.048 | 19.2 | 1.16 | 0.58 | 345.0 |
| roadNet-CA | 5,533,214 | 82 | OpenMP dynamic:64 4T | 1118.377 | 13.639 | 18.6 | 1.37 | 0.34 | 405.8 |
| rmat-s18 | 7,610,360 | 26 | sequential | 395.737 | 15.221 | 9.1 | 1.00 (baseline) | - | 501.5 |
| rmat-s18 | 7,610,360 | 26 | OpenMP static 1T | 351.439 | 13.517 | 1.2 | 1.13 | 1.13 | 563.0 |
| rmat-s18 | 7,610,360 | 26 | OpenMP static 2T | 181.619 | 6.985 | 1.5 | 2.18 | 1.09 | 1089.5 |
| rmat-s18 | 7,610,360 | 26 | OpenMP static 4T | 106.335 | 4.090 | 5.9 | 3.72 | 0.93 | 1860.9 |
| rmat-s18 | 7,610,360 | 26 | OpenMP dynamic:64 1T | 361.449 | 13.902 | 2.4 | 1.09 | 1.09 | 547.4 |
| rmat-s18 | 7,610,360 | 26 | OpenMP dynamic:64 2T | 191.412 | 7.362 | 1.2 | 2.07 | 1.03 | 1033.7 |
| rmat-s18 | 7,610,360 | 26 | OpenMP dynamic:64 4T | 131.973 | 5.076 | 18.7 | 3.00 | 0.75 | 1502.3 |
| rmat-s20 | 31,400,212 | 31 | sequential | 1894.667 | 61.118 | 25.2 | 1.00 (baseline) | - | 513.8 |
| rmat-s20 | 31,400,212 | 31 | OpenMP static 1T | 1913.120 | 61.714 | 11.2 | 0.99 | 0.99 | 508.8 |
| rmat-s20 | 31,400,212 | 31 | OpenMP static 2T | 981.687 | 31.667 | 13.0 | 1.93 | 0.97 | 991.7 |
| rmat-s20 | 31,400,212 | 31 | OpenMP static 4T | 938.272 | 30.267 | 14.3 | 2.02 | 0.50 | 1037.5 |
| rmat-s20 | 31,400,212 | 31 | OpenMP dynamic:64 1T | 1917.852 | 61.866 | 3.8 | 0.99 | 0.99 | 507.6 |
| rmat-s20 | 31,400,212 | 31 | OpenMP dynamic:64 2T | 1130.266 | 36.460 | 24.2 | 1.68 | 0.84 | 861.2 |
| rmat-s20 | 31,400,212 | 31 | OpenMP dynamic:64 4T | 978.942 | 31.579 | 15.8 | 1.94 | 0.48 | 994.3 |

`*` = more threads than logical cores (oversubscribed). Baseline = sequential pull PageRank, median over repetitions. MEPS = million CSR adjacency entries read per second.
