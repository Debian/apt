# Create the executable targets
add_executable(file file.cc)
add_executable(copy copy.cc)
add_executable(store store.cc)
add_executable(gpgv gpgv.cc)
add_executable(cdrom cdrom.cc)
add_executable(http http.cc http_main.cc rfc2553emu.cc connect.cc basehttp.cc)
add_executable(mirror mirror.cc http.cc rfc2553emu.cc connect.cc basehttp.cc)
if (HAVE_CURL)
        add_executable(curl curl.cc basehttp.cc)
endif()
add_executable(ftp ftp.cc rfc2553emu.cc connect.cc)
add_executable(rred rred.cc)
add_executable(rsh rsh.cc)

target_compile_definitions(http PRIVATE ${GNUTLS_DEFINITIONS})
target_include_directories(http PRIVATE ${GNUTLS_INCLUDE_DIR})
if (HAVE_CURL)
target_include_directories(curl PRIVATE ${CURL_INCLUDE_DIRS})
endif()

# Link the executables against the libraries
target_link_libraries(file apt-pkg)
target_link_libraries(copy apt-pkg)
target_link_libraries(store apt-pkg)
target_link_libraries(gpgv apt-pkg)
target_link_libraries(cdrom apt-pkg)
target_link_libraries(http apt-pkg ${GNUTLS_LIBRARIES})
target_link_libraries(mirror apt-pkg ${RESOLV_LIBRARIES} ${GNUTLS_LIBRARIES})
if (HAVE_CURL)
        target_link_libraries(curl apt-pkg ${CURL_LIBRARIES})
endif()
target_link_libraries(ftp apt-pkg ${GNUTLS_LIBRARIES})
target_link_libraries(rred apt-pkg)
target_link_libraries(rsh apt-pkg)

# Install the library
install(TARGETS file copy store gpgv cdrom http ftp rred rsh mirror
        RUNTIME DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/apt/methods)

add_slaves(${CMAKE_INSTALL_LIBEXECDIR}/apt/methods store gzip lzma bzip2 xz)
add_slaves(${CMAKE_INSTALL_LIBEXECDIR}/apt/methods rsh ssh)

set(curl_slaves curl+https curl+http)

if (FORCE_CURL)
        set(curl_slaves ${curl_slaves} https)
else()
        add_slaves(${CMAKE_INSTALL_LIBEXECDIR}/apt/methods http https)
endif()
if (HAVE_CURL)
        install(TARGETS curl RUNTIME DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/apt/methods)
        add_slaves(${CMAKE_INSTALL_LIBEXECDIR}/apt/methods curl ${curl_slaves})
endif()
