// pagerank_bench.cpp -- CS4 / S02 benchmark driver: sequential vs OpenMP parallel PageRank.
//
// Mirrors bfs_bench.cpp so the batch can be merged: same graph specs, flags, 19 required CSV
// columns, same extra columns (BFS-only ones are NA), Rep starts at 1, Run_ID = <name>-%06ld,
// schedule written as "static" / "dynamic:64". PageRank-specific columns are appended at the end.
//
//   ./bin/pagerank_bench --graph data/web-Google.txt --directed --name web-Google --threads 1,2,4
//   ./bin/pagerank_bench --graph rmat:18:16 --name rmat-s18 --threads 1,2,4 --schedules static,dynamic
//
// Graph specs: <path .txt/.mtx>, rmat:SCALE:EDGEFACTOR[:SEED], er:N:M[:SEED], grid:W:H
// Without --directed the graph is symmetrised (identical to the BFS runs). With --directed the
// edges are kept as given (use for web-Google); vertices with out-degree 0 are "dangling".
//
// Method: pull-style PageRank on the transposed graph, damping/tolerance(L1)/max-iter from options,
// dangling mass redistributed uniformly every iteration. Every parallel run (warm-up, timed,
// profiled) is verified against the sequential ranks; abort on mismatch.
#include <omp.h>
#include <sys/resource.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#include "graph.hpp"
#include "pagerank.hpp"

using namespace gp;

struct Options {
    std::string graph, name;
    std::vector<int> threads{1, 2, 4};
    std::vector<std::string> schedules{"static", "dynamic"};
    int reps = 10, profile_reps = 3, chunk = 64, max_iter = 100;
    double damping = 0.85, tol = 1e-8, verify_tol = 1e-9;
    bool directed = false;
    std::string out = "results/pr_raw.csv", stats_out = "results/pr_graph_stats.csv";
    std::string student_id = "S2", batch_id = "CS4", hardware = "unspecified";
};

static std::vector<std::string> split(const std::string& s, char d) {
    std::vector<std::string> out; std::stringstream ss(s); std::string item;
    while (std::getline(ss, item, d)) out.push_back(item);
    return out;
}
static void usage() {
    std::fprintf(stderr,
        "usage: pagerank_bench --graph <path|rmat:S:EF[:SEED]|er:N:M[:SEED]|grid:W:H> [--name NAME] [--directed]\n"
        "                      [--threads 1,2,4,8] [--schedules static,dynamic] [--chunk 64] [--reps 10]\n"
        "                      [--profile-reps 3] [--damping 0.85] [--tol 1e-8] [--max-iter 100]\n"
        "                      [--verify-tol 1e-9] [--out FILE] [--stats-out FILE]\n"
        "                      [--student-id S2] [--batch-id CS4] [--hardware \"...\"]\n");
}
static Options parse(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto need = [&](const char* flag) -> std::string {
            if (i + 1 >= argc) { std::fprintf(stderr, "missing value for %s\n", flag); usage(); std::exit(2); }
            return argv[++i];
        };
        if (a == "--graph") o.graph = need("--graph");
        else if (a == "--name") o.name = need("--name");
        else if (a == "--directed") o.directed = true;
        else if (a == "--threads") { o.threads.clear(); for (auto& t : split(need("--threads"), ',')) o.threads.push_back(std::stoi(t)); }
        else if (a == "--schedules") o.schedules = split(need("--schedules"), ',');
        else if (a == "--chunk") o.chunk = std::stoi(need("--chunk"));
        else if (a == "--reps") o.reps = std::stoi(need("--reps"));
        else if (a == "--profile-reps") o.profile_reps = std::stoi(need("--profile-reps"));
        else if (a == "--damping") o.damping = std::stod(need("--damping"));
        else if (a == "--tol") o.tol = std::stod(need("--tol"));
        else if (a == "--max-iter") o.max_iter = std::stoi(need("--max-iter"));
        else if (a == "--verify-tol") o.verify_tol = std::stod(need("--verify-tol"));
        else if (a == "--out") o.out = need("--out");
        else if (a == "--stats-out") o.stats_out = need("--stats-out");
        else if (a == "--student-id") o.student_id = need("--student-id");
        else if (a == "--batch-id") o.batch_id = need("--batch-id");
        else if (a == "--hardware") o.hardware = need("--hardware");
        else { std::fprintf(stderr, "unknown option %s\n", a.c_str()); usage(); std::exit(2); }
    }
    if (o.graph.empty()) { usage(); std::exit(2); }
    for (auto& s : o.schedules)
        if (s != "static" && s != "dynamic") { std::fprintf(stderr, "unknown schedule %s\n", s.c_str()); std::exit(2); }
    if (o.name.empty()) {
        std::string base = o.graph;
        const auto slash = base.find_last_of('/');
        if (slash != std::string::npos) base = base.substr(slash + 1);
        const auto dot = base.find_last_of('.');
        if (dot != std::string::npos && dot > 0) base = base.substr(0, dot);
        std::string clean; for (char c : base) clean += (c == ':') ? '-' : c;
        o.name = clean;
    }
    return o;
}

static Graph make_graph(const std::string& spec, bool symmetrize) {
    if (spec.rfind("rmat:", 0) == 0) {
        auto p = split(spec, ':');
        if (p.size() < 3) throw std::runtime_error("rmat spec: rmat:SCALE:EDGEFACTOR[:SEED]");
        return gen_rmat(std::stoi(p[1]), std::stoi(p[2]), p.size() > 3 ? std::stoull(p[3]) : 1, symmetrize);
    }
    if (spec.rfind("er:", 0) == 0) {
        auto p = split(spec, ':');
        if (p.size() < 3) throw std::runtime_error("er spec: er:N:M[:SEED]");
        return gen_er(static_cast<vid_t>(std::stoll(p[1])), std::stoull(p[2]), p.size() > 3 ? std::stoull(p[3]) : 1, symmetrize);
    }
    if (spec.rfind("grid:", 0) == 0) {
        auto p = split(spec, ':');
        if (p.size() < 3) throw std::runtime_error("grid spec: grid:W:H");
        return gen_grid(std::stoi(p[1]), std::stoi(p[2]));  // grids are always undirected
    }
    return load_graph_file(spec, symmetrize);
}

static std::string csv_field(const std::string& s) {
    if (s.find_first_of(",\"\n") == std::string::npos) return s;
    std::string r = "\"";
    for (char c : s) { if (c == '"') r += '"'; r += c; }
    return r + "\"";
}
static bool file_empty(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "r");
    if (!f) return true;
    std::fseek(f, 0, SEEK_END); const long sz = std::ftell(f); std::fclose(f);
    return sz <= 0;
}
static double current_rss_mb() {  // Linux /proc; -1 elsewhere
    FILE* f = std::fopen("/proc/self/statm", "r");
    if (!f) return -1.0;
    long size = 0, resident = 0;
    if (std::fscanf(f, "%ld %ld", &size, &resident) != 2) resident = -1;
    std::fclose(f);
    if (resident < 0) return -1.0;
    return (double)resident * (double)sysconf(_SC_PAGESIZE) / (1024.0 * 1024.0);
}
static double peak_rss_mb() {
    struct rusage ru; getrusage(RUSAGE_SELF, &ru);
#ifdef __APPLE__
    return (double)ru.ru_maxrss / (1024.0 * 1024.0);  // bytes on macOS
#else
    return (double)ru.ru_maxrss / 1024.0;             // KB on Linux
#endif
}
static double gini(std::vector<eid_t> d) {  // inequality of a degree distribution (0 = uniform)
    std::sort(d.begin(), d.end());
    double sum = 0, w = 0; const double n = (double)d.size();
    for (size_t i = 0; i < d.size(); ++i) { sum += (double)d[i]; w += (double)(i + 1) * (double)d[i]; }
    return sum > 0 ? 2.0 * w / (n * sum) - (n + 1.0) / n : 0.0;
}

static const char* RAW_HEADER =
    "Student_ID,Batch_ID,Workload,Dataset,Problem_Size,Algorithm,Parallel_Model,Hardware,Resource_Count,"
    "Threads,Execution_Time,Speedup,Efficiency,Communication_Time,Synchronization_Time,Scheduling_Time,"
    "Memory_Use,Energy_if_available,Run_ID,"
    "Vertices,Source_Vertex,Rep,Levels,Reached,Edges_Scanned,Schedule,Profiled,Merge_Time,Peak_RSS_MB,"
    "Logical_Cores,Verified,"
    "Iterations,Final_L1,Max_Abs_Diff,Directed,Damping,Tolerance,Max_Iter,Converged";

int main(int argc, char** argv) {
    const Options opt = parse(argc, argv);
    omp_set_dynamic(0);

    // ---- load / generate graph + transpose -------------------------------------
    const double tl0 = omp_get_wtime();
    Graph g;
    try { g = make_graph(opt.graph, !opt.directed); }
    catch (const std::exception& e) { std::fprintf(stderr, "error: %s\n", e.what()); return 1; }
    const double load_time = omp_get_wtime() - tl0;
    const double tt0 = omp_get_wtime();
    Graph gt = transpose(g);
    const double transpose_time = omp_get_wtime() - tt0;
    const eid_t entries = g.num_entries();
    const bool directed = opt.directed && opt.graph.rfind("grid:", 0) != 0;

    std::vector<uint32_t> outdeg(g.n);
    eid_t dangling = 0;
    for (vid_t v = 0; v < g.n; ++v) { outdeg[v] = (uint32_t)g.degree(v); dangling += (outdeg[v] == 0); }
    const DegreeStats ins = degree_stats(gt), outs = degree_stats(g);
    std::vector<eid_t> indeg(g.n);
    for (vid_t v = 0; v < g.n; ++v) indeg[v] = gt.degree(v);
    const double gini_in = gini(indeg);
    std::fprintf(stderr, "[%s] vertices=%d adjacency_entries=%lld directed=%d dangling=%lld max_in=%lld max_out=%lld "
                 "gini_in=%.3f load=%.3fs transpose=%.3fs\n", opt.name.c_str(), g.n, (long long)entries, (int)directed,
                 (long long)dangling, (long long)ins.max, (long long)outs.max, gini_in, load_time, transpose_time);

    // ---- thread checks ---------------------------------------------------------
    const int logical = omp_get_num_procs();
    for (int T : opt.threads) {
        int got = 0;
#pragma omp parallel num_threads(T)
        {
#pragma omp single
            got = omp_get_num_threads();
        }
        if (got != T) { std::fprintf(stderr, "error: requested %d threads but OpenMP provided %d\n", T, got); return 1; }
        if (T > logical)
            std::fprintf(stderr, "warning: %d threads > %d logical cores (oversubscribed; flagged via Logical_Cores)\n", T, logical);
    }

    // ---- outputs ---------------------------------------------------------------
    const bool need_header = file_empty(opt.out);
    FILE* out = std::fopen(opt.out.c_str(), "a");
    if (!out) { std::fprintf(stderr, "cannot open %s\n", opt.out.c_str()); return 1; }
    if (need_header) std::fprintf(out, "%s\n", RAW_HEADER);
    {
        const bool need_sh = file_empty(opt.stats_out);
        FILE* sf = std::fopen(opt.stats_out.c_str(), "a");
        if (sf) {
            if (need_sh)
                std::fprintf(sf, "Dataset,Vertices,Adj_Entries,Directed,Dangling,Avg_Degree,Max_In_Degree,Max_Out_Degree,"
                                 "InDeg_StdDev,OutDeg_StdDev,InDeg_Gini,CSR_MB,Load_Time_s,Transpose_Time_s,Graph_Spec\n");
            std::fprintf(sf, "%s,%d,%lld,%d,%lld,%.6f,%lld,%lld,%.6f,%.6f,%.6f,%.3f,%.3f,%.3f,%s\n", opt.name.c_str(), g.n,
                         (long long)entries, (int)directed, (long long)dangling, ins.avg, (long long)ins.max,
                         (long long)outs.max, ins.stddev, outs.stddev, gini_in,
                         (double)(g.bytes() + gt.bytes()) / (1024.0 * 1024.0), load_time, transpose_time,
                         csv_field(opt.graph).c_str());
            std::fclose(sf);
        }
    }

    PRParams prm; prm.damping = opt.damping; prm.tol = opt.tol; prm.max_iter = opt.max_iter;
    PRGraphT<eid_t, vid_t> in{g.n, gt.offsets.data(), gt.adj.data()};
    const std::string hw = csv_field(opt.hardware);
    long run_counter = 0;

    auto sched_label = [&](const std::string& s) { return s == "static" ? std::string("static") : "dynamic:" + std::to_string(opt.chunk); };
    auto set_sched = [&](const std::string& s) {
        if (s == "static") omp_set_schedule(omp_sched_static, 0);
        else omp_set_schedule(omp_sched_dynamic, opt.chunk);
    };

    auto emit = [&](const char* algo, const char* model, int T, const PRResult& r, const std::string& sched, int rep,
                    int profiled, double diff) {
        char sync_buf[64];
        if (profiled) std::snprintf(sync_buf, sizeof sync_buf, "%.9f", r.sync_time); else std::snprintf(sync_buf, sizeof sync_buf, "NA");
        std::fprintf(out,
                     "%s,%s,PageRank,%s,%lld,%s,%s,%s,%d,%d,%.9f,NA,NA,NA,%s,NA,%.3f,NA,%s-%06ld,"
                     "%d,NA,%d,NA,%d,%lld,%s,%d,NA,%.3f,%d,1,"
                     "%d,%.3e,%.3e,%d,%.3f,%.1e,%d,%d\n",
                     opt.student_id.c_str(), opt.batch_id.c_str(), opt.name.c_str(), (long long)entries, algo, model,
                     hw.c_str(), T, T, r.total_time, sync_buf, current_rss_mb(), opt.name.c_str(), ++run_counter,
                     g.n, rep, g.n, (long long)entries * r.iters, sched.c_str(), profiled, peak_rss_mb(), logical,
                     r.iters, r.final_l1, diff, (int)directed, prm.damping, prm.tol, prm.max_iter, r.converged ? 1 : 0);
        std::fflush(out);
    };

    // ---- reference (untimed) ----------------------------------------------------
    PRResult ref = pagerank_seq(in, outdeg, prm);
    std::fprintf(stderr, "[%s] reference: iterations=%d converged=%d final_l1=%.3e rank_sum=%.12f\n", opt.name.c_str(),
                 ref.iters, (int)ref.converged, ref.final_l1, ref.rank_sum);
    if (std::fabs(ref.rank_sum - 1.0) > 1e-9) { std::fprintf(stderr, "FATAL: ranks do not sum to 1\n"); return 3; }
    if (!ref.converged)
        std::fprintf(stderr, "warning: baseline did not converge within %d iterations (final L1 %.3e)\n", prm.max_iter, ref.final_l1);

    auto verify = [&](const char* what, int T, const std::string& s, const PRResult& r) {
        const double d = max_abs_diff(r.rank, ref.rank);
        if (d > opt.verify_tol || r.iters != ref.iters || std::fabs(r.rank_sum - 1.0) > 1e-9) {
            std::fprintf(stderr, "VERIFICATION FAILED: %s T=%d sched=%s max_abs_diff=%.3e iters=%d (ref %d) sum=%.12f\n",
                         what, T, s.c_str(), d, r.iters, ref.iters, r.rank_sum);
            std::exit(3);
        }
        return d;
    };

    // ---- warm-up (untimed) -------------------------------------------------------
    { PRResult w = pagerank_seq(in, outdeg, prm); verify("warm-up seq", 1, "NA", w); }
    for (int T : opt.threads) for (auto& s : opt.schedules) {
        set_sched(s);
        PRResult w = pagerank_omp(in, outdeg, prm, T, false);
        verify("warm-up", T, s, w);
    }

    // ---- timed repetitions: configurations interleaved inside each repetition ----
    for (int rep = 1; rep <= opt.reps; ++rep) {
        PRResult rs = pagerank_seq(in, outdeg, prm);
        emit("PR-seq-pull", "Sequential", 1, rs, "NA", rep, 0, verify("sequential", 1, "NA", rs));
        for (int T : opt.threads) for (auto& s : opt.schedules) {
            set_sched(s);
            PRResult r = pagerank_omp(in, outdeg, prm, T, false);
            emit("PR-omp-pull", "OpenMP", T, r, sched_label(s), rep, 0, verify("parallel", T, s, r));
        }
    }
    // ---- profiled repetitions (instrumented; separate rows, not used for speedup) -
    for (int rep = 1; rep <= opt.profile_reps; ++rep)
        for (int T : opt.threads) for (auto& s : opt.schedules) {
            set_sched(s);
            PRResult r = pagerank_omp(in, outdeg, prm, T, true);
            emit("PR-omp-pull", "OpenMP", T, r, sched_label(s), rep, 1, verify("parallel-profiled", T, s, r));
        }
    std::fclose(out);
    std::fprintf(stderr, "[%s] finished: %ld rows appended to %s\n", opt.name.c_str(), run_counter, opt.out.c_str());
    return 0;
}
