#pragma once
#include <vector>
#include <cstdint>

struct RunResult {
    int steps = 0;          // number of time steps executed until no burning
    int reached_bottom = 0; // 1 if fire ever reaches bottom row, else 0
};

class ForestFireMPI {
public:
    ForestFireMPI(int N, int mpi_rank, int mpi_size);

    // Init from scattered local rows (size local_rows*N), values 0/1/2/3
    void init_from_local_rows(const std::vector<int>& local_rows_no_ghost);

    // Random init for the local block. (Top row burning handled properly.)
    void init_random(double p, std::uint64_t seed);

    // Run until no burning. Returns global-consistent steps + reached_bottom.
    RunResult run();

    int N() const { return N_; }
    int local_rows() const { return local_rows_; }
    int start_row() const { return start_row_; }

private:
    int N_;
    int rank_;
    int size_;

    int local_rows_;
    int start_row_;

    // current/next grids include 2 ghost rows: [0] and [local_rows_+1]
    std::vector<int> cur_;
    std::vector<int> nxt_;

    inline int idx(int r, int c) const { return r * N_ + c; }

    void apply_top_row_burning();
    void exchange_halos();
    int  has_burning_local() const;
};
