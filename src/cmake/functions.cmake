# configure_deps(<target> <deps>...)
# <deps> is a list of dependencies.
# Checks if the dependency exists and configures include directories and linking libraries
function(configure_deps ARG_TARGET)
    set(ARG_DEPS ${ARGN})
    foreach (DEP IN LISTS ARG_DEPS)
        pkg_check_modules(${DEP} REQUIRED ${DEP})
        if (NOT ${DEP}_FOUND)
            message(FATAL_ERROR "Could not configure target ${ARG_TARGET} as ${DEP} was not found.")
        endif ()

        # set link directory, only required by apple with his weird library directories
        target_link_directories(${ARG_TARGET} PRIVATE ${${DEP}_LIBRARY_DIRS})
        # add the necessary includes
        target_include_directories(${ARG_TARGET} PRIVATE ${${DEP}_INCLUDE_DIRS})
        # link to the libraries
        target_link_libraries(${ARG_TARGET} ${${DEP}_LIBRARIES})
    endforeach (DEP)
endfunction(configure_deps)

# find_link(<target> <library> <PATH>...)
# <libraries> is a list of libraries.
# <PATH> find_library PATHS argument
# Checks if the library exists and calls target_link_libraries
function(find_link ARG_TARGET ARG_LIBRARY)
    set(ARG_PATHS ${ARGN})
    message(STATUS "Checking for library '${ARG_LIBRARY}'")
    find_library(${ARG_LIBRARY}_LIB NAMES ${ARG_LIBRARY} PATHS ${ARG_PATHS})
    if (NOT ${ARG_LIBRARY}_LIB)
        message(FATAL_ERROR "Could not link to target ${ARG_TARGET} as ${ARG_LIBRARY} was not found.")
    endif ()
    message(STATUS "  Found ${ARG_LIBRARY}")
    target_link_libraries(${ARG_TARGET} ${${ARG_LIBRARY}_LIB})
endfunction(find_link)
