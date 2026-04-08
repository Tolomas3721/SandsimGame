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
    SimulationShader(const std::vector<CellType> &cell_types);
    void compile();

    // give only the directory
    // the name will be the hash
    void save_shader(const std::string& dir);

private:
    bool is_already_compiled();
    void get_from_previously_compiled(const std::string& dir);
    std::uint64_t hash_shader();

    // these should get their params from public stuff
    // or from a config.ini file
    std::string make_constants();
    std::string make_shifts_and_masks();
    std::string make_getters();
    std::string make_cell_types_array();

    std::string make_prediction_map();
};
