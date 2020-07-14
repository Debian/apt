# translations.cmake - Translations using APT's translation system.
# Copyright (C) 2009, 2016 Julian Andres Klode <jak@debian.org>

function(apt_add_translation_domain)
    set(options)
    set(oneValueArgs DOMAIN)
    set(multiValueArgs TARGETS SCRIPTS EXCLUDE_LANGUAGES)
    cmake_parse_arguments(NLS "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    # Build the list of source files of the target
    set(files "")
    set(abs_files "")
    set(scripts "")
    set(abs_scripts "")
    set(mofiles)
    set(targets ${NLS_TARGETS})
    set(domain ${NLS_DOMAIN})
    set(xgettext_params
        --add-comments
        --foreign
        --package-name=${PROJECT_NAME}
        --package-version=${PACKAGE_VERSION}
        --msgid-bugs-address=${PACKAGE_MAIL}
    )
    foreach(source ${NLS_SCRIPTS})
            path_join(file "${CMAKE_CURRENT_SOURCE_DIR}" "${source}")
            file(RELATIVE_PATH relfile ${PROJECT_SOURCE_DIR} ${file})
            list(APPEND scripts ${relfile})
            list(APPEND abs_scripts ${file})
        endforeach()
    foreach(target ${targets})
        get_target_property(source_dir ${target} SOURCE_DIR)
        get_target_property(sources ${target} SOURCES)
        foreach(source ${sources})
            if (source MATCHES TARGET_OBJECTS)
                continue()
            endif()
            path_join(file "${source_dir}" "${source}")
            file(RELATIVE_PATH relfile ${PROJECT_SOURCE_DIR} ${file})
            set(files ${files} ${relfile})
            set(abs_files ${abs_files} ${file})
        endforeach()

        target_compile_definitions(${target} PRIVATE -DAPT_DOMAIN="${domain}")
    endforeach()

    if("${scripts}" STREQUAL "")
        set(sh_pot "/dev/null")
    else()
        set(sh_pot ${CMAKE_CURRENT_BINARY_DIR}/${domain}.sh.pot)
        # Create the template for this specific sub-domain
        add_custom_command (OUTPUT ${sh_pot}
            COMMAND xgettext ${xgettext_params} -L Shell
                             -o ${sh_pot} ${scripts}
            DEPENDS ${abs_scripts}
            VERBATIM
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        )
    endif()


    add_custom_command (OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${domain}.c.pot
        COMMAND xgettext ${xgettext_params} -k_ -kN_
                         --keyword=P_:1,2
                         -o ${CMAKE_CURRENT_BINARY_DIR}/${domain}.c.pot ${files}
        DEPENDS ${abs_files}
        VERBATIM
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    )

    # We are building a ${domain}.pot with a header for launchpad, but we also
    # build a ${domain.pot}-tmp as a byproduct. The msgfmt command than depend
    # on the byproduct while their target depends on the output, so that msgfmt
    # does not have to be rerun if nothing in the template changed.
    #
    # Make sure the .pot-tmp has no line numbers, to avoid useless rebuilding
    # of .mo files.
    add_custom_command (OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${domain}.pot
        BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/${domain}.pot-tmp
        COMMAND msgcomm --more-than=0 --omit-header --sort-by-file --add-location=file
                         ${sh_pot}
                         ${CMAKE_CURRENT_BINARY_DIR}/${domain}.c.pot
                         --output=${CMAKE_CURRENT_BINARY_DIR}/${domain}.pot-tmp0
        COMMAND msgcomm --more-than=0 --sort-by-file
                         ${sh_pot}
                         ${CMAKE_CURRENT_BINARY_DIR}/${domain}.c.pot
                         --output=${CMAKE_CURRENT_BINARY_DIR}/${domain}.pot
        COMMAND cmake -E copy_if_different
                         ${CMAKE_CURRENT_BINARY_DIR}/${domain}.pot-tmp0
                         ${CMAKE_CURRENT_BINARY_DIR}/${domain}.pot-tmp
        DEPENDS ${sh_pot}
                ${CMAKE_CURRENT_BINARY_DIR}/${domain}.c.pot
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    )

    # We need a target to depend on otherwise, the msgmerge might not get called
    # with the make generator
    add_custom_target(nls-${domain}-template DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${domain}.pot)

    # Build .mo files
    file(GLOB translations "${PROJECT_SOURCE_DIR}/po/*.po")
    list(SORT translations)
    foreach(file ${translations})
        get_filename_component(langcode ${file} NAME_WE)
        if ("${langcode}" IN_LIST NLS_EXCLUDE_LANGUAGES)
            continue()
        endif()
        set(outdir ${CMAKE_CURRENT_BINARY_DIR}/locale/${langcode}/LC_MESSAGES)
        file(MAKE_DIRECTORY ${outdir})
        # Command to merge and compile the messages. As explained in the custom
        # command for msgcomm, this depends on byproduct to avoid reruns
        add_custom_command(OUTPUT ${outdir}/${domain}.po
            COMMAND msgmerge -qo ${outdir}/${domain}.po ${file} ${CMAKE_CURRENT_BINARY_DIR}/${domain}.pot-tmp
            DEPENDS ${file} ${CMAKE_CURRENT_BINARY_DIR}/${domain}.pot-tmp
        )
        add_custom_command(OUTPUT ${outdir}/${domain}.mo
            COMMAND msgfmt --statistics -o ${outdir}/${domain}.mo  ${outdir}/${domain}.po
            DEPENDS ${outdir}/${domain}.po
        )

        set(mofiles ${mofiles} ${outdir}/${domain}.mo)
        install(FILES ${outdir}/${domain}.mo
                DESTINATION "${CMAKE_INSTALL_LOCALEDIR}/${langcode}/LC_MESSAGES")
    endforeach(file ${translations})

    add_custom_target(nls-${domain} ALL DEPENDS ${mofiles} nls-${domain}-template)
endfunction()

# Usage: apt_add_update_po(output domain [domain ...])
function(apt_add_update_po)
    set(options)
    set(oneValueArgs TEMPLATE)
    set(multiValueArgs DOMAINS EXCLUDE_LANGUAGES)
    cmake_parse_arguments(NLS "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    set(output ${CMAKE_CURRENT_SOURCE_DIR}/${NLS_TEMPLATE}.pot)
    foreach(domain ${NLS_DOMAINS})
        list(APPEND potfiles ${CMAKE_CURRENT_BINARY_DIR}/${domain}.pot)
    endforeach()

    get_filename_component(primary_name ${output} NAME_WE)
    add_custom_target(nls-${primary_name}
                       COMMAND msgcomm --sort-by-file --add-location=file
                                        --more-than=0 --output=${output}
                                ${potfiles}
                       DEPENDS ${potfiles})

    file(GLOB translations "${PROJECT_SOURCE_DIR}/po/*.po")
    if (NOT TARGET update-po)
        add_custom_target(update-po)
    endif()
    foreach(translation ${translations})
            get_filename_component(langcode ${translation} NAME_WE)
            if ("${langcode}" IN_LIST NLS_EXCLUDE_LANGUAGES)
                continue()
            endif()
            add_custom_target(update-po-${langcode}
                COMMAND msgmerge -q --previous --update --backup=none ${translation} ${output}
                DEPENDS nls-${primary_name}
            )
            add_dependencies(update-po update-po-${langcode})
    endforeach()
    add_dependencies(update-po nls-${primary_name})
endfunction()

function(apt_add_po_statistics excluded)
    add_custom_target(statistics)
    file(GLOB translations "${PROJECT_SOURCE_DIR}/po/*.po")
    foreach(translation ${translations})
            get_filename_component(langcode ${translation} NAME_WE)
            if ("${langcode}" IN_LIST excluded)
                add_custom_command(
                    TARGET statistics PRE_BUILD
                    COMMAND printf "%-6s " "${langcode}:"
                    COMMAND echo "ignored"
                    VERBATIM
                )
                continue()
            endif()
            add_custom_command(
                TARGET statistics PRE_BUILD
                COMMAND printf "%-6s " "${langcode}:"
                COMMAND msgfmt --statistics -o /dev/null ${translation}
                VERBATIM
            )
    endforeach()
endfunction()
