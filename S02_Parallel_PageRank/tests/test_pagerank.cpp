\
#include <cmath>
#include <cstdio>
#include <vector>
#include "../src/pagerank.hpp"

int main() {
    // Two vertices with one outgoing edge each (0 -> 1 and 1 -> 0).
    // Their PageRank vector is exactly uniform under the standard model.
    const int64_t offsets[] = {0, 1, 2};
    const int neighbours[] = {1, 0}; // CSR of the transposed graph; this graph is symmetric
    PRGraphT<int64_t, int> g{2, offsets, neighbours};
    const std::vector<uint32_t> outdeg{1, 1};
    PRParams p;
    p.damping = 0.85;
    p.tol = 1e-12;
    p.max_iter = 100;

    const auto seq = pagerank_seq(g, outdeg, p);
    const auto par_static = [&]() {
        omp_set_dynamic(0);
        omp_set_schedule(omp_sched_static, 1);
        return pagerank_omp(g, outdeg, p, 2, false);
    }();
    const auto par_dynamic = [&]() {
        omp_set_dynamic(0);
        omp_set_schedule(omp_sched_dynamic, 1);
        return pagerank_omp(g, outdeg, p, 2, false);
    }();

    bool ok = seq.converged && par_static.converged && par_dynamic.converged;
    for (double x : seq.rank) ok = ok && std::fabs(x - 0.5) < 1e-10;
    ok = ok && max_abs_diff(seq.rank, par_static.rank) < 1e-10;
    ok = ok && max_abs_diff(seq.rank, par_dynamic.rank) < 1e-10;
    ok = ok && std::fabs(seq.rank_sum - 1.0) < 1e-10;
    ok = ok && std::fabs(par_static.rank_sum - 1.0) < 1e-10;
    ok = ok && std::fabs(par_dynamic.rank_sum - 1.0) < 1e-10;

    if (!ok) {
        std::fprintf(stderr, "PageRank smoke test failed\n");
        return 1;
    }
    std::puts("PAGERANK TESTS PASSED");
    return 0;
}
