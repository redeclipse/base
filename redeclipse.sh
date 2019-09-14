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
    REDECLIPSE_MAKE="make -C src install"
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
                REDECLIPSE_MAKE="./src/macbuild.sh all install"
                ;;
            FreeBSD)
                REDECLIPSE_SUFFIX="_bsd"
                REDECLIPSE_TARGET="bsd"
                REDECLIPSE_BRANCH="inplace" # we don't have binaries for bsd yet sorry
                ;;
            MINGW*)
                REDECLIPSE_SUFFIX=".exe"
                REDECLIPSE_TARGET="windows"
                REDECLIPSE_MAKE="mingw32-make"
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
    if [ -e "${REDECLIPSE_PATH}/branch.txt" ]; then REDECLIPSE_BRANCH_CURRENT=`cat "${REDECLIPSE_PATH}/branch.txt"`; fi
    if [ -z "${REDECLIPSE_BRANCH+isset}" ]; then
        if [ -n "${REDECLIPSE_BRANCH_CURRENT+isset}" ]; then
            REDECLIPSE_BRANCH="${REDECLIPSE_BRANCH_CURRENT}"
        elif [ -e ".git" ]; then
            REDECLIPSE_BRANCH="devel"
        else
            REDECLIPSE_BRANCH="stable"
        fi
    fi
    if [ -z "${REDECLIPSE_HOME+isset}" ] && [ "${REDECLIPSE_BRANCH}" != "stable" ] && [ "${REDECLIPSE_BRANCH}" != "inplace" ] && [ "${REDECLIPSE_BRANCH}" != "steam" ]; then REDECLIPSE_HOME="home"; fi
    if [ -n "${REDECLIPSE_HOME+isset}" ]; then
        if [ ! -d "${REDECLIPSE_HOME}" ]; then
            mkdir -p "${REDECLIPSE_HOME}" >/dev/null 2>&1
        fi
        touch "${REDECLIPSE_HOME}/test.tmp" >/dev/null 2>&1
        if [ $? -ne 0 ]; then
            echo ""
            echo "Can not write to the requested home directory: ${REDECLIPSE_HOME}"
            echo "Please ensure the directory exists and is writable by the current user."
            echo ""
            return 1
        else
            rm -f "${REDECLIPSE_HOME}/test.tmp"
        fi
        REDECLIPSE_OPTIONS="-h${REDECLIPSE_HOME} ${REDECLIPSE_OPTIONS}"
    fi
    redeclipse_check
    return $?
}

redeclipse_check() {
    if [ "${REDECLIPSE_BRANCH}" != "inplace" ] && [ "${REDECLIPSE_BRANCH}" != "steam" ]; then
        echo ""
        echo "Checking for updates to \"${REDECLIPSE_BRANCH}\". To disable set: REDECLIPSE_BRANCH=\"inplace\""
        echo ""
        redeclipse_begin
        return $?
    fi
    redeclipse_runit
    return $?
}

redeclipse_begin() {
    REDECLIPSE_RETRY="false"
    redeclipse_update
    return $?
}

redeclipse_retry() {
    if [ "${REDECLIPSE_RETRY}" != "true" ]; then
        REDECLIPSE_RETRY="true"
        echo "Retrying..."
        redeclipse_update
        return $?
    fi
    redeclipse_runit
    return $?
}

redeclipse_update() {
    chmod +x "${REDECLIPSE_PATH}/bin/update.sh"
    REDECLIPSE_CALLED="true" . "${REDECLIPSE_PATH}/bin/update.sh"
    if [ $? -eq 0 ]; then
        redeclipse_runit
        return $?
    else
        redeclipse_retry
        return $?
    fi
    return 0
}

redeclipse_runit() {
    export REDECLIPSE_TARGET
    export REDECLIPSE_BRANCH
    export REDECLIPSE_ARCH
    export REDECLIPSE_MACHINE
    export REDECLIPSE_BINARY
    export REDECLIPSE_SUFFIX
    export REDECLIPSE_PATH
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

        exec "${REDECLIPSE_PATH}/bin/${REDECLIPSE_ARCH}/${REDECLIPSE_BINARY}${REDECLIPSE_SUFFIX}" ${REDECLIPSE_OPTIONS} ${REDECLIPSE_ARGS}
        if [ $? -ne 0 ]; then
            cd "${REDECLIPSE_PWD}"
            return 1
        fi
        cd "${REDECLIPSE_PWD}"
        return 0
    else
        if [ "${REDECLIPSE_BRANCH}" != "inplace" ] && [ "${REDECLIPSE_BRANCH}" != "steam" ] && [ "${REDECLIPSE_TRYUPDATE}" != "true" ]; then
            REDECLIPSE_TRYUPDATE="true"
            redeclipse_begin
            return $?
        fi
        if [ "${REDECLIPSE_ARCH}" = "amd64" ]; then
            REDECLIPSE_ARCH="x86"
            redeclipse_runit
            return $?
        elif [ "${REDECLIPSE_ARCH}" = "x86" ]; then
            REDECLIPSE_ARCH="native"
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
    ${REDECLIPSE_EXITR} 1
else
    ${REDECLIPSE_EXITR} 0
fi
