// pagerank.hpp -- S02 PageRank kernels (sequential pull baseline + OpenMP pull)
//
// Conventions (state these in the report):
//   * damping d = 0.85, convergence = L1 change between iterations < tol, iteration cap max_iter
//   * pull-style on the TRANSPOSED graph: rank'[v] = base + d * sum_{u->v} rank[u]/outdeg(u)
//   * dangling nodes (outdeg 0): their rank mass is summed each iteration and redistributed
//     uniformly:  base = (1-d)/n + d * dangling_mass / n
//   * ranks start at 1/n and always sum to 1 (checked by the bench via rank_sum)
//
// The kernels use a plain pointer view so they do not depend on graph.hpp's field names.
// Write one adapter in the bench, e.g.:
//   PRGraph in  {n, gt.<offsets>.data(), gt.<adjacency>.data()};   // transposed CSR (in-neighbours)
//   outdeg[u] = g.<offsets>[u+1] - g.<offsets>[u];                  // from the ORIGINAL graph
#pragma once
#include <omp.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

struct PRParams {
    double damping = 0.85;
    double tol = 1e-8;      // L1 change threshold
    int max_iter = 100;     // iteration cap
};

// CSR of IN-neighbours (transpose of the input graph).
template <class Off, class Idx>
struct PRGraphT {
    int64_t n;
    const Off* off;   // size n+1
    const Idx* adj;   // in-neighbour lists
};

struct PRResult {
    std::vector<double> rank;
    std::vector<double> iter_time;  // seconds per iteration
    double total_time = 0;          // seconds, whole iteration loop (parallel-region start-up included)
    int iters = 0;
    bool converged = false;
    double final_l1 = 0;
    double sync_time = 0;           // profiled runs: mean per-thread barrier idle, summed over iterations
    double rank_sum = 0;
};

namespace pr_detail {
struct alignas(64) Partial { double dang = 0, l1 = 0; double idle = 0; };
}

// ---------- sequential baseline ----------
template <class Off, class Idx>
PRResult pagerank_seq(const PRGraphT<Off, Idx>& g, const std::vector<uint32_t>& outdeg,
                      const PRParams& p) {
    const int64_t n = g.n;
    PRResult r;
    std::vector<double> cur(n, 1.0 / n), nxt(n), contrib(n);
    double t_all = omp_get_wtime();
    for (int it = 0; it < p.max_iter; ++it) {
        double t0 = omp_get_wtime();
        double dang = 0;
        for (int64_t u = 0; u < n; ++u) {
            if (outdeg[u] == 0) { dang += cur[u]; contrib[u] = 0; }
            else contrib[u] = cur[u] / outdeg[u];
        }
        const double base = (1.0 - p.damping) / n + p.damping * dang / n;
        double l1 = 0;
        for (int64_t v = 0; v < n; ++v) {
            double s = 0;
            for (Off e = g.off[v]; e < g.off[v + 1]; ++e) s += contrib[g.adj[e]];
            nxt[v] = base + p.damping * s;
            l1 += std::fabs(nxt[v] - cur[v]);
        }
        cur.swap(nxt);
        r.iter_time.push_back(omp_get_wtime() - t0);
        r.iters = it + 1; r.final_l1 = l1;
        if (l1 < p.tol) { r.converged = true; break; }
    }
    r.total_time = omp_get_wtime() - t_all;
    r.rank = std::move(cur);
    for (double x : r.rank) r.rank_sum += x;
    return r;
}

// ---------- OpenMP pull ----------
// Schedule is chosen by the caller via omp_set_schedule(omp_sched_static/dynamic, chunk) beforehand
// (loops use schedule(runtime)). One parallel region for the whole run; no atomics needed in pull.
// profile=true: per-thread idle time at the end-of-iteration barrier is recorded (same idea as bfs.hpp).
template <class Off, class Idx>
PRResult pagerank_omp(const PRGraphT<Off, Idx>& g, const std::vector<uint32_t>& outdeg,
                      const PRParams& p, int threads, bool profile) {
    const int64_t n = g.n;
    PRResult r;
    std::vector<double> bufA(n, 1.0 / n), bufB(n), contrib(n);
    double* cur = bufA.data();
    double* nxt = bufB.data();
    std::vector<pr_detail::Partial> part(threads);
    bool done = false;
    int iters = 0; double final_l1 = 0; bool conv = false;
    double iter_t0 = 0;

    double t_all = omp_get_wtime();
    #pragma omp parallel num_threads(threads)
    {
        const int tid = omp_get_thread_num();
        while (!done) {
            #pragma omp single
            { iter_t0 = omp_get_wtime(); }          // implicit barrier

            // phase A: contributions + dangling mass
            double dang = 0;
            #pragma omp for schedule(runtime)
            for (int64_t u = 0; u < n; ++u) {
                if (outdeg[u] == 0) { dang += cur[u]; contrib[u] = 0; }
                else contrib[u] = cur[u] / outdeg[u];
            }                                         // implicit barrier
            part[tid].dang = dang;
            #pragma omp barrier
            double dsum = 0;
            for (int t = 0; t < threads; ++t) dsum += part[t].dang;
            const double base = (1.0 - p.damping) / n + p.damping * dsum / n;

            // phase B: pull over in-neighbours
            double l1 = 0;
            #pragma omp for schedule(runtime) nowait
            for (int64_t v = 0; v < n; ++v) {
                double s = 0;
                for (Off e = g.off[v]; e < g.off[v + 1]; ++e) s += contrib[g.adj[e]];
                const double x = base + p.damping * s;
                nxt[v] = x;
                l1 += std::fabs(x - cur[v]);
            }
            part[tid].l1 = l1;
            double t_end = profile ? omp_get_wtime() : 0;
            #pragma omp barrier
            if (profile) part[tid].idle += omp_get_wtime() - t_end;

            #pragma omp single
            {
                double tot = 0;
                for (int t = 0; t < threads; ++t) tot += part[t].l1;
                std::swap(cur, nxt);
                ++iters; final_l1 = tot;
                r.iter_time.push_back(omp_get_wtime() - iter_t0);
                if (tot < p.tol) { conv = true; done = true; }
                else if (iters >= p.max_iter) done = true;
            }                                         // implicit barrier publishes `done`
        }
    }
    r.total_time = omp_get_wtime() - t_all;
    r.iters = iters; r.final_l1 = final_l1; r.converged = conv;
    if (profile) {
        double s = 0;
        for (auto& x : part) s += x.idle;
        r.sync_time = s / threads;
    }
    r.rank.assign(cur, cur + n);
    for (double x : r.rank) r.rank_sum += x;
    return r;
}

// Verification helper: max |a-b|. Use a tolerance (reductions reorder sums), and also compare
// iteration counts -- a different count near the threshold shows up as a diff of order `tol`.
inline double max_abs_diff(const std::vector<double>& a, const std::vector<double>& b) {
    double m = 0;
    for (size_t i = 0; i < a.size(); ++i) m = std::max(m, std::fabs(a[i] - b[i]));
    return m;
}
