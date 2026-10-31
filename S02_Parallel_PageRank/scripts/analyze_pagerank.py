#!/usr/bin/env python3
"""Derive speedup/efficiency/throughput from the raw PageRank measurements and produce the
summary table and figures for the CS4 / S02 report.

Inputs : results/pr_raw.csv, results/pr_graph_stats.csv   (written by pagerank_bench)
Outputs: results/pr_master_rows.csv   one row per run, required schema, Speedup/Efficiency filled in
         results/pr_summary.csv       per dataset x schedule x thread-count statistics
         results/pr_overhead.csv      barrier-idle breakdown from the profiled runs
         results/pr_schedule_compare.csv  static vs dynamic speedup per dataset and thread count
         results/pr_summary_table.md  the summary table in Markdown
         results/figures_pr/*.png|svg plots (this script is the editable source of the plots)

Definitions (all derived here from raw data, never typed in by hand):
  baseline        = median execution time of the sequential pull PageRank for the same dataset
  Speedup         = baseline / parallel execution time      (both run to the same convergence)
  Efficiency      = Speedup / Threads
  Time per iter   = Execution_Time / Iterations
  MEPS            = Edges_Scanned / Execution_Time / 1e6    (CSR adjacency entries read per second;
                    Edges_Scanned = Problem_Size x Iterations)
  Barrier idle    = Synchronization_Time / Execution_Time of the profiled runs
Only non-profiled runs are used for speedup/efficiency/throughput; profiled runs are
instrumented and are used only for the barrier-idle breakdown.
"""
import argparse
import os
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

PALETTE = ["#0072B2", "#E69F00", "#009E73", "#D55E00", "#CC79A7", "#56B4E9", "#F0E442", "#000000"]
MARKERS = ["o", "s", "^", "D", "v", "P", "X", "*"]
INK, GRID = "#222222", "#DDDDDD"
SEQ = "PR-seq-pull"


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


def sched_style(s):
    return "-" if s == "static" else "--"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--raw", default="results/pr_raw.csv")
    ap.add_argument("--stats", default="results/pr_graph_stats.csv")
    ap.add_argument("--outdir", default="results")
    a = ap.parse_args()
    figdir = os.path.join(a.outdir, "figures_pr")
    os.makedirs(figdir, exist_ok=True)
    style()

    raw = pd.read_csv(a.raw, na_values=["NA"])
    stats = pd.read_csv(a.stats) if os.path.exists(a.stats) else pd.DataFrame()
    if raw.empty:
        sys.exit("no rows in " + a.raw)
    if not (raw["Verified"] == 1).all():
        sys.exit("raw data contains unverified runs")
    raw["Schedule"] = raw["Schedule"].fillna("seq")
    conv = raw.groupby("Dataset").Converged.min()
    for d in conv[conv == 0].index:
        print(f"WARNING: {d} did not converge within the iteration cap in at least one run", file=sys.stderr)

    # ---------------------------------------------------------------- derived metrics
    seq = raw[(raw.Algorithm == SEQ) & (raw.Profiled == 0)]
    base = seq.groupby("Dataset").Execution_Time.median().rename("Baseline_Time")
    m = raw.merge(base, on="Dataset", how="left")
    timed = m.Profiled == 0
    m["Speedup"] = np.where(timed, m.Baseline_Time / m.Execution_Time, np.nan)
    m["Efficiency"] = m.Speedup / m.Threads
    m["Time_per_iter"] = m.Execution_Time / m.Iterations
    m["MEPS"] = np.where(timed, m.Edges_Scanned / m.Execution_Time / 1e6, np.nan)
    m["Oversubscribed"] = m.Threads > m.Logical_Cores
    master = m[list(raw.columns) + ["Baseline_Time", "Time_per_iter", "MEPS", "Oversubscribed"]]
    master.to_csv(os.path.join(a.outdir, "pr_master_rows.csv"), index=False, na_rep="NA")

    # ---------------------------------------------------------------- summary
    t = m[timed].copy()
    rows = []
    for (ds, algo, sch, thr), grp in t.groupby(["Dataset", "Algorithm", "Schedule", "Threads"]):
        x = grp.Execution_Time
        med = x.median()
        rows.append({
            "Dataset": ds, "Algorithm": algo, "Schedule": sch, "Threads": thr,
            "Problem_Size": int(grp.Problem_Size.iloc[0]), "Vertices": int(grp.Vertices.iloc[0]),
            "Directed": int(grp.Directed.iloc[0]), "Iterations": int(grp.Iterations.median()), "Runs": len(grp),
            "Time_mean_s": x.mean(), "Time_median_s": med,
            "Time_std_s": x.std(ddof=1) if len(x) > 1 else np.nan,
            "Time_CV_pct": 100 * x.std(ddof=1) / x.mean() if len(x) > 1 else np.nan,
            "Time_min_s": x.min(), "Time_max_s": x.max(),
            "Time_per_iter_ms": 1e3 * med / grp.Iterations.median(),
            "Speedup_vs_seq": base[ds] / med, "Efficiency": base[ds] / med / thr,
            "MEPS_median": grp.MEPS.median(), "Oversubscribed": bool(grp.Oversubscribed.iloc[0]),
        })
    summ = pd.DataFrame(rows)
    p1 = summ[(summ.Algorithm != SEQ) & (summ.Threads == 1)].set_index(["Dataset", "Schedule"]).Time_median_s
    summ["Speedup_vs_parallel_1T"] = [
        np.nan if r.Algorithm == SEQ else p1.get((r.Dataset, r.Schedule), np.nan) / r.Time_median_s
        for r in summ.itertuples()]
    order = summ.groupby("Dataset").Problem_Size.first().sort_values().index.tolist()
    summ["Dataset"] = pd.Categorical(summ.Dataset, order, ordered=True)
    summ["_par"] = (summ.Algorithm != SEQ).astype(int)
    summ["_s"] = (summ.Schedule != "static").astype(int)
    summ = summ.sort_values(["Dataset", "_par", "_s", "Threads"]).drop(columns=["_par", "_s"]).reset_index(drop=True)
    summ.to_csv(os.path.join(a.outdir, "pr_summary.csv"), index=False)

    md = ["| Dataset | Adj. entries | Iter. | Config | Median time (ms) | ms / iter | CV (%) | Speedup vs seq | Efficiency | MEPS |",
          "|---|---:|---:|---|---:|---:|---:|---:|---:|---:|"]
    for r in summ.itertuples():
        seqrow = r.Algorithm == SEQ
        cfg = "sequential" if seqrow else f"OpenMP {r.Schedule} {r.Threads}T" + ("*" if r.Oversubscribed else "")
        sp = "1.00 (baseline)" if seqrow else f"{r.Speedup_vs_seq:.2f}"
        ef = "-" if seqrow else f"{r.Efficiency:.2f}"
        md.append(f"| {r.Dataset} | {r.Problem_Size:,} | {r.Iterations} | {cfg} | {r.Time_median_s * 1e3:.3f} | "
                  f"{r.Time_per_iter_ms:.3f} | {r.Time_CV_pct:.1f} | {sp} | {ef} | {r.MEPS_median:.1f} |")
    md += ["", "`*` = more threads than logical cores (oversubscribed). Baseline = sequential pull PageRank, "
           "median over repetitions. MEPS = million CSR adjacency entries read per second."]
    with open(os.path.join(a.outdir, "pr_summary_table.md"), "w") as f:
        f.write("\n".join(md) + "\n")

    # ---------------------------------------------------------------- static vs dynamic
    par = summ[summ.Algorithm != SEQ]
    sq = summ[summ.Algorithm == SEQ]
    piv = par.pivot_table(index=["Dataset", "Threads"], columns="Schedule", values="Speedup_vs_seq", observed=True)
    cmp_ = pd.DataFrame()
    if "static" in piv.columns:
        dyn = [c for c in piv.columns if c.startswith("dynamic")]
        if dyn:
            cmp_ = piv.rename(columns={dyn[0]: "dynamic"}).reset_index()
            cmp_["dynamic_over_static"] = cmp_["dynamic"] / cmp_["static"]
            cmp_.to_csv(os.path.join(a.outdir, "pr_schedule_compare.csv"), index=False)

    # ---------------------------------------------------------------- overhead breakdown (profiled)
    prof = m[m.Profiled == 1].copy()
    ov = pd.DataFrame()
    if not prof.empty:
        prof["Barrier_idle_frac"] = prof.Synchronization_Time / prof.Execution_Time
        ov = prof.groupby(["Dataset", "Schedule", "Threads"]).agg(
            Runs=("Run_ID", "count"), Exec_profiled_s=("Execution_Time", "median"),
            Barrier_wait_avg_s=("Synchronization_Time", "median"),
            Barrier_idle_frac=("Barrier_idle_frac", "median"), Iterations=("Iterations", "median")).reset_index()
        ov["Barrier_idle_per_iter_ms"] = 1e3 * ov.Barrier_wait_avg_s / ov.Iterations
        ov.to_csv(os.path.join(a.outdir, "pr_overhead.csv"), index=False)

    # ---------------------------------------------------------------- figures
    ds_colors = {d: PALETTE[i % len(PALETTE)] for i, d in enumerate(order)}
    ds_markers = {d: MARKERS[i % len(MARKERS)] for i, d in enumerate(order)}
    thr_list = sorted(par.Threads.unique())
    scheds = sorted(par.Schedule.unique(), key=lambda s: (s != "static", s))

    def thread_axis(ax):
        ax.set_xscale("log", base=2)
        ax.set_xticks(thr_list)
        ax.set_xticklabels([str(x) for x in thr_list])

    def legend_with_schedules(ax, ncol=2):
        h, l = ax.get_legend_handles_labels()
        for s in scheds:
            h.append(plt.Line2D([], [], color="#555555", ls=sched_style(s)))
            l.append(f"schedule: {s}")
        ax.legend(h, l, ncol=ncol, fontsize=8)

    # Fig 1: time vs problem size (sequential + best schedule at each thread count)
    fig, ax = plt.subplots(figsize=(6.6, 4.4))
    cfgs = [("sequential", sq.set_index("Dataset"))]
    for T in thr_list:
        best = par[par.Threads == T].sort_values("Time_median_s").groupby("Dataset", observed=True).first()
        cfgs.append((f"{T} threads (best schedule)", best))
    for i, (lab, d) in enumerate(cfgs):
        d = d.reindex([o for o in order if o in d.index])
        ax.plot(d.Problem_Size, d.Time_median_s, marker=MARKERS[i % 8], color=PALETTE[i % 8], label=lab)
    ax.set_xscale("log"); ax.set_yscale("log")
    ax.set_xlabel("Problem size (CSR adjacency entries)"); ax.set_ylabel("Median time to convergence (s)")
    ax.set_title("PageRank execution time vs graph size")
    ax.legend(fontsize=8)
    save(fig, figdir, "fig1_time_vs_size")

    # Fig 2 & 3: speedup / efficiency vs threads
    for name, col, ylabel, title in [
            ("fig2_speedup_vs_threads", "Speedup_vs_seq", "Speedup over sequential PageRank", "Speedup vs threads"),
            ("fig3_efficiency_vs_threads", "Efficiency", "Parallel efficiency (speedup / threads)", "Efficiency vs threads")]:
        fig, ax = plt.subplots(figsize=(6.6, 4.4))
        for d in order:
            for s in scheds:
                g = par[(par.Dataset == d) & (par.Schedule == s)].sort_values("Threads")
                if g.empty:
                    continue
                ax.plot(g.Threads, g[col], marker=ds_markers[d], color=ds_colors[d], ls=sched_style(s),
                        label=d if s == scheds[0] else None)
        if col == "Speedup_vs_seq":
            ax.plot([1, max(thr_list)], [1, max(thr_list)], ls=":", color="#777777", lw=1.2, label="ideal (linear)")
            ax.axhline(1.0, color=INK, lw=0.5)
        else:
            ax.axhline(1.0, ls=":", color="#777777", lw=1.2, label="ideal (1.0)")
        thread_axis(ax)
        ax.set_xlabel("Threads"); ax.set_ylabel(ylabel); ax.set_title(title)
        legend_with_schedules(ax)
        save(fig, figdir, name)

    # Fig 4: throughput vs threads
    fig, ax = plt.subplots(figsize=(6.6, 4.4))
    for d in order:
        for s in scheds:
            g = par[(par.Dataset == d) & (par.Schedule == s)].sort_values("Threads")
            if g.empty:
                continue
            ax.plot(g.Threads, g.MEPS_median, marker=ds_markers[d], color=ds_colors[d], ls=sched_style(s),
                    label=d if s == scheds[0] else None)
    thread_axis(ax); ax.set_yscale("log")
    ax.set_xlabel("Threads"); ax.set_ylabel("Throughput (MEPS = 10^6 adjacency entries read / s)")
    ax.set_title("PageRank throughput vs threads")
    legend_with_schedules(ax)
    save(fig, figdir, "fig4_throughput_vs_threads")

    # Fig 5: barrier idle fraction vs threads (profiled runs)
    if not ov.empty:
        fig, ax = plt.subplots(figsize=(6.6, 4.4))
        for d in order:
            for s in scheds:
                g = ov[(ov.Dataset == d) & (ov.Schedule == s)].sort_values("Threads")
                if g.empty:
                    continue
                ax.plot(g.Threads, g.Barrier_idle_frac * 100, marker=ds_markers[d], color=ds_colors[d],
                        ls=sched_style(s), label=d if s == scheds[0] else None)
        thread_axis(ax)
        ax.set_xlabel("Threads"); ax.set_ylabel("Barrier idle time (% of profiled run time, per-thread avg)")
        ax.set_title("Idle time at the end-of-iteration barrier (profiled runs)")
        legend_with_schedules(ax)
        save(fig, figdir, "fig5_barrier_idle_vs_threads")

    # Fig 6: speedup at max threads vs in-degree inequality (Gini) -- descriptive, not a causal claim
    if not stats.empty and "InDeg_Gini" in stats.columns:
        tmax = max(thr_list)
        st = stats.drop_duplicates("Dataset", keep="last").set_index("Dataset")
        fig, ax = plt.subplots(figsize=(6.6, 4.4))
        for d in order:
            if d not in st.index:
                continue
            for s in scheds:
                g = par[(par.Dataset == d) & (par.Schedule == s) & (par.Threads == tmax)]
                if g.empty:
                    continue
                ax.scatter(st.loc[d, "InDeg_Gini"], g.Speedup_vs_seq.iloc[0], color=ds_colors[d],
                           marker=ds_markers[d], s=60, facecolors=ds_colors[d] if s == scheds[0] else "none",
                           label=d if s == scheds[0] else None)
        ax.axhline(1.0, ls=":", color="#777777", lw=1.2)
        ax.set_xlabel("In-degree Gini coefficient (0 = uniform, 1 = maximally skewed)")
        ax.set_ylabel(f"Speedup at {tmax} threads")
        ax.set_title("Speedup vs in-degree skew (filled = static, hollow = dynamic)")
        ax.legend(fontsize=8, ncol=2)
        save(fig, figdir, "fig6_speedup_vs_indegree_gini")

    print(f"wrote {a.outdir}/pr_master_rows.csv, pr_summary.csv, pr_overhead.csv, pr_schedule_compare.csv, "
          f"pr_summary_table.md and figures in {figdir}")
    print()
    print("\n".join(md))


if __name__ == "__main__":
    main()
