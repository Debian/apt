# Include apt-pkg directly, as some files have #include <system.h>
include_directories(${PROJECT_BINARY_DIR}/include/apt-pkg)

add_definitions("-DAPT_PKG_EXPOSE_STRING_VIEW")

file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/include/apt-pkg/)
execute_process(COMMAND ${PERL_EXECUTABLE} ${PROJECT_SOURCE_DIR}/triehash/triehash.pl
                        --ignore-case
                         --header ${PROJECT_BINARY_DIR}/include/apt-pkg/tagfile-keys.h
                         --code ${CMAKE_CURRENT_BINARY_DIR}/tagfile-keys.cc
                         --enum-class
                         --enum-name pkgTagSection::Key
                         --function-name pkgTagHash
                         --include "<apt-pkg/tagfile.h>"
                         ${CMAKE_CURRENT_SOURCE_DIR}/tagfile-keys.list)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "tagfile-keys.list")


# Set the version of the library
execute_process(COMMAND awk -v ORS=. "/^\#define APT_PKG_M/ {print \$3}"
                COMMAND sed "s/\\.\$//"
                INPUT_FILE ${CMAKE_CURRENT_SOURCE_DIR}/contrib/macros.h
                OUTPUT_VARIABLE MAJOR OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND grep "^#define APT_PKG_RELEASE"
                COMMAND cut -d " " -f 3
                INPUT_FILE ${CMAKE_CURRENT_SOURCE_DIR}/contrib/macros.h
                OUTPUT_VARIABLE MINOR OUTPUT_STRIP_TRAILING_WHITESPACE)

message(STATUS "Building libapt-pkg ${MAJOR} (release ${MINOR})")
set(APT_PKG_MAJOR ${MAJOR} PARENT_SCOPE) # exporting for methods/CMakeLists.txt

# Definition of the C++ files used to build the library - note that this
# is expanded at CMake time, so you have to rerun cmake if you add or remove
# a file (you can just run cmake . in the build directory)
file(GLOB_RECURSE library "*.cc"  "${CMAKE_CURRENT_BINARY_DIR}/tagfile-keys.cc")
file(GLOB_RECURSE headers "*.h")

# Create a library using the C++ files
add_library(apt-pkg SHARED ${library})
add_dependencies(apt-pkg apt-pkg-versionscript)
# Link the library and set the SONAME
target_include_directories(apt-pkg
                           PRIVATE ${ZLIB_INCLUDE_DIRS}
                                   ${BZIP2_INCLUDE_DIR}
                                   ${LZMA_INCLUDE_DIRS}
                                   ${LZ4_INCLUDE_DIRS}
                                   ${ICONV_INCLUDE_DIRS}
)

target_link_libraries(apt-pkg
                      PRIVATE -lutil ${CMAKE_DL_LIBS} ${RESOLV_LIBRARIES}
                             ${CMAKE_THREAD_LIBS_INIT}
                             ${ZLIB_LIBRARIES}
                             ${BZIP2_LIBRARIES}
                             ${LZMA_LIBRARIES}
                             ${LZ4_LIBRARIES}
                             ${ICONV_LIBRARIES}
)
set_target_properties(apt-pkg PROPERTIES VERSION ${MAJOR}.${MINOR})
set_target_properties(apt-pkg PROPERTIES SOVERSION ${MAJOR})
add_version_script(apt-pkg)

# Install the library and the header files
install(TARGETS apt-pkg LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES ${headers} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/apt-pkg)
flatify(${PROJECT_BINARY_DIR}/include/apt-pkg/ "${headers}")

if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
  target_link_libraries(apt-pkg PUBLIC noprofile)
endif()
