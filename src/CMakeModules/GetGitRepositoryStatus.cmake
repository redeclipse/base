# - Contains functions to check local Git repository status
#
#  it_repository_dirty(<statevar>)
#
# Returns true if files in the Git repository have been modified, returns
# false otherwise. Also returns false when Git executable cannot be found.
#
# Copyright 2016 TheAssassin. Published under the terms of the zlib license
# (you can find a copy in the file LICENSE in the directory doc/).


function(git_repository_dirty _var)
    if(NOT GIT_FOUND)
        find_package(Git QUIET)
    endif()

    if(NOT GIT_FOUND)
        set(${_var} false)
        return()
    endif()

    execute_process(COMMAND
        "${GIT_EXECUTABLE}"
        diff-index
        --quiet
        HEAD
        ${ARGN}
        WORKING_DIRECTORY
        "${CMAKE_CURRENT_SOURCE_DIR}"
        RESULT_VARIABLE
        res
        ERROR_QUIET)

    if(NOT res EQUAL 0)
        set(${_var} true)
        return()
    endif()

    set(${_var} false)
endfunction()
