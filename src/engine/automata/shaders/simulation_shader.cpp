#include "simulation_shader.h"

#include <ostream>
#include <filesystem>
#include <fstream>

#include "prediction_map_maker.h"



SimulationShader::SimulationShader(const std::vector<CellType> &cell_types, const std::string& dir){
    hashed_value = hash(cell_types);
    compile(cell_types, dir);
}

void SimulationShader::compile(const std::vector<CellType> &cell_types, const std::string& dir){    
    if(hashed_value == 0){
        hashed_value = hash(cell_types);
    }

    // cant compile twice
    if(program != 0){
        return;
    }

    std::filesystem::path filepath(dir);
    filepath.append(std::to_string(hashed_value));

    program = get_from_previously_compiled(filepath.string());
    
    if(program == 0){
        program = glCreateProgram();
        GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
        std::string code = "";

        code += "#version 430 core\n";
        code += "layout(local_size_x = 256) in;";
        code += make_uniforms_and_buffers();
        code += make_constants();
        code += make_shifts_and_masks();
        code += make_prediction_map();
        code += make_cell_types_array(cell_types);
        code += make_getters();

        const char* c_str_code = code.data();

        glShaderSource(shader, 1, &c_str_code, nullptr);
        glCompileShader(shader);

        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

        if(!success){
            return;
            // TODO: LOG("bitchass error")
            //char log[1024];
            //glGetShaderInfoLog(shader, 1024, nullptr, log);
            //std::cerr << "Compute shader error:\n" << log << std::endl;
        }

        glad_glAttachShader(program, shader);
        glLinkProgram(program);
        glDeleteShader(shader);

        save_shader(filepath.string() + ".bin");
        // also save the source code (for debugging)
        std::ofstream file(filepath.string() + "_code.comp");
        if(file.is_open()){
            file.write(code.c_str(), code.size());
            file.close();
        }
    }
}

// give only the directory
// the name will be the hash
void SimulationShader::save_shader(const std::string& filepath){
    GLint binaryLength = 0;
    glGetProgramiv(program, GL_PROGRAM_BINARY_LENGTH, &binaryLength);

    if(binaryLength <= 0){
        return;
    }

    std::vector<char> binary(binaryLength);
    GLenum format;
    
    glGetProgramBinary(program, binaryLength, NULL, &format, binary.data());

    std::ofstream file(filepath, std::ios::out | std::ios::binary);
    // doesnt really matter if its not saved, it will just be recompiled next time
    if(file.is_open()){
        file.write(reinterpret_cast<const char*>(&format), sizeof(GLenum));
        file.write(binary.data(), binaryLength);
        file.close();
    }
}

bool SimulationShader::is_already_compiled(const std::string& dir){
    for(const auto& file : std::filesystem::directory_iterator(dir)){
        std::string name = file.path().filename().string();
        std::uint64_t name_hash = std::stoull(name);

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

std::string SimulationShader::make_uniforms_and_buffers(){
    return 
    // uniforms
    "layout(location = 0) uniform ivec2 CHUNK_ORIGIN;"
    "layout(location = 1) uniform ivec2 CURRENT_STEP_OFFSET;"
    "layout(location = 2) uniform int RAND_VALUE;"

    "layout(std430, binding = 0) buffer cells_buf { uint cells[]; };"
    "layout(std430, binding = 1) readonly buffer updated_chunks { uint active_subchunks[]; };"
    "layout(std430, binding = 2) writeonly buffer updated_chunks { bool subchunks[]; };"
    ;
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
    constexpr int x_bits = std::bit_width(Chunk::CHUNK_SIZE_X) - 1;
    constexpr int y_bits = x_bits + std::bit_width(Chunk::CHUNK_SIZE_Y) - 1;
    constexpr int chunk_x_bits = y_bits + std::bit_width(SimulationMap::MAP_SIZE_X) - 1;
    // unused
    //constexpr int chunk_y_bits = chunk_x_bits + std::bit_width(SimulationMap::MAP_SIZE_Y) - 1;
    return 
    // some necessary constants
    "const uint x_bits = " + std::to_string(x_bits) + ";"
    "const uint y_bits = " + std::to_string(y_bits) + ";"
    "const uint chunk_x_bits = " + std::to_string(y_bits) + ";"
    // cell data
    "uint get_main_type(uint cell){return (cell >> MAIN_TYPE_SHIFT) & MAIN_TYPE_MASK;}"
    "uint get_subtype(uint cell){return (cell >> SUBTYPE_SHIFT) & SUBTYPE_MASK;}"
    "uint get_color_id(uint cell){return (cell >> COLOR_SHIFT) & COLOR_MASK;}"
    "uint get_full_type(uint cell){return cell & ((MAIN_TYPE_MASK << MAIN_TYPE_SHIFT) | (SUBTYPE_MASK << SUBTYPE_SHIFT));}"
    "uint get_full_color_id(uint cell){return cell & ((COLOR_MASK << COLOR_SHIFT) | (SUBTYPE_MASK << SUBTYPE_SHIFT) | (MAIN_TYPE_MASK << MAIN_TYPE_SHIFT));}"
    // celltype data
    "float get_density(uint full_type){return cell_types[full_type].density;}"
    "float get_flammability(uint full_type){return cell_types[full_type].flammability;}"
    "float get_conductivity(uint full_type){return cell_types[full_type].conductivity;}"
    // cell position
    "uint get_cell(uint pos){ return cells[pos];}"
    "uint get_pos(uint x, uint y, uint chunk_x, uint chunk_y){ return (x << 0) + (y << x_bits) + (chunk_x << y_bits) + (chunk_y << chunk_x_bits);}"
    // subchunks
    "void set_subchunk_active(uint pos){"
        "uint subpos_x = pos & (CHUNK_SIZE_X - 1);"
        "subpos_x /= SUBCHUNK_SIZE_X;"
        "uint subpos_y = (pos >> x_bits) & (CHUNK_SIZE_Y - 1);"
        "subpos_y /= SUBCHUNK_SIZE_Y;"
        "pos >>= chunk_x_bits;"
        "subchunks[subpos_x + (subpos_y * SUBCHUNK_SIZE_X) + pos * SUBCHUNK_SIZE_Y * SUBCHUNK_SIZE_X] = true;"
    "}"
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
    auto map = PredictionMapMaker::generate();

    std::string res = "const uint[] TYPE_MAP_PREDICTION = uint[](";
    for(const std::uint32_t moves : map){
        res += std::to_string(moves) + ',';
    }
    // remove the last comma
    res.pop_back();
    res += ");";

    return res;
}

std::string SimulationShader::make_main_code(){
    return 
    R"(
    uint rand() {
        uint state = gl_GlobalInvocationID.x * 1973u * RAND_VALUE;
        state ^= (state << 13);
        state ^= (state >> 17);
        state ^= (state << 5);
        return state & 3;
    }

    void main() {
        uint subchunk_id = gl_GlobalInvocationID.x / TILES_PER_SUBCHUNK;
        uint subchunk = active_subchunks[subchunk_id];
        uint chunk_x = subchunk / (CHUNK_SIZE_X / SUBCHUNK_SIZE_X);
        uint chunk_y = subchunk / (CHUNK_SIZE_Y / SUBCHUNK_SIZE_Y);
        uint tile = gl_GlobalInvocationID.x % TILES_PER_SUBCHUNK;
        uint tile_x = 2 * (tile % SUBCHUNK_SIZE_X);
        uint tile_y = 2 * (tile / SUBCHUNK_SIZE_X);

        uint pos = tile_x | (tile_y << x_bits) | (chunk_x << y_bits) | (chunk_y << chunk_x_bits);

        uint cells[4] = uint[](
            cells[pos + (1 << x_bits)],
            cells[pos + (1 << x_bits) + 1],
            cells[pos],
            cells[pos + 1]
        );

        uint index = (get_main_type(cells[0]) << 6) 
                | (get_main_type(cells[1]) << 4)
                | (get_main_type(cells[2]) << 2)
                | (get_main_type(cells[3]));

        uint moves = TYPE_MAP_PREDICTION[index];
        uint random_shift = 8 * rand();
        moves = (moves >> random_shift) & 0xFF;

        const uint NO_MOVE = (0 << 6) | (1 << 4) | (2 << 2) | 3;

        if(moves == NO_MOVE) return;

        for(int i = 0; i < 4; i++){
            uint where = (moves >> (2 * (3-i))) & 3;
            if(where != i){
                write_cell(cells[i], pos + OFFSETS[smol_offsets[where]]);
                set_subchunk_active(pos + OFFSETS[smol_offsets[where]]);
            }
        }
    })";
}

std::string SimulationShader::make_perfect_mixing_hashmap(){

}
