#include "ForestFire.h"
#include "GridIO.h"
#include <mpi.h>

#include <iostream>
#include <string>
#include <vector>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

struct Args {
    int N = 100;
    double p = 0.6;
    int M = 1;
    std::uint64_t seed = 12345;
    std::string input_file = ""; // if set, read initial grid
};

static void usage(int rank) {
    if (rank != 0) return;
    std::cout <<
R"(Usage:
  mpirun -np <tasks> ./forest_fire [--N N] [--p p] [--M M] [--seed s] [--input file]

Notes:
  - If --input is provided, N is inferred from the file (must be square).
  - States: 0 empty, 1 alive, 2 burning, 3 dead.
  - If input contains only 0/1 (common), top row trees are ignited automatically.

Examples:
  mpirun -np 4 ./forest_fire --N 100 --p 0.6 --M 50 --seed 42
  mpirun -np 8 ./forest_fire --input input_grid.txt --M 1
)";
}

static Args parse_args(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        std::string key = argv[i];
        auto need = [&](const char* name) {
            if (i + 1 >= argc) throw std::runtime_error(std::string("Missing value for ") + name);
        };

        if (key == "--N") { need("--N"); a.N = std::stoi(argv[++i]); }
        else if (key == "--p") { need("--p"); a.p = std::stod(argv[++i]); }
        else if (key == "--M") { need("--M"); a.M = std::stoi(argv[++i]); }
        else if (key == "--seed") { need("--seed"); a.seed = (std::uint64_t)std::stoull(argv[++i]); }
        else if (key == "--input") { need("--input"); a.input_file = argv[++i]; }
        else if (key == "--help" || key == "-h") { /* handled in main */ }
        else throw std::runtime_error("Unknown argument: " + key);
    }
    return a;
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank = 0, size = 1;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    try {
        for (int i = 1; i < argc; ++i) {
            std::string k = argv[i];
            if (k == "--help" || k == "-h") { usage(rank); MPI_Finalize(); return 0; }
        }

        Args args = parse_args(argc, argv);
        if (args.M <= 0) throw std::runtime_error("M must be positive.");
        if (args.p < 0.0 || args.p > 1.0) throw std::runtime_error("p must be in [0,1].");

        int N = args.N;
        std::vector<int> global_grid; // only valid on root if using input

        if (!args.input_file.empty()) {
            // Root reads, broadcasts N
            GlobalGrid gg = read_grid_text_root_only(args.input_file, rank);
            if (rank == 0) N = gg.N;

            MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);

            if (rank == 0) global_grid = std::move(gg.data);
        } else {
            MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);
        }

        // Scatter input grid rows if needed
        int local_rows = 0, start_row = 0;
        std::vector<int> local_no_ghost;

        if (!args.input_file.empty()) {
            scatter_grid_rows(global_grid, N, local_no_ghost, local_rows, start_row, rank, size);
        } else {
            // For random init, we still want local_rows/start_row consistent with ForestFireMPI
            const int base = N / size;
            const int rem  = N % size;
            local_rows = base + (rank < rem ? 1 : 0);
            start_row  = rank * base + (rank < rem ? rank : rem);
        }

        ForestFireMPI sim(N, rank, size);

        // Timing
        MPI_Barrier(MPI_COMM_WORLD);
        const double t0 = MPI_Wtime();

        long long sum_steps = 0;
        long long sum_reach = 0;

        for (int run = 0; run < args.M; ++run) {
            if (!args.input_file.empty()) {
                // Same initial grid each time
                sim.init_from_local_rows(local_no_ghost);
            } else {
                // Different seed per run
                sim.init_random(args.p, args.seed + (std::uint64_t)run * 1315423911ULL);
            }

            RunResult r = sim.run();
            if (rank == 0) {
                sum_steps += r.steps;
                sum_reach += r.reached_bottom;
            }
        }

        MPI_Barrier(MPI_COMM_WORLD);
        const double t1 = MPI_Wtime();

        if (rank == 0) {
            const double total_time = t1 - t0;
            const double avg_steps  = (double)sum_steps / (double)args.M;
            const double prob_reach = (double)sum_reach / (double)args.M;

            // Simple, script-friendly output (CSV-like)
            std::cout
                << "N," << N
                << ",p," << args.p
                << ",M," << args.M
                << ",tasks," << size
                << ",avg_steps," << avg_steps
                << ",prob_reach_bottom," << prob_reach
                << ",total_time_s," << total_time
                << ",avg_time_per_run_s," << (total_time / (double)args.M)
                << "\n";
        }

    } catch (const std::exception& e) {
        if (rank == 0) {
            std::cerr << "ERROR: " << e.what() << "\n\n";
            usage(rank);
        }
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    MPI_Finalize();
    return 0;
}
