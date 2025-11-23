# Sources for the main application
set(APP_SOURCES
    beatbox.c
)

# Create the executable named 'beatbox'
add_executable(beatbox ${APP_SOURCES})

# Link the executable to the static library, using the new name beatbox_lib. 
target_link_libraries(beatbox PRIVATE
    beatbox_lib 
)