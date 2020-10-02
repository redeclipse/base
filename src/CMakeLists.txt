cmake_minimum_required(VERSION 3.13)

### project ###
project(
    redeclipse
)

### functions ###
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/functions.cmake)

### cmake ###
# stop in source builds
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

# set default build type
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type (release or debug)" FORCE)
endif ()

message(STATUS "CMAKE_VERSION: ${CMAKE_VERSION}")
message(STATUS "CMAKE_GENERATOR: ${CMAKE_GENERATOR}")
message(STATUS "C Compiler executable: ${CMAKE_C_COMPILER}")
message(STATUS "CXX Compiler executable: ${CMAKE_CXX_COMPILER}")
message(STATUS "Linker executable: ${CMAKE_LINKER}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# set C++ standard
set(CMAKE_CXX_STANDARD 11)

### architecture ###
set(RE_ARCH_86 OFF)
set(RE_ARCH_64 OFF)
set(RE_ARCH_ppc64 OFF)
set(RE_ARCH_ppc OFF)
set(RE_ARCH_arm OFF)
set(RE_ARCH_aarch64 OFF)
set(RE_ARCH_NATIVE OFF)
if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "amd64.*|x86_64.*|AMD64.*")
    set(RE_ARCH "amd64")
    set(RE_ARCH_64 ON)
elseif (${CMAKE_SYSTEM_PROCESSOR} MATCHES "i686.*|i386.*|x86.*")
    set(RE_ARCH "x86")
    set(RE_ARCH_86 ON)
elseif (${CMAKE_SYSTEM_PROCESSOR} MATCHES "ppc64*")
    set(RE_ARCH "ppc64")
    set(RE_ARCH_ppc64 ON)
elseif (${CMAKE_SYSTEM_PROCESSOR} MATCHES "ppc|powerpc")
    set(RE_ARCH "ppc")
    set(RE_ARCH_ppc ON)
elseif (${CMAKE_SYSTEM_PROCESSOR} MATCHES "armv*")
    set(RE_ARCH "arm")
    set(RE_ARCH_arm ON)
elseif (${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch64")
    set(RE_ARCH "aarch64")
    set(RE_ARCH_aarch64 ON)
else ()
    set(RE_ARCH "native")
    set(RE_ARCH_NATIVE ON)
endif ()

### OS ###

set(RE_OS_LINUX OFF)
set(RE_OS_WINDOWS OFF)
set(RE_OS_APPLE OFF)
set(RE_OS_OTHER OFF)
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(RE_OS_LINUX ON)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    set(RE_OS_WINDOWS ON)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(RE_OS_APPLE ON)
else ()
    set(RE_OS_OTHER ON)
endif ()

# set compile options
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fsigned-char -fno-exceptions -fno-rtti -Wno-invalid-offsetof")
if (${RE_ARCH_86} OR ${RE_ARCH_64})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffast-math")
endif ()

### output name suffix ###
if (${RE_OS_WINDOWS})
    set(RE_NAME_SUFFIX "")
elseif (${RE_OS_LINUX})
    set(RE_NAME_SUFFIX "_linux")
elseif (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    set(RE_NAME_SUFFIX "_bsd")
else ()
    set(RE_NAME_SUFFIX "_native")
endif ()

### options ###
option(BUILD_CLIENT "Build client?" ON)
option(WANT_STEAM "Build client with Steam support?" OFF)
option(WANT_DISCORD "Build client with Discord support?" OFF)
option(BUILD_TESSFONT "Build tessfont?" ON)
option(BUILD_SERVER "Build server?" ON)
option(BUILD_GENKEY "Build genkey?" ON)

# disable steam and discord support if it is not possible to link them
if (NOT ${BUILD_CLIENT})
    message(WARNING "Disabling Steam and Discord support as client is not built.")
    set(WANT_STEAM OFF CACHE BOOL "" FORCE)
    set(WANT_DISCORD OFF CACHE BOOL "" FORCE)
endif ()

# steam not for native
if (${WANT_STEAM} AND (${RE_OS_OTHER} OR ${RE_ARCH_NATIVE}))
    message(WARNING "Disabling Steam support, only supported for Linux/Windows/Apple and x86/amd64.")
    set(WANT_STEAM OFF CACHE BOOL "" FORCE)
endif ()
# discord only for non linux or linux + x64
if (${WANT_DISCORD} AND ((${RE_OS_LINUX} AND ${RE_ARCH_86}) OR ${RE_ARCH_NATIVE}))
    message(WARNING "Disabling Discord support, only supported for Windows and Linux amd64.")
    set(WANT_DISCORD OFF CACHE BOOL "" FORCE)
endif ()

message(STATUS "Build client: ${BUILD_CLIENT}")
message(STATUS "  with Steam support: ${WANT_STEAM}")
message(STATUS "  with Discord support: ${WANT_DISCORD}")
message(STATUS "Build server: ${BUILD_SERVER}")
message(STATUS "Build genkey: ${BUILD_GENKEY}")
message(STATUS "Build tessfont: ${BUILD_TESSFONT}")

### configuring ###
# Use pkg-config to configure dependencies later
find_package(PkgConfig REQUIRED)

# configure enet
# BUILD_GENKEY requires only enet headers
if (${BUILD_CLIENT} OR ${BUILD_SERVER} OR ${BUILD_GENKEY})
    set(ENET_SOURCE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/enet)
    add_subdirectory(${ENET_SOURCE_DIRECTORY})
endif ()

# sqlite3 need special compile flags
set_source_files_properties(
    support/sqlite3.c PROPERTIES
    COMPILE_FLAGS "-DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION"
)

### configure general includes ###
set(
    RE_INCLUDE_DIRS
    ${ENET_SOURCE_DIRECTORY}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/engine
    ${CMAKE_CURRENT_SOURCE_DIR}/game
    ${CMAKE_CURRENT_SOURCE_DIR}/shared
    ${CMAKE_CURRENT_SOURCE_DIR}/support
)

### building ###
if (${BUILD_CLIENT})
    add_executable(
        redeclipse_client

        engine/aa.cpp
        engine/bih.cpp
        engine/blend.cpp
        engine/cdpi.cpp
        engine/client.cpp
        engine/command.cpp
        engine/console.cpp
        engine/dynlight.cpp
        engine/grass.cpp
        engine/http.cpp
        engine/irc.cpp
        engine/light.cpp
        engine/main.cpp
        engine/master.cpp
        engine/material.cpp
        engine/menus.cpp
        engine/movie.cpp
        engine/normal.cpp
        engine/octa.cpp
        engine/octaedit.cpp
        engine/octarender.cpp
        engine/physics.cpp
        engine/pvs.cpp
        engine/rendergl.cpp
        engine/renderlights.cpp
        engine/rendermodel.cpp
        engine/renderparticles.cpp
        engine/rendersky.cpp
        engine/rendertext.cpp
        engine/renderva.cpp
        engine/server.cpp
        engine/serverbrowser.cpp
        engine/shader.cpp
        engine/sound.cpp
        engine/stain.cpp
        engine/texture.cpp
        engine/ui.cpp
        engine/water.cpp
        engine/wind.cpp
        engine/world.cpp
        engine/worldio.cpp
        game/ai.cpp
        game/bomber.cpp
        game/capture.cpp
        game/client.cpp
        game/defend.cpp
        game/entities.cpp
        game/game.cpp
        game/hud.cpp
        game/physics.cpp
        game/projs.cpp
        game/scoreboard.cpp
        game/server.cpp
        game/waypoint.cpp
        game/weapons.cpp
        shared/crypto.cpp
        shared/geom.cpp
        shared/glemu.cpp
        shared/stream.cpp
        shared/tools.cpp
        shared/zip.cpp
        support/jsmn.c
        support/sqlite3.c

        redeclipse.rc
    )
    if (${RE_OS_WINDOWS} AND ${CMAKE_BUILD_TYPE} MATCHES "Release")
        set_target_properties(redeclipse_client PROPERTIES WIN32 TRUE)
    elseif (${RE_OS_APPLE})
        target_sources(
            redeclipse_client PRIVATE

            xcode/macutils.mm
            xcode/SDLmain.m
            xcode/ConsoleView.m
        )
        set_target_properties(redeclipse_client PROPERTIES MACOSX_BUNDLE TRUE)
    endif ()
    target_include_directories(redeclipse_client PRIVATE ${RE_INCLUDE_DIRS})

    # configure steam
    if (${WANT_STEAM})
        if (${RE_OS_LINUX})
            find_link(redeclipse_client libsteam_api.so ${CMAKE_CURRENT_SOURCE_DIR}/../bin/${RE_ARCH}/)
        elseif (${RE_OS_WINDOWS} AND ${RE_ARCH_86})
            find_link(redeclipse_client steam_api ${CMAKE_CURRENT_SOURCE_DIR}/lib/x86/)
        elseif (${RE_OS_WINDOWS} AND ${RE_ARCH_64})
            find_link(redeclipse_client steam_api64 ${CMAKE_CURRENT_SOURCE_DIR}/lib/amd64/)
        elseif (${RE_OS_APPLE})
            find_link(redeclipse_client steam_api ${CMAKE_CURRENT_SOURCE_DIR}/../bin/redeclipse.app/Contents/Frameworks/)
        endif ()
        target_include_directories(redeclipse_client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/steam)
        target_compile_definitions(redeclipse_client PRIVATE USE_STEAM=1)
    endif ()
    # configure discord
    if (${WANT_DISCORD})
        if (${RE_OS_LINUX})
            find_link(redeclipse_client libdiscord-rpc.so ${CMAKE_CURRENT_SOURCE_DIR}/../bin/${RE_ARCH}/)
        elseif (${RE_OS_WINDOWS})
            find_link(redeclipse_client discord-rpc ${CMAKE_CURRENT_SOURCE_DIR}/lib/${RE_ARCH}/)
        elseif (${RE_OS_APPLE})
            find_link(redeclipse_client discord-rpc ${CMAKE_CURRENT_SOURCE_DIR}/../bin/redeclipse.app/Contents/Frameworks/)
        endif ()
        target_compile_definitions(redeclipse_client PRIVATE USE_DISCORD=1)
    endif ()

    # link libraries and OS depending libraries
    target_link_libraries(redeclipse_client enet)
    set(RE_CLIENT_DEPS zlib sdl2 SDL2_image SDL2_mixer)
    if (${RE_OS_LINUX})
        set(RE_CLIENT_DEPS ${RE_CLIENT_DEPS} x11 gl)
        find_link(redeclipse_client rt)
    elseif (${RE_OS_WINDOWS})
        set(RE_CLIENT_DEPS ${RE_CLIENT_DEPS} freeglut)
        find_link(redeclipse_client opengl32)
    elseif (${RE_OS_APPLE})
        set(RE_CLIENT_DEPS ${RE_CLIENT_DEPS} freeglut)
        find_link(redeclipse_client OpenGL)
        find_link(redeclipse_client Cocoa)
    else ()
        set(RE_CLIENT_DEPS ${RE_CLIENT_DEPS} gl)
    endif ()
    configure_deps(redeclipse_client ${RE_CLIENT_DEPS})

    set_target_properties(redeclipse_client PROPERTIES OUTPUT_NAME "redeclipse${RE_NAME_SUFFIX}")
    install(
        TARGETS redeclipse_client
        DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/../bin/${RE_ARCH}/
    )
endif ()

if (${BUILD_SERVER})
    add_executable(
        redeclipse_server

        engine/cdpi.cpp
        engine/command.cpp
        engine/http.cpp
        engine/irc.cpp
        engine/master.cpp
        engine/server.cpp
        game/server.cpp
        shared/crypto.cpp
        shared/geom.cpp
        shared/stream.cpp
        shared/tools.cpp
        shared/zip.cpp
        support/jsmn.c
        support/sqlite3.c
    )
    if (${RE_OS_APPLE})
        target_sources(redeclipse_server PRIVATE xcode/macutils.mm)
    endif ()
    target_include_directories(redeclipse_server PRIVATE ${RE_INCLUDE_DIRS})
    target_compile_definitions(redeclipse_server PRIVATE STANDALONE=1)

    # link libraries
    target_link_libraries(redeclipse_server enet)
    set(RE_SERVER_DEPS zlib)
    if (${RE_OS_APPLE})
        find_link(redeclipse_server Cocoa)
    endif ()
    configure_deps(redeclipse_server ${RE_SERVER_DEPS})

    set_target_properties(redeclipse_server PROPERTIES OUTPUT_NAME "redeclipse_server${RE_NAME_SUFFIX}")
    install(
        TARGETS redeclipse_server
        DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/../bin/${RE_ARCH}/
    )
endif ()

if (${BUILD_GENKEY})
    add_executable(
        redeclipse_genkey

        shared/genkey.cpp
        shared/crypto.cpp
    )
    target_include_directories(redeclipse_genkey PRIVATE ${RE_INCLUDE_DIRS})
    target_compile_definitions(redeclipse_genkey PRIVATE STANDALONE=1)

    # link libraries
    set(RE_GENKEY_DEPS zlib)
    configure_deps(redeclipse_genkey ${RE_GENKEY_DEPS})

    set_target_properties(redeclipse_genkey PROPERTIES OUTPUT_NAME "genkey${RE_NAME_SUFFIX}")
    install(
        TARGETS redeclipse_genkey
        DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/../bin/${RE_ARCH}/
    )
endif ()

if (${BUILD_TESSFONT})
    add_executable(
        redeclipse_tessfont

        shared/tessfont.cpp
    )
    target_include_directories(redeclipse_tessfont PRIVATE ${RE_INCLUDE_DIRS})
    target_compile_definitions(redeclipse_tessfont PRIVATE STANDALONE=1)

    # link libraries
    set(RE_TESSFONT_DEPS zlib freetype2)
    configure_deps(redeclipse_tessfont ${RE_TESSFONT_DEPS})

    set_target_properties(redeclipse_tessfont PROPERTIES OUTPUT_NAME "tessfont${RE_NAME_SUFFIX}")
    install(
        TARGETS redeclipse_tessfont
        DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/../bin/${RE_ARCH}/
    )
endif ()
