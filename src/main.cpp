#include "cell.h"
#include "chunk.h"
#include "cell_info.h"
#include "simulation_map.h"
#include "shaders/prediction_map_maker.h"
#include "shaders/simulation_shader.h"

#include "glfw/glfw3.h"
#include "glad/glad.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <unordered_map>
#include <thread>
#include <unordered_set>

void applyBrush(GLuint ssbo, int worldX, int worldY, uint32_t cellValue,
                int brushSize, int SIM_CHUNKS_X, int SIM_CHUNKS_Y, int CHUNK_SIZE)
{
    int half = brushSize / 2;

    // group edits by chunk
    std::unordered_map<int, std::vector<std::pair<int, uint32_t>>> edits;

    for(int dy = -half; dy < half; dy++){
        for(int dx = -half; dx < half; dx++){
            int px = worldX + dx;
            int py = worldY + dy;

            int chunk_x = px / CHUNK_SIZE;
            int chunk_y = py / CHUNK_SIZE;
            int cell_x  = ((px % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE;
            int cell_y  = ((py % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE;

            if(chunk_x < 0 || chunk_x > SIM_CHUNKS_X) continue;
            if(chunk_y < 0 || chunk_y > SIM_CHUNKS_Y) continue;

            int chunkIndex = chunk_y + (SIM_CHUNKS_Y + 2) * chunk_x;
            int cellIndex  = cell_y + CHUNK_SIZE * cell_x;
            edits[chunkIndex].push_back({cellIndex, cellValue});
        }
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    size_t chunkBytes = CHUNK_SIZE * CHUNK_SIZE * sizeof(uint32_t);

    for(auto& [chunkIndex, cellEdits] : edits){
        // sort by cell index so writes are sequential
        std::sort(cellEdits.begin(), cellEdits.end());

        for(auto& [cellIndex, value] : cellEdits){
            size_t byteOffset = chunkIndex * chunkBytes + cellIndex * sizeof(uint32_t);
            glBufferSubData(GL_SHADER_STORAGE_BUFFER, byteOffset, sizeof(uint32_t), &value);
        }
    }
}
void markBrushActive(GLuint chunksToUpdateNext, int worldX, int worldY,
                     int brushSize, int SIM_CHUNKS_X, int SIM_CHUNKS_Y, int CHUNK_SIZE)
{
    int half = brushSize / 2;
    std::unordered_set<int> marked;

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, chunksToUpdateNext);

    auto markSubchunk = [&](int chunk_x, int chunk_y, int subchunk_x, int subchunk_y){
        // handle subchunk overflow into neighboring chunks
        chunk_x += subchunk_x / 16;
        chunk_y += subchunk_y / 16;
        subchunk_x = ((subchunk_x % 16) + 16) % 16;
        subchunk_y = ((subchunk_y % 16) + 16) % 16;

        if(chunk_x < 1 || chunk_x > SIM_CHUNKS_X) return;
        if(chunk_y < 1 || chunk_y > SIM_CHUNKS_Y) return;

        int chunkIndex = (chunk_y) + SIM_CHUNKS_Y * (chunk_x);
        int subchunkIndex = chunkIndex * 256 + subchunk_y * 16 + subchunk_x;

        if(marked.count(subchunkIndex)) return;
        marked.insert(subchunkIndex);

        uint32_t one = 1;
        glBufferSubData(GL_SHADER_STORAGE_BUFFER,
                        subchunkIndex * sizeof(uint32_t),
                        sizeof(uint32_t), &one);
    };

    for(int dy = -half; dy < half; dy++){
        for(int dx = -half; dx < half; dx++){
            int px = worldX + dx;
            int py = worldY + dy;

            int chunk_x = px / CHUNK_SIZE;
            int chunk_y = py / CHUNK_SIZE;
            int cell_x  = ((px % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE;
            int cell_y  = ((py % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE;

            if(chunk_x < 0 || chunk_x > SIM_CHUNKS_X) continue;
            if(chunk_y < 0 || chunk_y > SIM_CHUNKS_Y) continue;

            int subchunk_x = cell_x / 16;
            int subchunk_y = cell_y / 16;

            // mark self and all 4 neighbors
            markSubchunk(chunk_x, chunk_y, subchunk_x,     subchunk_y    );
            markSubchunk(chunk_x, chunk_y, subchunk_x + 1, subchunk_y    );
            markSubchunk(chunk_x, chunk_y, subchunk_x - 1, subchunk_y    );
            markSubchunk(chunk_x, chunk_y, subchunk_x,     subchunk_y + 1);
            markSubchunk(chunk_x, chunk_y, subchunk_x,     subchunk_y - 1);
        }
    }
}


// ---------- Fullscreen shader ----------
GLuint createScreenProgram(){
    const char* vsSrc = R"(
        #version 460 core
        void main() {
            vec2 pos = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
            gl_Position = vec4(pos * 2.0 - 1.0, 0.0, 1.0);
        }
    )";

    const char* fsSrc = R"(
        #version 460 core

        // MUST be compile-time constants (or #defines)
        #define SIM_CHUNKS_X 8
        #define SIM_CHUNKS_Y 8
        #define CHUNK_SIZE 256

        struct Chunk { uint cells[256 * 256];};

        layout(std430, binding = 0) readonly buffer Cells { Chunk cells[]; };
        layout(std430, binding = 1) readonly buffer Palette { vec4 palette[]; };

        layout(location = 0) uniform int width;   // total world width = SIM_CHUNKS_X * CHUNK_SIZE
        layout(location = 1) uniform int height;
        layout(location = 2) uniform int scale;
        layout(location = 3) uniform ivec2 offset;

        out vec4 FragColor;

        void main() {
            ivec2 pixel = ivec2(gl_FragCoord.xy) / scale + offset.xy;

            if(pixel.x < 0 || pixel.x >= width || pixel.y < 0 || pixel.y >= height){
                discard;
            }

            uvec4 pos = uvec4(
                pixel.x % 256, 
                pixel.y % 256,
                pixel.x / 256, 
                pixel.y / 256
            );

            uint cell = cells[pos.w * 8 + pos.z].cells[pos.y * 256 + pos.x];;
            FragColor = palette[(cell >> 12) & 7];
        }
    )";
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vsSrc, nullptr);
    glCompileShader(vs);

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fsSrc, nullptr);
    glCompileShader(fs);

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    glDeleteShader(vs);
    glDeleteShader(fs);

    return prog;
}



void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    int* scalePtr = (int*)glfwGetWindowUserPointer(window);

    if (yoffset > 0) (*scalePtr)++;
    if (yoffset < 0) (*scalePtr)--;

    if (*scalePtr < 1) *scalePtr = 1;
    if (*scalePtr > 64) *scalePtr = 64;
    std::cout << "SCALE: " << *scalePtr << '\n';
}

int scale = 1;

int main() {
    if(!glfwInit()){
        std::cout << "GLFW init failed\n";
        return -1;
    }
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
    
    const int SCREEN_WIDTH = 480;
    const int SCREEN_HEIGHT = 270;
    GLFWwindow* window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Compute", nullptr, nullptr);
    
    glfwMakeContextCurrent(window);
    //glfwSwapInterval(0); // disable vsync
    gladLoadGL();

    
    glViewport(0, 0, 4 * SCREEN_WIDTH, 4 * SCREEN_HEIGHT);
    glfwSetWindowUserPointer(window, &scale);
    glfwSetScrollCallback(window, scroll_callback);


    // ---------- Shader programs ----------
    auto cell_types = CellInfo::load_cell_types("src/game/cells.csv");
    SimulationShader compute_shader(cell_types, "src/game/compiled_shaders");
    GLuint screenProgram = createScreenProgram();

    // ---------- Palette ----------
    GLfloat palette[256 * 4];
    for(int i = 0; i < 256; i++){
        palette[i * 4 + 0] = 1;             // R
        palette[i * 4 + 1] = 0;             // G
        palette[i * 4 + 2] = 0;             // B
        palette[i * 4 + 3] = 1;             // A
    }
    palette[0] = 1;
    palette[1] = 1;
    palette[2] = 1;

    palette[4] = 0;
    palette[5] = 0.2;
    palette[6] = 0.8;

    palette[8] = 1;
    palette[9] = 1;
    palette[10] = 0.2;

    palette[12] = 0.6;
    palette[13] = 0.6;
    palette[14] = 0.6;

    glUseProgram(screenProgram);
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    GLuint paletteSSBO;
    glGenBuffers(1, &paletteSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, paletteSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(palette), palette, GL_STATIC_DRAW);
    
    
    SimulationMap cells;
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<uint32_t> dist(0, 3); // 0-3 types

    
    std::cout << "creating random map...\n";
    int type = 0;
    for(size_t chunk_x = 0; chunk_x < SimulationMap::MAP_SIZE_X; chunk_x++){
        for(size_t chunk_y = 0; chunk_y < SimulationMap::MAP_SIZE_Y; chunk_y++){
            for(size_t x = 0; x < Chunk::CHUNK_SIZE_X; x++){
                for(size_t y = 0; y < Chunk::CHUNK_SIZE_Y; y++){
                    //auto& v = cells[chunk_x, chunk_y, x, y];
                    auto& v = cells.get_cell(chunk_x, chunk_y, x, y);
                    type = dist(rng);
                    if(type == 2 && dist(rng) <= 2){
                        type = 0;
                    } else if(type == 3){// && (dist(rng) <= 2 || dist(rng) <= 2)){
                        type = 0;
                    }
                    int subtype = 0;
                    if(type == 1) subtype = dist(rng) % 2;
                    if(y == 0 && x > 10){
                        type = 3;
                    }
                    int color = type;
                    if(chunk_x == 2 && chunk_y == 1){
                        //color = 4;
                    }
                    v = Cell(CellInfo::MainType(type), CellInfo::SubType(subtype), color);
                }
            }
        }
    }

    compute_shader.use();

    // create SSBO
    std::cout << "linking ssbo...\n";
    GLuint ssbo;
    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, cells.size()*sizeof(Cell), cells.data(), GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);


    std::cout << "linking ssbo step 2...\n";
    // Link SSBO to the texture
    glUseProgram(screenProgram);
    int offsetX = 0;
    int offsetY = 0;
    glUniform1i(glGetUniformLocation(screenProgram, "scale"), scale);
    glUniform2i(glGetUniformLocation(screenProgram, "offset"), offsetX, offsetY);

    glUniform1i(glGetUniformLocation(screenProgram, "width"), SimulationMap::MAP_SIZE_Y * Chunk::CHUNK_SIZE_Y);
    glUniform1i(glGetUniformLocation(screenProgram, "height"), SimulationMap::MAP_SIZE_X * Chunk::CHUNK_SIZE_X);
    
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);



    int count = 999;
    auto begin = std::chrono::high_resolution_clock::now();
    auto end = std::chrono::high_resolution_clock::now();
    double fps = 60.0;
    bool paused = false;
    bool step_once = false;
    int is_pressed = 0;
    int step2 = 0;
    std::cout << "starting...\n";
    while(!glfwWindowShouldClose(window)){
        auto frame_begin = std::chrono::high_resolution_clock::now();
        if(!paused || step_once){
            compute_shader.run();
        }
        // --- Render pass ---
        //glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(screenProgram);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);        // cells
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, paletteSSBO); // palette

        static int g = 0;
        g++;
        if(g == 1){
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glfwSwapBuffers(window);
            glfwPollEvents();
            g = 0;
        }
        

        const int speed = 4; // movement speed (in pixels, before scaling)


        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)  offsetX -= speed;
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) offsetX += speed;
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)    offsetY += speed;
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)  offsetY -= speed;
        if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) paused = true;
        if (glfwGetKey(window, GLFW_KEY_U) == GLFW_PRESS) paused = false;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) step_once = true;
        if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) {is_pressed++;} else {is_pressed = 0;}
        step_once = false;
        if(is_pressed == 1){
            step_once = true;
        }
        const unsigned SAND = (2 << 12) | 2;
        const unsigned WATER = (1 << 12) | 1;

        if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS){
            double mx, my;
            glfwGetCursorPos(window, &mx, &my);
            int worldX = (int)(mx + offsetX) / scale;
            int worldY = (int)(SCREEN_HEIGHT - my + offsetY) / scale;
            applyBrush(ssbo, worldX, worldY, WATER , 32, 8, 8, 256);
            // figure out which buffer is currently "next"

            GLuint currentNext = compute_shader.get_bullshit_TODOREMOVE();
            markBrushActive(currentNext, worldX, worldY, 32, 8, 8, 256);
        }

        if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS){
            double mx, my;
            glfwGetCursorPos(window, &mx, &my);
            int worldX = (int)(mx + offsetX) / scale;
            int worldY = (int)(SCREEN_HEIGHT - my + offsetY) / scale;
            applyBrush(ssbo, worldX, worldY, SAND, 32, 8, 8, 256);
            // figure out which buffer is currently "next"
            GLuint currentNext = compute_shader.get_bullshit_TODOREMOVE();

            markBrushActive(currentNext, worldX, worldY, 32, 8, 8, 256);
        }

        glUniform1i(glGetUniformLocation(screenProgram, "scale"), scale);
        glUniform2i(glGetUniformLocation(screenProgram, "offset"), offsetX, offsetY);
        const int MA_XCOUNT = 1000;
        if(count == MA_XCOUNT){
            end = std::chrono::high_resolution_clock::now();
            size_t duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count();
            fps = (double)1 / (double(duration) / 1e9) * (double)MA_XCOUNT;
            std::cout << "Frame time: " <<  double(duration) / 1e3 / (double)MA_XCOUNT << " us\n";
            
            std::cout << fps << " fps\n";
            count = 0;
            begin = std::chrono::high_resolution_clock::now();
        }
        count++;
        
        auto frame_end = std::chrono::high_resolution_clock::now();
        auto duration2 = std::chrono::duration_cast<std::chrono::nanoseconds>(frame_end - frame_begin);
        auto expected = std::chrono::nanoseconds(16666667);
        //std::cout << "TIME: " << duration2.count()/1e6 << " ms\n";
        auto start4 = std::chrono::high_resolution_clock::now();

        std::chrono::nanoseconds time_remaining = expected;
        if(!(glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)){
            std::chrono::milliseconds safe_sleep_limit(15); 
            if (expected > safe_sleep_limit) {
                //std::this_thread::sleep_for(expected - safe_sleep_limit);
                time_remaining = end - std::chrono::high_resolution_clock::now();
            }
            //while (std::chrono::high_resolution_clock::now() < end);
        }
    }

    glfwTerminate();
}