#!/bin/bash
#SBATCH -J ff_N500_fix
#SBATCH -p test
#SBATCH -A phys033164
#SBATCH --ntasks=16
#SBATCH --cpus-per-task=1
#SBATCH --time=00:25:00
#SBATCH --array=0-5%3
#SBATCH -o slurm_logs/%x_%A_%a.out
#SBATCH -e slurm_logs/%x_%A_%a.err

set -euo pipefail
cd "${SLURM_SUBMIT_DIR:-$PWD}"
mkdir -p slurm_logs

# Exactly the missing (p,seed,M) triples:
P=(0.60 0.60 0.58 0.58 0.58 0.62)
S=(52004 52004 52004 52004 52004 52000)
M=(100   200   50   100  200  200)

idx=${SLURM_ARRAY_TASK_ID}
p="${P[$idx]}"
seed="${S[$idx]}"
Mm="${M[$idx]}"
N=500

outdir="results/raw/N500/p${p}"
mkdir -p "$outdir"

run_csv="${outdir}/run_seed${seed}_M${Mm}_tasks${SLURM_NTASKS}.csv"
run_log="${outdir}/run_seed${seed}_M${Mm}_tasks${SLURM_NTASKS}.log"

echo "RERUN N=${N} p=${p} M=${Mm} seed=${seed} tasks=${SLURM_NTASKS}"

t0=$(date +%s.%N)

# Allow mpirun to fail without killing the script (we still want logs)
set +e
mpirun --bind-to none -np "$SLURM_NTASKS" ./forest_fire --N "$N" --p "$p" --M "$Mm" --seed "$seed" > "$run_log" 2>&1
rc=$?
set -e

t1=$(date +%s.%N)
wall=$(awk -v t0="$t0" -v t1="$t1" 'BEGIN{printf "%.6f", (t1-t0)}')

avg_steps=$(awk -F, '{for(i=1;i<=NF;i++) if($i=="avg_steps"){print $(i+1); exit}}' "$run_log" || true)
prob_rb=$(awk -F, '{for(i=1;i<=NF;i++) if($i=="prob_reach_bottom"){print $(i+1); exit}}' "$run_log" || true)
total_time=$(awk -F, '{for(i=1;i<=NF;i++) if($i=="total_time_s"){print $(i+1); exit}}' "$run_log" || true)

if [[ -z "${avg_steps}" ]]; then avg_steps="nan"; fi
if [[ -z "${prob_rb}" ]]; then prob_rb="nan"; fi
if [[ -z "${total_time}" ]]; then total_time="nan"; fi

{
  echo "N,p,M,seed,tasks,avg_steps,prob_reach_bottom,total_time_s,wall_time_s,mpirun_rc"
  echo "${N},${p},${Mm},${seed},${SLURM_NTASKS},${avg_steps},${prob_rb},${total_time},${wall},${rc}"
} > "$run_csv"

echo "Wrote: $run_csv"
echo "mpirun_rc=${rc}"
