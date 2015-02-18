#!/bin/sh

if [ -z "${REDECLIPSE_PATH+isset}" ]; then REDECLIPSE_PATH="$(cd "$(dirname "$0")" && pwd)"; fi
if [ -z "${REDECLIPSE_BINARY+isset}" ]; then REDECLIPSE_BINARY="redeclipse"; fi
REDECLIPSE_SCRIPT="$0"
REDECLIPSE_SUFFIX=""
REDECLIPSE_OPTIONS=""
REDECLIPSE_MAKE="make"

function redeclipse_setup {
    REDECLIPSE_SYSTEM="$(uname -s)"
    case "${REDECLIPSE_SYSTEM}" in
        Linux)
            REDECLIPSE_SUFFIX="_linux"
            ;;
        FreeBSD)
            REDECLIPSE_SUFFIX="_freebsd"
            ;;
        MINGW*)
            REDECLIPSE_SUFFIX=".exe"
            REDECLIPSE_MAKE="mingw32-make"
            REDECLIPSE_ARCH="x86"
            if [ "${PROCESSOR_ARCHITECTURE}" = "amd64" ] || [ "${PROCESSOR_ARCHITECTURE}" = "AMD64" ] || [ "${PROCESSOR_ARCHITEW6432}" = "amd64" ] || [ "${PROCESSOR_ARCHITEW6432}" = "AMD64" ]; then
                REDECLIPSE_ARCH="amd64"
            fi
            ;;
        *)
            echo "Unsupported system: ${REDECLIPSE_SYSTEM}"
            exit 1
            ;;
    esac
    if [ -z "${REDECLIPSE_ARCH+isset}" ]; then
        REDECLIPSE_MACHINE="$(uname -m)"
        case "${REDECLIPSE_MACHINE}" in
            i486|i586|i686)
                REDECLIPSE_ARCH="x86"
                ;;
            x86_64|amd64)
                REDECLIPSE_ARCH="amd64"
                ;;
            *)
                echo "Unsupported architecture: ${REDECLIPSE_MACHINE}"
                exit 1
                ;;
        esac
    fi
    if [ -z "${REDECLIPSE_BRANCH+isset}" ]; then
        REDECLIPSE_BRANCH="stable"
        if [ -a ".git" ]; then REDECLIPSE_BRANCH="devel"; fi
        if [ -a "${REDECLIPSE_PATH}/bin/branch.txt" ]; then REDECLIPSE_BRANCH=`cat "${REDECLIPSE_PATH}/bin/branch.txt"`; fi
    fi
    if [ "${REDECLIPSE_BRANCH}" != "stable" ] && [ "${REDECLIPSE_BRANCH}" != "devel" ] && [ "${REDECLIPSE_BRANCH}" != "source" ] && [ "${REDECLIPSE_BRANCH}" != "inplace" ]; then
        REDECLIPSE_BRANCH="inplace"
    fi
    if [ -z "${REDECLIPSE_HOME+isset}" ] && [ "${REDECLIPSE_BRANCH}" != "stable" ] && [ "${REDECLIPSE_BRANCH}" != "inplace" ]; then REDECLIPSE_HOME="home"; fi
    if [ -z "${REDECLIPSE_HOME+isset}" ]; then REDECLIPSE_OPTIONS="-h\"${REDECLIPSE_HOME}\" ${REDECLIPSE_OPTIONS}"; fi
    redeclipse_check
}

function redeclipse_check {
    if [ "${REDECLIPSE_BRANCH}" = "stable" ] || [ "${REDECLIPSE_BRANCH}" = "devel" ]; then
        echo ""
        echo "This is where we would check for updates." #Checking for updates. To disable set: REDECLIPSE_BRANCH=\"inplace\"
        echo ""
        #redeclipse_begin
        #return 0
    fi
    redeclipse_runit
}

function redeclipse_begin {
    REDECLIPSE_RETRY="false"
    redeclipse_update
}

function redeclipse_retry {
    if [ "${REDECLIPSE_RETRY}" != "true" ]; then
        REDECLIPSE_RETRY="true"
        echo "Retrying..."
        redeclipse_update
        return 0
    fi
    redeclipse_runit
}

function redeclipse_update {
    REDECLIPSE_BINVER=`cat "${REDECLIPSE_PATH}/bin/version.txt"`
    source "${REDECLIPSE_PATH}/bin/update.sh" && redeclipse_success || redeclipse_retry
}

function redeclipse_success {
    if [ "${REDECLIPSE_BRANCH}" = "stable" ]; then
        REDECLIPSE_BINNEW=`cat "${REDECLIPSE_PATH}/bin/version.txt"`
        if [ "${REDECLIPSE_BINVER}" != "${REDECLIPSE_BINNEW}" ]; then
            redeclipse_update
            return 0
        fi
    fi
    redeclipse_runit
}

function redeclipse_runit {
    if [ -a "${REDECLIPSE_PATH}/bin/${REDECLIPSE_ARCH}/${REDECLIPSE_BINARY}${REDECLIPSE_SUFFIX}" ]; then
        pushd "${REDECLIPSE_PATH}" || redeclipse_error
        exec "bin/${REDECLIPSE_ARCH}/${REDECLIPSE_BINARY}${REDECLIPSE_SUFFIX}" ${REDECLIPSE_OPTIONS} "$@" || (
            popd
            redeclipse_error
        )
        popd
        return 0
    else
        if [ "${REDECLIPSE_BRANCH}" = "source" ]; then
            ${REDECLIPSE_MAKE} -C src all install && ( redeclipse_runit; return 0 )
            REDECLIPSE_BRANCH="devel"
        fi
        if [ "${REDECLIPSE_BRANCH}" != "inplace" ] && [ "${REDECLIPSE_TRYUPDATE}" != "true" ]; then
            REDECLIPSE_TRYUPDATE="true"
            redeclipse_begin
            return 0
        fi
        if [ "${REDECLIPSE_ARCH}" != "x86" ]; then
            REDECLIPSE_ARCH="x86"
            redeclipse_runit
            return 0
        fi
        echo "Unable to find a working binary."
    fi
    redeclipse_error
}

function redeclipse_error {
    echo "There was an error running Red Eclipse."
    exit 1
}

redeclipse_setup
