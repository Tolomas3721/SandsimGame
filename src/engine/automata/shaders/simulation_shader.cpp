#include "simulation_shader.h"

#include <array>
#include <ostream>
#include <filesystem>
#include <fstream>
#include <memory>
#include <numeric>
#include <random>
#include <unordered_set>

#include "prediction_map_maker.h"

#include <chrono>

SimulationShader::SimulationShader(const std::vector<CellType> &cell_types, const std::string& dir){
    program = 0;
    hashed_value = hash(cell_types);
    compile(cell_types, dir);
}

void SimulationShader::run(){
    //glFinish();
    auto start = std::chrono::high_resolution_clock::now();

    glUseProgram(program);

    constexpr std::array<std::pair<int, int>, 4> offsets = {{
        {1, -1}, {0, 0}, {-1, 1}, {0, 0}
        //{2, 1}, {-1, 0}, {-1, -1}, {0, -2}
        //{0, 0}, {1, -1}
    }};
    
    const GLuint rand_value_loc = 1;
    static std::uint32_t frame = 1;
    glUniform1ui(rand_value_loc, frame);
    
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, active_subchunks);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, updated_subchunks);
    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, update_counter);
    for(size_t i = 0; i < offsets.size(); i++){
        const GLuint offset_loc = 0;
        glUniform2i(offset_loc, offsets[i].first, offsets[i].second);
        glDispatchComputeIndirect(0);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);// | GL_BUFFER_UPDATE_BARRIER_BIT);
    }

    //glFinish();
    auto start2 = std::chrono::high_resolution_clock::now();

    glUseProgram(subchunk_compacter);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, update_counter);

    constexpr std::uint32_t reset[3] = {0, 1, 1};
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, update_counter);
    constexpr uint32_t zero = 0;
    glClearBufferSubData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, 0, sizeof(uint32_t), GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);
    //glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(reset), reset);
    glMemoryBarrier(GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
    
    glUniform1ui(0, frame);
    constexpr size_t interior_subchunks_amount = (SimulationMap::MAP_SIZE_X * Chunk::CHUNK_SIZE_X / Chunk::SUBCHUNK_SIZE_X - 2) 
                                        * (Chunk::CHUNK_SIZE_Y / Chunk::SUBCHUNK_SIZE_Y * SimulationMap::MAP_SIZE_Y - 2);
    glDispatchCompute((interior_subchunks_amount + 255) / 256, 1, 1);
    glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);
    frame++;
    //glFinish();

    static int step = -5;
    step++;
    step %= 256;
    auto end = std::chrono::high_resolution_clock::now();
    if(step <= 0){
        //std::cout << "dispatch: " << std::chrono::duration_cast<std::chrono::microseconds>(start2 - start).count() << " us\n";
        //std::cout << "compaction: " << std::chrono::duration_cast<std::chrono::microseconds>(end - start2).count() << " us\n";
    }
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
    // yes, we need to check again, stfu
    if(program != 0){
        std::cout << "program already exists\n";
        return;
    }

    // make the compacter

    std::string code = R"(
        #version 460 core
        layout(local_size_x = 256) in;
        layout(location = 0) uniform uint frame;

        layout(std430, binding = 0) buffer counter_buf { uvec3 counter; };
        layout(std430, binding = 1) writeonly buffer active_chunks { uint active_subchunks[]; };
        layout(std430, binding = 2) readonly buffer updated_chunks { uint subchunks[]; };
        const uint x_max = )" + std::to_string(SimulationMap::MAP_SIZE_X * Chunk::CHUNK_SIZE_X / Chunk::SUBCHUNK_SIZE_X) + R"(;
        
        shared uint buff_needed;
        void main(){
            uint interior_x = gl_GlobalInvocationID.x % (x_max - 2) + 1;
            uint interior_y = gl_GlobalInvocationID.x / (x_max - 2) + 1;
            uint subchunk_id = interior_y * x_max + interior_x;

            uint id = 0;
            bool is_active = subchunks[subchunk_id] == frame;
            if(gl_LocalInvocationID.x == 0){
                buff_needed = 0;
            }
            memoryBarrierShared();
            barrier();

            
            if(is_active){
                id = atomicAdd(buff_needed, 1);
            }
            barrier();

            if(gl_LocalInvocationID.x == 0 && buff_needed != 0){
                buff_needed = atomicAdd(counter.x, buff_needed);
            }
            memoryBarrierShared();
            barrier();

            if(is_active){
                active_subchunks[buff_needed + id] = subchunk_id;
            }
        }
    )";

    subchunk_compacter = glCreateProgram();
    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);

    char* c_str_code = code.data();
    glShaderSource(shader, 1, &c_str_code, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if(!success){
        std::cout << "failed to compile compacter\n";
        char log[1024];
        glGetShaderInfoLog(shader, 1024, nullptr, log);
        std::cerr << "Compacter shader error:\n" << log << std::endl;
        return;
        // TODO: LOG("bitchass error")
        //char log[1024];
        //glGetShaderInfoLog(shader, 1024, nullptr, log);
        //std::cerr << "Compute shader error:\n" << log << std::endl;
    }

    glAttachShader(subchunk_compacter, shader);
    glLinkProgram(subchunk_compacter);
    glDeleteShader(shader);

    glUseProgram(subchunk_compacter);

    std::array<uint32_t, subchunks_amount> base_active_subchunks;
    const size_t border_x = SimulationMap::MAP_SIZE_X * Chunk::CHUNK_SIZE_X / Chunk::SUBCHUNK_SIZE_X;
    const size_t border_y = SimulationMap::MAP_SIZE_Y * Chunk::CHUNK_SIZE_Y / Chunk::SUBCHUNK_SIZE_Y;
    for(size_t i = 0; i < subchunks_amount; i++){
        if(i % border_x == 0 || i % border_x == border_x - 1
        || i % border_y == 0 || i % border_y == border_y - 1){
            base_active_subchunks[i] = 0;
        } else {
            base_active_subchunks[i] = i;
        }
    }
    glGenBuffers(1, &active_subchunks);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, active_subchunks);
    glBufferData(GL_SHADER_STORAGE_BUFFER, subchunks_amount*sizeof(uint32_t), base_active_subchunks.data(), GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, active_subchunks);
    
    
    glGenBuffers(1, &updated_subchunks);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, updated_subchunks);
    glBufferData(GL_SHADER_STORAGE_BUFFER, subchunks_amount*sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, updated_subchunks);

    // counter, wow
    glGenBuffers(1, &update_counter);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, update_counter);
    std::uint32_t dispatch[3] = {subchunks_amount - 2 * (border_x + border_y - 2), 1, 1};
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(dispatch), dispatch, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, update_counter);

    // make the actual sim shader

    program = glCreateProgram();
    shader = glCreateShader(GL_COMPUTE_SHADER);
    code = "";

    code += "#version 460 core\n";
    const unsigned int tiles_per_subchunk = Chunk::SUBCHUNK_SIZE_X / 2 * Chunk::SUBCHUNK_SIZE_Y / 2;
    code += "layout(local_size_x = " + std::to_string(tiles_per_subchunk) + ") in;";
    code += make_constants();
    code += make_uniforms_and_buffers();
    code += make_shifts_and_masks();
    code += make_prediction_map();
    code += make_cell_types_array(cell_types);
    code += make_getters();
    code += make_main_code();

    c_str_code = code.data();

    glShaderSource(shader, 1, &c_str_code, nullptr);
    glCompileShader(shader);

    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    // also save the source code (for debugging)
    std::ofstream file(filepath.string() + "_code.comp");
    if(file.is_open()){
        file.write(code.c_str(), code.size());
        file.close();
    }
    if(!success){
        std::cout << "failed to compile simulation\n";
        char log[1024];
        glGetShaderInfoLog(shader, 1024, nullptr, log);
        std::cerr << "Simulation shader error:\n" << log << std::endl;
        return;
        // TODO: LOG("bitchass error")
        //char log[1024];
        //glGetShaderInfoLog(shader, 1024, nullptr, log);
        //std::cerr << "Compute shader error:\n" << log << std::endl;
    }
    
    
    
    glAttachShader(program, shader);
    glLinkProgram(program);
    glDeleteShader(shader);

    glUseProgram(program);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, active_subchunks);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, updated_subchunks);

    save_shader(filepath.string() + ".bin");
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
    hashed += Cell::MASKS::MAIN_TYPE + Cell::MASKS::SUBTYPE * Cell::MASKS::COLOR;

    // hash the celltypes
    for(const CellType& type : cell_types){
        std::uint64_t hash = std::hash<float>{}(type.density);
        hash ^= hashed + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        
        hash = std::hash<float>{}(type.flammability);
        hash ^= hashed + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        
        hash = std::hash<float>{}(type.conductivity);
        hashed ^= hash + 0x9e3779b9 + (hashed << 6) + (hashed >> 2);
    }

    return hashed;
}

constexpr std::string SimulationShader::make_uniforms_and_buffers(){
    return
    // uniforms
    "layout(location = 0) uniform ivec2 CURRENT_STEP_OFFSET;\n"
    "layout(location = 1) uniform uint RAND_VALUE;\n"

    // buffers
    "struct Chunk {\n"
    "   uint cells[CHUNK_SIZE_X * CHUNK_SIZE_Y];\n"
    "};\n"
    "layout(std430, binding = 0) buffer cells_buf { Chunk cells[]; };\n"
    "layout(std430, binding = 1) readonly buffer active_chunks { uint active_subchunks[]; };\n"
    "layout(std430, binding = 2) writeonly buffer updated_chunks { uint subchunks[]; };\n"
    ;
}

// these should get their params from public stuff
// or from a config.ini file
constexpr std::string SimulationShader::make_constants(){
    return 
    // chunk related constants
    "const uint CHUNK_SIZE_X = " + std::to_string(Chunk::CHUNK_SIZE_X) + ";\n" +
    "const uint CHUNK_SIZE_Y = " + std::to_string(Chunk::CHUNK_SIZE_Y) + ";\n" +
    "const uint SUBCHUNK_SIZE_X = " + std::to_string(Chunk::SUBCHUNK_SIZE_X) + ";\n" +
    "const uint SUBCHUNK_SIZE_Y = " + std::to_string(Chunk::SUBCHUNK_SIZE_Y) + ";\n" +
    "const uint TILES_PER_SUBCHUNK = " + std::to_string(Chunk::SUBCHUNK_SIZE_X * Chunk::SUBCHUNK_SIZE_Y / 4) + ";\n" +
    "const uint NUMBER_CHUNKS_X = " + std::to_string(SimulationMap::MAP_SIZE_X) + ";\n" +
    "const uint NUMBER_SUBCHUNKS_X = CHUNK_SIZE_X * NUMBER_CHUNKS_X / SUBCHUNK_SIZE_X;"
    // cell related constants
    "const uint GAS = " + std::to_string(static_cast<std::uint32_t>(CellInfo::MainType::GAS)) + ";\n" +
    "const uint LIQUID = " + std::to_string(static_cast<std::uint32_t>(CellInfo::MainType::LIQUID)) + ";\n" +
    "const uint POWDER = " + std::to_string(static_cast<std::uint32_t>(CellInfo::MainType::POWDER)) + ";\n" +
    "const uint SOLID = " + std::to_string(static_cast<std::uint32_t>(CellInfo::MainType::SOLID)) + ";\n"
    ;
}

constexpr std::string SimulationShader::make_shifts_and_masks(){
    return
    // masks
    "const uint MAIN_TYPE_MASK = " + std::to_string(static_cast<std::uint32_t>(Cell::MASKS::MAIN_TYPE)) + ";\n" +
    "const uint SUBTYPE_MASK = " + std::to_string(static_cast<std::uint32_t>(Cell::MASKS::SUBTYPE)) + ";\n" +
    "const uint COLOR_MASK = " + std::to_string(static_cast<std::uint32_t>(Cell::MASKS::COLOR)) + ";\n" +
    // shifts
    "const uint MAIN_TYPE_SHIFT = " + std::to_string(static_cast<std::uint32_t>(Cell::SHIFTS::MAIN_TYPE)) + ";\n" +
    "const uint SUBTYPE_SHIFT = " + std::to_string(static_cast<std::uint32_t>(Cell::SHIFTS::SUBTYPE)) + ";\n" +
    "const uint COLOR_SHIFT = " + std::to_string(static_cast<std::uint32_t>(Cell::SHIFTS::COLOR)) + ";\n"
    ;
}

constexpr std::string SimulationShader::make_getters(){
    return 
    // some necessary constants
    "const uint CHUNK_COUNT_X = " + std::to_string(SimulationMap::MAP_SIZE_X) + ";\n"
    "const uint CHUNK_COUNT_Y = " + std::to_string(SimulationMap::MAP_SIZE_Y) + ";\n"
    // cell data
    "uint get_main_type(uint cell){return (cell >> MAIN_TYPE_SHIFT) & MAIN_TYPE_MASK;}\n"
    "uint get_subtype(uint cell){return (cell >> SUBTYPE_SHIFT) & SUBTYPE_MASK;}\n"
    "uint get_color_id(uint cell){return (cell >> COLOR_SHIFT) & COLOR_MASK;}\n"
    "uint get_full_type(uint cell){return cell & ((MAIN_TYPE_MASK << MAIN_TYPE_SHIFT) | (SUBTYPE_MASK << SUBTYPE_SHIFT));}\n"
    "uint get_full_color_id(uint cell){return cell & ((COLOR_MASK << COLOR_SHIFT) | (SUBTYPE_MASK << SUBTYPE_SHIFT) | (MAIN_TYPE_MASK << MAIN_TYPE_SHIFT));}\n"
    // celltype data
    "float get_density(uint full_type){return cell_types[full_type].density;}\n"
    "float get_flammability(uint full_type){return cell_types[full_type].flammability;}\n"
    "float get_conductivity(uint full_type){return cell_types[full_type].conductivity;}\n"
    // cell position
    "uint get_cell(uvec4 pos){\n"
    "   pos.w += pos.y / CHUNK_SIZE_Y;\n"
    "   pos.y = pos.y % CHUNK_SIZE_Y;\n"
    "   pos.z += pos.x / CHUNK_SIZE_X;\n"
    "   pos.x = pos.x % CHUNK_SIZE_X;\n"
    "   return cells[pos.w * CHUNK_COUNT_X + pos.z].cells[pos.y * CHUNK_SIZE_X + pos.x];\n"
    "}\n"
    "void write_cell(uvec4 pos, uint value){\n"
    "   pos.w += pos.y / CHUNK_SIZE_Y;\n"
    "   pos.y = pos.y % CHUNK_SIZE_Y;\n"
    "   pos.z += pos.x / CHUNK_SIZE_X;\n"
    "   pos.x = pos.x % CHUNK_SIZE_X;\n"
    "   cells[pos.w * CHUNK_COUNT_X + pos.z].cells[pos.y * CHUNK_SIZE_X + pos.x] = value;\n"
    "}\n"
    // subchunks
    "void set_subchunk_active(uvec4 pos){\n"
    "   uint subchunk_x = pos.x / SUBCHUNK_SIZE_X + pos.z * (CHUNK_SIZE_X / SUBCHUNK_SIZE_X);\n"
    "   uint subchunk_y = pos.y / SUBCHUNK_SIZE_Y + pos.w * (CHUNK_SIZE_Y / SUBCHUNK_SIZE_Y);\n"
    "   uint subchunk_id = subchunk_y * NUMBER_SUBCHUNKS_X + subchunk_x;\n"
    "   subchunks[subchunk_id] = RAND_VALUE;\n"
    "}\n"
    ;
}

constexpr std::string SimulationShader::make_cell_types_array(const std::vector<CellType>& cell_types){
    std::string res = 
        "struct CellType {\n"
            "float density;\n"
            "float flammability;\n"
            "float conductivity;\n"
        "};\n"
        "const CellType[] cell_types = CellType[](\n"
    ;

    res.reserve(cell_types.size() * 32);

    for(const CellType& type : cell_types){
        res += "CellType(";

        // must all be in the same order as they are in the shader's struct shown just above
        res += std::to_string(type.density) + ',';
        res += std::to_string(type.flammability) + ',';
        res += std::to_string(type.conductivity);// + ',';

        res += "),";
    }
    // remove the last comma
    res.pop_back();

    res += ");\n";
    return res;
}

constexpr std::string SimulationShader::make_prediction_map(){
    auto map = PredictionMapMaker::generate();

    std::string res = "const uint[] TYPE_MAP_PREDICTION = uint[](\n";
    for(const std::uint32_t moves : map){
        res += std::to_string(moves) + ',';
    }
    // remove the last comma
    res.pop_back();
    res += ");\n";

    return res;
}

constexpr std::string SimulationShader::make_main_code(){
    return 
    R"(
    uint rand() {
        uint state = gl_GlobalInvocationID.x ^ (1973u * RAND_VALUE);
        state ^= (state << 13);
        state ^= (state >> 17);
        state ^= (state << 5);
        state = state & (7 << 1);
        // 3 = 50%
        // 2 = 25%
        // 1 = 12.5%
        // 0 = 12.5%
        // 0b1111111110100100
        state = (0xFFA8 >> state) & 3;
        return state << 3;
    }

    shared bool is_inactive;

    void main() {
        if(gl_LocalInvocationID.x == 0) is_inactive = true;
        const uint TILES_PER_SUBCHUNK_X = SUBCHUNK_SIZE_X / 2;
        uint subchunk_id = active_subchunks[gl_WorkGroupID.x];
        uint subchunk_y = subchunk_id / NUMBER_SUBCHUNKS_X;
        uint subchunk_x = subchunk_id % NUMBER_SUBCHUNKS_X;

        uint global_y = subchunk_y * SUBCHUNK_SIZE_Y + 2 * ((gl_LocalInvocationID.x / TILES_PER_SUBCHUNK_X) % TILES_PER_SUBCHUNK_X) + CURRENT_STEP_OFFSET.y;
        uint global_x = subchunk_x * SUBCHUNK_SIZE_X + 2 * (gl_LocalInvocationID.x % TILES_PER_SUBCHUNK_X) + CURRENT_STEP_OFFSET.x;

        uvec4 pos = uvec4(
            global_x % CHUNK_SIZE_X, 
            global_y % CHUNK_SIZE_Y,
            global_x / CHUNK_SIZE_X, 
            global_y / CHUNK_SIZE_Y
        );
        
        const uvec4 offsets[] = uvec4[](
            uvec4(0, 1, 0, 0), 
            uvec4(1, 1, 0, 0), 
            uvec4(0, 0, 0, 0), 
            uvec4(1, 0, 0, 0)
        );
        
        uint tile[4] = uint[](
            get_cell(pos + offsets[0]),
            get_cell(pos + offsets[1]),
            get_cell(pos),// + offsets[2]), // useless, since 0
            get_cell(pos + offsets[3])
        );

        uint index = (get_main_type(tile[0]) << 6)
                | (get_main_type(tile[1]) << 4)
                | (get_main_type(tile[2]) << 2)
                | (get_main_type(tile[3]));

        uint moves = TYPE_MAP_PREDICTION[index];
        const uint NO_MOVE = (0 << 6) | (1 << 4) | (2 << 2) | 3;
        const uint TRUE_NO_MOVE = (NO_MOVE | (NO_MOVE << 8) | (NO_MOVE << 16) | (NO_MOVE << 24));

        barrier();
        if(moves != TRUE_NO_MOVE){
            is_inactive = false;
        }
        barrier();
        if(is_inactive){// || (moves == TRUE_NO_MOVE && gl_LocalInvocationID.x != 0)){
            return;
        }

        moves = (moves >> rand()) & 0xFF;

        if(moves != NO_MOVE){
            uint where = (moves >> 6) & 3;
            write_cell(pos + offsets[0], tile[where]);

            where = (moves >> 4) & 3;
            write_cell(pos + offsets[1], tile[where]);

            where = (moves >> 2) & 3;
            write_cell(pos/* + offsets[2]*/, tile[where]);

            where = (moves >> 0) & 3;
            write_cell(pos + offsets[3], tile[where]);
        }

        if(gl_LocalInvocationID.x == 0){
            subchunks[subchunk_id - 1] = RAND_VALUE;
            subchunks[subchunk_id] = RAND_VALUE;
            subchunks[subchunk_id + 1] = RAND_VALUE;

            subchunks[subchunk_id + NUMBER_SUBCHUNKS_X - 1] = RAND_VALUE;
            subchunks[subchunk_id + NUMBER_SUBCHUNKS_X] = RAND_VALUE;
            subchunks[subchunk_id + NUMBER_SUBCHUNKS_X + 1] = RAND_VALUE;

            subchunks[subchunk_id - NUMBER_SUBCHUNKS_X - 1] = RAND_VALUE;
            subchunks[subchunk_id - NUMBER_SUBCHUNKS_X] = RAND_VALUE;
            subchunks[subchunk_id - NUMBER_SUBCHUNKS_X + 1] = RAND_VALUE;

            //set_subchunk_active(pos + offsets[0]);
            //set_subchunk_active(pos + offsets[1]);
            //set_subchunk_active(pos + offsets[2]);
            //set_subchunk_active(pos + offsets[3]);
        }
    })";
}

constexpr std::string SimulationShader::make_perfect_mixing_hashmap(){
    return "";
}
