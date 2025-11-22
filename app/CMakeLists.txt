# Create the final executable
add_executable(beatbox beatbox.c)

# Link the "beatbox" executable against our two libraries.
# The pthread flags are inherited from the root.
target_link_libraries(beatbox PRIVATE
    beatbox_lib
    # controls_lib
)

# --- Deployment Rule ---

# Install the "beatbox" executable to the destination
# required by the assignment.
install(TARGETS beatbox
    DESTINATION ${CMAKE_INSTALL_PREFIX}/public/myApps
)