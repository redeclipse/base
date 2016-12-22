cmake_minimum_required(VERSION 2.8.11)

# Use pkg-config to configure dependencies later
find_package(PkgConfig REQUIRED)
# the dependencies required by all platforms
set(client_deps zlib sdl2 SDL2_image SDL2_mixer gl sqlite3)

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
    link_libraries(rt)
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

# configure dependencies with pkg-config
foreach(dep IN LISTS client_deps)
    pkg_check_modules(${dep} REQUIRED ${dep})

    # add the necessary includes
    foreach(include_dir IN LISTS ${dep}_INCLUDE_DIRS)
        include_directories(${include_dir})
    endforeach(include_dir)

    # link to the libraries
    foreach(lib IN LISTS ${dep}_LIBRARIES)
        link_libraries(${lib})
    endforeach(lib)

    # tell the compiler where to find the libraries' binaries
    foreach(lib_dir IN LISTS ${dep}_LIBRARY_DIRS)
        link_directories(${lib_dir})
    endforeach(lib_dir)
endforeach(dep)

# configure enet
set(ENET_SOURCE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/enet)
add_subdirectory(${ENET_SOURCE_DIRECTORY})

# configure local includes
include_directories(
    ${ENET_SOURCE_DIRECTORY}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/engine
    ${CMAKE_CURRENT_SOURCE_DIR}/game
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/shared
)

# add the client executable and link it to enet
add_executable(redeclipse${BIN_SUFFIX} ${client_sources})
target_link_libraries(redeclipse${BIN_SUFFIX} enet)

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
    target_link_libraries(redeclipse${BIN_SUFFIX} ${COCOA_LIBRARY})
    target_link_libraries(redeclipse_server${BIN_SUFFIX} ${COCOA_LIBRARY})
else(APPLE)
    # add the genkey executable
    add_executable(genkey${BIN_SUFFIX} ${genkey_sources})
endif(APPLE)

# install to ../bin/
if(${CMAKE_HOST_SYSTEM_PROCESSOR} MATCHES "i[3-6]86")
	set(ARCHITECTURE "x86")
elseif(${CMAKE_HOST_SYSTEM_PROCESSOR} MATCHES "x86_64")
	set(ARCHITECTURE "amd64")
else()
    set(ARCHITECTURE "native")
endif()
set(targets redeclipse${BIN_SUFFIX} redeclipse_server${BIN_SUFFIX} genkey${BIN_SUFFIX})
foreach(target IN LISTS targets)
    install(
        TARGETS ${target}
        DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/../bin/${ARCHITECTURE}/
    )
endforeach()
