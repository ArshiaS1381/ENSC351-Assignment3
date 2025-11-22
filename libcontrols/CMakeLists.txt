# Create a static library named "controls_lib"
add_library(controls_lib STATIC
    accelerometer.c
    accelerometer.h
    inputMan.c
    inputMan.h
    intervalTimer.c
    intervalTimer.h
    joystick.c
    joystick.h
    rotary.c
    rotary.h
    udpServer.c
    udpServer.h
)

# Tell any target that *uses* this library where to find its
# header files.
target_include_directories(controls_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# No other libraries needed; pthreads is handled by the root file.