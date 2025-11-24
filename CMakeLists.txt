cmake_minimum_required(VERSION 3.10)
# Name the project and specify C language
project(BeatBox C)

# Set the C standard to C99 (common for embedded/system development)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)

# Binaries go to build/bin
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/build/bin)
# Libraries go to build/lib
set(LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/build/lib)

# Add subdirectories to the build system
add_subdirectory(lib_beatbox)
add_subdirectory(app)

# Set the install folder to the specific path required by the PDF
set(CMAKE_INSTALL_PREFIX $ENV{HOME}/ensc351/public/myApps)

# Install the Executable (Requirement: deploy to ~/ensc351/public/myApps/)
install(TARGETS beatbox DESTINATION .)

# Install Audio Files (Requirement: deploy to .../beatbox-wav-files/)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets/wave-files/ DESTINATION beatbox-wav-files)

# Install Node Server (Requirement: deploy to .../beatbox-server-copy/)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/as3-server/ DESTINATION beatbox-server-copy)
