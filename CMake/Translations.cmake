# translations.cmake - Translations using APT's translation system.
# Copyright (C) 2009, 2016 Julian Andres Klode <jak@debian.org>

function(apt_add_translation_domain domain)
    set(targets ${ARGN})
    # Build the list of source files of the target
    set(files "")
    set(abs_files "")
    foreach(target ${targets})
        get_target_property(source_dir ${target} SOURCE_DIR)
        get_target_property(sources ${target} SOURCES)
        foreach(source ${sources})
            string(SUBSTRING ${source} 0 1 init_char)
            string(COMPARE EQUAL ${init_char} "/" is_absolute)
            if (${is_absolute})
                set(file "${source}")
            else()
                set(file "${source_dir}/${source}")
            endif()
            file(RELATIVE_PATH relfile ${PROJECT_SOURCE_DIR} ${file})
            set(files ${files} ${relfile})
            set(abs_files ${abs_files} ${file})
        endforeach()

        target_compile_definitions(${target} PRIVATE -DAPT_DOMAIN="${domain}")
    endforeach()

    # Create the template for this specific sub-domain
    add_custom_command (OUTPUT ${PROJECT_BINARY_DIR}/${domain}.pot
        COMMAND xgettext --add-comments --foreign -k_ -kN_
                         -o ${PROJECT_BINARY_DIR}/${domain}.pot ${files}
        DEPENDS ${abs_files}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    )

    # Build .mo files
    file(GLOB translations "${PROJECT_SOURCE_DIR}/po/*.po")
    list(SORT translations)
    foreach(file ${translations})
        get_filename_component(langcode ${file} NAME_WE)
        set(outdir ${PROJECT_BINARY_DIR}/locale/${langcode}/LC_MESSAGES)
        file(MAKE_DIRECTORY ${outdir})
        # Command to merge and compile the messages
        add_custom_command(OUTPUT ${outdir}/${domain}.mo
            COMMAND msgmerge -qo - ${file} ${PROJECT_BINARY_DIR}/${domain}.pot |
                    msgfmt -o ${outdir}/${domain}.mo -
            DEPENDS ${file} ${PROJECT_BINARY_DIR}/${domain}.pot
        )

        set(mofiles ${mofiles} ${outdir}/${domain}.mo)
        install(FILES ${outdir}/${domain}.mo
                DESTINATION "${CMAKE_INSTALL_LOCALEDIR}/${langcode}/LC_MESSAGES")
    endforeach(file ${translations})

    add_custom_target(nls-${domain} ALL DEPENDS ${mofiles})
endfunction()
