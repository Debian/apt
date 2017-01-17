# Include apt-pkg directly, as some files have #include <system.h>
include_directories(${PROJECT_BINARY_DIR}/include/apt-pkg)

# Set the version of the library
set(MAJOR 2.0)
set(MINOR 0)
set(APT_INST_MAJOR ${MAJOR} PARENT_SCOPE)

# Definition of the C++ files used to build the library - note that this
# is expanded at CMake time, so you have to rerun cmake if you add or remove
# a file (you can just run cmake . in the build directory)
file(GLOB_RECURSE library "*.cc")
file(GLOB_RECURSE headers "*.h")

# Create a library using the C++ files
add_library(apt-inst SHARED ${library})

# Link the library and set the SONAME
target_link_libraries(apt-inst PUBLIC apt-pkg ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(apt-inst PRIVATE ${CMAKE_THREAD_LIBS_INIT})
set_target_properties(apt-inst PROPERTIES VERSION ${MAJOR}.${MINOR})
set_target_properties(apt-inst PROPERTIES SOVERSION ${MAJOR})
add_version_script(apt-inst)

# Install the library and the headers
install(TARGETS apt-inst LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES ${headers} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/apt-pkg)
flatify(${PROJECT_BINARY_DIR}/include/apt-pkg/ "${headers}")
