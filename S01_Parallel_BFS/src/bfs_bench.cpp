// bfs_bench.cpp -- CS4 / S01 benchmark driver: sequential vs OpenMP parallel BFS.
//
// For one graph it measures, for several sources, repetitions and thread counts:
//   * the sequential baseline (queue-based BFS)
//   * the OpenMP level-synchronous BFS (timed, un-instrumented runs)
//   * a few additional *profiled* OpenMP runs that record barrier-wait and
//     frontier-merge time (instrumented, therefore reported separately)
// Every parallel result is verified against the sequential distances.
//
// Output (appended): one CSV row per run (raw measurements only; Speedup and
// Efficiency are left as NA and are derived later by scripts/analyze.py).
//
// Usage example:
//   ./bfs_bench --graph data/ca-GrQc.txt --name ca-GrQc --threads 1,2,4
//               --reps 10 --sources 5 --out results/bfs_raw.csv
//   ./bfs_bench --graph rmat:18:16 --name rmat-s18 --threads 1,2,4
//
// Graph specs: <path to .txt/.mtx>, rmat:SCALE:EDGEFACTOR[:SEED], er:N:M[:SEED], grid:W:H
// All graphs are treated as UNDIRECTED (edges symmetrised, self-loops and
// duplicates removed).
#include <omp.h>
#include <sys/resource.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "bfs.hpp"
#include "graph.hpp"

using namespace gp;

struct Options {
    std::string graph;
    std::string name;
    std::vector<int> threads{1, 2, 4};
    int reps = 10;
    int sources = 5;
    int profile_reps = 3;
    int chunk = 64;
    uint64_t seed = 12345;
    std::string out = "results/bfs_raw.csv";
    std::string stats_out = "results/graph_stats.csv";
    std::string student_id = "S1";
    std::string batch_id = "CS4";
    std::string hardware = "unspecified";
};

static std::vector<std::string> split(const std::string& s, char d) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, d)) out.push_back(item);
    return out;
}

static void usage() {
    std::fprintf(stderr,
                 "usage: bfs_bench --graph <path|rmat:S:EF[:SEED]|er:N:M[:SEED]|grid:W:H> [--name NAME]\n"
                 "                 [--threads 1,2,4,8] [--reps 10] [--sources 5] [--profile-reps 3]\n"
                 "                 [--chunk 64] [--seed 12345] [--out FILE] [--stats-out FILE]\n"
                 "                 [--student-id S1] [--batch-id CS4] [--hardware \"...\"]\n");
}

static Options parse(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto need = [&](const char* flag) -> std::string {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "missing value for %s\n", flag);
                usage();
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "--graph") o.graph = need("--graph");
        else if (a == "--name") o.name = need("--name");
        else if (a == "--threads") {
            o.threads.clear();
            for (const auto& t : split(need("--threads"), ',')) o.threads.push_back(std::stoi(t));
        } else if (a == "--reps") o.reps = std::stoi(need("--reps"));
        else if (a == "--sources") o.sources = std::stoi(need("--sources"));
        else if (a == "--profile-reps") o.profile_reps = std::stoi(need("--profile-reps"));
        else if (a == "--chunk") o.chunk = std::stoi(need("--chunk"));
        else if (a == "--seed") o.seed = std::stoull(need("--seed"));
        else if (a == "--out") o.out = need("--out");
        else if (a == "--stats-out") o.stats_out = need("--stats-out");
        else if (a == "--student-id") o.student_id = need("--student-id");
        else if (a == "--batch-id") o.batch_id = need("--batch-id");
        else if (a == "--hardware") o.hardware = need("--hardware");
        else {
            std::fprintf(stderr, "unknown option %s\n", a.c_str());
            usage();
            std::exit(2);
        }
    }
    if (o.graph.empty()) {
        usage();
        std::exit(2);
    }
    if (o.name.empty()) {
        std::string base = o.graph;
        const auto slash = base.find_last_of('/');
        if (slash != std::string::npos) base = base.substr(slash + 1);
        const auto dot = base.find_last_of('.');
        if (dot != std::string::npos && dot > 0) base = base.substr(0, dot);
        std::string clean;
        for (char c : base) clean += (c == ':') ? '-' : c;
        o.name = clean;
    }
    return o;
}

static Graph make_graph(const std::string& spec) {
    if (spec.rfind("rmat:", 0) == 0) {
        auto p = split(spec, ':');
        if (p.size() < 3) throw std::runtime_error("rmat spec: rmat:SCALE:EDGEFACTOR[:SEED]");
        const uint64_t seed = p.size() > 3 ? std::stoull(p[3]) : 1;
        return gen_rmat(std::stoi(p[1]), std::stoi(p[2]), seed, true);
    }
    if (spec.rfind("er:", 0) == 0) {
        auto p = split(spec, ':');
        if (p.size() < 3) throw std::runtime_error("er spec: er:N:M[:SEED]");
        const uint64_t seed = p.size() > 3 ? std::stoull(p[3]) : 1;
        return gen_er(static_cast<vid_t>(std::stoll(p[1])), std::stoull(p[2]), seed, true);
    }
    if (spec.rfind("grid:", 0) == 0) {
        auto p = split(spec, ':');
        if (p.size() < 3) throw std::runtime_error("grid spec: grid:W:H");
        return gen_grid(std::stoi(p[1]), std::stoi(p[2]));
    }
    return load_graph_file(spec, true);
}

static std::string csv_field(const std::string& s) {
    if (s.find_first_of(",\"\n") == std::string::npos) return s;
    std::string r = "\"";
    for (char c : s) {
        if (c == '"') r += '"';
        r += c;
    }
    return r + "\"";
}

static bool file_empty(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "r");
    if (!f) return true;
    std::fseek(f, 0, SEEK_END);
    const long sz = std::ftell(f);
    std::fclose(f);
    return sz <= 0;
}

// current resident set size in MB (Linux /proc)
static double current_rss_mb() {
    FILE* f = std::fopen("/proc/self/statm", "r");
    if (!f) return -1.0;
    long size = 0, resident = 0;
    if (std::fscanf(f, "%ld %ld", &size, &resident) != 2) resident = -1;
    std::fclose(f);
    if (resident < 0) return -1.0;
    return static_cast<double>(resident) * static_cast<double>(sysconf(_SC_PAGESIZE)) / (1024.0 * 1024.0);
}

static double peak_rss_mb() {
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    return static_cast<double>(ru.ru_maxrss) / 1024.0;  // ru_maxrss is in KB on Linux
}

static const char* RAW_HEADER =
    "Student_ID,Batch_ID,Workload,Dataset,Problem_Size,Algorithm,Parallel_Model,Hardware,Resource_Count,"
    "Threads,Execution_Time,Speedup,Efficiency,Communication_Time,Synchronization_Time,Scheduling_Time,"
    "Memory_Use,Energy_if_available,Run_ID,"
    "Vertices,Source_Vertex,Rep,Levels,Reached,Edges_Scanned,Schedule,Profiled,Merge_Time,Peak_RSS_MB,"
    "Logical_Cores,Verified";

int main(int argc, char** argv) {
    const Options opt = parse(argc, argv);
    omp_set_dynamic(0);

    // ---- load / generate graph ------------------------------------------------
    const double tl0 = omp_get_wtime();
    Graph g;
    try {
        g = make_graph(opt.graph);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
    const double load_time = omp_get_wtime() - tl0;
    const eid_t entries = g.num_entries();
    std::fprintf(stderr, "[%s] vertices=%d adjacency_entries=%lld load_time=%.3fs\n", opt.name.c_str(), g.n,
                 static_cast<long long>(entries), load_time);

    // ---- structural statistics ------------------------------------------------
    const Components comp = connected_components(g);
    const vid_t lcc_id = comp.largest;
    const vid_t lcc_size = comp.size.empty() ? 0 : comp.size[lcc_id];
    const DegreeStats ds = degree_stats(g);

    std::vector<vid_t> lcc_vertices;
    lcc_vertices.reserve(lcc_size);
    for (vid_t v = 0; v < g.n; ++v)
        if (comp.label[v] == lcc_id) lcc_vertices.push_back(v);

    std::mt19937_64 rng(opt.seed);
    std::vector<vid_t> sources;
    std::sample(lcc_vertices.begin(), lcc_vertices.end(), std::back_inserter(sources),
                static_cast<size_t>(std::min<int64_t>(opt.sources, lcc_vertices.size())), rng);
    if (sources.empty()) {
        std::fprintf(stderr, "error: graph has no vertices\n");
        return 1;
    }

    // ---- check requested thread counts ----------------------------------------
    const int logical = omp_get_num_procs();
    int max_t = 1;
    for (int T : opt.threads) {
        int got = 0;
#pragma omp parallel num_threads(T)
        {
#pragma omp single
            got = omp_get_num_threads();
        }
        if (got != T) {
            std::fprintf(stderr, "error: requested %d threads but OpenMP provided %d (check OMP_THREAD_LIMIT)\n", T,
                         got);
            return 1;
        }
        if (T > logical)
            std::fprintf(stderr, "warning: %d threads > %d logical cores (oversubscribed; flagged via Logical_Cores)\n",
                         T, logical);
        max_t = std::max(max_t, T);
    }

    // ---- output files ---------------------------------------------------------
    const bool need_header = file_empty(opt.out);
    FILE* out = std::fopen(opt.out.c_str(), "a");
    if (!out) {
        std::fprintf(stderr, "cannot open %s\n", opt.out.c_str());
        return 1;
    }
    if (need_header) std::fprintf(out, "%s\n", RAW_HEADER);

    {
        const bool need_sh = file_empty(opt.stats_out);
        FILE* sf = std::fopen(opt.stats_out.c_str(), "a");
        if (sf) {
            if (need_sh)
                std::fprintf(sf,
                             "Dataset,Vertices,Adj_Entries,Edges_Undirected,Avg_Degree,Max_Degree,Degree_StdDev,"
                             "Components,LCC_Vertices,LCC_Fraction,CSR_MB,Load_Time_s,Graph_Spec\n");
            std::fprintf(sf, "%s,%d,%lld,%lld,%.6f,%lld,%.6f,%zu,%d,%.6f,%.3f,%.3f,%s\n", opt.name.c_str(), g.n,
                         static_cast<long long>(entries), static_cast<long long>(entries / 2), ds.avg,
                         static_cast<long long>(ds.max), ds.stddev, comp.size.size(), lcc_size,
                         static_cast<double>(lcc_size) / g.n, static_cast<double>(g.bytes()) / (1024.0 * 1024.0),
                         load_time, csv_field(opt.graph).c_str());
            std::fclose(sf);
        }
    }

    // ---- buffers --------------------------------------------------------------
    std::vector<vid_t> dist_ref(g.n), dist(g.n), queue(g.n);
    BfsWorkspace ws;
    ws.init(g.n, max_t);

    long run_counter = 0;
    const std::string hw = csv_field(opt.hardware);

    auto emit = [&](const char* algo, const char* model, int T, double time_s, vid_t src, int rep, int levels,
                    vid_t reached, long long scanned, const std::string& sched, int profiled, double sync_s,
                    double merge_s) {
        char sync_buf[64], merge_buf[64];
        if (profiled) {
            std::snprintf(sync_buf, sizeof sync_buf, "%.9f", sync_s);
            std::snprintf(merge_buf, sizeof merge_buf, "%.9f", merge_s);
        } else {
            std::snprintf(sync_buf, sizeof sync_buf, "NA");
            std::snprintf(merge_buf, sizeof merge_buf, "NA");
        }
        std::fprintf(out,
                     "%s,%s,BFS,%s,%lld,%s,%s,%s,%d,%d,%.9f,NA,NA,NA,%s,NA,%.3f,NA,%s-%06ld,"
                     "%d,%d,%d,%d,%d,%lld,%s,%d,%s,%.3f,%d,1\n",
                     opt.student_id.c_str(), opt.batch_id.c_str(), opt.name.c_str(), static_cast<long long>(entries),
                     algo, model, hw.c_str(), T, T, time_s, sync_buf, current_rss_mb(), opt.name.c_str(),
                     ++run_counter, g.n, src, rep, levels, reached, scanned, sched.c_str(), profiled, merge_buf,
                     peak_rss_mb(), logical);
        std::fflush(out);
    };

    const std::string sched = "dynamic:" + std::to_string(opt.chunk);  // no comma: keeps the CSV valid

    // ---- experiments ----------------------------------------------------------
    int src_idx = 0;
    for (vid_t src : sources) {
        ++src_idx;
        // reference run (untimed) -> expected distances and per-source constants
        std::fill(dist_ref.begin(), dist_ref.end(), -1);
        bfs_sequential(g, src, dist_ref.data(), queue.data());
        vid_t reached = 0;
        vid_t maxd = 0;
        long long scanned = 0;
        for (vid_t v = 0; v < g.n; ++v) {
            if (dist_ref[v] >= 0) {
                ++reached;
                maxd = std::max(maxd, dist_ref[v]);
                scanned += g.degree(v);
            }
        }
        const int levels = maxd + 1;
        if (reached != lcc_size) {
            std::fprintf(stderr, "internal error: BFS reached %d vertices but LCC has %d\n", reached, lcc_size);
            return 1;
        }

        auto verify = [&](const char* what, int T) {
            if (std::memcmp(dist.data(), dist_ref.data(), sizeof(vid_t) * g.n) != 0) {
                std::fprintf(stderr, "VERIFICATION FAILED: %s T=%d source=%d\n", what, T, src);
                std::exit(3);
            }
        };

        // warm-up (untimed): touches all memory and grows thread-local buffers
        std::fill(dist.begin(), dist.end(), -1);
        bfs_sequential(g, src, dist.data(), queue.data());
        for (int T : opt.threads) {
            std::fill(dist.begin(), dist.end(), -1);
            bfs_parallel(g, src, dist.data(), ws, T, opt.chunk, nullptr);
            verify("warm-up", T);
        }

        // timed repetitions; configurations are interleaved inside each repetition
        for (int rep = 1; rep <= opt.reps; ++rep) {
            std::fill(dist.begin(), dist.end(), -1);
            double t0 = omp_get_wtime();
            bfs_sequential(g, src, dist.data(), queue.data());
            double t1 = omp_get_wtime();
            verify("sequential", 1);
            emit("BFS-seq-queue", "Sequential", 1, t1 - t0, src, rep, levels, reached, scanned, "NA", 0, 0.0, 0.0);

            for (int T : opt.threads) {
                std::fill(dist.begin(), dist.end(), -1);
                t0 = omp_get_wtime();
                bfs_parallel(g, src, dist.data(), ws, T, opt.chunk, nullptr);
                t1 = omp_get_wtime();
                verify("parallel", T);
                emit("BFS-levelsync-topdown", "OpenMP", T, t1 - t0, src, rep, levels, reached, scanned, sched, 0, 0.0,
                     0.0);
            }
        }

        // profiled repetitions (instrumented; kept separate from timed rows)
        for (int rep = 1; rep <= opt.profile_reps; ++rep) {
            for (int T : opt.threads) {
                std::fill(dist.begin(), dist.end(), -1);
                BfsProfile pf;
                const double t0 = omp_get_wtime();
                bfs_parallel(g, src, dist.data(), ws, T, opt.chunk, &pf);
                const double t1 = omp_get_wtime();
                verify("parallel-profiled", T);
                if (pf.levels != levels) {
                    std::fprintf(stderr, "internal error: profiled levels %d != reference levels %d\n", pf.levels,
                                 levels);
                    return 1;
                }
                emit("BFS-levelsync-topdown", "OpenMP", T, t1 - t0, src, rep, levels, reached, scanned, sched, 1,
                     pf.barrier_wait_avg, pf.merge_time);
            }
        }
        std::fprintf(stderr, "[%s] source %d/%zu done (vertex %d, levels %d)\n", opt.name.c_str(), src_idx,
                     sources.size(), src, levels);
    }
    std::fclose(out);
    std::fprintf(stderr, "[%s] finished: %ld rows appended to %s\n", opt.name.c_str(), run_counter, opt.out.c_str());
    return 0;
}
