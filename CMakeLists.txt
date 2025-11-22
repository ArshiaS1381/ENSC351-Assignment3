cmake_minimum_required(VERSION 3.18)
project(BeatBox C)
set(CMAKE_C_STANDARD 11)

# --- Compiler and Linker Options ---

# Add compiler flags for strict warnings
add_compile_options(-Wall -Werror -Wpedantic -Wextra)
add_compile_options(-fdiagnostics-color)

# Add the -g flag for GDB and Valgrind
set(CMAKE_BUILD_TYPE Debug)

# --- Find System Libraries ---

# Find the ALSA library (libasound) required for audio
find_package(ALSA REQUIRED)

# --- PThread Configuration (Your A2 Method) ---

# Enable PThread library for linking (global)
# These flags will be inherited by all subdirectories
add_compile_options(-pthread)
add_link_options(-pthread)

# --- Build Subdirectories ---

add_subdirectory(libbeatbox)
# add_subdirectory(libcontrols)
add_subdirectory(app)

# --- Deployment Rules (for "cmake --install .") ---

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/assets/wave-files/
    DESTINATION ${CMAKE_INSTALL_PREFIX}/public/myApps/beatbox-wav-files
)

# Assuming server files are in "beatbox-server/"
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/beatbox-server/
    DESTINATION ${CMAKE_INSTALL_PREFIX}/public/myApps/beatbox-server-copy
)