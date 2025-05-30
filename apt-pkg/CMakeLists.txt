# Include apt-pkg directly, as some files have #include <system.h>
include_directories(${PROJECT_BINARY_DIR}/include/apt-pkg)

file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/include/apt-pkg/)
execute_process(COMMAND grep -v "^#" "${CMAKE_CURRENT_SOURCE_DIR}/tagfile-keys.list"
		OUTPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/tagfile-keys.clean.list"
		COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND ${TRIEHASH_EXECUTABLE}
                        --ignore-case
                         --header ${PROJECT_BINARY_DIR}/include/apt-pkg/tagfile-keys.h
                         --code ${CMAKE_CURRENT_BINARY_DIR}/tagfile-keys.cc
                         --enum-class
                         --enum-name pkgTagSection::Key
                         --function-name pkgTagHash
                         --include "<apt-pkg/tagfile.h>"
                         --include "<apt-pkg/header-is-private.h>"
                         "${CMAKE_CURRENT_BINARY_DIR}/tagfile-keys.clean.list"
                COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "tagfile-keys.list")
set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/tagfile-keys.cc" PROPERTIES COMPILE_DEFINITIONS APT_COMPILING_APT)


# Set the version of the library
execute_process(COMMAND awk -v ORS=. "/^\#define APT_PKG_M/ {print \$3}"
                COMMAND sed "s/\\.\$//"
                INPUT_FILE ${CMAKE_CURRENT_SOURCE_DIR}/contrib/macros.h
                OUTPUT_VARIABLE MAJOR OUTPUT_STRIP_TRAILING_WHITESPACE
                COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND grep "^#define APT_PKG_RELEASE"
                COMMAND cut -d " " -f 3
                INPUT_FILE ${CMAKE_CURRENT_SOURCE_DIR}/contrib/macros.h
                OUTPUT_VARIABLE MINOR OUTPUT_STRIP_TRAILING_WHITESPACE
                COMMAND_ERROR_IS_FATAL ANY)

message(STATUS "Building libapt-pkg ${MAJOR} (release ${MINOR})")
set(APT_PKG_MAJOR ${MAJOR} PARENT_SCOPE) # exporting for methods/CMakeLists.txt

configure_file(apt-pkg.pc.in ${CMAKE_CURRENT_BINARY_DIR}/apt-pkg.pc @ONLY)
configure_file(versionscript.in ${CMAKE_CURRENT_BINARY_DIR}/versionscript @ONLY)

# Definition of the C++ files used to build the library - note that this
# is expanded at CMake time, so you have to rerun cmake if you add or remove
# a file (you can just run cmake . in the build directory)
file(GLOB_RECURSE library "*.cc"  "${CMAKE_CURRENT_BINARY_DIR}/tagfile-keys.cc")
file(GLOB_RECURSE headers "*.h")

# Create a library using the C++ files
add_library(apt-pkg SHARED ${library})
# Link the library and set the SONAME
target_include_directories(apt-pkg
                           PRIVATE ${ZLIB_INCLUDE_DIRS}
                                   ${BZIP2_INCLUDE_DIR}
                                   ${LZMA_INCLUDE_DIRS}
                                   ${LZ4_INCLUDE_DIRS}
                                   $<$<BOOL:${ZSTD_FOUND}>:${ZSTD_INCLUDE_DIRS}>
                                   $<$<BOOL:${UDEV_FOUND}>:${UDEV_INCLUDE_DIRS}>
                                   $<$<BOOL:${SYSTEMD_FOUND}>:${SYSTEMD_INCLUDE_DIRS}>
                                   ${ICONV_INCLUDE_DIRS}
                                   ${OPENSSL_INCLUDE_DIRS}
                                   $<$<BOOL:${XXHASH_FOUND}>:${XXHASH_INCLUDE_DIRS}>
)

target_link_libraries(apt-pkg
                      PRIVATE -lutil ${CMAKE_DL_LIBS} ${RESOLV_LIBRARIES}
                             ${CMAKE_THREAD_LIBS_INIT}
                             ${ZLIB_LIBRARIES}
                             ${BZIP2_LIBRARIES}
                             ${LZMA_LIBRARIES}
                             ${LZ4_LIBRARIES}
                             $<$<BOOL:${ZSTD_FOUND}>:${ZSTD_LIBRARIES}>
                             $<$<BOOL:${UDEV_FOUND}>:${UDEV_LIBRARIES}>
                             $<$<BOOL:${SYSTEMD_FOUND}>:${SYSTEMD_LIBRARIES}>
                             ${ICONV_LIBRARIES}
                             OpenSSL::Crypto
                             $<$<BOOL:${XXHASH_FOUND}>:${XXHASH_LIBRARIES}>
)
set_target_properties(apt-pkg PROPERTIES VERSION ${MAJOR}.${MINOR})
set_target_properties(apt-pkg PROPERTIES SOVERSION ${MAJOR})
set_target_properties(apt-pkg PROPERTIES CXX_VISIBILITY_PRESET hidden)
target_link_libraries(apt-pkg PRIVATE -Wl,-version-script="${CMAKE_CURRENT_BINARY_DIR}/versionscript")

# Install the library and the header files
install(TARGETS apt-pkg LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES ${headers} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/apt-pkg)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/apt-pkg.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
flatify(${PROJECT_BINARY_DIR}/include/apt-pkg/ "${headers}")

if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
  target_link_libraries(apt-pkg PUBLIC noprofile)
endif()
