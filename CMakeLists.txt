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
<<<<<<< HEAD
add_subdirectory(app)
=======
add_subdirectory(app)

# --- Assignment 3 Compliance Deployment ---

# 1. Set the install folder to the specific path required by the PDF
# We use $ENV{HOME} to get the user's home directory (e.g., /home/phillip)
set(CMAKE_INSTALL_PREFIX $ENV{HOME}/ensc351/public/myApps)

# 2. Install the Executable (Requirement: deploy to ~/ensc351/public/myApps/)
install(TARGETS beatbox DESTINATION .)

# 3. Install Audio Files (Requirement: deploy to .../beatbox-wav-files/)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets/wave-files/ 
        DESTINATION beatbox-wav-files)

# 4. Install Node Server (Requirement: deploy to .../beatbox-server-copy/)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/as3-server/ 
        DESTINATION beatbox-server-copy)
>>>>>>> new_feature_branch
