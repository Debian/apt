include(Documentation)

set(LINGUAS
    de
    es
    fr
    it
    ja
    nl
    pl
    pt_BR
    pt
)

set(ENTITIES
    apt.ent
    apt-verbatim.ent
    ../vendor/${CURRENT_VENDOR}/apt-vendor.ent
)

if(WITH_DOC)
add_docbook(apt-doc HTML TEXT ALL
    DOCUMENTS
        guide.dbk
        offline.dbk
    INSTALL ${CMAKE_INSTALL_DOCDIR}/../apt-doc
    LINGUAS ${LINGUAS}
    DEPENDS ${ENTITIES}
)

add_docbook(libapt-pkg-doc HTML TEXT ALL
    DOCUMENTS
        design.dbk
        dpkg-tech.dbk
        files.dbk
        method.dbk
    INSTALL ${CMAKE_INSTALL_DOCDIR}/../libapt-pkg-doc
    LINGUAS ${LINGUAS}
    DEPENDS ${ENTITIES}
)

find_package(Doxygen)

file(GLOB_RECURSE apt_pkg "../apt-pkg/*.h" "../apt-pkg/*.cc")
configure_file(Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/doxygen.stamp
    COMMAND doxygen ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
    COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/doxygen.stamp
    DEPENDS ${apt_pkg} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
)

add_custom_target(apt-doxygen ALL
    DEPENDS ${PROJECT_BINARY_DIR}/doc/doxygen.stamp
)

install(DIRECTORY  ${PROJECT_BINARY_DIR}/doc/doxygen/html
        DESTINATION ${CMAKE_INSTALL_DOCDIR}/../libapt-pkg-doc)

endif()

add_docbook(apt-man MANPAGE ALL
    DOCUMENTS
        apt.8.xml
        apt-cache.8.xml
        apt-cdrom.8.xml
        apt.conf.5.xml
        apt-config.8.xml
        apt-extracttemplates.1.xml
        apt-ftparchive.1.xml
        apt-get.8.xml
        apt-key.8.xml
        apt-mark.8.xml
        apt_preferences.5.xml
        apt-secure.8.xml
        apt-sortpkgs.1.xml
        sources.list.5.xml
    DEPENDS ${ENTITIES}
    LINGUAS ${LINGUAS}
)

add_update_po4a(update-po4a po/${PROJECT_NAME}-doc.pot "po/header.pot.txt")

add_subdirectory(examples)
