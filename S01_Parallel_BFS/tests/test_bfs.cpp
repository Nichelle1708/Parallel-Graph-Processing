// test_bfs.cpp -- correctness tests for graph.hpp and bfs.hpp. Run with `make test`.
#include <omp.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <random>
#include <string>
#include <vector>

#include "../src/bfs.hpp"
#include "../src/graph.hpp"

using namespace gp;

static int failures = 0;
#define CHECK(cond, msg)                                                          \
    do {                                                                          \
        if (!(cond)) {                                                            \
            std::fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, msg);    \
            ++failures;                                                           \
        }                                                                         \
    } while (0)

static std::vector<vid_t> run_seq(const Graph& g, vid_t src) {
    std::vector<vid_t> d(g.n, -1), q(g.n);
    bfs_sequential(g, src, d.data(), q.data());
    return d;
}

static std::vector<vid_t> run_par(const Graph& g, vid_t src, int T, int chunk, BfsWorkspace& ws, BfsProfile* p) {
    std::vector<vid_t> d(g.n, -1);
    bfs_parallel(g, src, d.data(), ws, T, chunk, p);
    return d;
}

static void compare_all(const Graph& g, const char* label, const std::vector<vid_t>& sources) {
    BfsWorkspace ws;
    ws.init(g.n, 8);
    for (vid_t s : sources) {
        const auto ref = run_seq(g, s);
        for (int T : {1, 2, 3, 4, 7, 8}) {
            for (int chunk : {1, 16, 64}) {
                const auto got = run_par(g, s, T, chunk, ws, nullptr);
                char msg[160];
                std::snprintf(msg, sizeof msg, "%s: parallel != sequential (src=%d T=%d chunk=%d)", label, s, T, chunk);
                CHECK(got == ref, msg);
            }
            BfsProfile pf;
            const auto got = run_par(g, s, T, 16, ws, &pf);
            CHECK(got == ref, "profiled run differs from sequential");
            vid_t maxd = 0;
            for (vid_t x : ref) maxd = std::max(maxd, x);
            CHECK(pf.levels == maxd + 1, "profile level count wrong");
            CHECK(pf.barrier_wait_avg >= 0.0 && pf.merge_time >= 0.0, "negative profile time");
        }
    }
}

static void test_build_csr() {
    // duplicates, a self-loop, both directions of one edge
    std::vector<Edge> e = {{0, 1}, {1, 0}, {0, 1}, {2, 2}, {1, 2}, {3, 0}};
    Graph g = build_csr(4, e, true);
    CHECK(g.n == 4, "n");
    CHECK(g.num_entries() == 6, "entries after symmetrise+dedupe (edges 0-1,1-2,0-3 => 6)");
    CHECK(g.degree(0) == 2 && g.degree(1) == 2 && g.degree(2) == 1 && g.degree(3) == 1, "degrees");
    for (vid_t u = 0; u < g.n; ++u)
        for (eid_t i = g.offsets[u] + 1; i < g.offsets[u + 1]; ++i)
            CHECK(g.adj[i - 1] < g.adj[i], "adjacency sorted & unique");

    Graph d = build_csr(4, e, false);  // directed: 0->1, 1->0, 1->2, 3->0 (dupes + loop removed)
    CHECK(d.num_entries() == 4, "directed entries");
    Graph t = transpose(d);
    CHECK(t.num_entries() == 4, "transpose entries");
    CHECK(t.degree(0) == 2 && t.degree(1) == 1 && t.degree(2) == 1 && t.degree(3) == 0, "transpose in-degrees");
    Graph tt = transpose(t);
    CHECK(tt.offsets == d.offsets && tt.adj == d.adj, "transpose(transpose(g)) == g");
}

static void test_components() {
    // path 0-1-2, path 3-4, isolated vertex 5
    std::vector<Edge> e = {{0, 1}, {1, 2}, {3, 4}};
    Graph g = build_csr(6, e, true);
    Components c = connected_components(g);
    CHECK(c.size.size() == 3, "three components");
    CHECK(c.size[c.largest] == 3, "largest component has 3 vertices");
}

static void test_loaders() {
    {
        std::ofstream f("/tmp/_gp_test_snap.txt");
        f << "# comment line\n# Nodes: 4 Edges: 3\n10\t20\n20\t30\n30\t10\n\n40 10\n";
    }
    Graph g = load_graph_file("/tmp/_gp_test_snap.txt", true);
    CHECK(g.n == 4, "snap loader compacts ids to 4 vertices");
    CHECK(g.num_entries() == 8, "snap loader: triangle + pendant => 4 undirected edges => 8 entries");
    {
        std::ofstream f("/tmp/_gp_test.mtx");
        f << "%%MatrixMarket matrix coordinate real symmetric\n% comment\n3 3 2\n2 1 1.5\n3 2 2.5\n";
    }
    Graph m = load_graph_file("/tmp/_gp_test.mtx", true);
    CHECK(m.n == 3 && m.num_entries() == 4, "mtx loader: path of 3 vertices => 4 entries");
    std::remove("/tmp/_gp_test_snap.txt");
    std::remove("/tmp/_gp_test.mtx");
}

static void test_known_distances() {
    // grid: distance from corner 0 equals x + y
    const vid_t W = 37, H = 23;
    Graph g = gen_grid(W, H);
    BfsWorkspace ws;
    ws.init(g.n, 8);
    const auto d = run_par(g, 0, 4, 8, ws, nullptr);
    bool ok = true;
    for (vid_t y = 0; y < H; ++y)
        for (vid_t x = 0; x < W; ++x) ok = ok && (d[y * W + x] == x + y);
    CHECK(ok, "grid distances == Manhattan distance");

    // path graph: eccentricity n-1
    std::vector<Edge> e;
    const vid_t n = 5000;
    for (vid_t i = 0; i + 1 < n; ++i) e.push_back({i, i + 1});
    Graph p = build_csr(n, e, true);
    const auto dp = run_par(p, 0, 4, 1, ws, nullptr);
    CHECK(dp[n - 1] == n - 1, "path graph last vertex at distance n-1");
}

int main() {
    omp_set_dynamic(0);
    test_build_csr();
    test_components();
    test_loaders();
    test_known_distances();

    compare_all(gen_grid(60, 40), "grid", {0, 1234, 2399});
    compare_all(gen_er(20000, 100000, 7), "er-connected-ish", {0, 5, 19999});
    compare_all(gen_er(20000, 9000, 7), "er-sparse-disconnected", {0, 100, 12345});  // many unreached vertices
    compare_all(gen_rmat(12, 16, 3), "rmat-12", {0, 17, 4000});

    if (failures == 0) std::printf("ALL TESTS PASSED\n");
    else std::printf("%d CHECK(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
