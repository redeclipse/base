#!/bin/sh
if [ "${REDECLIPSE_CALLED}" = "true" ]; then REDECLIPSE_EXITR="return"; else REDECLIPSE_EXITR="exit"; fi
REDECLIPSE_SCRIPT="$0"
REDECLIPSE_ARGS=$@
REDECLIPSE_SYSTEM="$(uname -s)"

redeclipse_path() {
    if [ -z "${REDECLIPSE_PATH+isset}" ]; then REDECLIPSE_PATH="$(cd "$(dirname "$0")" && pwd)"; fi
}

redeclipse_init() {
    if [ -z "${REDECLIPSE_BINARY+isset}" ]; then REDECLIPSE_BINARY="redeclipse"; fi
    REDECLIPSE_SUFFIX=""
    REDECLIPSE_START="exec"
}

redeclipse_setup() {
    if [ -z "${REDECLIPSE_TARGET+isset}" ]; then
        REDECLIPSE_MACHINE="$(uname -m)"
        case "${REDECLIPSE_SYSTEM}" in
            Linux)
                REDECLIPSE_SUFFIX="_linux"
                REDECLIPSE_TARGET="linux"
                ;;
            Darwin)
		        REDECLIPSE_SUFFIX="_universal"
		        REDECLIPSE_TARGET="macos"
                REDECLIPSE_ARCH="redeclipse.app/Contents/MacOS"
                ;;
            FreeBSD)
                REDECLIPSE_SUFFIX="_bsd"
                REDECLIPSE_TARGET="bsd"
                ;;
            MINGW*)
                REDECLIPSE_SUFFIX=".exe"
                REDECLIPSE_TARGET="windows"
                if [ -n "${PROCESSOR_ARCHITEW6432+isset}" ]; then
                    REDECLIPSE_MACHINE="${PROCESSOR_ARCHITEW6432}"
                else
                    REDECLIPSE_MACHINE="${PROCESSOR_ARCHITECTURE}"
                fi
                ;;
            *)
                echo "Unsupported system: ${REDECLIPSE_SYSTEM}"
                return 1
                ;;
        esac
    fi
    if [ -z "${REDECLIPSE_ARCH+isset}" ] && [ "${REDECLIPSE_TARGET}" != "macos" ]; then
        case "${REDECLIPSE_MACHINE}" in
            i486|i586|i686|x86)
                REDECLIPSE_ARCH="x86"
                ;;
            x86_64|[Aa][Mm][Dd]64)
                REDECLIPSE_ARCH="amd64"
                ;;
            arm|armv*)
                REDECLIPSE_ARCH="arm"
                ;;
            *)
                REDECLIPSE_ARCH="native"
                ;;
        esac
    fi
    if [ -n "${REDECLIPSE_HOME+isset}" ]; then
        REDECLIPSE_OPTIONS="-h${REDECLIPSE_HOME} ${REDECLIPSE_OPTIONS}"
    fi
    redeclipse_runit
    return $?
}

redeclipse_runit() {
    if [ -e "${REDECLIPSE_PATH}/bin/${REDECLIPSE_ARCH}/${REDECLIPSE_BINARY}${REDECLIPSE_SUFFIX}" ]; then
        REDECLIPSE_PWD=`pwd`
        export REDECLIPSE_PWD
        cd "${REDECLIPSE_PATH}" || return 1
        case "${REDECLIPSE_SYSTEM}" in
            Linux|FreeBSD)
                export LD_LIBRARY_PATH="${REDECLIPSE_PATH}/bin/${REDECLIPSE_ARCH}:${LD_LIBRARY_PATH}"
                ;;
            Darwin)
                export DYLD_LIBRARY_PATH="${REDECLIPSE_PATH}/bin/redeclipse.app/Contents/Frameworks:${DYLD_LIBRARY_PATH}"
                ;;
        esac
        ${REDECLIPSE_START} "${REDECLIPSE_PATH}/bin/${REDECLIPSE_ARCH}/${REDECLIPSE_BINARY}${REDECLIPSE_SUFFIX}" ${REDECLIPSE_OPTIONS} ${REDECLIPSE_ARGS}
        if [ $? -ne 0 ]; then
            cd "${REDECLIPSE_PWD}"
            return 1
        fi
        cd "${REDECLIPSE_PWD}"
        return 0
    else
        if [ "${REDECLIPSE_ARCH}" = "amd64" ]; then
            REDECLIPSE_ARCH="x86"
            redeclipse_runit
            return $?
        fi
        echo "Unable to find a working binary."
    fi
    return 1
}

redeclipse_path
redeclipse_init
redeclipse_setup

if [ $? -ne 0 ]; then
    echo ""
    echo "There was an error running Red Eclipse."
    echo "Press any key to continue..."
    read REDECLIPSE_PAUSE
    ${REDECLIPSE_EXITR} 1
else
    ${REDECLIPSE_EXITR} 0
fi
