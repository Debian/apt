# po4a/docbook documentation support for CMake
# - see documentation of add_docbook()
#
# Copyright (C) 2016 Julian Andres Klode <jak@debian.org>.
#
# Permission is hereby granted, free of charge, to any person
# obtaining a copy of this software and associated documentation files
# (the "Software"), to deal in the Software without restriction,
# including without limitation the rights to use, copy, modify, merge,
# publish, distribute, sublicense, and/or sell copies of the Software,
# and to permit persons to whom the Software is furnished to do so,
# subject to the following conditions:
#
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
# BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
# ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.


# Split up a string of the form DOCUMENT[.DOCUMENT][.LANGUAGE][.SECTION].EXTENSION
#
# There might be up to two parts in the document name. The language must be
# a two char language code like de, or a 5 char code of the form de_DE.
function(po4a_components doc lang sec ext translated_full_document)
    get_filename_component(name ${translated_full_document} NAME)
    string(REPLACE "." ";" name "${name}")          # Make it a list

    list(GET name 0 _doc)   # First element is always the document
    list(GET name 1 _lang)  # Second *might* be a language
    list(GET name -2 _sec)  # Second-last *might* be a section
    list(GET name -1 _ext)  # Last element is always the file type

    # If the language code is neither a file type, nor a section, nor a language
    # assume it is part of the file name and use the next component as the lang.
    if(_lang AND NOT _lang MATCHES "^(xml|dbk|[0-9]|[a-z][a-z]|[a-z][a-z]_[A-Z][A-Z])$")
        set(_doc "${_doc}.${_lang}")
        list(GET name 2 _lang)
    endif()
    # If no language is present, we get a section; both not present => type
    if(_lang MATCHES "xml|dbk|[0-9]")
        set(_lang "")
    endif()
    if(NOT _sec MATCHES "^[0-9]$")        # A (manpage) section must be a number
        set(_sec "")
    endif()

    set(${doc} ${_doc} PARENT_SCOPE)
    set(${lang} ${_lang} PARENT_SCOPE)
    set(${sec} ${_sec} PARENT_SCOPE)
    set(${ext} ${_ext} PARENT_SCOPE)
endfunction()


# Process one document
function(po4a_one stamp_out out full_document language deps)
    path_join(full_path "${CMAKE_CURRENT_SOURCE_DIR}" "${full_document}")
    po4a_components(document _ section ext "${full_document}")

    # Calculate target file name
    set(dest "${language}/${document}.${language}")
    if(section)
        set(dest "${dest}.${section}")
    endif()

    # po4a might drop files not translated enough, so build a stamp file
    set(stamp ${CMAKE_CURRENT_BINARY_DIR}/${dest}.po4a-stamp)
    add_custom_command(
        OUTPUT ${stamp}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/${language}
        COMMAND po4a --previous --no-backups
                     --package-name='${PROJECT}-doc'
                     --package-version='${PACKAGE_VERSION}'
                     --msgid-bugs-address='${PACKAGE_MAIL}'
                     --translate-only ${dest}.${ext}
                     --srcdir ${CMAKE_CURRENT_SOURCE_DIR}
                     --destdir ${CMAKE_CURRENT_BINARY_DIR}
                      ${CMAKE_CURRENT_SOURCE_DIR}/po4a.conf
        COMMAND ${CMAKE_COMMAND} -E touch ${stamp}
        COMMENT "Generating ${dest}.${ext} (or dropping it)"
        DEPENDS ${full_document} ${deps} po/${language}.po
    )
    # Return result
    set(${stamp_out} ${stamp} PARENT_SCOPE)
    set(${out} ${CMAKE_CURRENT_BINARY_DIR}/${dest}.${ext} PARENT_SCOPE)
endfunction()

function(xsltproc_one)
    set(generated "")
    set(options HTML TEXT MANPAGE)
    set(oneValueArgs STAMP STAMP_OUT FULL_DOCUMENT)
    set(multiValueArgs INSTALL DEPENDS)
    cmake_parse_arguments(DOC "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    po4a_components(document language section ext "${DOC_FULL_DOCUMENT}")

    # Default parameters
    set(params
        --nonet
        --xinclude
        --stringparam chunk.quietly yes
        --stringparam man.output.quietly yes
        --path ${PROJECT_SOURCE_DIR}/vendor/${CURRENT_VENDOR}/
        --path ${CMAKE_CURRENT_SOURCE_DIR}/
    )

    # Parameters if localized
    if(language)
        list(APPEND params -stringparam l10n.gentext.default.language ${language})
    endif()

    path_join(full_input_path ${CMAKE_CURRENT_SOURCE_DIR} ${DOC_FULL_DOCUMENT})

    if (DOC_MANPAGE)
        if (language)
        set(manpage_output "${CMAKE_CURRENT_BINARY_DIR}/${language}/${document}.${section}")
        else()
        set(manpage_output "${CMAKE_CURRENT_BINARY_DIR}/${document}.${section}")
        endif()
        set(manpage_stylesheet "${CMAKE_CURRENT_SOURCE_DIR}/manpage-style.xsl")

        install(FILES ${manpage_output}
                DESTINATION ${CMAKE_INSTALL_MANDIR}/${language}/man${section}
                OPTIONAL)
    endif()
    if (DOC_HTML)
        if (language)
        set(html_output "${CMAKE_CURRENT_BINARY_DIR}/${language}/${document}.${language}.html")
        else()
        set(html_output "${CMAKE_CURRENT_BINARY_DIR}/${document}.html")
        endif()
        set(html_params --stringparam base.dir ${html_output})
        set(html_stylesheet "${CMAKE_CURRENT_SOURCE_DIR}/docbook-html-style.xsl")
        install(DIRECTORY ${html_output}
                DESTINATION ${DOC_INSTALL}
                OPTIONAL)

    endif()
    if (DOC_TEXT)
        if (language)
        set(text_output "${CMAKE_CURRENT_BINARY_DIR}/${language}/${document}.${language}.text")
        else()
        set(text_output "${CMAKE_CURRENT_BINARY_DIR}/${document}.text")
        endif()
        set(text_params --stringparam base.dir ${text_output})
        set(text_stylesheet "${CMAKE_CURRENT_SOURCE_DIR}/docbook-text-style.xsl")

        file(RELATIVE_PATH text_output_relative ${CMAKE_CURRENT_BINARY_DIR} ${text_output})

        add_custom_command(OUTPUT ${text_output}.w3m-stamp
                            COMMAND ${PROJECT_SOURCE_DIR}/CMake/run_if_exists.sh
                                    --stdout ${text_output}
                                    ${text_output}.html
                                    env LC_ALL=C.UTF-8 w3m -cols 78 -dump
                                    -o display_charset=UTF-8
                                    -no-graph -T text/html ${text_output}.html
                            COMMAND ${CMAKE_COMMAND} -E touch ${text_output}.w3m-stamp
                            COMMENT "Generating ${text_output_relative} (if not dropped by po4a)"
                            DEPENDS "${text_output}.html.xsltproc-stamp"
                            )
        list(APPEND generated ${text_output}.w3m-stamp)

        install(FILES ${text_output}
                DESTINATION ${DOC_INSTALL}
                OPTIONAL)
        set(text_output "${text_output}.html")
    endif()

    foreach(type in manpage html text)
        if (NOT ${type}_output)
            continue()
        endif()

        set(output ${${type}_output})
        set(stylesheet ${${type}_stylesheet})
        set(type_params ${${type}_params})
        file(RELATIVE_PATH output_relative ${CMAKE_CURRENT_BINARY_DIR} ${output})

        add_custom_command(OUTPUT ${output}.xsltproc-stamp
                COMMAND ${PROJECT_SOURCE_DIR}/CMake/run_if_exists.sh
                        ${full_input_path}
                        xsltproc ${params} ${type_params} -o ${output}
                                 ${stylesheet}
                                 ${full_input_path}
                COMMAND ${CMAKE_COMMAND} -E touch ${output}.xsltproc-stamp
                COMMENT "Generating ${output_relative} (if not dropped by po4a)"
                DEPENDS ${DOC_STAMP} ${DOC_DEPENDS})

        list(APPEND generated ${output}.xsltproc-stamp)
    endforeach()

    set(${DOC_STAMP_OUT} ${generated} PARENT_SCOPE)
endfunction()


# add_docbook(Name [ALL] [HTML] [TEXT] [MANPAGE]
#             [INSTALL install dir]
#             [DEPENDS depend ...]
#             [DOCUMENTS documents ...]
#             [LINGUAS lingua ...])
#
# Generate a target called name with all the documents being converted to
# the chosen output formats and translated to the chosen languages using po4a.
#
# For the translation support, the po4a.conf must be written so that
# translations for a document guide.xml are written to LANG/guide.LANG.xml,
# and for a manual page man.5.xml to a file called LANG/man.LANG.5.xml.
#
# The guide and manual page names may also contain a second component separated
# by a dot, it must however not be a valid language code.
#
# Note that po4a might chose not to generate a translated manual page for a
# given language if the translation rate is not high enough. We deal with this
# by creating stamp files.
function(add_docbook target)
    set(generated "")
    set(options HTML TEXT MANPAGE ALL)
    set(multiValueArgs INSTALL DOCUMENTS LINGUAS DEPENDS)
    cmake_parse_arguments(DOC "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if (DOC_HTML)
        list(APPEND formats HTML)
    endif()
    if (DOC_TEXT)
        list(APPEND formats TEXT)
    endif()
    if (DOC_MANPAGE)
        list(APPEND formats MANPAGE)
    endif()

    foreach(document ${DOC_DOCUMENTS})
        foreach(lang ${DOC_LINGUAS})
            po4a_one(po4a_stamp po4a_out ${document} "${lang}" "${DOC_DEPENDS}")
            xsltproc_one(STAMP_OUT xslt_stamp
                         STAMP ${po4a_stamp}
                         FULL_DOCUMENT ${po4a_out}
                         INSTALL ${DOC_INSTALL}
                         ${formats})

            list(APPEND stamps ${xslt_stamp})
        endforeach()
            xsltproc_one(STAMP_OUT xslt_stamp
                         STAMP ${document}
                         FULL_DOCUMENT ${document}
                         INSTALL ${DOC_INSTALL}
                         ${formats})

            list(APPEND stamps ${xslt_stamp})
    endforeach()

    if (DOC_ALL)
        add_custom_target(${target} ALL DEPENDS ${stamps})
    else()
        add_custom_target(${target} DEPENDS ${stamps})
    endif()
endfunction()

# Add an update-po4a target
function(add_update_po4a target pot header)
    set(WRITE_HEADER "")

    if (header)
        set(WRITE_HEADER
            COMMAND sed -n "/^\#$/,$p" ${pot} > ${pot}.headerfree
            COMMAND cat ${header} ${pot}.headerfree > ${pot}
            COMMAND rm ${pot}.headerfree
        )
    endif()
    add_custom_target(${target}
        COMMAND po4a --previous --no-backups --force --no-translations
                --msgmerge-opt --add-location=file
                --porefs noline,wrap
                --package-name=${PROJECT_NAME}-doc --package-version=${PACKAGE_VERSION}
                --msgid-bugs-address=${PACKAGE_MAIL} po4a.conf
        ${WRITE_HEADER}
        VERBATIM
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endfunction()
