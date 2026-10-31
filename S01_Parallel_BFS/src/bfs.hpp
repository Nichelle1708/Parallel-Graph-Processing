// bfs.hpp -- sequential baseline and OpenMP level-synchronous BFS.
//
// Sequential baseline : standard queue-based BFS (one FIFO array).
// Parallel version    : level-synchronous, top-down BFS.
//   * every thread scans a dynamically scheduled chunk of the current frontier
//   * an unvisited neighbour is claimed with a relaxed atomic compare-and-swap
//     on dist[v] (-1 -> level+1); the claiming thread appends it to its own
//     thread-local next-frontier buffer
//   * after an explicit barrier, thread-local buffers are concatenated into the
//     global next frontier (prefix sum over buffer sizes, then parallel copy)
//
// BFS distances do not depend on the order in which threads claim vertices, so
// the parallel result must equal the sequential result exactly; the benchmark
// verifies this on every run.
#pragma once

#include <omp.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "graph.hpp"

namespace gp {

// ---------------------------------------------------------------------------
// Sequential baseline
// ---------------------------------------------------------------------------
inline void bfs_sequential(const Graph& g, vid_t src, vid_t* dist, vid_t* queue) {
    const eid_t* off = g.offsets.data();
    const vid_t* adj = g.adj.data();
    eid_t head = 0, tail = 0;
    dist[src] = 0;
    queue[tail++] = src;
    while (head < tail) {
        const vid_t u = queue[head++];
        const vid_t du = dist[u] + 1;
        for (eid_t e = off[u]; e < off[u + 1]; ++e) {
            const vid_t v = adj[e];
            if (dist[v] == -1) {
                dist[v] = du;
                queue[tail++] = v;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Parallel BFS
// ---------------------------------------------------------------------------
struct alignas(64) PaddedVec {
    std::vector<vid_t> v;
};
struct alignas(64) PaddedDouble {
    double v = 0.0;
};

// Reusable buffers so that memory allocation is not part of the timed region.
struct BfsWorkspace {
    std::unique_ptr<vid_t[]> buf_a, buf_b;
    std::vector<PaddedVec> local;
    std::vector<PaddedDouble> arrive, wait;
    std::vector<eid_t> offs;
    int max_threads = 0;

    void init(vid_t n, int max_t) {
        buf_a.reset(new vid_t[static_cast<size_t>(n) + 1]);
        buf_b.reset(new vid_t[static_cast<size_t>(n) + 1]);
        max_threads = max_t;
        local.assign(max_t, PaddedVec{});
        arrive.assign(max_t, PaddedDouble{});
        wait.assign(max_t, PaddedDouble{});
        offs.assign(static_cast<size_t>(max_t) + 1, 0);
    }
};

// Filled only when profiling is requested (profiled runs are kept separate
// from the timed runs because the extra timestamps perturb the measurement).
struct BfsProfile {
    double barrier_wait_avg = 0.0;  // per-thread idle time at the end-of-level barrier, summed over levels, averaged over threads (s)
    double merge_time = 0.0;        // wall time of the frontier-concatenation phase, summed over levels (s)
    int levels = 0;                 // number of BFS levels processed
};

inline void bfs_parallel(const Graph& g, vid_t src, vid_t* dist, BfsWorkspace& ws, int T, int chunk,
                         BfsProfile* prof) {
    const eid_t* off = g.offsets.data();
    const vid_t* adj = g.adj.data();
    vid_t* curp = ws.buf_a.get();
    vid_t* nxtp = ws.buf_b.get();
    int64_t fsize = 1;
    int64_t total = 0;
    double merge_time = 0.0;
    int levels = 0;
    const bool profiling = (prof != nullptr);

    for (int t = 0; t < T; ++t) {
        ws.local[t].v.clear();
        ws.wait[t].v = 0.0;
    }
    dist[src] = 0;
    curp[0] = src;

#pragma omp parallel num_threads(T) shared(curp, nxtp, fsize, total, merge_time, levels)
    {
        const int tid = omp_get_thread_num();
        std::vector<vid_t>& local = ws.local[tid].v;
        vid_t depth = 0;
        for (;;) {
            local.clear();
            const vid_t nd = depth + 1;

#pragma omp for schedule(dynamic, chunk) nowait
            for (int64_t i = 0; i < fsize; ++i) {
                const vid_t u = curp[i];
                for (eid_t e = off[u]; e < off[u + 1]; ++e) {
                    const vid_t v = adj[e];
                    if (__atomic_load_n(&dist[v], __ATOMIC_RELAXED) == -1) {
                        vid_t expected = -1;
                        if (__atomic_compare_exchange_n(&dist[v], &expected, nd, false, __ATOMIC_RELAXED,
                                                        __ATOMIC_RELAXED))
                            local.push_back(v);
                    }
                }
            }
            if (profiling) ws.arrive[tid].v = omp_get_wtime();
#pragma omp barrier
            double t_merge0 = 0.0;
            if (profiling) {
                if (tid == 0) t_merge0 = omp_get_wtime();
                double mx = 0.0;
                for (int t = 0; t < T; ++t) mx = std::max(mx, ws.arrive[t].v);
                ws.wait[tid].v += mx - ws.arrive[tid].v;
            }
#pragma omp single
            {
                eid_t acc = 0;
                for (int t = 0; t < T; ++t) {
                    ws.offs[t] = acc;
                    acc += static_cast<eid_t>(ws.local[t].v.size());
                }
                total = acc;
            }  // implicit barrier
            std::copy(local.begin(), local.end(), nxtp + ws.offs[tid]);
#pragma omp barrier
#pragma omp single
            {
                std::swap(curp, nxtp);
                fsize = total;
                ++levels;
            }  // implicit barrier
            if (profiling && tid == 0) merge_time += omp_get_wtime() - t_merge0;
            ++depth;
            if (fsize == 0) break;
        }
    }

    if (profiling) {
        double s = 0.0;
        for (int t = 0; t < T; ++t) s += ws.wait[t].v;
        prof->barrier_wait_avg = s / T;
        prof->merge_time = merge_time;
        prof->levels = levels;
    }
}

}  // namespace gp
