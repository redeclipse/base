cmake_minimum_required(VERSION 2.8.11)

# link_deps([FATAL] DEPS <deps>...)
# Calls link_directories for <deps>. <deps> is a list of dependencies.
# If FATAL is specified then it exits on failure. Otherwise it returns
# and sets link_deps_failed.
function(link_deps_dirs)
    cmake_parse_arguments(link_deps_dirs "FATAL" "TARGET" "DEPS" "${ARGN}")
    foreach(dep IN LISTS link_deps_dirs_DEPS)
        if(${link_deps_dirs_FATAL})
            pkg_check_modules(${dep} REQUIRED ${dep})
        else()
            pkg_check_modules(${dep} ${dep})
        endif()
        if(NOT ${dep}_FOUND)
            set(link_deps_dirs_failed true PARENT_SCOPE)
            return()
        endif()

        # tell the compiler where to find the libraries' binaries
        foreach(lib_dir IN LISTS ${dep}_LIBRARY_DIRS)
            message("library dir: ${lib_dir}")
            link_directories(${lib_dir})
        endforeach(lib_dir)
    endforeach(dep)
endfunction(link_deps_dirs)

# configure_deps(TARGET <target> [FATAL] DEPS <deps>...)
# Configures dependencies for <target>. <deps> is a list of dependencies.
# If FATAL is specified then it exits on failure. Otherwise it returns
# and sets configure_deps_failed.
function(configure_deps)
    cmake_parse_arguments(configure_deps "FATAL" "TARGET" "DEPS" "${ARGN}")
    foreach(dep IN LISTS configure_deps_DEPS)
        if(${configure_deps_FATAL})
            pkg_check_modules(${dep} REQUIRED ${dep})
        else()
            pkg_check_modules(${dep} ${dep})
        endif()
        if(NOT ${dep}_FOUND)
            set(configure_deps_failed true PARENT_SCOPE)
            return()
        endif()

        # add the necessary includes
        foreach(include_dir IN LISTS ${dep}_INCLUDE_DIRS)
            include_directories(${include_dir})
        endforeach(include_dir)

        # link to the libraries
        foreach(lib IN LISTS ${dep}_LIBRARIES)
            target_link_libraries(${configure_deps_TARGET} ${lib})
        endforeach(lib)
    endforeach(dep)
endfunction(configure_deps)

# set default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type (release or debug)" FORCE)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# set compile options
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fsigned-char -ffast-math -fno-exceptions -fno-rtti -Wno-invalid-offsetof")

# Use pkg-config to configure dependencies later
find_package(PkgConfig REQUIRED)
# the dependencies required by all platforms
set(client_deps zlib sdl2 SDL2_image SDL2_mixer gl)
set(server_deps zlib)
set(genkey_deps zlib)

set(BUILD_CLIENT true CACHE BOOL "Build client?")
if(NOT ${BUILD_CLIENT})
    message(STATUS "Will not build client as requested")
endif()

set(WANT_STEAM true CACHE BOOL "Build with Steam support?")
if(NOT ${WANT_STEAM})
    message(STATUS "Will not build Steam support as requested")
endif()

set(WANT_DISCORD true CACHE BOOL "Build with Discord support?")
if(NOT ${WANT_DISCORD})
    message(STATUS "Will not build Discord support as requested")
endif()

# the client depends on almost all the source files
file(GLOB client_sources engine/*.cpp game/*.cpp shared/*.cpp support/sqlite3.c)

# the server requires less source files
file(GLOB server_sources
    shared/crypto.cpp
    shared/geom.cpp
    shared/stream.cpp
    shared/tools.cpp
    shared/zip.cpp
    support/sqlite3.c
    engine/cdpi.cpp
    engine/command.cpp
    engine/irc.cpp
    engine/master.cpp
    engine/server.cpp
    game/server.cpp
)

# genkey is a rather simple application
file(GLOB genkey_sources
    engine/genkey.cpp
    shared/crypto.cpp
)

# neither server nor client need genkey.cpp
# to avoid warnings about duplicate main()s, it has to be removed from their source lists
file(GLOB genkey_cpp_path engine/genkey.cpp)
list(REMOVE_ITEM client_sources ${genkey_cpp_path})
list(REMOVE_ITEM server_sources ${genkey_cpp_path})

# make sure sqlite3 is built with the correct flags
file(GLOB sqlite3_cpp_path support/sqlite3.c)
set_source_files_properties(support/sqlite3.c PROPERTIES
    COMPILE_FLAGS "-DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION"
)

# platform specific code
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    list(APPEND client_deps x11)
    set(link_libs rt)
    if(${WANT_STEAM})
        add_library(steam_lib SHARED IMPORTED)
        if(${CMAKE_HOST_SYSTEM_PROCESSOR} MATCHES "i[3-6]86")
            set_target_properties(steam_lib PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/../bin/x86/libsteam_api.so)
        elseif(${CMAKE_HOST_SYSTEM_PROCESSOR} MATCHES "x86_64")
            set_target_properties(steam_lib PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/../bin/amd64/libsteam_api.so)
        endif()
        list(APPEND link_libs steam_lib)
    endif()
    if(${WANT_DISCORD})
        if(${CMAKE_HOST_SYSTEM_PROCESSOR} MATCHES "x86_64")
            add_library(discord_lib SHARED IMPORTED)
            set_target_properties(discord_lib PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/../bin/amd64/libdiscord-rpc.so)
            list(APPEND link_libs discord_lib)
        endif()
    endif()
    link_libraries(${link_libs})
    set(BIN_SUFFIX "_linux")
elseif(APPLE)
    # build OS X specific Objective-C code
    file(GLOB mac_client_sources
        xcode/main.m xcode/macutils.mm
        xcode/SDLmain.m xcode/ConsoleView.m
    )
    file(GLOB mac_server_sources
        xcode/macutils.mm
    )
    list(APPEND client_sources ${mac_client_sources})
    list(APPEND server_sources ${mac_server_sources})
    set(BIN_SUFFIX "_osx")
elseif(MINGW)
    link_libraries(ws2_32 winmm)
    set(BIN_SUFFIX "_windows")
else()
    set(BIN_SUFFIX "_native")
endif()

if(${BUILD_CLIENT})
    # set up library directories
    link_deps_dirs(DEPS ${client_deps})
    if(link_deps_dirs_failed)
        message(FATAL_ERROR "Could not find required packages for the client. You can run cmake with -DBUILD_CLIENT=0 to exclude the client.")
    endif()
    # add the client executable and link it to enet
    add_executable(redeclipse${BIN_SUFFIX} ${client_sources})
    target_link_libraries(redeclipse${BIN_SUFFIX} enet)
endif()

# set up library directories
link_deps_dirs(FATAL DEPS ${server_deps})
# add the server executable and link it to enet
# (define STANDALONE to "notify" the preprocessor that the server is built this time)
add_executable(redeclipse_server${BIN_SUFFIX} ${server_sources})
target_link_libraries(redeclipse_server${BIN_SUFFIX} enet)
set_target_properties(redeclipse_server${BIN_SUFFIX} PROPERTIES
    COMPILE_FLAGS "-DSTANDALONE"
)

if(APPLE)
    # include framework required in xcode/ code
    find_library(COCOA_LIBRARY Cocoa)
    if(${BUILD_CLIENT})
        target_link_libraries(redeclipse${BIN_SUFFIX} ${COCOA_LIBRARY})
    endif()
    target_link_libraries(redeclipse_server${BIN_SUFFIX} ${COCOA_LIBRARY})
else(APPLE)
    # add the genkey executable
    add_executable(genkey${BIN_SUFFIX} ${genkey_sources})
    set_target_properties(genkey${BIN_SUFFIX} PROPERTIES
        COMPILE_FLAGS "-DSTANDALONE"
    )
endif(APPLE)

# configure dependencies
if(${BUILD_CLIENT})
    configure_deps(TARGET redeclipse${BIN_SUFFIX} DEPS ${client_deps})
    if(configure_deps_failed)
        message(FATAL_ERROR "Could not find required packages for the client. You can run cmake with -DBUILD_CLIENT=0 to exclude the client.")
    endif()
endif()
configure_deps(TARGET redeclipse_server${BIN_SUFFIX} FATAL DEPS ${server_deps})
configure_deps(TARGET genkey${BIN_SUFFIX} FATAL DEPS ${genkey_deps})

# configure enet
set(ENET_SOURCE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/enet)
add_subdirectory(${ENET_SOURCE_DIRECTORY})

# configure local includes
include_directories(
    ${ENET_SOURCE_DIRECTORY}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/engine
    ${CMAKE_CURRENT_SOURCE_DIR}/game
    ${CMAKE_CURRENT_SOURCE_DIR}/shared
    ${CMAKE_CURRENT_SOURCE_DIR}/support
)
if(${WANT_STEAM})
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/steam)
    add_definitions("-DUSE_STEAM=1")
endif()
if(${WANT_DISCORD})
    add_definitions("-DUSE_DISCORD=1")
endif()

# include the headers for the libraries bundled in ../bin
if(MINGW)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
endif()

# install to ../bin/
if(${CMAKE_HOST_SYSTEM_PROCESSOR} MATCHES "i[3-6]86")
	set(ARCHITECTURE "x86")
elseif(${CMAKE_HOST_SYSTEM_PROCESSOR} MATCHES "x86_64")
	set(ARCHITECTURE "amd64")
else()
    set(ARCHITECTURE "native")
endif()

set(targets redeclipse_server${BIN_SUFFIX} genkey${BIN_SUFFIX})
if(${BUILD_CLIENT})
    set(targets redeclipse${BIN_SUFFIX} ${targets})
endif()

foreach(target IN LISTS targets)
    install(
        TARGETS ${target}
        DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/../bin/${ARCHITECTURE}/
    )
endforeach()
