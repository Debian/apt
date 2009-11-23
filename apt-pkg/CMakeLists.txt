# Include directories.
include_directories(${PROJECT_BINARY_DIR}/include/apt-pkg)

# Definition of C++ files
file(GLOB core "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")
file(GLOB contrib "${CMAKE_CURRENT_SOURCE_DIR}/contrib/*.cc")
file(GLOB deb "${CMAKE_CURRENT_SOURCE_DIR}/deb/*.cc")
# Definition of the headers
file(GLOB_RECURSE headers "*.h")

set(MAJOR 4.8) # Increase if ABI changes
set(APT_PKG_MAJOR 4.8 PARENT_SCOPE)
set(MINOR 1) # Increase otherwise
# Create a library using the C++ files
add_library(apt-pkg SHARED ${core} ${deb} ${contrib})

# Set the version and the soversion property
set_target_properties(apt-pkg PROPERTIES VERSION ${MAJOR}.${MINOR})
set_target_properties(apt-pkg PROPERTIES SOVERSION ${MAJOR})

# Link against util and dl, and the threading library
target_link_libraries(apt-pkg -lutil -ldl ${CMAKE_THREAD_LIBS_INIT})

# Install the library and the header files
install(TARGETS apt-pkg LIBRARY DESTINATION lib)
flatify(${PROJECT_BINARY_DIR}/include/apt-pkg/ "${headers}")
install(FILES ${headers} DESTINATION include/apt-pkg)

# Build the translations (including those for methods)
file(GLOB methods "../methods/*.cc")
apt_add_translation_domain(libapt-pkg${MAJOR} "${core};${contrib};${deb};${methods}")
