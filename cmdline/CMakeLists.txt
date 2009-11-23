# Create the executable tasks
add_executable(apt-cache apt-cache.cc)
add_executable(apt-get apt-get.cc acqprogress.cc)
add_executable(apt-config apt-config.cc)
add_executable(apt-cdrom apt-cdrom.cc)
add_executable(apt-sortpkgs apt-sortpkgs.cc)
add_executable(apt-extracttemplates apt-extracttemplates.cc)

# Link the executables against the libraries
target_link_libraries(apt-cache apt-pkg)
target_link_libraries(apt-get apt-pkg)
target_link_libraries(apt-config apt-pkg)
target_link_libraries(apt-cdrom apt-pkg)
target_link_libraries(apt-sortpkgs apt-pkg)
target_link_libraries(apt-extracttemplates apt-pkg apt-inst)

# Install the executables
install(TARGETS apt-cache apt-get apt-config apt-cdrom apt-sortpkgs
                apt-extracttemplates
		RUNTIME DESTINATION bin)

# Install the not-to-be-compiled programs
INSTALL(PROGRAMS apt-key apt-mark DESTINATION bin)

# Build the translations
FILE(GLOB programs "*.cc")
apt_add_translation_domain(apt "${programs}")
