# Forest Fire MPI Mini Project (SCIF30005)

A parallel **C++/MPI** implementation of a forest fire cellular automaton, developed for the **SCIF30005** mini project.  
The project investigates:

- **Model behaviour** as tree density \( p \) changes
- **Convergence** with respect to the number of repeats \( M \)
- **Strong-scaling performance** on BluePebble (MPI task count vs runtime/speedup)

This repository also includes **Python plotting scripts**, **Slurm job scripts**, and **supporting workflow files** used to generate the final figures and analysis.

---

## Project overview

The forest fire model is simulated on an \( N \times N \) grid:

- Each cell is one of: empty, living tree, burning tree, dead tree
- Trees are placed initially with probability \( p \)
- The **top row is ignited**
- Fire spreads using a **von Neumann neighbourhood** (up/down/left/right)
- Simulation stops when no cells are burning

For each run, the code records:
- whether the fire reaches the **bottom row**
- the number of **timesteps until extinction**

Repeated runs are used to estimate:
- \( \hat{P}(\text{reach bottom}) \)
- average burn duration (steps)

---

## Repository contents

> Adjust this list if your final file names differ.

### Core C++ / MPI source
- `main.cpp` — program entry point / CLI handling
- `ForestFire.cpp`, `ForestFire.h` — model implementation and simulation logic
- `GridIO.cpp`, `GridIO.h` — grid input/output utilities
- `CellState.h` — cell state definitions

### Analysis / plotting
- `plot_all_figures.py` — generates analysis figures from CSV outputs
- `plot_speedup.py` — generates speedup plot for performance analysis

### Job scripts (BluePebble / Slurm)
- `sbatch_sweep_N100.sh` — science sweep for \( N=100 \)
- `sbatch_sweep_N500.sh` — science sweep for \( N=500 \)
- `sbatch_rerun_missing_N500.sh` — rerun script for missing/failed cases

### Data outputs (examples)
- `science_all_runs.csv` — merged raw run-level outputs
- `science_summary.csv` — aggregated summary statistics (if included)

### Figures (examples)
- `prob_vs_p_N100.png`
- `prob_vs_p_N500.png`
- `steps_vs_p_N100.png`
- `steps_vs_p_N500.png`
- `convergence_prob_vs_M_N100.png`
- `convergence_prob_vs_M_N500.png`
- `speedup.png`

### Report / write-up (optional)
- `forest_fire.pdf` — project report (if included)

---

## Requirements

### On BluePebble (recommended)
- C++ compiler (e.g. `g++`)
- MPI implementation (OpenMPI)
- Slurm scheduler

### Local machine (for plotting)
- Python 3.x
- `numpy`
- `pandas`
- `matplotlib`

Install plotting dependencies locally (if needed):
```bash
pip install numpy pandas matplotlib
