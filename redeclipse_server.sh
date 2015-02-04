#!/bin/sh
# APP_PATH should refer to the directory in which Red Eclipse data files are placed.
#APP_PATH=~/redeclipse
#APP_PATH=/usr/local/redeclipse
APP_PATH="$(cd "$(dirname "$0")" && pwd)"

# APP_OPTIONS contains any command line options you would like to start Red Eclipse with.
APP_OPTIONS=""

# SYSTEM_NAME should be set to the name of your operating system.
#SYSTEM_NAME=Linux
SYSTEM_NAME="$(uname -s)"

# MACHINE_NAME should be set to the name of your processor.
#MACHINE_NAME=i686
MACHINE_NAME="$(uname -m)"

if [ -x "${APP_PATH}/bin/redeclipse_server_native" ]
then
    SYSTEM_SUFFIX="_native"
    APP_ARCH=""
else
    case "$SYSTEM_NAME" in
    Linux)
        SYSTEM_SUFFIX="_linux"
        ;;
    FreeBSD)
        SYSTEM_SUFFIX="_freebsd"
        ;;
    *)
        SYSTEM_SUFFIX="_unknown"
        ;;
    esac

    case "$MACHINE_NAME" in
    i486|i586|i686)
        APP_ARCH="x86/"
        ;;
    x86_64|amd64)
        APP_ARCH="amd64/"
        ;;
    *)
        SYSTEM_SUFFIX="_native"
        APP_ARCH=""
        ;;
    esac
fi

if [ -x "${APP_PATH}/bin/${APP_ARCH}redeclipse_server${SYSTEM_SUFFIX}" ]
then
    cd "$APP_PATH" || exit 1
    exec "${APP_PATH}/bin/${APP_ARCH}redeclipse_server${SYSTEM_SUFFIX}" $APP_OPTIONS "$@"
else
    echo "Your platform does not have a pre-compiled Red Eclipse server."
    echo -n "Would you like to build one now? [Yn] "
    read CC
    if [ "$CC" != "n" ]; then
        cd "${APP_PATH}/src" || exit 1
        make clean install-server
        echo "Build complete, please try running the script again."
    else
        echo "Please follow the following steps to build:"
        echo "1) Ensure you have the zlib *DEVELOPMENT* libraries installed."
        echo "2) Change directory to src/ and type \"make clean install-server\"."
        echo "3) If the build succeeds, return to this directory and run this script again."
        exit 1
    fi
fi

