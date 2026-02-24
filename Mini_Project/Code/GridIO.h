#pragma once
#include <string>
#include <vector>

struct GlobalGrid {
    int N = 0;                 // grid is N x N
    std::vector<int> data;     // row-major, size N*N
};

// Root reads the whole file into GlobalGrid, infers N.
// Non-root ranks call with empty filename; they’ll just get N broadcast later.
GlobalGrid read_grid_text_root_only(const std::string& filename, int mpi_rank);

// Scatter a global grid (root) to local blocks of rows on all ranks.
// Each rank receives local_rows*N ints into local_no_ghost (size local_rows*N).
void scatter_grid_rows(
    const std::vector<int>& global, int N,
    std::vector<int>& local_no_ghost,
    int& local_rows, int& start_row,
    int mpi_rank, int mpi_size
);
