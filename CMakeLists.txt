cmake_minimum_required(VERSION 3.20)

project(SandsimGame LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -------------------------
# Source files (fully recursive)
# -------------------------
file(GLOB_RECURSE SOURCES
    src/*.cpp
    src/*.c
)

# -------------------------
# Executable
# -------------------------
add_executable(game ${SOURCES})

# -----------------------------------
# Recursively collect include dirs
# -----------------------------------
function(get_all_dirs result dir)
    file(GLOB_RECURSE children LIST_DIRECTORIES true ${dir}/*)
    set(dirs "")
    foreach(child ${children})
        if(IS_DIRECTORY ${child})
            list(APPEND dirs ${child})
        endif()
    endforeach()
    set(${result} ${dirs} PARENT_SCOPE)
endfunction()

get_all_dirs(SRC_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/src)

target_include_directories(game PRIVATE
    include
    include/glad
    include/GLFW
    ${SRC_INCLUDE_DIRS}
)

# -------------------------
# Windows OpenGL + GLFW linking
# -------------------------
target_link_directories(game PRIVATE lib)

target_link_libraries(game PRIVATE
    glfw3dll
    opengl32
    gdi32
    user32
    shell32
)

# static linking
target_link_options(game PRIVATE
    -static
    -static-libgcc
    -static-libstdc++
)
# -------------------------
# Output directory
# -------------------------
set_target_properties(game PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(game PRIVATE GLFW_DLL)
target_compile_options(game PRIVATE
    -Wall 
    -Wextra
    -std=c++23
    -Ofast
    -ffast-math
    -fno-math-errno
    #-fno-exceptions
    -Os
    -g
)