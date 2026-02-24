from pathlib import Path
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt

# -----------------------
# Paths
# -----------------------
ROOT = Path(__file__).resolve().parent
CSV_PATH = ROOT / "science_all_runs.csv"
FIG_DIR = ROOT / "figs"
TABLE_DIR = ROOT / "tables"
FIG_DIR.mkdir(exist_ok=True)
TABLE_DIR.mkdir(exist_ok=True)

# -----------------------
# Load data
# -----------------------
df = pd.read_csv(CSV_PATH)

# Numeric conversion
for c in ["N", "M", "seed", "tasks"]:
    df[c] = pd.to_numeric(df[c], errors="coerce")
for c in ["p", "avg_steps", "prob_reach_bottom", "total_time_s", "wall_time_s"]:
    df[c] = pd.to_numeric(df[c], errors="coerce")

# Drop bad parses
df = df.dropna(subset=["N", "p", "M", "avg_steps", "prob_reach_bottom"])

# -----------------------
# Summary stats with 95% CI across seeds
# -----------------------
def summarize(g):
    n = len(g)
    mean_steps = g["avg_steps"].mean()
    mean_prob = g["prob_reach_bottom"].mean()

    # Standard error
    se_steps = g["avg_steps"].std(ddof=1) / np.sqrt(n) if n > 1 else 0.0
    se_prob = g["prob_reach_bottom"].std(ddof=1) / np.sqrt(n) if n > 1 else 0.0

    # 95% CI (normal approx)
    ci_steps = 1.96 * se_steps
    ci_prob = 1.96 * se_prob

    return pd.Series({
        "n_runs": n,
        "avg_steps_mean": mean_steps,
        "avg_steps_ci95": ci_steps,
        "prob_mean": mean_prob,
        "prob_ci95": ci_prob
    })

summary = (
    df.groupby(["N", "p", "M"], as_index=False)
      .apply(summarize)
      .reset_index(drop=True)
)

summary.to_csv(TABLE_DIR / "science_summary.csv", index=False)

# -----------------------
# Helper save
# -----------------------
def save(fig, name):
    fig.tight_layout()
    fig.savefig(FIG_DIR / f"{name}.png", dpi=250)
    fig.savefig(FIG_DIR / f"{name}.pdf")
    plt.close(fig)
    print(f"[OK] wrote figs/{name}.png and figs/{name}.pdf")

# -----------------------
# Choose baseline M for p-sweep plots
# -----------------------
M_BASE = {100: 200, 500: 100}  # match how you ran them

# -----------------------
# Plot: prob reach bottom vs p (baseline M)
# -----------------------
for N in [100, 500]:
    m0 = M_BASE[N]
    d = summary[(summary["N"] == N) & (summary["M"] == m0)].sort_values("p")
    fig = plt.figure()
    plt.errorbar(d["p"], d["prob_mean"], yerr=d["prob_ci95"], marker="o", capsize=3)
    plt.xlabel("p")
    plt.ylabel("P(reach bottom)")
    plt.title(f"P(reach bottom) vs p (N={N}, M={m0})")
    plt.ylim(-0.05, 1.05)
    save(fig, f"prob_vs_p_N{N}")

# -----------------------
# Plot: avg steps vs p (baseline M)
# -----------------------
for N in [100, 500]:
    m0 = M_BASE[N]
    d = summary[(summary["N"] == N) & (summary["M"] == m0)].sort_values("p")
    fig = plt.figure()
    plt.errorbar(d["p"], d["avg_steps_mean"], yerr=d["avg_steps_ci95"], marker="o", capsize=3)
    plt.xlabel("p")
    plt.ylabel("Average steps")
    plt.title(f"Average steps vs p (N={N}, M={m0})")
    save(fig, f"steps_vs_p_N{N}")

# -----------------------
# Plot: convergence vs M near transition (p in {0.58, 0.60, 0.62})
# -----------------------
P_CONV = [0.58, 0.60, 0.62]
for N in [100, 500]:
    d = summary[(summary["N"] == N) & (summary["p"].isin(P_CONV))].copy()
    if d.empty:
        print(f"[WARN] No convergence data for N={N}")
        continue
    fig = plt.figure()
    for p in sorted(d["p"].unique()):
        sub = d[d["p"] == p].sort_values("M")
        plt.errorbar(sub["M"], sub["prob_mean"], yerr=sub["prob_ci95"], marker="o", capsize=3, label=f"p={p:g}")
    plt.xscale("log")
    plt.xlabel("M")
    plt.ylabel("P(reach bottom)")
    plt.title(f"Convergence near transition (N={N})")
    plt.ylim(-0.05, 1.05)
    plt.legend()
    save(fig, f"convergence_prob_vs_M_N{N}")

# -----------------------
# Plot: speedup vs tasks (your measured timings + ideal)
# -----------------------
T100 = {1:15.225, 2:15.6005, 4:8.55156, 8:5.51891, 16:8.32291}
T500 = {1:461.751, 2:354.978, 4:195.055, 8:100.018, 16:60.7459}

def speedup(T):
    t1 = T[1]
    xs = sorted(T.keys())
    ys = [t1 / T[p] for p in xs]
    return xs, ys

x1, s100 = speedup(T100)
x2, s500 = speedup(T500)
ideal = x2[:]  # ideal speedup = tasks

fig = plt.figure()
plt.plot(x1, s100, marker="o", label="N=100")
plt.plot(x2, s500, marker="o", label="N=500")
plt.plot(x2, ideal, linestyle="--", label="Ideal")
plt.xlabel("MPI tasks")
plt.ylabel("Speedup (T1/Tp)")
plt.xticks(x2)
plt.title("Strong scaling speedup")
plt.legend()
save(fig, "speedup")

print("[DONE] All figures saved in:", FIG_DIR)
