#!/bin/sh
if [ "${REDECLIPSE_CALLED}" = "true" ]; then REDECLIPSE_EXITU="return"; else REDECLIPSE_EXITU="exit"; fi
if [ "${REDECLIPSE_DEPLOY}" != "true" ]; then REDECLIPSE_DEPLOY="false"; fi
REDECLIPSE_SCRIPT="$0"

redeclipse_update_path() {
    if [ -z "${REDECLIPSE_PATH+isset}" ]; then REDECLIPSE_PATH="$(cd "$(dirname "$0")" && cd .. && pwd)"; fi
}

redeclipse_update_init() {
    if [ -z "${REDECLIPSE_SOURCE+isset}" ]; then REDECLIPSE_SOURCE="http://redeclipse.net/files"; fi
    if [ -z "${REDECLIPSE_GITHUB+isset}" ]; then REDECLIPSE_GITHUB="https://github.com/red-eclipse"; fi
    if [ -z "${REDECLIPSE_CACHE+isset}" ]; then
        if [ "${REDECLIPSE_TARGET}" = "windows" ]; then
            REDECLIPSE_WINDOCS=`reg query "HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders" //v "Personal" | tr -d '\r' | tr -d '\n' | sed -e 's/.*\(.\):\\\/\/\1\//g;s/\\\/\//g'`
            if [ -d "${REDECLIPSE_WINDOCS}" ]; then
                REDECLIPSE_CACHE="${REDECLIPSE_WINDOCS}/My Games/Red Eclipse/cache"
                return 0
            fi
        elif [ "${REDECLIPSE_TARGET}" = "macos" ]; then
            REDECLIPSE_CACHE="${HOME}/Library/Application Support/Red Eclipse/cache"
        else
            REDECLIPSE_CACHE="${HOME}/.redeclipse/cache"
        fi
    fi
    return 0
}

redeclipse_update_setup() {
    if [ -z "${REDECLIPSE_TARGET+isset}" ]; then
        REDECLIPSE_SYSTEM="$(uname -s)"
        REDECLIPSE_MACHINE="$(uname -m)"
        case "${REDECLIPSE_SYSTEM}" in
            Linux)
                REDECLIPSE_TARGET="linux"
                ;;
            Darwin)
                REDECLIPSE_TARGET="macos"
                ;;
            FreeBSD)
                REDECLIPSE_TARGET="bsd"
                REDECLIPSE_BRANCH="source" # we don't have binaries for bsd yet sorry
                ;;
            MINGW*)
                REDECLIPSE_TARGET="windows"
                ;;
            *)
                echo "Unsupported system: ${REDECLIPSE_SYSTEM}"
                return 1
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
    REDECLIPSE_UPDATE="${REDECLIPSE_BRANCH}"
    REDECLIPSE_TEMP="${REDECLIPSE_CACHE}/${REDECLIPSE_BRANCH}"
    case "${REDECLIPSE_TARGET}" in
        windows)
            REDECLIPSE_BLOB="zipball"
            REDECLIPSE_ARCHIVE="windows.zip"
            REDECLIPSE_ARCHEXT="zip"
            ;;
        linux)
            REDECLIPSE_BLOB="tarball"
            REDECLIPSE_ARCHIVE="linux.tar.gz"
            REDECLIPSE_ARCHEXT="tar.gz"
            ;;
        macos)
            REDECLIPSE_BLOB="tarball"
            REDECLIPSE_ARCHIVE="macos.tar.gz"
            REDECLIPSE_ARCHEXT="tar.gz"
            ;;
        *)
            echo "Unsupported update target: ${REDECLIPSE_SYSTEM}"
            return 1
            ;;
    esac
    redeclipse_update_branch
    return $?
}

redeclipse_update_branch() {
    echo "branch: ${REDECLIPSE_UPDATE}"
    echo "folder: ${REDECLIPSE_PATH}"
    echo "cached: ${REDECLIPSE_TEMP}"
    if [ -z `which curl` ]; then
        if [ -z `which wget` ]; then
            echo "Unable to find curl or wget, are you sure you have one installed?"
            return 1
        else
            REDECLIPSE_DOWNLOADER="wget --no-check-certificate -U \"redeclipse-${REDECLIPSE_UPDATE}\" -O"
        fi
    else
        REDECLIPSE_DOWNLOADER="curl -L -k -f -A \"redeclipse-${REDECLIPSE_UPDATE}\" -o"
    fi
    if [ "${REDECLIPSE_BLOB}" = "zipball" ]; then
        if [ -z `which unzip` ]; then
            echo "Unable to find unzip, are you sure you have it installed?"
            return 1
        fi
        REDECLIPSE_UNZIP="unzip -o"
    fi
    if [ -z `which tar` ]; then
        echo "Unable to find tar, are you sure you have it installed?"
        return 1
    fi
    REDECLIPSE_TAR="tar -xv"
    if ! [ -d "${REDECLIPSE_TEMP}" ]; then mkdir -p "${REDECLIPSE_TEMP}"; fi
    echo "#"'!'"/bin/sh" > "${REDECLIPSE_TEMP}/install.sh"
    if [ "${REDECLIPSE_BRANCH}" = "devel" ]; then
        redeclipse_update_bins_run
        return $?
    fi
    redeclipse_update_module
    return $?
}

redeclipse_update_module() {
    echo "modules: Updating.."
    ${REDECLIPSE_DOWNLOADER} "${REDECLIPSE_TEMP}/mods.txt" "${REDECLIPSE_SOURCE}/${REDECLIPSE_UPDATE}/mods.txt"
    if ! [ -e "${REDECLIPSE_TEMP}/mods.txt" ]; then
        echo "modules: Failed to retrieve update information."
        return 1
    fi
    REDECLIPSE_MODULE_LIST=`cat "${REDECLIPSE_TEMP}/mods.txt"`
    if [ -z "${REDECLIPSE_MODULE_LIST}" ]; then
        echo "modules: Failed to get list, aborting.."
        return 1
    else
        if [ -d "${REDECLIPSE_TEMP}/data.txt" ]; then rm -rfv "${REDECLIPSE_TEMP}/data.txt"; fi
        if [ -d "${REDECLIPSE_TEMP}/data.zip" ]; then rm -rfv "${REDECLIPSE_TEMP}/data.zip"; fi
        if [ -d "${REDECLIPSE_TEMP}/data.tar.gz" ]; then rm -rfv "${REDECLIPSE_TEMP}/data.tar.gz"; fi
        echo "modules: Prefetching versions.."
        REDECLIPSE_MODULE_PREFETCH=""
        for a in ${REDECLIPSE_MODULE_LIST}; do
            rm -f "${REDECLIPSE_TEMP}/${a}.txt"
            if [ "${a}" = "base" ]; then
                ${REDECLIPSE_DOWNLOADER} "${REDECLIPSE_TEMP}/${a}.txt" "${REDECLIPSE_SOURCE}/${REDECLIPSE_UPDATE}/${a}.txt"
                REDECLIPSE_MODULE_RUN="${a}"
                if [ -n "${REDECLIPSE_MODULE_RUN}" ]; then
                    redeclipse_update_module_run
                    if [ $? -ne 0 ]; then
                        echo "${REDECLIPSE_MODULE_RUN}: There was an error updating the module, aborting.."
                        return 1
                    elif [ "${REDECLIPSE_DEPLOY}" != "true" ]
                        echo "${REDECLIPSE_MODULE_RUN}: Not updated, skipping other modules.."
                        redeclipse_update_bins_run
                        return $?
                    fi
                fi
            elif [ -n "${REDECLIPSE_MODULE_PREFETCH}" ]; then
                REDECLIPSE_MODULE_PREFETCH="${REDECLIPSE_MODULE_PREFETCH},${a}"
            else
                REDECLIPSE_MODULE_PREFETCH="${a}"
            fi
        done
        if [ -n "${REDECLIPSE_MODULE_PREFETCH}" ]; then
            ${REDECLIPSE_DOWNLOADER} "${REDECLIPSE_TEMP}/#1.txt" "${REDECLIPSE_SOURCE}/${REDECLIPSE_UPDATE}/{${REDECLIPSE_MODULE_PREFETCH}}.txt"
            for a in ${REDECLIPSE_MODULE_LIST}; do
                if [ "${a}" != "base" ]; then
                    REDECLIPSE_MODULE_RUN="${a}"
                    if [ -n "${REDECLIPSE_MODULE_RUN}" ]; then
                        redeclipse_update_module_run
                        if [ $? -ne 0 ]; then
                            echo "${REDECLIPSE_MODULE_RUN}: There was an error updating the module, aborting.."
                            return 1
                        fi
                    fi
                fi
            done
        else
            echo "modules: Failed to get version information, aborting.."
            return 1
        fi
    fi
    redeclipse_update_bins_run
    return $?
}

redeclipse_update_module_run() {
    if [ "${REDECLIPSE_MODULE_RUN}" = "base" ]; then
        REDECLIPSE_MODULE_DIR=""
    else
        REDECLIPSE_MODULE_DIR="/data/${REDECLIPSE_MODULE_RUN}"
    fi
    if  [ -e "${REDECLIPSE_PATH}${REDECLIPSE_MODULE_DIR}/version.txt" ]; then
        redeclipse_update_module_ver
        return $?
    fi
    echo "${REDECLIPSE_MODULE_RUN}: Unable to find version.txt. Will start from scratch."
    REDECLIPSE_MODULE_INSTALLED="none"
    echo "mkdir -p \"${REDECLIPSE_PATH}${REDECLIPSE_MODULE_DIR}\"" >> "${REDECLIPSE_TEMP}/install.sh"
    redeclipse_update_module_get
    return $?
}

redeclipse_update_module_ver() {
    if [ -e "${REDECLIPSE_PATH}${REDECLIPSE_MODULE_DIR}/version.txt" ]; then REDECLIPSE_MODULE_INSTALLED=`cat "${REDECLIPSE_PATH}${REDECLIPSE_MODULE_DIR}/version.txt"`; fi
    if [ -z "${REDECLIPSE_MODULE_INSTALLED}" ]; then REDECLIPSE_MODULE_INSTALLED="none"; fi
    echo "${REDECLIPSE_MODULE_RUN}: ${REDECLIPSE_MODULE_INSTALLED} is installed."
    redeclipse_update_module_get
    return $?
}

redeclipse_update_module_get() {
    if ! [ -e "${REDECLIPSE_TEMP}/${REDECLIPSE_MODULE_RUN}.txt" ]; then
        echo "${REDECLIPSE_MODULE_RUN}: Failed to retrieve update information."
        return 1
    fi
    REDECLIPSE_MODULE_REMOTE=`cat "${REDECLIPSE_TEMP}/${REDECLIPSE_MODULE_RUN}.txt"`
    if [ -z "${REDECLIPSE_MODULE_REMOTE}" ]; then
        echo "${REDECLIPSE_MODULE_RUN}: Failed to read update information."
        return 1
    fi
    echo "${REDECLIPSE_MODULE_RUN}: ${REDECLIPSE_MODULE_REMOTE} is the current version."
    if [ "${REDECLIPSE_MODULE_REMOTE}" = "${REDECLIPSE_MODULE_INSTALLED}" ]; then
        echo "echo \"${REDECLIPSE_MODULE_RUN}: already up to date.\"" >> "${REDECLIPSE_TEMP}/install.sh"
        return $?
    fi
    redeclipse_update_module_blob
    return $?
}

redeclipse_update_module_blob() {
    if [ -e "${REDECLIPSE_TEMP}/${REDECLIPSE_MODULE_RUN}.${REDECLIPSE_ARCHEXT}" ]; then
        rm -f "${REDECLIPSE_TEMP}/${REDECLIPSE_MODULE_RUN}.${REDECLIPSE_ARCHEXT}"
    fi
    echo "${REDECLIPSE_MODULE_RUN}: Downloading ${REDECLIPSE_GITHUB}/${REDECLIPSE_MODULE_RUN}/${REDECLIPSE_BLOB}/${REDECLIPSE_MODULE_REMOTE}"
    ${REDECLIPSE_DOWNLOADER} "${REDECLIPSE_TEMP}/${REDECLIPSE_MODULE_RUN}.${REDECLIPSE_ARCHEXT}" "${REDECLIPSE_GITHUB}/${REDECLIPSE_MODULE_RUN}/${REDECLIPSE_BLOB}/${REDECLIPSE_MODULE_REMOTE}"
    if ! [ -e "${REDECLIPSE_TEMP}/${REDECLIPSE_MODULE_RUN}.${REDECLIPSE_ARCHEXT}" ]; then
        echo "${REDECLIPSE_MODULE_RUN}: Failed to retrieve update package."
        return 1
    fi
    redeclipse_update_module_blob_deploy
    return $?
}

redeclipse_update_module_blob_deploy() {
    echo "echo \"${REDECLIPSE_MODULE_RUN}: deploying blob.\"" >> "${REDECLIPSE_TEMP}/install.sh"
    if [ "${REDECLIPSE_BLOB}" = "zipball" ]; then
        echo "${REDECLIPSE_UNZIP} -o \"${REDECLIPSE_TEMP}/${REDECLIPSE_MODULE_RUN}.${REDECLIPSE_ARCHEXT}\" -d \"${REDECLIPSE_TEMP}\" && (" >> "${REDECLIPSE_TEMP}/install.sh"
    else
        echo "${REDECLIPSE_TAR} -f \"${REDECLIPSE_TEMP}/${REDECLIPSE_MODULE_RUN}.${REDECLIPSE_ARCHEXT}\" -C \"${REDECLIPSE_TEMP}\" && (" >> "${REDECLIPSE_TEMP}/install.sh"
    fi
    if [ "${REDECLIPSE_MODULE_RUN}" != "base" ]; then
        echo "   rm -rf \"${REDECLIPSE_PATH}${REDECLIPSE_MODULE_DIR}\"" >> "${REDECLIPSE_TEMP}/install.sh"
        echo "   mkdir -p \"${REDECLIPSE_PATH}${REDECLIPSE_MODULE_DIR}\"" >> "${REDECLIPSE_TEMP}/install.sh"
    fi
    echo "   cp -Rfv \"${REDECLIPSE_TEMP}/red-eclipse-${REDECLIPSE_MODULE_RUN}-$(echo "$REDECLIPSE_MODULE_REMOTE" | cut -b 1-7)/\"* \"${REDECLIPSE_PATH}${REDECLIPSE_MODULE_DIR}\"" >> "${REDECLIPSE_TEMP}/install.sh"
    echo "   rm -rf \"${REDECLIPSE_TEMP}/red-eclipse-${REDECLIPSE_MODULE_RUN}-$(echo "$REDECLIPSE_MODULE_REMOTE" | cut -b 1-7)\"" >> "${REDECLIPSE_TEMP}/install.sh"
    echo "   echo \"${REDECLIPSE_MODULE_REMOTE}\" > \"${REDECLIPSE_PATH}${REDECLIPSE_MODULE_DIR}/version.txt\"" >> "${REDECLIPSE_TEMP}/install.sh"
    echo ") || (" >> "${REDECLIPSE_TEMP}/install.sh"
    echo "    rm -f \"${REDECLIPSE_TEMP}/${REDECLIPSE_MODULE_RUN}.txt\"" >> "${REDECLIPSE_TEMP}/install.sh"
    echo "    exit 1" >> "${REDECLIPSE_TEMP}/install.sh"
    echo ")" >> "${REDECLIPSE_TEMP}/install.sh"
    REDECLIPSE_DEPLOY="true"
    return $?
}

redeclipse_update_bins_run() {
    echo "bins: Updating.."
    rm -f "${REDECLIPSE_TEMP}/bins.txt"
    ${REDECLIPSE_DOWNLOADER} "${REDECLIPSE_TEMP}/bins.txt" "${REDECLIPSE_SOURCE}/${REDECLIPSE_UPDATE}/bins.txt"
    if [ -e "${REDECLIPSE_PATH}/bin/version.txt" ]; then REDECLIPSE_BINS=`cat "${REDECLIPSE_PATH}/bin/version.txt"`; fi
    if [ -z "${REDECLIPSE_BINS}" ]; then REDECLIPSE_BINS="none"; fi
    echo "bins: ${REDECLIPSE_BINS} is installed."
    redeclipse_update_bins_get
    return $?
}

redeclipse_update_bins_get() {
    if ! [ -e "${REDECLIPSE_TEMP}/bins.txt" ]; then
        echo "bins: Failed to retrieve update information."
        return 1
    fi
    REDECLIPSE_BINS_REMOTE=`cat "${REDECLIPSE_TEMP}/bins.txt"`
    if [ -z "${REDECLIPSE_BINS_REMOTE}" ]; then
        echo "bins: Failed to read update information."
        return 1
    fi
    echo "bins: ${REDECLIPSE_BINS_REMOTE} is the current version."
    if [ "${REDECLIPSE_TRYUPDATE}" != "true" ] && [ "${REDECLIPSE_BINS_REMOTE}" = "${REDECLIPSE_BINS}" ]; then
        echo "echo \"bins: already up to date.\"" >> "${REDECLIPSE_TEMP}/install.sh"
        redeclipse_update_deploy
        return $?
    fi
    redeclipse_update_bins_blob
    return $?
}

redeclipse_update_bins_blob() {
    if [ -e "${REDECLIPSE_TEMP}/${REDECLIPSE_ARCHIVE}" ]; then
        rm -f "${REDECLIPSE_TEMP}/${REDECLIPSE_ARCHIVE}"
    fi
    echo "bins: Downloading ${REDECLIPSE_SOURCE}/${REDECLIPSE_UPDATE}/${REDECLIPSE_ARCHIVE}"
    ${REDECLIPSE_DOWNLOADER} "${REDECLIPSE_TEMP}/${REDECLIPSE_ARCHIVE}" "${REDECLIPSE_SOURCE}/${REDECLIPSE_UPDATE}/${REDECLIPSE_ARCHIVE}"
    if ! [ -e "${REDECLIPSE_TEMP}/${REDECLIPSE_ARCHIVE}" ]; then
        echo "bins: Failed to retrieve bins update package."
        return 1
    fi
    redeclipse_update_bins_deploy
    return $?
}

redeclipse_update_bins_deploy() {
    echo "echo \"bins: deploying blob.\"" >> "${REDECLIPSE_TEMP}/install.sh"
    if [ "${REDECLIPSE_TARGET}" = "windows" ]; then
        echo "${REDECLIPSE_UNZIP} -o \"${REDECLIPSE_TEMP}/${REDECLIPSE_ARCHIVE}\" -d \"${REDECLIPSE_PATH}\" && (" >> "${REDECLIPSE_TEMP}/install.sh"
    else
        echo "${REDECLIPSE_TAR} -f \"${REDECLIPSE_TEMP}/${REDECLIPSE_ARCHIVE}\" -C \"${REDECLIPSE_PATH}\" && (" >> "${REDECLIPSE_TEMP}/install.sh"
    fi
    echo "    echo \"${REDECLIPSE_BINS_REMOTE}\" > \"${REDECLIPSE_PATH}/bin/version.txt\"" >> "${REDECLIPSE_TEMP}/install.sh"
    echo ") || (" >> "${REDECLIPSE_TEMP}/install.sh"
    echo "    rm -f \"${REDECLIPSE_TEMP}/bins.txt\"" >> "${REDECLIPSE_TEMP}/install.sh"
    echo "    exit 1" >> "${REDECLIPSE_TEMP}/install.sh"
    echo ")" >> "${REDECLIPSE_TEMP}/install.sh"
    REDECLIPSE_DEPLOY="true"
    redeclipse_update_deploy
    return $?
}

redeclipse_update_deploy() {
    if [ "${REDECLIPSE_DEPLOY}" != "true" ]; then return 0; fi
    echo "deploy: \"${REDECLIPSE_TEMP}/install.sh\""
    chmod ugo+x "${REDECLIPSE_TEMP}/install.sh"
    REDECLIPSE_INSTALL="exec"
    touch test.tmp && (
        rm -f test.tmp
        redeclipse_update_unpack
        return $?
    ) || (
        echo "Administrator permissions are required to deploy the files."
        if [ -z `which sudo` ]; then
            echo "Unable to find sudo, are you sure it is installed?"
            return 1
        else
            REDECLIPSE_INSTALL="sudo exec"
            redeclipse_update_unpack
            return $?
        fi
    )
    return $?
}

redeclipse_update_unpack() {
    ${REDECLIPSE_INSTALL} "${REDECLIPSE_TEMP}/install.sh" && (
        echo "${REDECLIPSE_BRANCH}" > "${REDECLIPSE_PATH}/branch.txt"
        return 0
    ) || (
        echo "There was an error deploying the files."
        return 1
    )
}

redeclipse_update_path
redeclipse_update_init
redeclipse_update_setup

if [ $? -ne 0 ]; then
    ${REDECLIPSE_EXITU} 1
else
    ${REDECLIPSE_EXITU} 0
fi
