# Include directories.
include_directories(${PROJECT_BINARY_DIR}/include)
include_directories(${PROJECT_BINARY_DIR}/include/apt-pkg)

# Set the version of the library
set(MAJOR 1.1)
set(MINOR 0)
add_definitions(-DAPT_DOMAIN="${libapt-inst${MAJOR}}")

# Definition of C++ files
file(GLOB core "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")
file(GLOB contrib "${CMAKE_CURRENT_SOURCE_DIR}/contrib/*.cc")
file(GLOB deb "${CMAKE_CURRENT_SOURCE_DIR}/deb/*.cc")
# Definition of the headers
file(GLOB_RECURSE headers "*.h")

# Create a library using the C++ files
add_library(apt-inst SHARED ${core} ${deb} ${contrib})
# Link against apt-pkg
link_directories(${PROJECT_BINARY_DIR}/apt-pkg)
target_link_libraries(apt-inst apt-pkg)

# Add the SONAME to the library
set_target_properties(apt-inst PROPERTIES VERSION ${MAJOR}.${MINOR})
set_target_properties(apt-inst PROPERTIES SOVERSION ${MAJOR})

# Install the library
install(TARGETS apt-inst LIBRARY DESTINATION lib)
install(FILES ${headers} DESTINATION include/apt-pkg)

# Threading support
find_package(Threads)
if (CMAKE_USE_PTHREADS_INIT)
	target_link_libraries(apt-inst ${CMAKE_THREAD_LIBS_INIT})
endif(CMAKE_USE_PTHREADS_INIT)

# Flatten our header structure
foreach(header ${headers})
	get_filename_component(tgt ${header} NAME)
	configure_file(${header} ${PROJECT_BINARY_DIR}/include/apt-pkg/${tgt} @ONLY)
endforeach(header)


