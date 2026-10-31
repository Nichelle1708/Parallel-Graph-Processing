#!/usr/bin/env python3
"""Derive speedup/efficiency from the raw BFS measurements and produce the summary
table and figures for the CS4 / S01 report.

Inputs : results/bfs_raw.csv, results/graph_stats.csv   (written by bfs_bench)
Outputs: results/bfs_master_rows.csv  one row per run, required schema, Speedup/Efficiency filled in
         results/bfs_summary.csv      per dataset x thread-count statistics
         results/bfs_overhead.csv     barrier-wait / merge breakdown from the profiled runs
         results/summary_table.md     the summary table in Markdown
         results/figures/*.png|svg    plots (this script is the editable source of the plots)

Definitions (all derived here from raw data, never typed in by hand):
  baseline        = median execution time of the sequential queue-based BFS for the same dataset
                    (and, for per-row values, the same source vertex)
  Speedup         = baseline / parallel execution time
  Efficiency      = Speedup / Threads
  MTEPS           = Edges_Scanned / Execution_Time / 1e6   (adjacency entries scanned per second;
                    on a symmetrised graph this counts every undirected edge twice)
  Barrier idle    = Synchronization_Time / Execution_Time of the profiled runs
  Merge fraction  = Merge_Time / Execution_Time of the profiled runs
Only non-profiled runs are used for speedup/efficiency/throughput; profiled runs are
instrumented and are used only for the overhead breakdown.
"""
import argparse
import os
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

# Okabe-Ito colour-blind-safe palette, used in a fixed order (never cycled).
PALETTE = ["#0072B2", "#E69F00", "#009E73", "#D55E00", "#CC79A7", "#56B4E9", "#F0E442", "#000000"]
MARKERS = ["o", "s", "^", "D", "v", "P", "X", "*"]

INK = "#222222"
GRID = "#DDDDDD"


def style():
    plt.rcParams.update({
        "figure.dpi": 120, "savefig.dpi": 200, "font.size": 10, "axes.edgecolor": INK,
        "axes.labelcolor": INK, "xtick.color": INK, "ytick.color": INK, "text.color": INK,
        "axes.spines.top": False, "axes.spines.right": False, "axes.grid": True,
        "grid.color": GRID, "grid.linewidth": 0.6, "axes.axisbelow": True,
        "lines.linewidth": 1.8, "lines.markersize": 6, "legend.frameon": False,
    })


def save(fig, outdir, name):
    fig.tight_layout()
    for ext in ("png", "svg"):
        fig.savefig(os.path.join(outdir, f"{name}.{ext}"))
    plt.close(fig)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--raw", default="results/bfs_raw.csv")
    ap.add_argument("--stats", default="results/graph_stats.csv")
    ap.add_argument("--outdir", default="results")
    a = ap.parse_args()
    figdir = os.path.join(a.outdir, "figures")
    os.makedirs(figdir, exist_ok=True)
    style()

    raw = pd.read_csv(a.raw, na_values=["NA"])
    stats = pd.read_csv(a.stats) if os.path.exists(a.stats) else pd.DataFrame()
    if raw.empty:
        sys.exit("no rows in " + a.raw)
    if not (raw["Verified"] == 1).all():
        sys.exit("raw data contains unverified runs")

    # ---------------------------------------------------------------- derived metrics
    seq = raw[(raw.Parallel_Model == "Sequential") & (raw.Profiled == 0)]
    base_src = seq.groupby(["Dataset", "Source_Vertex"]).Execution_Time.median().rename("Baseline_Time")
    base_ds = seq.groupby("Dataset").Execution_Time.median().rename("Baseline_Time_Dataset")

    m = raw.merge(base_src, on=["Dataset", "Source_Vertex"], how="left")
    timed = m.Profiled == 0
    m["Speedup"] = np.where(timed, m.Baseline_Time / m.Execution_Time, np.nan)
    m["Efficiency"] = m.Speedup / m.Threads
    m["MTEPS"] = np.where(timed, m.Edges_Scanned / m.Execution_Time / 1e6, np.nan)
    m["Oversubscribed"] = m.Threads > m.Logical_Cores
    master_cols = list(raw.columns)
    master = m[master_cols + ["Baseline_Time", "MTEPS", "Oversubscribed"]]
    master.to_csv(os.path.join(a.outdir, "bfs_master_rows.csv"), index=False, na_rep="NA")

    # ---------------------------------------------------------------- summary table
    t = m[timed].copy()
    rows = []
    for (ds, algo, thr), grp in t.groupby(["Dataset", "Algorithm", "Threads"]):
        x = grp.Execution_Time
        med = x.median()
        rows.append({
            "Dataset": ds, "Algorithm": algo, "Threads": thr,
            "Problem_Size": int(grp.Problem_Size.iloc[0]), "Vertices": int(grp.Vertices.iloc[0]),
            "Levels_median": grp.Levels.median(), "Runs": len(grp),
            "Time_mean_s": x.mean(), "Time_median_s": med, "Time_std_s": x.std(ddof=1) if len(x) > 1 else np.nan,
            "Time_CV_pct": 100 * x.std(ddof=1) / x.mean() if len(x) > 1 else np.nan,
            "Time_min_s": x.min(), "Time_max_s": x.max(),
            "Speedup_vs_seq": base_ds[ds] / med, "Efficiency": base_ds[ds] / med / thr,
            "MTEPS_median": grp.MTEPS.median(), "Oversubscribed": bool(grp.Oversubscribed.iloc[0]),
        })
    summ = pd.DataFrame(rows)
    # self-relative speedup of the parallel code (its own 1-thread run as reference)
    p1 = summ[(summ.Algorithm != "BFS-seq-queue") & (summ.Threads == 1)].set_index("Dataset").Time_median_s
    summ["Speedup_vs_parallel_1T"] = [
        (p1.get(r.Dataset, np.nan) / r.Time_median_s) if r.Algorithm != "BFS-seq-queue" else np.nan
        for r in summ.itertuples()
    ]
    order = summ.groupby("Dataset").Problem_Size.first().sort_values().index.tolist()
    summ["Dataset"] = pd.Categorical(summ.Dataset, order, ordered=True)
    summ["_is_par"] = (summ.Algorithm != "BFS-seq-queue").astype(int)  # sequential baseline row first
    summ = summ.sort_values(["Dataset", "_is_par", "Threads"]).drop(columns="_is_par").reset_index(drop=True)
    summ.to_csv(os.path.join(a.outdir, "bfs_summary.csv"), index=False)

    # markdown table (parallel rows + sequential baseline)
    md = ["| Dataset | Adj. entries | Levels | Config | Median time (ms) | CV (%) | Speedup vs seq | Efficiency | MTEPS |",
          "|---|---:|---:|---|---:|---:|---:|---:|---:|"]
    for r in summ.itertuples():
        cfg = "sequential" if r.Algorithm == "BFS-seq-queue" else f"OpenMP {r.Threads}T" + ("*" if r.Oversubscribed else "")
        sp = "1.00 (baseline)" if r.Algorithm == "BFS-seq-queue" else f"{r.Speedup_vs_seq:.2f}"
        ef = "-" if r.Algorithm == "BFS-seq-queue" else f"{r.Efficiency:.2f}"
        md.append(f"| {r.Dataset} | {r.Problem_Size:,} | {r.Levels_median:.0f} | {cfg} | {r.Time_median_s * 1e3:.3f} | "
                  f"{r.Time_CV_pct:.1f} | {sp} | {ef} | {r.MTEPS_median:.1f} |")
    md.append("")
    md.append("`*` = more threads than logical cores (oversubscribed). Baseline = sequential queue-based BFS, "
              "median over all sources and repetitions.")
    with open(os.path.join(a.outdir, "summary_table.md"), "w") as f:
        f.write("\n".join(md) + "\n")

    # ---------------------------------------------------------------- overhead breakdown (profiled runs)
    prof = m[(m.Profiled == 1)].copy()
    ov = pd.DataFrame()
    if not prof.empty:
        prof["Barrier_idle_frac"] = prof.Synchronization_Time / prof.Execution_Time
        prof["Merge_frac"] = prof.Merge_Time / prof.Execution_Time
        ov = prof.groupby(["Dataset", "Threads"]).agg(
            Runs=("Run_ID", "count"), Exec_profiled_s=("Execution_Time", "median"),
            Barrier_wait_avg_s=("Synchronization_Time", "median"), Merge_s=("Merge_Time", "median"),
            Barrier_idle_frac=("Barrier_idle_frac", "median"), Merge_frac=("Merge_frac", "median"),
            Levels=("Levels", "median"),
        ).reset_index()
        ov.to_csv(os.path.join(a.outdir, "bfs_overhead.csv"), index=False)

    # ---------------------------------------------------------------- figures
    par = summ[summ.Algorithm != "BFS-seq-queue"]
    sq = summ[summ.Algorithm == "BFS-seq-queue"]
    ds_colors = {d: PALETTE[i % len(PALETTE)] for i, d in enumerate(order)}
    ds_markers = {d: MARKERS[i % len(MARKERS)] for i, d in enumerate(order)}
    thr_list = sorted(par.Threads.unique())

    # Fig 1: execution time vs problem size
    fig, ax = plt.subplots(figsize=(6.6, 4.4))
    cfgs = [("sequential", sq.set_index("Dataset"))] + [(f"{T} threads", par[par.Threads == T].set_index("Dataset")) for T in thr_list]
    for i, (lab, d) in enumerate(cfgs):
        d = d.reindex([o for o in order if o in d.index])
        ax.plot(d.Problem_Size, d.Time_median_s, marker=MARKERS[i % 8], color=PALETTE[i % 8], label=lab)
    ax.set_xscale("log"); ax.set_yscale("log")
    ax.set_xlabel("Problem size (CSR adjacency entries)"); ax.set_ylabel("Median BFS time (s)")
    ax.set_title("Execution time vs graph size")
    ax.legend(title="Configuration")
    save(fig, figdir, "fig1_time_vs_size")

    # Fig 2 & 3: speedup / efficiency vs threads
    for name, col, ylabel, title in [("fig2_speedup_vs_threads", "Speedup_vs_seq", "Speedup over sequential BFS", "Speedup vs threads"),
                                      ("fig3_efficiency_vs_threads", "Efficiency", "Parallel efficiency (speedup / threads)", "Efficiency vs threads")]:
        fig, ax = plt.subplots(figsize=(6.6, 4.4))
        for d in order:
            g = par[par.Dataset == d].sort_values("Threads")
            ax.plot(g.Threads, g[col], marker=ds_markers[d], color=ds_colors[d], label=d)
        if col == "Speedup_vs_seq":
            lim = max(thr_list)
            ax.plot([1, lim], [1, lim], ls="--", color="#777777", lw=1.2, label="ideal (linear)")
        else:
            ax.axhline(1.0, ls="--", color="#777777", lw=1.2, label="ideal (1.0)")
        ax.axhline(1.0 if col == "Speedup_vs_seq" else 0, color=INK, lw=0.5)
        ax.set_xscale("log", base=2); ax.set_xticks(thr_list); ax.set_xticklabels([str(x) for x in thr_list])
        ax.set_xlabel("Threads"); ax.set_ylabel(ylabel); ax.set_title(title)
        ax.legend(ncol=2, fontsize=8)
        save(fig, figdir, name)

    # Fig 4: throughput vs threads
    fig, ax = plt.subplots(figsize=(6.6, 4.4))
    for d in order:
        g = pd.concat([sq[sq.Dataset == d], par[par.Dataset == d]]).sort_values("Threads")
        ax.plot(g.Threads, g.MTEPS_median, marker=ds_markers[d], color=ds_colors[d], label=d)
    ax.set_xscale("log", base=2); ax.set_xticks(sorted(summ.Threads.unique()))
    ax.set_xticklabels([str(x) for x in sorted(summ.Threads.unique())])
    ax.set_yscale("log")
    ax.set_xlabel("Threads (1 = sequential baseline at first point, then parallel code)")
    ax.set_ylabel("Throughput (MTEPS = 10^6 adjacency entries scanned / s)")
    ax.set_title("Traversal throughput vs threads")
    ax.legend(ncol=2, fontsize=8)
    save(fig, figdir, "fig4_throughput_vs_threads")

    # Fig 5: overhead breakdown at the largest profiled thread count
    if not ov.empty:
        tmax = ov.Threads.max()
        o = ov[ov.Threads == tmax].set_index("Dataset").reindex([d for d in order if d in set(ov.Dataset)])
        other = (1 - o.Barrier_idle_frac - o.Merge_frac).clip(lower=0)
        fig, ax = plt.subplots(figsize=(6.6, 4.4))
        x = np.arange(len(o))
        ax.bar(x, o.Barrier_idle_frac * 100, color=PALETTE[3], label="idle at end-of-level barrier (per-thread avg)")
        ax.bar(x, o.Merge_frac * 100, bottom=o.Barrier_idle_frac * 100, color=PALETTE[1], label="frontier merge (single-phase wall time)")
        ax.bar(x, other * 100, bottom=(o.Barrier_idle_frac + o.Merge_frac) * 100, color=PALETTE[0], label="remainder (traversal work, region start-up)")
        ax.set_xticks(x); ax.set_xticklabels(o.index, rotation=25, ha="right")
        ax.set_ylabel("% of profiled execution time"); ax.set_ylim(0, 100)
        ax.set_title(f"Where the time goes at {int(tmax)} threads (profiled runs)")
        ax.legend(fontsize=8, loc="upper center", bbox_to_anchor=(0.5, -0.32))
        save(fig, figdir, "fig5_overhead_breakdown")

    # Fig 6: speedup at max threads vs number of BFS levels
    tmax = max(thr_list)
    g = par[par.Threads == tmax]
    fig, ax = plt.subplots(figsize=(6.6, 4.4))
    for r in g.itertuples():
        ax.scatter(r.Levels_median, r.Speedup_vs_seq, color=ds_colors[r.Dataset], marker=ds_markers[r.Dataset], s=60, label=r.Dataset)
    ax.axhline(1.0, ls="--", color="#777777", lw=1.2)
    ax.set_xscale("log")
    ax.set_xlabel("BFS levels (median over sources; a measure of how sequential the traversal is)")
    ax.set_ylabel(f"Speedup at {tmax} threads")
    ax.set_title("Speedup vs number of BFS levels")
    ax.legend(ncol=2, fontsize=8)
    save(fig, figdir, "fig6_speedup_vs_levels")

    print(f"wrote {a.outdir}/bfs_master_rows.csv, bfs_summary.csv, bfs_overhead.csv, summary_table.md and 6 figures")
    print()
    print("\n".join(md))


if __name__ == "__main__":
    main()
