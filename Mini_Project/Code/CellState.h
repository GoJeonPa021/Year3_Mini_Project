#pragma once
#include <cstdint>

enum class CellState : std::int32_t {
    Empty   = 0,
    Alive   = 1,
    Burning = 2,
    Dead    = 3
};

inline bool is_valid_state(int v) {
    return v == 0 || v == 1 || v == 2 || v == 3;
}
