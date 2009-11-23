# Create the executable tasks
add_executable(apt-ftparchive apt-ftparchive.cc cachedb.cc writer.cc
                              contents.cc override.cc multicompress.cc)

# Link the executables against the libraries
target_link_libraries(apt-ftparchive apt-inst apt-pkg db)

# Install the executables
install(TARGETS apt-ftparchive RUNTIME DESTINATION bin)

# Translation support
apt_add_translation_domain(apt-ftparchive "apt-ftparchive.cc;cachedb.cc;"
                          "writer.cc;contents.cc;override.cc;multicompress.cc")
