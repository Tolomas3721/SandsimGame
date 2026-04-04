#pragma once

#include <array>
#include <bit>
#include <mdspan>
#include <span>
#include <stdexcept>

#include "cell_info.h"
#include "cell.h"



class Chunk {
public:
    // must all be powers of 2
    static constexpr unsigned int CELL_X = 256;
    static_assert(std::has_single_bit(CELL_X), "CELL_X in Chunk class is not power of 2");
    static constexpr unsigned int CELL_Y = 256;
    static_assert(std::has_single_bit(CELL_X), "CELL_Y in Chunk class is not power of 2");

    static constexpr unsigned int SUBCHUNK_X = 8;
    static_assert(CELL_X % SUBCHUNK_X == 0 && SUBCHUNK_X <= CELL_X, "SUBCHUNK_X in Chunk class has an invalid value");
    static constexpr unsigned int SUBCHUNK_Y = 8;
    static_assert(CELL_Y % SUBCHUNK_Y == 0 && SUBCHUNK_Y <= CELL_Y, "SUBCHUNK_Y in Chunk class has an invalid value");

    
    inline Cell& get_cell(unsigned int x, unsigned int y){
        if(x >= CELL_X || y >= CELL_X) throw std::out_of_range("cell position is out of range");
        return cells[y][x];
    }

    // doesnt do bounds check, fuck you
    inline Cell& operator[](size_t x, size_t y){
        return cells[y][x];
    }

    inline std::mdspan<Cell, std::extents<size_t, SUBCHUNK_X, SUBCHUNK_Y>> get_subchunk(unsigned int x, unsigned int y){
        return std::mdspan<Cell, std::extents<size_t, SUBCHUNK_X, SUBCHUNK_Y>>(&cells[y * SUBCHUNK_Y][x * SUBCHUNK_X]);
    }

private:
    // use X as the minor for better cache access
    // but its all 2x2 anyway,unless maybe liquids will move sideways more
    std::array<std::array<Cell, CELL_X>, CELL_Y> cells;
};
