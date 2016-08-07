include(ExternalProject)

set(GTEST_ROOT "/usr/src/gtest" CACHE FILEPATH "Path to GTest CMake project")

message(STATUS "Found GTest at ${GTEST_ROOT}")

if (EXISTS ${GTEST_ROOT})

ExternalProject_Add(gtest PREFIX ./gtest
                          SOURCE_DIR ${GTEST_ROOT}
                          INSTALL_COMMAND true)

link_directories(${CMAKE_CURRENT_BINARY_DIR}/gtest/src/gtest-build)
FILE(GLOB files gtest_runner.cc *-helpers.cc *_test.cc)
add_executable(libapt_test ${files})
target_link_libraries(libapt_test -lgtest ${CMAKE_THREAD_LIBS_INIT} apt-private apt-inst)
add_dependencies(libapt_test gtest)
add_test(NAME AptTests
         COMMAND libapt_test
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

endif()
