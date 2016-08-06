install(PROGRAMS install setup update
        DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/dpkg/methods/apt)
install(FILES desc.apt names
        DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/dpkg/methods/apt)
