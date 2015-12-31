if (NOT MSVC)
    # sqlite3 produces warnings, so don't treat them as errors (assume that if compiles correctly)
    set_source_files_properties(sqlite3.c PROPERTIES COMPILE_FLAGS -Wno-error)
endif()
add_library(sqlite3 sqlite3.c)
if (UNIX AND NOT APPLE)
    # needs dl library
    target_link_libraries(sqlite3 dl)
endif()
