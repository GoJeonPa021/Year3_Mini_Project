#pragma once
#include <string>
#include <vector>

struct GlobalGrid {
    int N = 0;                 // grid is N x N
    std::vector<int> data;     
};

// Root reads the whole file into GlobalGrid
// Non-root ranks call with empty filename
GlobalGrid read_grid_text_root_only(const std::string& filename, int mpi_rank);

// Scatter a global grid to local blocks of rows on all ranks.
// Each rank receives local_rows*N ints into local_no_ghost
void scatter_grid_rows(
    const std::vector<int>& global, int N,
    std::vector<int>& local_no_ghost,
    int& local_rows, int& start_row,
    int mpi_rank, int mpi_size
);
