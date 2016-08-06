include(CheckCXXCompilerFlag)

# Flatten our header structure
function(flatify target headers)
    foreach(header ${headers})
        get_filename_component(tgt ${header} NAME)
        configure_file(${header} ${target}/${tgt} @ONLY)
    endforeach(header ${headers})
endfunction()


function(add_optional_compile_options flags)
    foreach(flag ${flags})
        check_cxx_compiler_flag(-${flag} have-compiler-flag:-${flag})
        if (have-compiler-flag:-${flag})
            add_compile_options("-${flag}")
        endif()
    endforeach()
endfunction()

# Substitute vendor references in a file
function(add_vendor_file)
    set(options)
    set(oneValueArgs OUTPUT INPUT MODE)
    set(multiValueArgs VARIABLES)
    cmake_parse_arguments(AVF "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    message(STATUS "Configuring vendor file ${AVF_OUTPUT}")

    FILE(READ ${CMAKE_CURRENT_SOURCE_DIR}/${AVF_INPUT} input)
    foreach(variable ${AVF_VARIABLES})
        execute_process(COMMAND ../vendor/getinfo ${variable} OUTPUT_VARIABLE value OUTPUT_STRIP_TRAILING_WHITESPACE)
        string(REPLACE "&${variable};" "${value}" input "${input}")
    endforeach()
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${AVF_OUTPUT} "${input}")

    execute_process(COMMAND chmod ${AVF_MODE} ${CMAKE_CURRENT_BINARY_DIR}/${AVF_OUTPUT})
endfunction()

# Add symbolic links to a file
function(add_slaves destination master)
    set(slaves "")
    foreach(slave ${ARGN})
        add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${slave}
                           COMMAND ${CMAKE_COMMAND} -E create_symlink ${master} ${CMAKE_CURRENT_BINARY_DIR}/${slave})
        list(APPEND slaves ${CMAKE_CURRENT_BINARY_DIR}/${slave})
    endforeach()

    STRING(REPLACE "/" "-" master "${master}")
    add_custom_target(${master}-slaves ALL DEPENDS ${slaves})
    install(FILES ${slaves} DESTINATION ${destination})
endfunction()

# Generates a simple version script versioning everything with current SOVERSION
function(add_version_script target)
    get_target_property(soversion ${target} SOVERSION)
    set(script "${CMAKE_CURRENT_BINARY_DIR}/${target}.versionscript")
    string(REPLACE "-" "" name "${target}_${soversion}")
    string(TOUPPER "${name}" name)
    add_custom_command(OUTPUT "${script}"
                       COMMAND echo "${name} {global: *; };" > "${script}"
                       VERBATIM )
    add_custom_target(${target}-versionscript DEPENDS "${script}")
    target_link_libraries(${target} PRIVATE -Wl,-version-script="${script}")
    add_dependencies(${target} ${target}-versionscript)
endfunction()
