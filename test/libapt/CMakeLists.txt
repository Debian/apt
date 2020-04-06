if (WITH_TESTS)
   set(PROJECT_TEST_LIBRARIES apt-private)
   find_path(GTEST_ROOT src/gtest.cc
      /usr/src/googletest/googletest
      /usr/src/googletest
      /usr/src/gtest
   )
   find_package(GTest)
   set(GTEST_DEPENDENCIES)

   if(NOT GTEST_FOUND AND EXISTS ${GTEST_ROOT})
      include(ExternalProject)
      ExternalProject_Add(gtest PREFIX ./gtest
                                SOURCE_DIR ${GTEST_ROOT}
                                INSTALL_COMMAND true)

      link_directories(${CMAKE_CURRENT_BINARY_DIR}/gtest/src/gtest-build)
      link_directories(${CMAKE_CURRENT_BINARY_DIR}/gtest/src/gtest-build/lib)

      set(GTEST_LIBRARIES "-lgtest")
      set(GTEST_DEPENDENCIES "gtest")
      set(GTEST_FOUND TRUE)
      find_path(GTEST_INCLUDE_DIRS NAMES gtest/gtest.h PATHS ${GTEST_ROOT}/include)

      message(STATUS "Found GTest at ${GTEST_ROOT}, headers at ${GTEST_INCLUDE_DIRS}")
   endif()

   if (NOT GTEST_FOUND)
      message(FATAL_ERROR "Could not find GTest")
   endif()


   # gtest produces some warnings with the set of warnings we activate,
   # so disable the offending warnings while compiling tests for now
   add_optional_compile_options(Wno-undef)
   add_optional_compile_options(Wno-ctor-dtor-privacy)
   # Do not force override for gtest, gtest is missing override specifiers
   add_optional_compile_options(Wno-suggest-override)

   # Definition of the C++ files used to build the test binary - note that this
   # is expanded at CMake time, so you have to rerun cmake if you add or remove
   # a file (you can just run cmake . in the build directory)
   file(GLOB files gtest_runner.cc *-helpers.cc *_test.cc)
   add_executable(lib${PROJECT_NAME}_test ${files})
   target_include_directories(lib${PROJECT_NAME}_test PRIVATE ${GTEST_INCLUDE_DIRS})
   target_link_libraries(lib${PROJECT_NAME}_test ${GTEST_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${PROJECT_TEST_LIBRARIES})
   if (GTEST_DEPENDENCIES)
      add_dependencies(lib${PROJECT_NAME}_test ${GTEST_DEPENDENCIES})
   endif()
   add_test(NAME ${PROJECT_NAME}Tests
            COMMAND lib${PROJECT_NAME}_test
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()
