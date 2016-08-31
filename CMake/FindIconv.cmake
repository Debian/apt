find_path(ICONV_INCLUDE_DIR NAMES iconv.h)

find_library(ICONV_LIBRARY NAMES iconv)
if (ICONV_LIBRARY)
    set(ICONV_SYMBOL_FOUND "${ICONV_LIBRARY}")
else()
    check_function_exists(iconv_open ICONV_SYMBOL_FOUND)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Iconv DEFAULT_MESSAGE ICONV_INCLUDE_DIR ICONV_SYMBOL_FOUND)

if(ICONV_LIBRARY)
    set(ICONV_LIBRARIES "${ICONV_LIBRARY}")
else()
    set(ICONV_LIBRARIES)
endif()
set(ICONV_INCLUDE_DIRS "${ICONV_INCLUDE_DIR}")

mark_as_advanced(ICONV_LIBRARY ICONV_INCLUDE_DIR)
