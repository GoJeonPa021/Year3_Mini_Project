#!/bin/bash
#SBATCH -J ff_N500_sweep
#SBATCH -p test
#SBATCH -A phys033164
#SBATCH --ntasks=16
#SBATCH --cpus-per-task=1
#SBATCH --time=00:25:00
#SBATCH --array=0-54%10
#SBATCH -o slurm_logs/%x_%A_%a.out
#SBATCH -e slurm_logs/%x_%A_%a.err

set -euo pipefail

# -----------------------
# Experiment parameters
# -----------------------
N=500
M_BASE=100
SEEDS_PER_P=5

P_LIST=(0.40 0.45 0.50 0.55 0.60 0.65 0.70 0.75 0.80 0.58 0.62)

P_CONV=("0.58" "0.60" "0.62")
M_CONV=(25 50 100 200)

# -----------------------
# Paths and setup
# -----------------------
cd "${SLURM_SUBMIT_DIR:-$PWD}"
mkdir -p slurm_logs results/raw/N${N}

# -----------------------
# Map array id -> (p, seed)
# -----------------------
IDX=${SLURM_ARRAY_TASK_ID}
P_INDEX=$(( IDX / SEEDS_PER_P ))
S_INDEX=$(( IDX % SEEDS_PER_P ))

p="${P_LIST[$P_INDEX]}"
seed=$(( 52000 + S_INDEX ))

Ms=("$M_BASE")
for pc in "${P_CONV[@]}"; do
  if [[ "$p" == "$pc" ]]; then
    Ms=("${M_CONV[@]}")
  fi
done

# -----------------------
# Run each chosen M
# -----------------------
for M in "${Ms[@]}"; do
  outdir="results/raw/N${N}/p${p}"
  mkdir -p "$outdir"

  run_csv="${outdir}/run_seed${seed}_M${M}_tasks${SLURM_NTASKS}.csv"
  run_log="${outdir}/run_seed${seed}_M${M}_tasks${SLURM_NTASKS}.log"

  echo "N=${N} p=${p} M=${M} seed=${seed} tasks=${SLURM_NTASKS}"
  t0=$(date +%s.%N)

  set +e
  mpirun --bind-to none -np "$SLURM_NTASKS" ./forest_fire --N "$N" --p "$p" --M "$M" --seed "$seed" > "$run_log" 2>&1
  rc=$?
  set -e

  wall=$(awk -v t0="$t0" -v t1="$t1" 'BEGIN{printf "%.6f", (t1-t0)}')

  avg_steps=$(awk -F, '{for(i=1;i<=NF;i++) if($i=="avg_steps"){print $(i+1); exit}}' "$run_log" || true)
  prob_rb=$(awk -F, '{for(i=1;i<=NF;i++) if($i=="prob_reach_bottom"){print $(i+1); exit}}' "$run_log" || true)
  total_time=$(awk -F, '{for(i=1;i<=NF;i++) if($i=="total_time_s"){print $(i+1); exit}}' "$run_log" || true)

  if [[ -z "${avg_steps}" ]]; then avg_steps="nan"; fi
  if [[ -z "${prob_rb}" ]]; then prob_rb="nan"; fi
  if [[ -z "${total_time}" ]]; then total_time="nan"; fi

  {
    echo "N,p,M,seed,tasks,avg_steps,prob_reach_bottom,total_time_s,wall_time_s,mpirun_rc"
    echo "${N},${p},${M},${seed},${SLURM_NTASKS},${avg_steps},${prob_rb},${total_time},${wall},${rc}"
  } > "$run_csv"

  echo "Wrote: $run_csv"
done
