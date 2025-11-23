cmake_minimum_required(VERSION 3.10)
# Name the project and specify C language
project(BeatBox C)

# Set the C standard to C99 (common for embedded/system development)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)

# --- Define Output Directories for BeagleBone Deployment ---
# Binaries go to build/bin
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/build/bin)
# Libraries go to build/lib
set(LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/build/lib)

# Add subdirectories to the build system
add_subdirectory(lib_beatbox)
add_subdirectory(app)