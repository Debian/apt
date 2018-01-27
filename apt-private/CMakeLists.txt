# Set the version of the library
set(MAJOR 0.0)
set(MINOR 0)

# Definition of the C++ files used to build the library - note that this
# is expanded at CMake time, so you have to rerun cmake if you add or remove
# a file (you can just run cmake . in the build directory)
file(GLOB_RECURSE library "*.cc")
file(GLOB_RECURSE headers "*.h")

# Create a library using the C++ files
add_library(apt-private SHARED ${library})

# Link the library and set the SONAME
target_link_libraries(apt-private PUBLIC apt-pkg)
set_target_properties(apt-private PROPERTIES VERSION ${MAJOR}.${MINOR})
set_target_properties(apt-private PROPERTIES SOVERSION ${MAJOR})
set_target_properties(apt-private PROPERTIES CXX_VISIBILITY_PRESET hidden)
add_version_script(apt-private)

# Install the library and the headers
install(TARGETS apt-private
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        NAMELINK_SKIP)

flatify(${PROJECT_BINARY_DIR}/include/apt-private/ "${headers}")
