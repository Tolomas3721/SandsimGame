#pragma once

#include <array>
#include <bit>
#include <memory>
#include <stdexcept>

#include "cell.h"
#include "chunk.h"



class SimulationMap {
public:
    SimulationMap(){ chunks = std::make_unique<std::array<std::array<Chunk, MAP_SIZE_X>, MAP_SIZE_Y>>(); }
    
    inline Cell& get_cell(size_t x, size_t y){
        // error handling is done in there, buddy
        return get_cell(x / Chunk::CHUNK_SIZE_X, y / Chunk::CHUNK_SIZE_Y, x % Chunk::CHUNK_SIZE_X, y % Chunk::CHUNK_SIZE_Y);
    }

    inline Cell& get_cell(size_t chunk_x, size_t chunk_y, size_t x, size_t y){
        if(chunk_x >= MAP_SIZE_X || chunk_y >= MAP_SIZE_Y) throw std::out_of_range("chunk position is out of range");
        return (*chunks)[chunk_y][chunk_x].get_cell(x, y);
    }

    inline Cell& operator[](size_t chunk_x, size_t chunk_y, size_t x, size_t y){
        return (*chunks)[chunk_y][chunk_x][x, y];
    }

    inline constexpr size_t size(){ return MAP_SIZE_X * MAP_SIZE_Y * Chunk::CHUNK_SIZE_X * Chunk::CHUNK_SIZE_Y; }

    inline Cell* data(){ return reinterpret_cast<Cell*>(chunks.get()); }

    static constexpr size_t MAP_SIZE_X = 8;
    static_assert(std::has_single_bit(MAP_SIZE_X) && MAP_SIZE_X > 0, "MAP_SIZE_X in SimulationMap class is not power of 2");
    static constexpr size_t MAP_SIZE_Y = 8;
    static_assert(std::has_single_bit(MAP_SIZE_Y) && MAP_SIZE_Y > 0, "MAP_SIZE_Y in SimulationMap class is not power of 2");

private:
    std::unique_ptr<std::array<std::array<Chunk, MAP_SIZE_X>, MAP_SIZE_Y>> chunks;
};
