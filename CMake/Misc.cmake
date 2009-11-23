# Flatten our header structure
macro(flatify target headers)
	foreach(header ${headers})
		get_filename_component(tgt ${header} NAME)
		configure_file(${header} ${target}/${tgt} @ONLY)
	endforeach(header ${headers})
endmacro(flatify target headers)
