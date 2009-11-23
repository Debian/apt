# APT_PKG_MAJOR set globally in ../apt-pkg/CMakeLists.txt, translations are
# built there.
add_definitions(-DAPT_DOMAIN="libapt-pkg${APT_PKG_MAJOR}")

# Create the executable targets
add_executable(file file.cc)
add_executable(copy copy.cc)
add_executable(gzip gzip.cc)
add_executable(gpgv gpgv.cc)
add_executable(cdrom cdrom.cc)
add_executable(http http.cc rfc2553emu.cc connect.cc)
add_executable(https https.cc)
add_executable(ftp ftp.cc rfc2553emu.cc connect.cc)
add_executable(rred rred.cc)
add_executable(rsh rsh.cc)

# Link the executables against the libraries
target_link_libraries(file apt-pkg)
target_link_libraries(copy apt-pkg)
target_link_libraries(gzip apt-pkg)
target_link_libraries(gpgv apt-pkg)
target_link_libraries(cdrom apt-pkg)
target_link_libraries(http apt-pkg)
target_link_libraries(https apt-pkg curl)
target_link_libraries(ftp apt-pkg)
target_link_libraries(rred apt-pkg)
target_link_libraries(rsh apt-pkg)

# Install the library
install(TARGETS file copy gzip gpgv cdrom http https ftp rred rsh
        RUNTIME DESTINATION lib/apt/methods)
