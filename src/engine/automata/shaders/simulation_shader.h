#pragma once

#include "glad.h"
#include "glfw3.h"

#include <array>
#include <cstdint>
#include <vector>

#include "cell.h"
#include "cell_info.h"
#include "simulation_map.h"



class SimulationShader
{
public:
    SimulationShader(const std::vector<CellType> &cell_types, const std::string& dir);
    void compile(const std::string& dir);

    // give only the directory
    // the name will be the hash
    void save_shader(const std::string& dir);

private:
    std::uint64_t hashed_value;

    bool is_already_compiled(const std::string& dir);
    GLuint get_from_previously_compiled(const std::string& filename);
    std::uint64_t hash(const std::vector<CellType>& cell_types);

    // these should get their params from public stuff
    // or from a config.ini file
    constexpr std::string make_constants();
    constexpr std::string make_shifts_and_masks();
    constexpr std::string make_getters();
    // the array MUST be sorted
    constexpr std::string make_cell_types_array(const std::vector<CellType>& cell_types);

    constexpr std::string make_prediction_map();
    constexpr std::string make_perfect_mixing_hashmap();
};
