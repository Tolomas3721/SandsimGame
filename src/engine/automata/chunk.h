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
    static constexpr size_t CHUNK_SIZE_X = 256;
    static_assert(std::has_single_bit(CHUNK_SIZE_X), "CHUNK_SIZE_X in Chunk class is not power of 2");
    static constexpr size_t CHUNK_SIZE_Y = 256;
    static_assert(std::has_single_bit(CHUNK_SIZE_Y), "CHUNK_SIZE_Y in Chunk class is not power of 2");

    static constexpr size_t SUBCHUNK_SIZE_X = 8;
    static_assert(CHUNK_SIZE_X % SUBCHUNK_SIZE_X == 0 && SUBCHUNK_SIZE_X <= CHUNK_SIZE_X, "SUBCHUNK_X in Chunk class has an invalid value");
    static constexpr size_t SUBCHUNK_SIZE_Y = 8;
    static_assert(CHUNK_SIZE_Y % SUBCHUNK_SIZE_Y == 0 && SUBCHUNK_SIZE_Y <= CHUNK_SIZE_Y, "SUBCHUNK_Y in Chunk class has an invalid value");

    
    inline Cell& get_cell(size_t x, size_t y){
        if(x >= CHUNK_SIZE_X || y >= CHUNK_SIZE_X) throw std::out_of_range("cell position is out of range");
        return cells[y][x];
    }

    // doesnt do bounds check, fuck you
    inline Cell& operator[](size_t x, size_t y){
        return cells[y][x];
    }

    inline std::mdspan<Cell, std::extents<size_t, SUBCHUNK_SIZE_X, SUBCHUNK_SIZE_Y>> get_subchunk(size_t x, size_t y){
        return std::mdspan<Cell, std::extents<size_t, SUBCHUNK_SIZE_X, SUBCHUNK_SIZE_Y>>(
            &cells[y * SUBCHUNK_SIZE_Y][x * SUBCHUNK_SIZE_X]
        );
    }

private:
    // use X as the minor for better cache access
    // but its all 2x2 anyway,unless maybe liquids will move sideways more
    std::array<std::array<Cell, CHUNK_SIZE_X>, CHUNK_SIZE_Y> cells;
};
