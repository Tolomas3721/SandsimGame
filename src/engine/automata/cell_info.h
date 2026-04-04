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
    std::vector<CellType> load_cell_types(std::string path){
        if(!path.ends_with(".csv")){
            // LOG
            // wrong format, though we could still try to parse it
            return {};
        }

        std::ifstream file(path);
        if(!file.is_open()){
            // LOG("could not open file :(")
            return {};
        }

        // parse the headers
        std::string line;
        std::getline(file, line);
        std::stringstream line_stream(std::move(line));
        std::string field;
        std::vector<std::string> headers;

        while(std::getline(line_stream, field, ',')) {
            headers.push_back(field);
        }

        // parse the csv file
        std::vector<CellType> cell_types;

        while(std::getline(file, line)){
            line_stream.str(std::move(line));
            std::unordered_map<std::string, std::string> mappings;
            // store default values
            CellType cell = {};

            auto current_header = headers.begin();
            while(std::getline(line_stream, field, ',') && current_header < headers.end()){
                mappings.insert(std::make_pair(*current_header, field));
                current_header++;
            }

            auto it = mappings.find("density");
            if(it != mappings.end()) cell.density = std::atof(it->second.c_str());
            
            it = mappings.find("flammability");
            if(it != mappings.end()) cell.flammability = std::atof(it->second.c_str());
            
            it = mappings.find("conductivity");
            if(it != mappings.end()) cell.conductivity = std::atof(it->second.c_str());
            
            it = mappings.find("name");
            if(it != mappings.end()) cell.name = it->second;

            it = mappings.find("type");
            if(it != mappings.end()) cell.type = std::stoul(it->second.c_str());

            cell_types.push_back(cell);
        }
        
        file.close();
        // make sure their id is in order
        std::sort(cell_types.begin(), cell_types.end(), [](CellType a, CellType b){
            return a.type < b.type;
        });
        for(int i = 1; i < cell_types.size(); i++){
            if(cell_types[i].type == cell_types[i - 1].type){
                // type cant be doubled, we report the error
                // LOG("error, two cells have the same type, give the cells, la bla bla")
                cell_types.erase(cell_types.begin() + i);
                i--;
            }
        }
        return cell_types;
    }

    std::unordered_map<std::string, unsigned int> make_cell_id_lookup(std::vector<CellType> cell_types){
        std::unordered_map<std::string, unsigned int> mappings(cell_types.size());

        for(auto& cell : cell_types){
            mappings.insert({cell.name, cell.type});
        }

        return mappings;
    }



    enum class MainType : std::uint32_t {
        GAS = 0,
        LIQUID = 1,
        POWDER = 2,
        SOLID = 3
    };

    // here for some subtypes, but most are defined at runtime
    // these go per main type, so multiple can have same value as long as they are not the same main type
    enum class SubType {
        AIR = 0,
        WATER = 0,
        SAND = 0,
        ROCK = 0
    };
}
