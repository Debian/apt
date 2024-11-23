# Create the executable tasks
add_executable(apt apt.cc)
add_executable(apt-cache apt-cache.cc)
add_executable(apt-get apt-get.cc)
add_executable(apt-mark apt-mark.cc)
add_executable(apt-config apt-config.cc)
add_executable(apt-cdrom apt-cdrom.cc)
add_executable(apt-helper apt-helper.cc)
add_executable(apt-sortpkgs apt-sortpkgs.cc)
add_executable(apt-extracttemplates apt-extracttemplates.cc)
add_executable(apt-internal-solver apt-internal-solver.cc)
add_executable(apt-dump-solver apt-dump-solver.cc)
add_executable(apt-internal-planner apt-internal-planner.cc)

# Link the executables against the libraries
target_link_libraries(apt apt-pkg apt-private)
target_link_libraries(apt-cache apt-pkg apt-private)
target_link_libraries(apt-get apt-pkg apt-private)
target_link_libraries(apt-config apt-pkg apt-private)
target_link_libraries(apt-cdrom apt-pkg apt-private)
target_link_libraries(apt-helper apt-pkg apt-private)
target_link_libraries(apt-mark apt-pkg apt-private)
target_link_libraries(apt-sortpkgs apt-pkg apt-private)
target_link_libraries(apt-extracttemplates apt-pkg apt-private)
target_link_libraries(apt-internal-solver apt-pkg apt-private)
target_link_libraries(apt-dump-solver apt-pkg apt-private)
target_link_libraries(apt-internal-planner apt-pkg apt-private)

set_target_properties(apt-dump-solver
                      PROPERTIES RUNTIME_OUTPUT_DIRECTORY solvers
                                 RUNTIME_OUTPUT_NAME dump)
set_target_properties(apt-internal-solver
                      PROPERTIES RUNTIME_OUTPUT_DIRECTORY solvers
                                 RUNTIME_OUTPUT_NAME apt)
set_target_properties(apt-internal-planner
                      PROPERTIES RUNTIME_OUTPUT_DIRECTORY planners
                                 RUNTIME_OUTPUT_NAME apt)

# Install the executables
install(TARGETS apt apt-cache apt-get apt-config apt-cdrom apt-mark apt-sortpkgs
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(TARGETS apt-helper apt-extracttemplates RUNTIME DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/apt/)
install(TARGETS apt-dump-solver apt-internal-solver RUNTIME DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/apt/solvers)
install(TARGETS apt-internal-planner RUNTIME DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/apt/planners)

add_links(${CMAKE_INSTALL_LIBEXECDIR}/apt/planners ../solvers/dump planners/dump)
add_links(${CMAKE_INSTALL_LIBEXECDIR}/apt/solvers apt solver3)
