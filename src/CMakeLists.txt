cmake_minimum_required(VERSION 3.17)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds would overwrite src/Makefile, use a build directory instead: cmake -S src -B build")
endif()

# mirrors the Makefile, which remains the reference build; on macOS use "cmake -G Xcode" to generate an Xcode project
project(redeclipse C CXX)

if(WIN32)
    message(FATAL_ERROR "Windows builds use the Makefile (MinGW or MSVC cross-compilation).")
endif()

option(BUILD_CLIENT "Build the client" ON)
option(WANT_STEAM "Build with Steam support" OFF)
option(WANT_DISCORD "Build with Discord support" OFF)

set(PLATFORM_BUILD 0 CACHE STRING "Build number")
set(PLATFORM_BRANCH "none" CACHE STRING "Build branch")
set(PLATFORM_REVISION "" CACHE STRING "Build revision")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type (Release or Debug)" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -fomit-frame-pointer -ffast-math -fno-finite-math-only")
set(CMAKE_CXX_FLAGS_DEBUG "-ggdb3 -D_DEBUG")
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wall> $<$<COMPILE_LANGUAGE:CXX>:-fsigned-char> $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
                    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti> $<$<COMPILE_LANGUAGE:CXX>:-Wno-invalid-offsetof>)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
    set(PLATFORM_BIN arm64)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^arm")
    set(PLATFORM_BIN arm)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
    set(PLATFORM_BIN amd64)
else()
    set(PLATFORM_BIN x86)
endif()
get_filename_component(ROOTDIR ${CMAKE_CURRENT_SOURCE_DIR}/.. ABSOLUTE)
set(BINDIR ${ROOTDIR}/bin/${PLATFORM_BIN})
set(INSTDIR ${BINDIR} CACHE PATH "Directory the binaries are installed to")

if(APPLE)
    set(BIN_SUFFIX _macos)
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(BIN_SUFFIX _linux)
elseif(CMAKE_SYSTEM_NAME MATCHES "BSD")
    set(BIN_SUFFIX _bsd)
else()
    set(BIN_SUFFIX _native)
endif()

if(APPLE)
    # Homebrew installs openal-soft keg-only, so pkg-config can't find it by default
    execute_process(COMMAND brew --prefix openal-soft OUTPUT_VARIABLE BREW_OPENAL OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(BREW_OPENAL)
        set(ENV{PKG_CONFIG_PATH} "${BREW_OPENAL}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
    endif()
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(ZLIB REQUIRED IMPORTED_TARGET zlib)

add_subdirectory(enet EXCLUDE_FROM_ALL)

set(VERSION_DEFINES VERSION_BUILD=${PLATFORM_BUILD} VERSION_BRANCH="${PLATFORM_BRANCH}" VERSION_REVISION="${PLATFORM_REVISION}")
set(GAME_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR} shared engine game enet/include support)

if(WANT_STEAM)
    find_library(STEAM_API steam_api PATHS ${BINDIR} NO_DEFAULT_PATH REQUIRED)
    list(APPEND VERSION_DEFINES USE_STEAM=1)
    list(APPEND GAME_INCLUDES steam)
endif()

file(GLOB CLIENT_SOURCES CONFIGURE_DEPENDS engine/*.cpp game/*.cpp shared/*.cpp)
list(REMOVE_ITEM CLIENT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/genkey.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/pch.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/tessfont.cpp
)

set(SERVER_SOURCES
    shared/crypto.cpp
    shared/geom.cpp
    shared/stream.cpp
    shared/tools.cpp
    shared/zip.cpp
    shared/prop.cpp
    engine/cdpi.cpp
    engine/command.cpp
    engine/http.cpp
    engine/irc.cpp
    engine/master.cpp
    engine/server.cpp
    game/server.cpp
)

set(TARGETS redeclipse_server genkey tessfont)

if(BUILD_CLIENT)
    if(APPLE)
        pkg_check_modules(CLIENT_DEPS REQUIRED IMPORTED_TARGET sdl2 SDL2_image openal sndfile)
    else()
        pkg_check_modules(CLIENT_DEPS REQUIRED IMPORTED_TARGET x11 sdl2 SDL2_image openal sndfile gl)
    endif()

    add_executable(redeclipse ${CLIENT_SOURCES} $<$<CONFIG:Debug>:${CMAKE_CURRENT_SOURCE_DIR}/tests/slotmanager.cpp>)
    target_compile_definitions(redeclipse PRIVATE ${VERSION_DEFINES})
    target_include_directories(redeclipse PRIVATE ${GAME_INCLUDES})
    target_link_libraries(redeclipse PRIVATE enet PkgConfig::CLIENT_DEPS PkgConfig::ZLIB)

    if(APPLE)
        target_compile_definitions(redeclipse PRIVATE GL_SILENCE_DEPRECATION)
        target_link_libraries(redeclipse PRIVATE "-framework OpenGL")
    elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
        target_link_libraries(redeclipse PRIVATE rt)
    endif()

    if(WANT_STEAM)
        target_link_libraries(redeclipse PRIVATE ${STEAM_API})
    endif()
    if(WANT_DISCORD AND CMAKE_SIZEOF_VOID_P EQUAL 8)
        find_library(DISCORD_RPC discord-rpc PATHS ${BINDIR} NO_DEFAULT_PATH REQUIRED)
        target_compile_definitions(redeclipse PRIVATE USE_DISCORD=1)
        target_link_libraries(redeclipse PRIVATE ${DISCORD_RPC})
    endif()

    list(PREPEND TARGETS redeclipse)
endif()

add_executable(redeclipse_server ${SERVER_SOURCES})
target_compile_definitions(redeclipse_server PRIVATE STANDALONE ${VERSION_DEFINES})
target_include_directories(redeclipse_server PRIVATE ${GAME_INCLUDES})
target_link_libraries(redeclipse_server PRIVATE enet PkgConfig::ZLIB)
if(WANT_STEAM)
    target_link_libraries(redeclipse_server PRIVATE ${STEAM_API})
endif()

add_executable(genkey shared/genkey.cpp shared/crypto.cpp)
target_compile_definitions(genkey PRIVATE STANDALONE ${VERSION_DEFINES})
target_include_directories(genkey PRIVATE ${GAME_INCLUDES})
target_link_libraries(genkey PRIVATE PkgConfig::ZLIB)

pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)
add_executable(tessfont shared/tessfont.cpp)
target_compile_definitions(tessfont PRIVATE STANDALONE)
target_link_libraries(tessfont PRIVATE PkgConfig::FREETYPE PkgConfig::ZLIB)

foreach(target IN LISTS TARGETS)
    set_target_properties(${target} PROPERTIES
        OUTPUT_NAME ${target}${BIN_SUFFIX}
        BUILD_RPATH ${BINDIR}
        XCODE_GENERATE_SCHEME ON
        XCODE_SCHEME_WORKING_DIRECTORY ${ROOTDIR}
    )
    install(TARGETS ${target} DESTINATION ${INSTDIR})
endforeach()
