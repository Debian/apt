# Include directories.
include_directories(${PROJECT_BINARY_DIR}/include)
link_directories(${PROJECT_BINARY_DIR}/apt-pkg ${PROJECT_BINARY_DIR}/apt-inst)

# Create the executable tasks
add_executable(apt-ftparchive apt-ftparchive.cc cachedb.cc writer.cc
                              contents.cc override.cc multicompress.cc)

# Link the executables against the libraries
target_link_libraries(apt-ftparchive apt-inst apt-pkg db)

# Install the executables
install(TARGETS apt-ftparchive RUNTIME DESTINATION bin)
