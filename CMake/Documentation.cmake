# Copyright (C) 2009, 2016 Julian Andres Klode <jak@debian.org>.
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

function(add_docbook target sourcefiles installdest)
    foreach(file ${sourcefiles})
        get_filename_component(relfile ${file} NAME)
        string(REPLACE ".dbk" "" manual ${relfile})
        get_filename_component(absolute ${file} ABSOLUTE)

        add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${manual}.html/
            COMMAND xsltproc --nonet --novalid --xinclude
                             --stringparam base.dir ${CMAKE_CURRENT_BINARY_DIR}/${manual}.html/
                             --path ${CMAKE_CURRENT_SOURCE_DIR}/../vendor/${CURRENT_VENDOR}/
                             --path ${CMAKE_CURRENT_SOURCE_DIR}/
                             ${CMAKE_CURRENT_SOURCE_DIR}/docbook-html-style.xsl
                             ${absolute}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            DEPENDS ${file}
        )
        set(commands ${commands} ${CMAKE_CURRENT_BINARY_DIR}/${manual}.html)
        if (NOT ${installdest} EQUAL "" )
        install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${manual}.html
            DESTINATION ${installdest})
        endif()
    endforeach(file ${sourcefiles})

    add_custom_target(${target} ALL DEPENDS ${commands})
endfunction()


function(add_po4a type master po target deps)
    add_custom_command(OUTPUT ${target}
        COMMAND po4a-translate --keep 0 -f ${type} -m ${master}
                               -p ${po} -l ${target}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS ${deps} ${master} ${po})
endfunction()


# Macro for XML man pages.
function(add_xml_manpages target manpages translations entities)
    foreach(manpage ${manpages})
        string(LENGTH ${manpage} manpage_length)
        math(EXPR manpage_length ${manpage_length}-1)
        string(SUBSTRING ${manpage} ${manpage_length} 1 section)

        add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${manpage}
                COMMAND xsltproc --path ${CMAKE_CURRENT_SOURCE_DIR}/../vendor/${CURRENT_VENDOR}/
                                 --path ${CMAKE_CURRENT_SOURCE_DIR}/
                                 ${CMAKE_CURRENT_SOURCE_DIR}/manpage-style.xsl
                                 ${CMAKE_CURRENT_SOURCE_DIR}/${manpage}.xml
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${manpage}.xml
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/manpage-style.xsl
        )


        set(commands ${commands} ${CMAKE_CURRENT_BINARY_DIR}/${manpage})

        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${manpage}
                DESTINATION ${CMAKE_INSTALL_MANDIR}/man${section})

        # Add the translations for the manpage.
        foreach(translation ${translations})
            set(entities)
            # transdir = shortcut to the output directory for translations.
            set(transdir ${CMAKE_CURRENT_BINARY_DIR}/${translation})

            add_po4a(docbook ${manpage}.xml po/${translation}.po
                             ${transdir}/${manpage}.xml "${ent_cmds}")


            add_custom_command(OUTPUT ${transdir}/${manpage}
                COMMAND xsltproc --path ${CMAKE_CURRENT_SOURCE_DIR}/../vendor/${CURRENT_VENDOR}/
                                 --path ${CMAKE_CURRENT_SOURCE_DIR}/
                                 --stringparam l10n.gentext.default.language ${translation}
                                 ${CMAKE_CURRENT_SOURCE_DIR}/manpage-style.xsl
                                 ${transdir}/${manpage}.xml
                WORKING_DIRECTORY ${transdir}
                DEPENDS ${transdir}/${manpage}.xml
                DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/manpage-style.xsl)

            set(nls-cmd ${nls-cmd} ${transdir}/${manpage})
            install(FILES ${transdir}/${manpage}
                    DESTINATION ${CMAKE_INSTALL_MANDIR}/${translation}/man${section})

        endforeach(translation ${translations})
    endforeach(manpage ${manpages})

    add_custom_target(${target} ALL DEPENDS ${commands})
    # Sort the list of the translations.
    list(SORT nls-cmd)
    add_custom_target(nls-${target} ALL DEPENDS ${nls-cmd})
endfunction()


function(add_manpages target manpages translations)
    foreach(man ${manpages})
        string(LENGTH ${man} manpage_length)
        math(EXPR manpage_length ${manpage_length}-1)
        string(SUBSTRING ${man} ${manpage_length} 1 section)
        install(FILES ${man} DESTINATION ${CMAKE_INSTALL_MANDIR}/man${section})

        if (USE_NLS)
            foreach(translation ${translations})
                set(transdir ${CMAKE_CURRENT_BINARY_DIR}/${translation})
                add_po4a(man ${man} po/${translation}.po ${transdir}/${man} "")
                install(FILES ${transdir}/${man}
                        DESTINATION ${CMAKE_INSTALL_MANDIR}/${translation}/man${section})
                set(files ${files} ${transdir}/${man})
            endforeach(translation ${translations})
        endif()
    endforeach(man ${manpages})
    add_custom_target(${target} ALL DEPENDS ${files})
endfunction()
