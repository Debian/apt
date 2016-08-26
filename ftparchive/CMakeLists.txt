include_directories(${BERKELEY_DB_INCLUDE_DIRS})
# Create the executable tasks
file(GLOB_RECURSE source "*.cc")
add_executable(apt-ftparchive ${source})

# Link the executables against the libraries
target_include_directories(apt-ftparchive PRIVATE ${BERKELEY_DB_INCLUDE_DIRS})
target_link_libraries(apt-ftparchive apt-inst apt-pkg apt-private ${BERKELEY_DB_LIBRARIES})

# Install the executables
install(TARGETS apt-ftparchive RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
