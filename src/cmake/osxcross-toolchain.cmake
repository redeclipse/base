#####################################################
# -- RED ECLIPSE macOS CROSS COMPILING TOOLCHAIN -- #
#####################################################
#
# This toolchain is built for cross compiling Red Eclipse on Linux for macOS
# hosts with the help of osxcross (https://github.com/tpoechtrager/osxcross).
#
# This specific toolchain can be used after setting up an OS X 10.9 tarball
# with osxcross.
# To use it with other versions, either copy and modify this file, or set
# the according variables like so:
#
# $ cd src
# $ mkdir build && cd build
# $ cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/osxcross-toolchain.cmake \
#   -DCMAKE_OSX_SYSROOT=<path/to/osxcross/target/SDK/<version>/> \
#   -DCMAKE_OSX_DEPLOYMENT_TARGET=<version>
# $ make -j$(nproc)
#
# Dependencies can be installed with osxcross-macports:
# $ osxcross-macports install -v libsdl2 libsdl2_image libsdl2_mixer mesa

# build on macOS
set(CMAKE_SYSTEM_NAME Darwin)

# might have to be adjusted depending on the tarball that is used
set(TOOLCHAIN_PREFIX x86_64-apple-darwin13)

# cross compilers to use for C and C++
set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-clang)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-clang++)

# utilities
set(PKG_CONFIG_EXECUTABLE ${TOOLCHAIN_PREFIX}-pkg-config)

# assumes osxcross to be in the home directory
# can be overwritten externally by using -DCMAKE_OSX_SYSROOT and
# -DCMAKE_OSX_DEPLOYMENT_TARGET
set(CMAKE_OSX_SYSROOT ~/osxcross/target/SDK/MacOSX10.9.sdk/)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.9")

# modify default behavior of FIND_XXX() commands to
# search for headers/libs in the target environment and
# search for programs in the build host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
