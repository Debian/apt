include_directories(${BERKELEY_DB_INCLUDE_DIRS})
# Definition of the C++ files used to build the program - note that this
# is expanded at CMake time, so you have to rerun cmake if you add or remove
# a file (you can just run cmake . in the build directory)
file(GLOB_RECURSE source "*.cc")
add_executable(apt-ftparchive ${source})

# Link the executables against the libraries
target_include_directories(apt-ftparchive PRIVATE ${BERKELEY_DB_INCLUDE_DIRS})
target_link_libraries(apt-ftparchive apt-inst apt-pkg apt-private ${BERKELEY_DB_LIBRARIES})

# Install the executables
install(TARGETS apt-ftparchive RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
