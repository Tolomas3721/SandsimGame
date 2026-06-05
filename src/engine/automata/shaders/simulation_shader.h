#pragma once

#include "glad.h"
#include "glfw3.h"

#include <array>
#include <cstdint>
#include <vector>

#include "cell.h"
#include "cell_info.h"
#include "simulation_map.h"



class SimulationShader {
public:
    // will not compile anything until compile() is called
    SimulationShader(){ hashed_value = 0; program = 0; };
    // will compile the shader automatically
    SimulationShader(const std::vector<CellType> &cell_types, const std::string& dir);

    void run();
    void compile(const std::vector<CellType> &cell_types, const std::string& dir);

    // give only the directory
    // the name will be the hash
    void save_shader(const std::string& filepath);

    inline void use(){glUseProgram(program);}

    GLuint get_bullshit_TODOREMOVE(){ return active_subchunks;}

private:
    std::uint64_t hashed_value;
    GLuint program;

    GLuint subchunk_compacter;
    GLuint update_counter;

    GLuint active_subchunks;
    GLuint updated_subchunks;
    static constexpr size_t subchunks_amount = SimulationMap::MAP_SIZE_X * SimulationMap::MAP_SIZE_Y * Chunk::CHUNK_SIZE_X / Chunk::SUBCHUNK_SIZE_X * Chunk::CHUNK_SIZE_Y / Chunk::SUBCHUNK_SIZE_Y;

    // this is pretty useless
    // actually, use it for detecting collisions
    bool is_already_compiled(const std::string& dir);
    GLuint get_from_previously_compiled(const std::string& filename);
    std::uint64_t hash(const std::vector<CellType>& cell_types);

    constexpr std::string make_uniforms_and_buffers();
    // these should get their params from public stuff
    // or from a config.ini file
    constexpr std::string make_constants();
    constexpr std::string make_shifts_and_masks();
    constexpr std::string make_getters();
    // the array MUST be sorted
    constexpr std::string make_cell_types_array(const std::vector<CellType>& cell_types);

    constexpr std::string make_prediction_map();
    constexpr std::string make_main_code();
    constexpr std::string make_perfect_mixing_hashmap();
};
