# Include apt-pkg directly, as some files have #include <system.h>
include_directories(${PROJECT_BINARY_DIR}/include/apt-pkg)

# Set the version of the library
set(MAJOR 4.8) # Increase if ABI changes
set(MINOR 1) # Increase otherwise
set(APT_PKG_MAJOR 4.8 PARENT_SCOPE) # exporting for methods/CMakeLists.txt

# Definition of the C++ files used to build the library
file(GLOB_RECURSE library "*.cc")
file(GLOB_RECURSE headers "*.h")

# Create a library using the C++ files
add_library(apt-pkg SHARED ${library})

# Link the library and set the SONAME
target_link_libraries(apt-pkg -lutil -ldl ${CMAKE_THREAD_LIBS_INIT})
set_target_properties(apt-pkg PROPERTIES VERSION ${MAJOR}.${MINOR})
set_target_properties(apt-pkg PROPERTIES SOVERSION ${MAJOR})

# Install the library and the header files
install(TARGETS apt-pkg LIBRARY DESTINATION lib)
install(FILES ${headers} DESTINATION include/apt-pkg)
flatify(${PROJECT_BINARY_DIR}/include/apt-pkg/ "${headers}")

# Build the translations (including those for methods)
file(GLOB methods "../methods/*.cc")
apt_add_translation_domain(libapt-pkg${MAJOR} "${library};${methods}")
