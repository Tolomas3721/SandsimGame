#pragma once

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <unordered_map>
#include <vector>



// stores data shared between all cells of the same type
struct CellType {
public:
    std::uint32_t type;
    float density;
    float flammability;
    float conductivity;
    std::string name;

    CellType(){
        type = DEFAULT_TYPE;
        density = DEFAULT_DENSITY;
        flammability = DEFAULT_FLAMMABILITY;
        conductivity = DEFAULT_CONDUCTIVITY;
        name = DEFAULT_NAME;
    }

private:
    static constexpr float DEFAULT_DENSITY = 1.0;
    static constexpr float DEFAULT_FLAMMABILITY = 0.0;
    static constexpr float DEFAULT_CONDUCTIVITY = 0.0;
    // compiler wont allow std::string here, works the same anyway
    static constexpr const char* DEFAULT_NAME = "Bagel bag";
    // could be any, doesnt matter
    // if we still have this, then we have an issue anyway lol
    static constexpr std::uint32_t DEFAULT_TYPE = 0;
};



namespace CellInfo {

    // returns an array sorted by ascending type_id of all the cell types, or empty in case of errors
    std::vector<CellType> load_cell_types(const std::string& path);

    std::unordered_map<std::string, unsigned int> make_cell_id_lookup(const std::vector<CellType>& cell_types);

    enum MainType : std::uint32_t {
        GAS = 0,
        LIQUID = 1,
        POWDER = 2,
        SOLID = 3
    };

    // here for some subtypes, but most are defined at runtime
    // these go per main type, so multiple can have same value as long as they are not the same main type
    enum SubType {
        AIR = 0,
        WATER = 1,
        SAND = 2,
        ROCK = 3
    };
}
