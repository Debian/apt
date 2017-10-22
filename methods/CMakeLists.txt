# Create the executable targets
include_directories($<$<BOOL:${SECCOMP_FOUND}>:${SECCOMP_INCLUDE_DIR}>)

add_executable(file file.cc)
add_executable(copy copy.cc)
add_executable(store store.cc)
add_executable(gpgv gpgv.cc)
add_executable(cdrom cdrom.cc)
add_executable(http http.cc http_main.cc rfc2553emu.cc connect.cc basehttp.cc)
add_executable(mirror mirror.cc http.cc rfc2553emu.cc connect.cc basehttp.cc)
add_executable(ftp ftp.cc rfc2553emu.cc connect.cc)
add_executable(rred rred.cc)
add_executable(rsh rsh.cc)

target_compile_definitions(http PRIVATE ${GNUTLS_DEFINITIONS})
target_include_directories(http PRIVATE ${GNUTLS_INCLUDE_DIR})

# Link the executables against the libraries
target_link_libraries(file apt-pkg $<$<BOOL:${SECCOMP_FOUND}>:${SECCOMP_LIBRARIES}>)
target_link_libraries(copy apt-pkg $<$<BOOL:${SECCOMP_FOUND}>:${SECCOMP_LIBRARIES}>)
target_link_libraries(store apt-pkg $<$<BOOL:${SECCOMP_FOUND}>:${SECCOMP_LIBRARIES}>)
target_link_libraries(gpgv apt-pkg $<$<BOOL:${SECCOMP_FOUND}>:${SECCOMP_LIBRARIES}>)
target_link_libraries(cdrom apt-pkg $<$<BOOL:${SECCOMP_FOUND}>:${SECCOMP_LIBRARIES}>)
target_link_libraries(http apt-pkg ${GNUTLS_LIBRARIES} $<$<BOOL:${SECCOMP_FOUND}>:${SECCOMP_LIBRARIES}>)
target_link_libraries(mirror apt-pkg ${RESOLV_LIBRARIES} ${GNUTLS_LIBRARIES} $<$<BOOL:${SECCOMP_FOUND}>:${SECCOMP_LIBRARIES}>)
target_link_libraries(ftp apt-pkg ${GNUTLS_LIBRARIES} $<$<BOOL:${SECCOMP_FOUND}>:${SECCOMP_LIBRARIES}>)
target_link_libraries(rred apt-pkg $<$<BOOL:${SECCOMP_FOUND}>:${SECCOMP_LIBRARIES}>)
target_link_libraries(rsh apt-pkg $<$<BOOL:${SECCOMP_FOUND}>:${SECCOMP_LIBRARIES}>)

# Install the library
install(TARGETS file copy store gpgv cdrom http ftp rred rsh mirror
        RUNTIME DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/apt/methods)

add_slaves(${CMAKE_INSTALL_LIBEXECDIR}/apt/methods store gzip lzma bzip2 xz)
add_slaves(${CMAKE_INSTALL_LIBEXECDIR}/apt/methods rsh ssh)


add_slaves(${CMAKE_INSTALL_LIBEXECDIR}/apt/methods http https)
