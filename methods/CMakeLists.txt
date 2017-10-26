# Create the executable targets
include_directories($<$<BOOL:${SECCOMP_FOUND}>:${SECCOMP_INCLUDE_DIR}>)
link_libraries(apt-pkg $<$<BOOL:${SECCOMP_FOUND}>:${SECCOMP_LIBRARIES}>)

add_library(httplib OBJECT http.cc basehttp.cc)
add_library(connectlib OBJECT connect.cc rfc2553emu.cc)

add_executable(file file.cc)
add_executable(copy copy.cc)
add_executable(store store.cc)
add_executable(gpgv gpgv.cc)
add_executable(cdrom cdrom.cc)
add_executable(http http_main.cc $<TARGET_OBJECTS:httplib> $<TARGET_OBJECTS:connectlib>)
add_executable(mirror mirror.cc $<TARGET_OBJECTS:httplib> $<TARGET_OBJECTS:connectlib>)
add_executable(ftp ftp.cc $<TARGET_OBJECTS:connectlib>)
add_executable(rred rred.cc)
add_executable(rsh rsh.cc)

target_compile_definitions(connectlib PRIVATE ${GNUTLS_DEFINITIONS})
target_include_directories(connectlib PRIVATE ${GNUTLS_INCLUDE_DIR})

# Additional libraries to link against for networked stuff
target_link_libraries(http ${GNUTLS_LIBRARIES})
target_link_libraries(mirror ${RESOLV_LIBRARIES} ${GNUTLS_LIBRARIES})
target_link_libraries(ftp ${GNUTLS_LIBRARIES})

# Install the library
install(TARGETS file copy store gpgv cdrom http ftp rred rsh mirror
        RUNTIME DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/apt/methods)

add_slaves(${CMAKE_INSTALL_LIBEXECDIR}/apt/methods store)
add_slaves(${CMAKE_INSTALL_LIBEXECDIR}/apt/methods rsh ssh)


add_slaves(${CMAKE_INSTALL_LIBEXECDIR}/apt/methods http https)
