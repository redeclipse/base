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
        fi
        REDECLIPSE_CACHE="${HOME}/.redeclipse/cache"
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
        echo "Unable to find curl, are you sure you have it installed?"
        return 1
    fi
    REDECLIPSE_CURL="curl --location --insecure --fail --user-agent \"redeclipse-${REDECLIPSE_UPDATE}\""
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
    REDECLIPSE_TAR="tar --gzip --extract --verbose --overwrite"
    if [ -z `which git` ]; then
        echo "Unable to find git, are you sure you have it installed?"
        return 1
    fi
    REDECLIPSE_GITAPPLY="git apply --ignore-space-change --ignore-whitespace --verbose --stat --apply"
    if ! [ -d "${REDECLIPSE_TEMP}" ]; then mkdir -p "${REDECLIPSE_TEMP}"; fi
    echo "#"'!'"/bin/sh" > "${REDECLIPSE_TEMP}/install.sh"
    echo "REDECLIPSE_ERROR=\"false\"" >> "${REDECLIPSE_TEMP}/install.sh"
    if [ "${REDECLIPSE_BRANCH}" = "devel" ]; then
        redeclipse_update_bins_run
        return $?
    fi
    redeclipse_update_module
    return $?
}

redeclipse_update_module() {
    ${REDECLIPSE_CURL} --silent --output "${REDECLIPSE_TEMP}/modules.txt" "${REDECLIPSE_SOURCE}/${REDECLIPSE_UPDATE}/modules.txt"
    if ! [ -e "${REDECLIPSE_TEMP}/modules.txt" ]; then
        echo "Failed to retrieve modules update information."
        redeclipse_update_bins_run
        return $?
    fi
    REDECLIPSE_MODULE_LIST=`cat "${REDECLIPSE_TEMP}/modules.txt"`
    if [ -z "${REDECLIPSE_MODULE_LIST}" ]; then
        echo "Failed to get module list, continuing.."
    else
        for a in ${REDECLIPSE_MODULE_LIST}; do
            REDEECLIPSE_MODULE_RUN="${a}"
            if [ -n "${REDEECLIPSE_MODULE_RUN}" ]; then
                redeclipse_update_module_run
                if [ $? -ne 0 ]; then
                    echo "${REDEECLIPSE_MODULE_RUN}: There was an error updating the module, continuing.."
                fi
            fi
        done
    fi
    redeclipse_update_bins_run
    return $?
}

redeclipse_update_module_run() {
    echo ""
    if [ "${REDEECLIPSE_MODULE_RUN}" = "base" ]; then
        REDEECLIPSE_MODULE_DIR=""
    else
        REDEECLIPSE_MODULE_DIR="/${REDEECLIPSE_MODULE_RUN}"
    fi
    if  [ -e "${REDECLIPSE_PATH}${REDEECLIPSE_MODULE_DIR}/readme.txt" ]; then
        redeclipse_update_module_ver
        return $?
    fi
    echo "${REDEECLIPSE_MODULE_RUN}: Unable to find \".${REDEECLIPSE_MODULE_DIR}/readme.txt\". Will start from scratch."
    REDECLIPSE_MODULE_INSTALLED="none"
    echo "mkdir -p \"${REDECLIPSE_PATH}${REDEECLIPSE_MODULE_DIR}\"" >> "${REDECLIPSE_TEMP}/install.sh"
    redeclipse_update_module_get
    return $?
}

redeclipse_update_module_ver() {
    echo ""
    if [ -e "${REDECLIPSE_PATH}${REDEECLIPSE_MODULE_DIR}/version.txt" ]; then REDECLIPSE_MODULE_INSTALLED=`cat "${REDECLIPSE_PATH}${REDEECLIPSE_MODULE_DIR}/version.txt"`; fi
    if [ -z "${REDECLIPSE_MODULE_INSTALLED}" ]; then REDECLIPSE_MODULE_INSTALLED="none"; fi
    echo "${REDEECLIPSE_MODULE_RUN}: ${REDECLIPSE_MODULE_INSTALLED} is installed."
    REDECLIPSE_MODULE_CACHED="none"
    if ! [ -e "${REDECLIPSE_TEMP}/${REDEECLIPSE_MODULE_RUN}.txt" ]; then
        redeclipse_update_module_get
        return $?
    fi
    REDECLIPSE_MODULE_CACHED=`cat "${REDECLIPSE_TEMP}/${REDEECLIPSE_MODULE_RUN}.txt"`
    if [ -z "${REDECLIPSE_MODULE_CACHED}" ]; then REDECLIPSE_MODULE_CACHED="none"; fi
    echo "${REDEECLIPSE_MODULE_RUN}: ${REDECLIPSE_MODULE_CACHED} is in the cache."
    rm -f "${REDECLIPSE_TEMP}/${REDEECLIPSE_MODULE_RUN}.txt"
    redeclipse_update_module_get
    return $?
}

redeclipse_update_module_get() {
    ${REDECLIPSE_CURL} --silent --output "${REDECLIPSE_TEMP}/${REDEECLIPSE_MODULE_RUN}.txt" "${REDECLIPSE_SOURCE}/${REDECLIPSE_UPDATE}/${REDEECLIPSE_MODULE_RUN}.txt"
    if ! [ -e "${REDECLIPSE_TEMP}/${REDEECLIPSE_MODULE_RUN}.txt" ]; then
        echo "${REDEECLIPSE_MODULE_RUN}: Failed to retrieve update information."
        return $?
    fi
    REDECLIPSE_MODULE_REMOTE=`cat "${REDECLIPSE_TEMP}/${REDEECLIPSE_MODULE_RUN}.txt"`
    if [ -z "${REDECLIPSE_MODULE_REMOTE}" ]; then
        echo "${REDEECLIPSE_MODULE_RUN}: Failed to read update information."
        return $?
    fi
    echo "${REDEECLIPSE_MODULE_RUN}: ${REDECLIPSE_MODULE_REMOTE} is the current version."
    if [ "${REDECLIPSE_MODULE_REMOTE}" = "${REDECLIPSE_MODULE_INSTALLED}" ]; then
        echo "echo \"${REDEECLIPSE_MODULE_RUN}: already up to date.\"" >> "${REDECLIPSE_TEMP}/install.sh"
        return $?
    fi
    if [ "${REDECLIPSE_MODULE_INSTALLED}" = "none" ]; then
        redeclipse_update_module_blob
        return $?
    fi
    redeclipse_update_module_patch
    return $?
}

redeclipse_update_module_patch() {
    if [ -e "${REDECLIPSE_TEMP}/${REDEECLIPSE_MODULE_RUN}.patch" ]; then rm -f "${REDECLIPSE_TEMP}/${REDEECLIPSE_MODULE_RUN}.patch"; fi
    if [ -e "${REDECLIPSE_TEMP}/${REDEECLIPSE_MODULE_RUN}.${REDECLIPSE_ARCHEXT}" ]; then rm -f "${REDECLIPSE_TEMP}/${REDEECLIPSE_MODULE_RUN}.${REDECLIPSE_ARCHEXT}"; fi
    echo "${REDEECLIPSE_MODULE_RUN}: ${REDECLIPSE_GITHUB}/${REDEECLIPSE_MODULE_RUN}/compare/${REDECLIPSE_MODULE_INSTALLED}...${REDECLIPSE_MODULE_REMOTE}.patch"
    echo ""
    ${REDECLIPSE_CURL} --output "${REDECLIPSE_TEMP}/${REDEECLIPSE_MODULE_RUN}.patch" "${REDECLIPSE_GITHUB}/${REDEECLIPSE_MODULE_RUN}/compare/${REDECLIPSE_MODULE_INSTALLED}...${REDECLIPSE_MODULE_REMOTE}.patch"
    if ! [ -e "${REDECLIPSE_TEMP}/${REDEECLIPSE_MODULE_RUN}.patch" ]; then
        echo "${REDEECLIPSE_MODULE_RUN}: Failed to retrieve update package. Downloading full zip instead."
        redeclipse_update_module_blob
        return $?
    fi
    redeclipse_update_module_patch_deploy
    return $?
}

redeclipse_update_module_patch_deploy() {
    echo "echo \"${REDEECLIPSE_MODULE_RUN}: applying patches.\"" >> "${REDECLIPSE_TEMP}/install.sh"
    echo "${REDECLIPSE_GITAPPLY} --directory=\"${REDECLIPSE_PATH}${REDEECLIPSE_MODULE_DIR}\" \"${REDECLIPSE_TEMP}/${REDEECLIPSE_MODULE_RUN}.patch\" && (" >> "${REDECLIPSE_TEMP}/install.sh"
    echo "    echo \"${REDECLIPSE_MODULE_REMOTE}\" > \"${REDECLIPSE_PATH}${REDEECLIPSE_MODULE_DIR}/version.txt\"" >> "${REDECLIPSE_TEMP}/install.sh"
    echo ") || (" >> "${REDECLIPSE_TEMP}/install.sh"
    echo "    echo \"none\" > \"${REDECLIPSE_PATH}${REDEECLIPSE_MODULE_DIR}/version.txt\"" >> "${REDECLIPSE_TEMP}/install.sh"
    echo "    rm -f \"${REDECLIPSE_TEMP}/${REDEECLIPSE_MODULE_RUN}.txt\"" >> "${REDECLIPSE_TEMP}/install.sh"
    echo "    REDECLIPSE_ERROR=\"true\"" >> "${REDECLIPSE_TEMP}/install.sh"
    echo ")" >> "${REDECLIPSE_TEMP}/install.sh"
    REDECLIPSE_DEPLOY="true"
    return $?
}

redeclipse_update_module_blob() {
    if [ -e "${REDECLIPSE_TEMP}/${REDEECLIPSE_MODULE_RUN}.${REDECLIPSE_ARCHEXT}" ]; then
        if [ "${REDECLIPSE_MODULE_CACHED}" = "${REDECLIPSE_MODULE_REMOTE}" ]; then
            echo "${REDEECLIPSE_MODULE_RUN}: Using cached file \"${REDECLIPSE_TEMP}/${REDEECLIPSE_MODULE_RUN}.${REDECLIPSE_ARCHEXT}\""
            redeclipse_update_module_blob_deploy
            return $?
        else
            rm -f "${REDECLIPSE_TEMP}/${REDEECLIPSE_MODULE_RUN}.${REDECLIPSE_ARCHEXT}"
        fi
    fi
    echo "${REDEECLIPSE_MODULE_RUN}: ${REDECLIPSE_GITHUB}/${REDEECLIPSE_MODULE_RUN}/${REDECLIPSE_BLOB}/${REDECLIPSE_MODULE_REMOTE}"
    echo ""
    ${REDECLIPSE_CURL} --output "${REDECLIPSE_TEMP}/${REDEECLIPSE_MODULE_RUN}.${REDECLIPSE_ARCHEXT}" "${REDECLIPSE_GITHUB}/${REDEECLIPSE_MODULE_RUN}/${REDECLIPSE_BLOB}/${REDECLIPSE_MODULE_REMOTE}"
    if ! [ -e "${REDECLIPSE_TEMP}/${REDEECLIPSE_MODULE_RUN}.${REDECLIPSE_ARCHEXT}" ]; then
        echo "${REDEECLIPSE_MODULE_RUN}: Failed to retrieve update package."
        return $?
    fi
    redeclipse_update_module_blob_deploy
    return $?
}

redeclipse_update_module_blob_deploy() {
    echo "echo \"${REDEECLIPSE_MODULE_RUN}: deploying blob.\"" >> "${REDECLIPSE_TEMP}/install.sh"
    if [ "${REDECLIPSE_BLOB}" = "zipball" ]; then
        echo "${REDECLIPSE_UNZIP} -o \"${REDECLIPSE_TEMP}/${REDEECLIPSE_MODULE_RUN}.${REDECLIPSE_ARCHEXT}\" -d \"${REDECLIPSE_TEMP}\" && (" >> "${REDECLIPSE_TEMP}/install.sh"
    else
        echo "${REDECLIPSE_TAR} --file=\"${REDECLIPSE_TEMP}/${REDEECLIPSE_MODULE_RUN}.${REDECLIPSE_ARCHEXT}\" --directory=\"${REDECLIPSE_TEMP}\" && (" >> "${REDECLIPSE_TEMP}/install.sh"
    fi
    echo "   cp --recursive --force --verbose \"${REDECLIPSE_TEMP}/red-eclipse-${REDEECLIPSE_MODULE_RUN}-$(echo "$REDECLIPSE_MODULE_REMOTE" | cut -b 1-7)/*\" \"${REDECLIPSE_PATH}${REDEECLIPSE_MODULE_DIR}\"" >> "${REDECLIPSE_TEMP}/install.sh"
    echo "   rm -rf \"${REDECLIPSE_TEMP}/red-eclipse-${REDEECLIPSE_MODULE_RUN}-$(echo "$REDECLIPSE_MODULE_REMOTE" | cut -b 1-7)\"" >> "${REDECLIPSE_TEMP}/install.sh"
    echo "   echo \"${REDECLIPSE_MODULE_REMOTE}\" > \"${REDECLIPSE_PATH}${REDEECLIPSE_MODULE_DIR}/version.txt\"" >> "${REDECLIPSE_TEMP}/install.sh"
    echo ") || (" >> "${REDECLIPSE_TEMP}/install.sh"
    echo "    rm -f \"${REDECLIPSE_TEMP}/${REDEECLIPSE_MODULE_RUN}.txt\"" >> "${REDECLIPSE_TEMP}/install.sh"
    echo "    REDECLIPSE_ERROR=\"true\"" >> "${REDECLIPSE_TEMP}/install.sh"
    echo ")" >> "${REDECLIPSE_TEMP}/install.sh"
    REDECLIPSE_DEPLOY="true"
    return $?
}

redeclipse_update_bins_run() {
    echo ""
    if [ -e "${REDECLIPSE_PATH}/bin/version.txt" ]; then REDECLIPSE_BINS=`cat "${REDECLIPSE_PATH}/bin/version.txt"`; fi
    if [ -z "${REDECLIPSE_BINS}" ]; then REDECLIPSE_BINS="none"; fi
    echo "bins: ${REDECLIPSE_BINS} is installed."
    REDECLIPSE_BINS_CACHED="none"
    if ! [ -e "${REDECLIPSE_TEMP}/bins.txt" ]; then
        redeclipse_update_bins_get
        return $?
    fi
    REDECLIPSE_BINS_CACHED=`cat "${REDECLIPSE_TEMP}/bins.txt"`
    if [ -z "${REDECLIPSE_BINS_CACHED}" ]; then REDECLIPSE_BINS_CACHED="none"; fi
    echo "bins: ${REDECLIPSE_BINS_CACHED} is in the cache."
    rm -f "${REDECLIPSE_TEMP}/bins.txt"
    redeclipse_update_bins_get
    return $?
}

redeclipse_update_bins_get() {
    ${REDECLIPSE_CURL} --silent --output "${REDECLIPSE_TEMP}/bins.txt" "${REDECLIPSE_SOURCE}/${REDECLIPSE_UPDATE}/bins.txt"
    if ! [ -e "${REDECLIPSE_TEMP}/bins.txt" ]; then
        echo "bins: Failed to retrieve update information."
        redeclipse_update_deploy
        return $?
    fi
    REDECLIPSE_BINS_REMOTE=`cat "${REDECLIPSE_TEMP}/bins.txt"`
    if [ -z "${REDECLIPSE_BINS_REMOTE}" ]; then
        echo "bins: Failed to read update information."
        redeclipse_update_deploy
        return $?
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
        if [ "${REDECLIPSE_BINS_CACHED}" = "${REDECLIPSE_BINS_REMOTE}" ]; then
            echo "bins: Using cached file \"${REDECLIPSE_TEMP}/${REDECLIPSE_ARCHIVE}\""
            redeclipse_update_bins_deploy
            return $?
        else
            rm -f "${REDECLIPSE_TEMP}/${REDECLIPSE_ARCHIVE}"
        fi
    fi
    echo "bins: ${REDECLIPSE_SOURCE}/${REDECLIPSE_UPDATE}/${REDECLIPSE_ARCHIVE}"
    echo ""
    ${REDECLIPSE_CURL} --output "${REDECLIPSE_TEMP}/${REDECLIPSE_ARCHIVE}" "${REDECLIPSE_SOURCE}/${REDECLIPSE_UPDATE}/${REDECLIPSE_ARCHIVE}"
    if ! [ -e "${REDECLIPSE_TEMP}/${REDECLIPSE_ARCHIVE}" ]; then
        echo "bins: Failed to retrieve bins update package."
        redeclipse_update_deploy
        return $?
    fi
    redeclipse_update_bins_deploy
    return $?
}

redeclipse_update_bins_deploy() {
    echo "echo \"bins: deploying blob.\"" >> "${REDECLIPSE_TEMP}/install.sh"
    if [ "${REDECLIPSE_TARGET}" = "windows" ]; then
        echo "${REDECLIPSE_UNZIP} -o \"${REDECLIPSE_TEMP}/${REDECLIPSE_ARCHIVE}\" -d \"${REDECLIPSE_PATH}\" && (" >> "${REDECLIPSE_TEMP}/install.sh"
    else
        echo "${REDECLIPSE_TAR} --file=\"${REDECLIPSE_TEMP}/${REDECLIPSE_ARCHIVE}\" --directory=\"${REDECLIPSE_PATH}\" && (" >> "${REDECLIPSE_TEMP}/install.sh"
    fi
    echo "    echo \"${REDECLIPSE_BINS_REMOTE}\" > \"${REDECLIPSE_PATH}/bin/version.txt\"" >> "${REDECLIPSE_TEMP}/install.sh"
    echo ") || (" >> "${REDECLIPSE_TEMP}/install.sh"
    echo "    rm -f \"${REDECLIPSE_TEMP}/bins.txt\"" >> "${REDECLIPSE_TEMP}/install.sh"
    echo "    REDECLIPSE_ERROR=\"true\"" >> "${REDECLIPSE_TEMP}/install.sh"
    echo ")" >> "${REDECLIPSE_TEMP}/install.sh"
    REDECLIPSE_DEPLOY="true"
    redeclipse_update_deploy
    return $?
}

redeclipse_update_deploy() {
    if [ "${REDECLIPSE_DEPLOY}" != "true" ]; then return 0; fi
    echo ""
    echo "if [ \"\${REDECLIPSE_ERROR}\" = \"true\" ]; then exit 1; else exit 0; fi" >> "${REDECLIPSE_TEMP}/install.sh"
    echo "deploy: \"${REDECLIPSE_TEMP}/install.sh\""
    echo ""
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
            redeclipse_update_unpack
            return $?
        fi
        REDECLIPSE_INSTALL="sudo exec"
    )
    return $?
}

redeclipse_update_unpack() {
    ${REDECLIPSE_INSTALL} "${REDECLIPSE_TEMP}/install.sh" && (
        echo ""
        echo "${REDECLIPSE_BRANCH}" > "${REDECLIPSE_PATH}/branch.txt"
        return 0
    ) || (
        echo ""
        echo "There was an error deploying the files."
        echo ""
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
