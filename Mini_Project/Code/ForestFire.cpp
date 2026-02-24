#include "ForestFire.h"
#include "CellState.h"
#include <mpi.h>

#include <random>
#include <algorithm>
#include <stdexcept>

ForestFireMPI::ForestFireMPI(int N, int mpi_rank, int mpi_size)
: N_(N), rank_(mpi_rank), size_(mpi_size)
{
    if (N_ <= 0) throw std::runtime_error("N must be positive.");

    const int base = N_ / size_;
    const int rem  = N_ % size_;
    local_rows_ = base + (rank_ < rem ? 1 : 0);
    start_row_  = rank_ * base + (rank_ < rem ? rank_ : rem);

    cur_.assign((size_t)(local_rows_ + 2) * (size_t)N_, (int)CellState::Empty);
    nxt_.assign((size_t)(local_rows_ + 2) * (size_t)N_, (int)CellState::Empty);
}

void ForestFireMPI::init_from_local_rows(const std::vector<int>& local_rows_no_ghost) {
    if ((int)local_rows_no_ghost.size() != local_rows_ * N_) {
        throw std::runtime_error("init_from_local_rows: wrong local buffer size.");
    }

    // Fill real rows into cur_ at r=1..local_rows_
    for (int r = 0; r < local_rows_; ++r) {
        for (int c = 0; c < N_; ++c) {
            cur_[idx(r + 1, c)] = local_rows_no_ghost[r * N_ + c];
        }
    }

    // Clear ghosts
    std::fill(cur_.begin(), cur_.begin() + N_, (int)CellState::Empty);
    std::fill(cur_.end() - N_, cur_.end(), (int)CellState::Empty);

    apply_top_row_burning();
}

void ForestFireMPI::init_random(double p, std::uint64_t seed) {
    if (p < 0.0 || p > 1.0) throw std::runtime_error("p must be in [0,1].");

    std::mt19937_64 rng(seed + (std::uint64_t)rank_ * 0x9E3779B97F4A7C15ULL);
    std::uniform_real_distribution<double> U(0.0, 1.0);

    // Fill real rows with Alive with prob p else Empty
    for (int r = 1; r <= local_rows_; ++r) {
        for (int c = 0; c < N_; ++c) {
            cur_[idx(r, c)] = (U(rng) < p) ? (int)CellState::Alive : (int)CellState::Empty;
        }
    }

    // Clear ghosts
    std::fill(cur_.begin(), cur_.begin() + N_, (int)CellState::Empty);
    std::fill(cur_.end() - N_, cur_.end(), (int)CellState::Empty);

    apply_top_row_burning();
}

void ForestFireMPI::apply_top_row_burning() {
    // Global top row = row 0. This rank owns it if start_row_ == 0.
    if (start_row_ == 0) {
        const int r_local = 1; // local row corresponding to global row 0
        for (int c = 0; c < N_; ++c) {
            if (cur_[idx(r_local, c)] == (int)CellState::Alive) {
                cur_[idx(r_local, c)] = (int)CellState::Burning;
            }
        }
    }
}

void ForestFireMPI::exchange_halos() {
    const int up   = (rank_ == 0) ? MPI_PROC_NULL : rank_ - 1;
    const int down = (rank_ == size_ - 1) ? MPI_PROC_NULL : rank_ + 1;

    // Send first real row (r=1) up, receive ghost row r=0 from up
    MPI_Sendrecv(
        &cur_[idx(1, 0)], N_, MPI_INT, up,   100,
        &cur_[idx(0, 0)], N_, MPI_INT, up,   200,
        MPI_COMM_WORLD, MPI_STATUS_IGNORE
    );

    // Send last real row (r=local_rows_) down, receive ghost row r=local_rows_+1 from down
    MPI_Sendrecv(
        &cur_[idx(local_rows_, 0)],     N_, MPI_INT, down, 200,
        &cur_[idx(local_rows_ + 1, 0)], N_, MPI_INT, down, 100,
        MPI_COMM_WORLD, MPI_STATUS_IGNORE
    );
}

int ForestFireMPI::has_burning_local() const {
    for (int r = 1; r <= local_rows_; ++r) {
        for (int c = 0; c < N_; ++c) {
            if (cur_[idx(r, c)] == (int)CellState::Burning) return 1;
        }
    }
    return 0;
}

RunResult ForestFireMPI::run() {
    RunResult res;

    // Check if any burning exists initially
    int burning_local = has_burning_local();
    int burning_global = 0;
    MPI_Allreduce(&burning_local, &burning_global, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);

    // Also check if burning already on bottom row initially
    int reached_local = 0;
    for (int r = 1; r <= local_rows_; ++r) {
        const int global_r = start_row_ + (r - 1);
        if (global_r == N_ - 1) {
            for (int c = 0; c < N_; ++c) {
                if (cur_[idx(r, c)] == (int)CellState::Burning) reached_local = 1;
            }
        }
    }
    int reached_global = 0;
    MPI_Allreduce(&reached_local, &reached_global, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
    res.reached_bottom = reached_global;

    while (burning_global) {
        exchange_halos();

        // Update into nxt_
        int next_burning_local = 0;
        int next_reached_local = 0;

        for (int r = 1; r <= local_rows_; ++r) {
            const int global_r = start_row_ + (r - 1);

            for (int c = 0; c < N_; ++c) {
                const int s = cur_[idx(r, c)];
                int out = s;

                if (s == (int)CellState::Alive) {
                    const bool left  = (c > 0)     && (cur_[idx(r, c - 1)] == (int)CellState::Burning);
                    const bool right = (c < N_ - 1) && (cur_[idx(r, c + 1)] == (int)CellState::Burning);
                    const bool up    =               (cur_[idx(r - 1, c)] == (int)CellState::Burning);
                    const bool down  =               (cur_[idx(r + 1, c)] == (int)CellState::Burning);

                    out = (left || right || up || down) ? (int)CellState::Burning : (int)CellState::Alive;
                } else if (s == (int)CellState::Burning) {
                    out = (int)CellState::Dead;
                } // Empty/Dead remain unchanged

                nxt_[idx(r, c)] = out;

                if (out == (int)CellState::Burning) {
                    next_burning_local = 1;
                    if (global_r == N_ - 1) next_reached_local = 1;
                }
            }
        }

        // Ghost rows of nxt_ not needed; just keep them clean
        std::fill(nxt_.begin(), nxt_.begin() + N_, (int)CellState::Empty);
        std::fill(nxt_.end() - N_, nxt_.end(), (int)CellState::Empty);

        // Global reductions for loop condition + reach-bottom
        int next_burning_global = 0;
        MPI_Allreduce(&next_burning_local, &next_burning_global, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);

        int next_reached_global = 0;
        MPI_Allreduce(&next_reached_local, &next_reached_global, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);

        if (next_reached_global) res.reached_bottom = 1;

        // Advance time
        res.steps += 1;
        std::swap(cur_, nxt_);

        burning_global = next_burning_global;
    }

    return res;
}
