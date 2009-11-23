# Include directories.
include_directories(${PROJECT_BINARY_DIR}/include)
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
add_library(apt-pkg SHARED  ${core} ${deb} ${contrib})

# Set the version and the soversion property
set_target_properties(apt-pkg PROPERTIES VERSION ${MAJOR}.${MINOR})
set_target_properties(apt-pkg PROPERTIES SOVERSION ${MAJOR})
add_definitions(-DAPT_DOMAIN="libapt-pkg${MAJOR}")

# Link against util and dl
target_link_libraries(apt-pkg util dl)

# Threading support
if (CMAKE_USE_PTHREADS_INIT)
	target_link_libraries(apt-pkg ${CMAKE_THREAD_LIBS_INIT})
endif(CMAKE_USE_PTHREADS_INIT)

# Install the library and the header files
install(TARGETS apt-pkg LIBRARY DESTINATION lib)
install(FILES ${headers} DESTINATION include/apt-pkg)
# Create a build-time header structure using our headers.
foreach(header ${headers})
	get_filename_component(tgt ${header} NAME)
	configure_file(${header} ${PROJECT_BINARY_DIR}/include/apt-pkg/${tgt} @ONLY)
endforeach(header)

# Build the translations (including those for methods)
file(GLOB methods "../methods/*.cc")
set(files ${core} ${contrib} ${deb} ${methods})
apt_add_translation_domain(libapt-pkg${MAJOR} "${files}")
