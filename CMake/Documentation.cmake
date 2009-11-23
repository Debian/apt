# Copyright (C) 2009 Julian Andres Klode <jak@debian.org>.
# Licensed under the same terms as APT; i.e. GPL 2 or later.
# TODO: Integrate PO4A translations

macro(add_debiandoc target sourcefiles installdest)
	foreach(file ${sourcefiles})
		get_filename_component(relfile ${file} NAME)
		string(REPLACE ".sgml" "" manual ${relfile})
		get_filename_component(absolute ${file} ABSOLUTE)
		
		add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${manual}.html
			COMMAND debiandoc2html ${absolute}
			WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
			DEPENDS ${file}
		)
		set(commands ${commands} ${CMAKE_CURRENT_BINARY_DIR}/${manual}.html)
		if (NOT ${installdest} EQUAL "" )
		install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${manual}.html
			DESTINATION ${installdest})
		endif (NOT ${installdest} EQUAL "" )
	endforeach(file ${sourcefiles})

	add_custom_target(${target} ALL DEPENDS ${commands})
endmacro(add_debiandoc target sourcefiles installdest)


# Macro for XML man pages.
macro(add_xml_manpages target manpages)
	foreach(manpage ${manpages})
		string(LENGTH ${manpage} manpage_length)
		math(EXPR manpage_length ${manpage_length}-1)
		string(SUBSTRING ${manpage} ${manpage_length} 1 section)

		add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${manpage}
			COMMAND xmlto man ${CMAKE_CURRENT_SOURCE_DIR}/${manpage}.xml
			WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${manpage}.xml
		)
		set(commands ${commands} ${CMAKE_CURRENT_BINARY_DIR}/${manpage})
		
		install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${manpage}
				DESTINATION share/man/man${section})
	endforeach(manpage ${manpages})
	
	add_custom_target(${target} ALL DEPENDS ${commands})
endmacro(add_xml_manpages manpages)
