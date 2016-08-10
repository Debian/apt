install(FILES apt.conf apt-https-method-example.conf configure-index preferences
        DESTINATION ${CMAKE_INSTALL_DOCDIR}/examples)
install(FILES apt-ftparchive.conf ftp-archive.conf
        DESTINATION ${CMAKE_INSTALL_DOCDIR}/../apt-utils/examples)
