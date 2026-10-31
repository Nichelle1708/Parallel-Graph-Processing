// graph.hpp -- shared graph infrastructure for the CS4 batch (BCSE412L).
//
// Header-only. Used by Student 1 (BFS), and intended to be reused unchanged by
// Student 2 (PageRank) and Student 3 (graph structure vs performance) so that
// every student works on the *same* graphs and the same CSR representation.
//
// Representation: CSR (compressed sparse row)
//   offsets[v] .. offsets[v+1]  -> range in adj[] holding the out-neighbours of v
//   vertex ids are 32-bit (0..n-1), offsets are 64-bit.
//
// Preprocessing applied by build_csr() (identical for all students):
//   * self-loops are removed
//   * duplicate edges are removed
//   * adjacency lists are sorted ascending
//   * if symmetrize == true, every edge (u,v) is stored as both u->v and v->u
//
// Input loaders:
//   * SNAP-style edge lists ("u v" per line, '#' comment lines)
//   * MatrixMarket coordinate files (.mtx, '%' comment lines, first non-comment
//     line is the size header and is skipped; weights, if present, are ignored)
//   Vertex ids in files are compacted to 0..n-1 (relative order preserved).
//
// Synthetic generators (all seeded, deterministic):
//   * R-MAT (Graph500 initiator probabilities, vertex labels randomly permuted)
//   * Erdos-Renyi G(n, m) (m edge samples drawn with replacement, then deduplicated)
//   * 2-D grid W x H (4-neighbour)
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace gp {

using vid_t = int32_t;  // vertex id
using eid_t = int64_t;  // edge / offset index

struct Edge {
    vid_t u, v;
};

struct Graph {
    vid_t n = 0;
    std::vector<eid_t> offsets;  // size n+1
    std::vector<vid_t> adj;      // size offsets[n]

    eid_t num_entries() const { return offsets.empty() ? 0 : offsets[n]; }
    eid_t degree(vid_t v) const { return offsets[v + 1] - offsets[v]; }
    size_t bytes() const { return offsets.size() * sizeof(eid_t) + adj.size() * sizeof(vid_t); }
};

// ---------------------------------------------------------------------------
// CSR construction
// ---------------------------------------------------------------------------
inline Graph build_csr(vid_t n, const std::vector<Edge>& edges, bool symmetrize) {
    Graph g;
    g.n = n;
    std::vector<eid_t> off(static_cast<size_t>(n) + 1, 0);

    for (const Edge& e : edges) {
        if (e.u == e.v) continue;  // drop self-loops
        off[e.u + 1]++;
        if (symmetrize) off[e.v + 1]++;
    }
    for (vid_t v = 0; v < n; ++v) off[v + 1] += off[v];

    std::vector<vid_t> adj(static_cast<size_t>(off[n]));
    std::vector<eid_t> cur(off.begin(), off.end() - 1);
    for (const Edge& e : edges) {
        if (e.u == e.v) continue;
        adj[cur[e.u]++] = e.v;
        if (symmetrize) adj[cur[e.v]++] = e.u;
    }

    // sort + deduplicate each adjacency list
    std::vector<eid_t> newdeg(n, 0);
#pragma omp parallel for schedule(dynamic, 1024)
    for (vid_t u = 0; u < n; ++u) {
        auto b = adj.begin() + off[u];
        auto e = adj.begin() + off[u + 1];
        std::sort(b, e);
        newdeg[u] = std::unique(b, e) - b;
    }

    std::vector<eid_t> noff(static_cast<size_t>(n) + 1, 0);
    for (vid_t u = 0; u < n; ++u) noff[u + 1] = noff[u] + newdeg[u];
    for (vid_t u = 0; u < n; ++u) {
        if (noff[u] != off[u]) {
            std::copy(adj.begin() + off[u], adj.begin() + off[u] + newdeg[u], adj.begin() + noff[u]);
        }
    }
    adj.resize(static_cast<size_t>(noff[n]));
    adj.shrink_to_fit();

    g.offsets = std::move(noff);
    g.adj = std::move(adj);
    return g;
}

// Reverse every edge (needed for "pull"-style PageRank). Adjacency lists of the
// result are sorted because sources are visited in ascending order.
inline Graph transpose(const Graph& g) {
    Graph t;
    t.n = g.n;
    std::vector<eid_t> off(static_cast<size_t>(g.n) + 1, 0);
    for (eid_t i = 0; i < g.num_entries(); ++i) off[g.adj[i] + 1]++;
    for (vid_t v = 0; v < g.n; ++v) off[v + 1] += off[v];
    std::vector<vid_t> adj(static_cast<size_t>(off[g.n]));
    std::vector<eid_t> cur(off.begin(), off.end() - 1);
    for (vid_t u = 0; u < g.n; ++u)
        for (eid_t e = g.offsets[u]; e < g.offsets[u + 1]; ++e) adj[cur[g.adj[e]]++] = u;
    t.offsets = std::move(off);
    t.adj = std::move(adj);
    return t;
}

// ---------------------------------------------------------------------------
// File loading
// ---------------------------------------------------------------------------
inline bool ends_with(const std::string& s, const std::string& suf) {
    return s.size() >= suf.size() && s.compare(s.size() - suf.size(), suf.size(), suf) == 0;
}

// Reads "u v" pairs. Compacts ids to 0..n-1. Returns n via `n_out`.
inline void read_edge_list(const std::string& path, std::vector<Edge>& edges, vid_t& n_out) {
    FILE* f = std::fopen(path.c_str(), "r");
    if (!f) throw std::runtime_error("cannot open graph file: " + path);
    const bool is_mtx = ends_with(path, ".mtx");
    bool size_line_skipped = !is_mtx;

    std::vector<std::pair<uint64_t, uint64_t>> raw;
    uint64_t max_id = 0;
    std::vector<char> buf(1 << 16);
    while (std::fgets(buf.data(), static_cast<int>(buf.size()), f)) {
        const char* p = buf.data();
        while (*p == ' ' || *p == '\t') ++p;
        if (*p == '\0' || *p == '\n' || *p == '\r') continue;
        if (*p == '#' || *p == '%') continue;
        if (!size_line_skipped) {  // MatrixMarket "rows cols nnz" header
            size_line_skipped = true;
            continue;
        }
        char* end = nullptr;
        uint64_t a = std::strtoull(p, &end, 10);
        if (end == p) continue;
        p = end;
        uint64_t b = std::strtoull(p, &end, 10);
        if (end == p) continue;
        raw.emplace_back(a, b);
        max_id = std::max(max_id, std::max(a, b));
    }
    std::fclose(f);

    if (max_id > 500000000ULL) throw std::runtime_error("vertex ids too large for dense remap: " + path);
    std::vector<int32_t> remap(static_cast<size_t>(max_id) + 1, -1);
    for (const auto& pr : raw) {
        remap[pr.first] = 0;
        remap[pr.second] = 0;
    }
    int32_t next = 0;
    for (auto& r : remap)
        if (r == 0) r = next++;

    edges.clear();
    edges.reserve(raw.size());
    for (const auto& pr : raw) edges.push_back({remap[pr.first], remap[pr.second]});
    n_out = next;
}

inline Graph load_graph_file(const std::string& path, bool symmetrize) {
    std::vector<Edge> edges;
    vid_t n = 0;
    read_edge_list(path, edges, n);
    return build_csr(n, edges, symmetrize);
}

// ---------------------------------------------------------------------------
// Generators
// ---------------------------------------------------------------------------
// R-MAT with Graph500 initiator probabilities (A=0.57, B=0.19, C=0.19, D=0.05),
// n = 2^scale vertices, edgefactor*n edge samples, random vertex relabelling.
// This is an R-MAT-style generator, not the Graph500 reference implementation.
inline Graph gen_rmat(int scale, int edgefactor, uint64_t seed, bool symmetrize = true) {
    const vid_t n = static_cast<vid_t>(1) << scale;
    const uint64_t m = static_cast<uint64_t>(edgefactor) * static_cast<uint64_t>(n);
    const double A = 0.57, B = 0.19, C = 0.19;
    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<double> U(0.0, 1.0);

    std::vector<vid_t> perm(n);
    std::iota(perm.begin(), perm.end(), 0);
    std::shuffle(perm.begin(), perm.end(), rng);

    std::vector<Edge> edges;
    edges.reserve(m);
    for (uint64_t i = 0; i < m; ++i) {
        vid_t u = 0, v = 0;
        for (int bit = 0; bit < scale; ++bit) {
            const double r = U(rng);
            if (r < A) {
            } else if (r < A + B) {
                v |= (1 << bit);
            } else if (r < A + B + C) {
                u |= (1 << bit);
            } else {
                u |= (1 << bit);
                v |= (1 << bit);
            }
        }
        edges.push_back({perm[u], perm[v]});
    }
    return build_csr(n, edges, symmetrize);
}

// Erdos-Renyi G(n, m): m uniformly random (u,v) pairs; duplicates and
// self-loops are removed afterwards, so the final edge count is slightly below m.
inline Graph gen_er(vid_t n, uint64_t m, uint64_t seed, bool symmetrize = true) {
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<vid_t> D(0, n - 1);
    std::vector<Edge> edges;
    edges.reserve(m);
    for (uint64_t i = 0; i < m; ++i) edges.push_back({D(rng), D(rng)});
    return build_csr(n, edges, symmetrize);
}

// 2-D grid with 4-neighbour connectivity; vertex id = y*W + x.
inline Graph gen_grid(vid_t W, vid_t H) {
    std::vector<Edge> edges;
    edges.reserve(static_cast<size_t>(W) * H * 2);
    for (vid_t y = 0; y < H; ++y)
        for (vid_t x = 0; x < W; ++x) {
            const vid_t id = y * W + x;
            if (x + 1 < W) edges.push_back({id, id + 1});
            if (y + 1 < H) edges.push_back({id, id + W});
        }
    return build_csr(W * H, edges, true);
}

// ---------------------------------------------------------------------------
// Structural statistics (shared by all students; Student 3 builds on these)
// ---------------------------------------------------------------------------
struct Components {
    std::vector<vid_t> label;   // component id per vertex
    std::vector<vid_t> size;    // size per component
    vid_t largest = 0;          // id of largest component
};

// Connected components of a graph whose adjacency is symmetric.
inline Components connected_components(const Graph& g) {
    Components c;
    c.label.assign(g.n, -1);
    std::vector<vid_t> queue(g.n);
    for (vid_t s = 0; s < g.n; ++s) {
        if (c.label[s] != -1) continue;
        const vid_t id = static_cast<vid_t>(c.size.size());
        vid_t head = 0, tail = 0;
        queue[tail++] = s;
        c.label[s] = id;
        while (head < tail) {
            const vid_t u = queue[head++];
            for (eid_t e = g.offsets[u]; e < g.offsets[u + 1]; ++e) {
                const vid_t v = g.adj[e];
                if (c.label[v] == -1) {
                    c.label[v] = id;
                    queue[tail++] = v;
                }
            }
        }
        c.size.push_back(tail);
    }
    if (!c.size.empty())
        c.largest = static_cast<vid_t>(std::max_element(c.size.begin(), c.size.end()) - c.size.begin());
    return c;
}

struct DegreeStats {
    double avg = 0.0, stddev = 0.0;
    eid_t max = 0;
};

inline DegreeStats degree_stats(const Graph& g) {
    DegreeStats s;
    if (g.n == 0) return s;
    s.avg = static_cast<double>(g.num_entries()) / g.n;
    double acc = 0.0;
    for (vid_t v = 0; v < g.n; ++v) {
        const double d = static_cast<double>(g.degree(v));
        acc += (d - s.avg) * (d - s.avg);
        s.max = std::max(s.max, g.degree(v));
    }
    s.stddev = std::sqrt(acc / g.n);
    return s;
}

}  // namespace gp
