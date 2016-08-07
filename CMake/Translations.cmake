# translations.cmake - Translations using APT's translation system.
# Copyright (C) 2009, 2016 Julian Andres Klode <jak@debian.org>

function(apt_add_translation_domain)
    set(options)
    set(oneValueArgs DOMAIN)
    set(multiValueArgs TARGETS SCRIPTS)
    cmake_parse_arguments(NLS "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    # Build the list of source files of the target
    set(files "")
    set(abs_files "")
    set(scripts "")
    set(abs_scripts "")
    set(targets ${NLS_TARGETS})
    set(domain ${NLS_DOMAIN})
    foreach(source ${NLS_SCRIPTS})
            string(SUBSTRING ${source} 0 1 init_char)
            string(COMPARE EQUAL ${init_char} "/" is_absolute)
            if (${is_absolute})
                set(file "${source}")
            else()
                set(file "${CMAKE_CURRENT_SOURCE_DIR}/${source}")
            endif()
            file(RELATIVE_PATH relfile ${PROJECT_SOURCE_DIR} ${file})
            list(APPEND scripts ${relfile})
            list(APPEND abs_scripts ${file})
        endforeach()
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

    if("${scripts}" STREQUAL "")
        set(sh_pot "/dev/null")
    else()
        set(sh_pot ${PROJECT_BINARY_DIR}/${domain}.sh.pot)
        # Create the template for this specific sub-domain
        add_custom_command (OUTPUT ${sh_pot}
            COMMAND xgettext --add-comments --foreign -L Shell
                             -o ${sh_pot} ${scripts}
            DEPENDS ${abs_scripts}
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        )
    endif()


    add_custom_command (OUTPUT ${PROJECT_BINARY_DIR}/${domain}.c.pot
        COMMAND xgettext --add-comments --foreign -k_ -kN_
                         --keyword=P_:1,2
                         -o ${PROJECT_BINARY_DIR}/${domain}.c.pot ${files}
        DEPENDS ${abs_files}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    )

    add_custom_command (OUTPUT ${PROJECT_BINARY_DIR}/${domain}.pot
        COMMAND msgcomm --more-than=0 --sort-by-file
                         ${sh_pot}
                         ${PROJECT_BINARY_DIR}/${domain}.c.pot
                         --output=${PROJECT_BINARY_DIR}/${domain}.pot
        DEPENDS ${sh_pot}
                ${PROJECT_BINARY_DIR}/${domain}.c.pot
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
        add_custom_command(OUTPUT ${outdir}/${domain}.po
            COMMAND msgmerge -qo ${outdir}/${domain}.po ${file} ${PROJECT_BINARY_DIR}/${domain}.pot
            DEPENDS ${file} ${PROJECT_BINARY_DIR}/${domain}.pot
        )
        add_custom_command(OUTPUT ${outdir}/${domain}.mo
            COMMAND msgfmt --statistics -o ${outdir}/${domain}.mo  ${outdir}/${domain}.po
            DEPENDS ${outdir}/${domain}.po
        )

        set(mofiles ${mofiles} ${outdir}/${domain}.mo)
        install(FILES ${outdir}/${domain}.mo
                DESTINATION "${CMAKE_INSTALL_LOCALEDIR}/${langcode}/LC_MESSAGES")
    endforeach(file ${translations})

    add_custom_target(nls-${domain} ALL DEPENDS ${mofiles})
endfunction()
