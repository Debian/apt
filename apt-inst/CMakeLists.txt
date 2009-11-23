# Include apt-pkg directly, as some files have #include <system.h>
include_directories(${PROJECT_BINARY_DIR}/include/apt-pkg)

# Set the version of the library
set(MAJOR 1.1)
set(MINOR 0)

# Definition of the C++ files used to build the library
file(GLOB_RECURSE library "*.cc")
file(GLOB_RECURSE headers "*.h")

# Create a library using the C++ files
add_library(apt-inst SHARED ${library})

# Link the library and set the SONAME
target_link_libraries(apt-inst apt-pkg ${CMAKE_THREAD_LIBS_INIT})
set_target_properties(apt-inst PROPERTIES VERSION ${MAJOR}.${MINOR})
set_target_properties(apt-inst PROPERTIES SOVERSION ${MAJOR})

# Install the library and the headers
install(TARGETS apt-inst LIBRARY DESTINATION lib)
install(FILES ${headers} DESTINATION include/apt-pkg)
flatify(${PROJECT_BINARY_DIR}/include/apt-pkg/ "${headers}")

# Add the translations for libapt-inst, and set the translation domain.
apt_add_translation_domain(libapt-inst${MAJOR} "${library}")
