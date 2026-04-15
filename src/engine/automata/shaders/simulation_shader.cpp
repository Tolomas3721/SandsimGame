#include "simulation_shader.h"

#include <ostream>
#include <filesystem>
#include <fstream>

SimulationShader::SimulationShader(const std::vector<CellType> &cell_types, const std::string& dir){
    hashed_value = hash(cell_types);
}

void SimulationShader::compile(const std::string& dir){    
    char hash_name[8];

    std::memcpy(&hash_name, &hashed_value, 8);
    GLuint program = get_from_previously_compiled(dir + '/' + hash_name);
    
    if(program == 0){
        program = glCreateProgram();
    }
}

// give only the directory
// the name will be the hash
void SimulationShader::save_shader(const std::string& dir){
    
}

bool SimulationShader::is_already_compiled(const std::string& dir){
    for(const auto& file : std::filesystem::directory_iterator(dir)){
        std::string name = file.path().filename().string();
        std::uint64_t name_hash = 0;

        std::memcpy(&name_hash, name.data(), 8);

        if(name_hash == hashed_value) return true;
    }

    return false;
}

GLuint SimulationShader::get_from_previously_compiled(const std::string& filename){
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if(!file.is_open()) return 0;

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    GLenum format;
    file.read(reinterpret_cast<char*>(&format), sizeof(format));
    
    std::vector<char> buffer(size - sizeof(format));
    file.read(buffer.data(), buffer.size());

    GLuint program = glCreateProgram();
    glProgramBinary(program, format, buffer.data(), static_cast<GLsizei>(buffer.size()));

    // verify in case pilots changed and we need to recompile
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if(!success){
        glDeleteProgram(program);
        return 0;
    }

    return program;
}

std::uint64_t SimulationShader::hash(const std::vector<CellType>& cell_types){
    // start with the simple stuff, this should be good enough to avoid collisions
    // (its just some bullshit running on hopes and dreams)
    std::uint64_t hashed = Chunk::CHUNK_SIZE_X + (Chunk::CHUNK_SIZE_Y << Cell::SHIFTS::MAIN_TYPE);
    // more bullshit
    hashed += (Chunk::SUBCHUNK_SIZE_X + (Chunk::SUBCHUNK_SIZE_Y << Cell::SHIFTS::SUBTYPE)) << Cell::SHIFTS::COLOR;
    // sum of masks, still bullshit
    hashed += Cell::MASKS::MAIN_TYPE + Cell::MASKS::SUBTYPE + Cell::MASKS::COLOR;

    // hash the celltypes
    for(const CellType& type : cell_types){
        std::uint64_t hash = std::hash<float>{}(type.density);
        hash ^= hashed + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        
        std::uint64_t hash = std::hash<float>{}(type.flammability);
        hash ^= hashed + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        
        std::uint64_t hash = std::hash<float>{}(type.conductivity);
        hashed ^= hash + 0x9e3779b9 + (hashed << 6) + (hashed >> 2);
    }

    return hashed;
}

// these should get their params from public stuff
// or from a config.ini file
std::string SimulationShader::make_constants(){
    return 
    // chunk related constants
    "const uint CHUNK_SIZE_X = " + std::to_string(Chunk::CHUNK_SIZE_X) + ';' +
    "const uint CHUNK_SIZE_Y = " + std::to_string(Chunk::CHUNK_SIZE_Y) + ';' +
    "const uint SUBCHUNK_SIZE_X = " + std::to_string(Chunk::SUBCHUNK_SIZE_X) + ';' +
    "const uint SUBCHUNK_SIZE_Y = " + std::to_string(Chunk::SUBCHUNK_SIZE_Y) + ';' +
    // cell related constants
    "const uint GAS = " + std::to_string(static_cast<std::uint32_t>(CellInfo::MainType::GAS)) + ';' +
    "const uint LIQUID = " + std::to_string(static_cast<std::uint32_t>(CellInfo::MainType::LIQUID)) + ';' +
    "const uint POWDER = " + std::to_string(static_cast<std::uint32_t>(CellInfo::MainType::POWDER)) + ';' +
    "const uint SOLID = " + std::to_string(static_cast<std::uint32_t>(CellInfo::MainType::SOLID)) + ';'
    ;
}

std::string SimulationShader::make_shifts_and_masks(){
    return
    // masks
    "const uint MAIN_TYPE_MASK = " + std::to_string(static_cast<std::uint32_t>(Cell::MASKS::MAIN_TYPE)) + ';' +
    "const uint SUBTYPE_MASK = " + std::to_string(static_cast<std::uint32_t>(Cell::MASKS::SUBTYPE)) + ';' +
    "const uint COLOR_MASK = " + std::to_string(static_cast<std::uint32_t>(Cell::MASKS::COLOR)) + ';' +
    // shifts
    "const uint MAIN_TYPE_SHIFT = " + std::to_string(static_cast<std::uint32_t>(Cell::SHIFTS::MAIN_TYPE)) + ';' +
    "const uint SUBTYPE_SHIFT = " + std::to_string(static_cast<std::uint32_t>(Cell::SHIFTS::SUBTYPE)) + ';' +
    "const uint COLOR_SHIFT = " + std::to_string(static_cast<std::uint32_t>(Cell::SHIFTS::COLOR)) + ';'
    ;
}

std::string SimulationShader::make_getters(){
    return 
    // cell data
    "uint get_main_type(uint cell){return (cell >> MAIN_TYPE_SHIFT) & MAIN_TYPE_MASK;}"
    "uint get_subtype(uint cell){return (cell >> SUBTYPE_SHIFT) & SUBTYPE_MASK;}"
    "uint get_color_id(uint cell){return (cell >> COLOR_SHIFT) & COLOR_MASK;}"
    "uint get_full_type(uint cell){return cell & ((MASKS::MAIN_TYPE << SHIFTS::MAIN_TYPE) | (MASKS::SUBTYPE << SHIFTS::SUBTYPE));}"
    "uint get_full_color_id(uint cell){return cell & ((MASKS::COLOR << SHIFTS::COLOR) | (MASKS::SUBTYPE << SHIFTS::SUBTYPE) | (MASKS::MAIN_TYPE << SHIFTS::MAIN_TYPE));}"
    // celltype data
    "float get_density(uint full_type){return cell_types[full_type].density;}"
    "float get_flammability(uint full_type){return cell_types[full_type].flammability;}"
    "float get_conductivity(uint full_type){return cell_types[full_type].conductivity;}"
    ;
}

std::string SimulationShader::make_cell_types_array(const std::vector<CellType>& cell_types){
    std::string res = 
        "struct CellType {"
            "density;"
            "flammability;"
            "conductivity;"
        "};"
        "const CellType[] cell_types = CellType[]("
    ;

    res.reserve(cell_types.size() * 32);

    for(const CellType& type : cell_types){
        res += "CellType(";

        // must all be in the same order as they are in the shader's struct shown just above
        res += std::to_string(type.density) + ',';
        res += std::to_string(type.flammability) + ',';
        res += std::to_string(type.conductivity) + ',';

        res += "),";
    }
    // remove the last comma
    res.pop_back();

    res += ");";
}

std::string SimulationShader::make_prediction_map(){
    
}

std::string SimulationShader::make_perfect_mixing_hashmap(){

}
