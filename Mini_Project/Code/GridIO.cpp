#include "GridIO.h"
#include "CellState.h"
#include <mpi.h>

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <iostream>

static GlobalGrid read_grid_text(const std::string& filename) {
    std::ifstream in(filename);
    if (!in) throw std::runtime_error("Failed to open input grid file: " + filename);

    std::vector<std::vector<int>> rows;
    std::string line;

    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        std::vector<int> row;
        int v;
        while (iss >> v) {
            if (!is_valid_state(v)) {
                throw std::runtime_error("Input grid contains invalid state (must be 0/1/2/3).");
            }
            row.push_back(v);
        }
        if (!row.empty()) rows.push_back(std::move(row));
    }

    if (rows.empty()) throw std::runtime_error("Input grid file is empty.");

    const int N = static_cast<int>(rows.size());
    for (const auto& r : rows) {
        if ((int)r.size() != N) {
            throw std::runtime_error("Input grid must be square (N rows, N columns).");
        }
    }

    GlobalGrid gg;
    gg.N = N;
    gg.data.resize((size_t)N * (size_t)N);
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            gg.data[(size_t)i * N + j] = rows[i][j];
        }
    }
    return gg;
}

GlobalGrid read_grid_text_root_only(const std::string& filename, int mpi_rank) {
    GlobalGrid gg;
    if (mpi_rank == 0) {
        gg = read_grid_text(filename);
    }
    return gg;
}

void scatter_grid_rows(
    const std::vector<int>& global, int N,
    std::vector<int>& local_no_ghost,
    int& local_rows, int& start_row,
    int mpi_rank, int mpi_size
) {
    // Block row decomposition with remainder.
    const int base = N / mpi_size;
    const int rem  = N % mpi_size;
    local_rows = base + (mpi_rank < rem ? 1 : 0);
    start_row  = mpi_rank * base + (mpi_rank < rem ? mpi_rank : rem);

    std::vector<int> counts(mpi_size), displs(mpi_size);
    int disp = 0;
    for (int r = 0; r < mpi_size; ++r) {
        const int rows_r = base + (r < rem ? 1 : 0);
        counts[r] = rows_r * N;
        displs[r] = disp;
        disp += counts[r];
    }

    local_no_ghost.assign((size_t)local_rows * (size_t)N, 0);

    MPI_Scatterv(
        mpi_rank == 0 ? global.data() : nullptr,
        counts.data(),
        displs.data(),
        MPI_INT,
        local_no_ghost.data(),
        (int)local_no_ghost.size(),
        MPI_INT,
        0,
        MPI_COMM_WORLD
    );
}
