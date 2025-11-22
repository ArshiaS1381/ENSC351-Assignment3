# Create a static library named "beatbox_lib"
add_library(beatbox_lib STATIC
    audioMixer.c
    audioMixer.h
    beatGenerator.c
    beatGenerator.h
    udpServer.c
    udpServer.h
)

# Tell any target that *uses* this library (like your 'app')
# that it needs to include the current directory to find
# the header files (audioMixer.h, beatGenerator.h).
target_include_directories(beatbox_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link this library against ALSA
target_link_libraries(beatbox_lib PUBLIC
    ALSA::ALSA
)