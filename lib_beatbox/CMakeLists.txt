# Sources for the static library
set(LIB_SOURCES
    accelerometer.c
    audioMixer.c
    beatGenerator.c
    inputMan.c
    intervalTimer.c
    joystick.c
    mpc3208.c
    rotary.c
    udpServer.c
)

# Create the static library named 'beatbox_lib' (renamed from 'beatbox' to avoid conflict)
add_library(beatbox_lib STATIC ${LIB_SOURCES})

# Make sure 'app/' can find the header files in this directory
target_include_directories(beatbox_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Link the library to external dependencies (ALSA, Threading, GPIO)
target_link_libraries(beatbox_lib PUBLIC 
    asound      # Required for ALSA functions (audioMixer)
    pthread     # Required for multi-threading functions (BeatGenerator, InputMan, UDP, Rotary)
    gpiod       # Required for GPIO library (rotary encoder switch)
)