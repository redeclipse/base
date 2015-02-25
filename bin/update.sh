#!/bin/sh
if [ "${REDECLIPSE_CALLED}" = "true" ]; then REDECLIPSE_EXITU="return"; else REDECLIPSE_EXITU="exit"; fi
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
    if [ -z "${REDECLIPSE_BRANCH+isset}" ]; then
        REDECLIPSE_BRANCH="stable"
        if [ -e ".git" ]; then REDECLIPSE_BRANCH="master"; fi
        if [ -e "${REDECLIPSE_PATH}/bin/branch.txt" ]; then REDECLIPSE_BRANCH=`cat "${REDECLIPSE_PATH}/bin/branch.txt"`; fi
    fi
    if [ "${REDECLIPSE_BRANCH}" = "devel" ]; then REDECLIPSE_BRANCH="master"; fi
    if [ "${REDECLIPSE_BRANCH}" != "stable" ] && [ "${REDECLIPSE_BRANCH}" != "master" ]; then
        echo "Unsupported update branch: \"${REDECLIPSE_BRANCH}\""
        return 1
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
    echo "Branch: ${REDECLIPSE_UPDATE}"
    echo "Folder: ${REDECLIPSE_PATH}"
    echo "Cached: ${REDECLIPSE_TEMP}"
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
    if [ "${REDECLIPSE_BRANCH}" != "stable" ]; then
        redeclipse_update_bins
        return $?
    fi
    redeclipse_update_base
    return $?
}

redeclipse_update_base() {
    echo ""
    if [ -e "${REDECLIPSE_PATH}/bin/base.txt" ]; then REDECLIPSE_BASE=`cat "${REDECLIPSE_PATH}/bin/base.txt"`; fi
    if [ -z "${REDECLIPSE_BASE}" ]; then REDECLIPSE_BASE="none"; fi
    echo "[I] base: ${REDECLIPSE_BASE}"
    REDECLIPSE_BASE_CACHED="none"
    if ! [ -e "${REDECLIPSE_TEMP}/base.txt" ]; then
        redeclipse_update_baseget
        return $?
    fi
    REDECLIPSE_BASE_CACHED=`cat "${REDECLIPSE_TEMP}/base.txt"`
    if [ -z "${REDECLIPSE_BASE_CACHED}" ]; then REDECLIPSE_BASE_CACHED="none"; fi
    echo "[C] base: ${REDECLIPSE_BASE_CACHED}"
    rm -f "${REDECLIPSE_TEMP}/base.txt"
    redeclipse_update_baseget
    return $?
}

redeclipse_update_baseget() {
    ${REDECLIPSE_CURL} --silent --output "${REDECLIPSE_TEMP}/base.txt" "${REDECLIPSE_SOURCE}/${REDECLIPSE_UPDATE}/base.txt"
    if ! [ -e "${REDECLIPSE_TEMP}/base.txt" ]; then
        echo "Failed to retrieve base update information."
        redeclipse_update_data
        return $?
    fi
    REDECLIPSE_BASE_REMOTE=`cat "${REDECLIPSE_TEMP}/base.txt"`
    if [ -z "${REDECLIPSE_BASE_REMOTE}" ]; then
        echo "Failed to read base update information."
        redeclipse_update_data
        return $?
    fi
    echo "[R] base: ${REDECLIPSE_BASE_REMOTE}"
    if [ "${REDECLIPSE_BASE_REMOTE}" = "${REDECLIPSE_BASE}" ]; then
        redeclipse_update_data
        return $?
    fi
    if [ "${REDECLIPSE_BASE}" = "none" ]; then
        redeclipse_update_baseblob
        return $?
    fi
    redeclipse_update_basepatch
    return $?
}

redeclipse_update_basepatch() {
    if [ -e "${REDECLIPSE_TEMP}/base.patch" ]; then rm -f "${REDECLIPSE_TEMP}/base.patch"; fi
    if [ -e "${REDECLIPSE_TEMP}/base.${REDECLIPSE_ARCHEXT}" ]; then rm -f "${REDECLIPSE_TEMP}/base.${REDECLIPSE_ARCHEXT}"; fi
    echo "[D] base: ${REDECLIPSE_GITHUB}/base/compare/${REDECLIPSE_BASE}...${REDECLIPSE_BASE_REMOTE}.patch"
    echo ""
    ${REDECLIPSE_CURL} --output "${REDECLIPSE_TEMP}/base.patch" "${REDECLIPSE_GITHUB}/base/compare/${REDECLIPSE_BASE}...${REDECLIPSE_BASE_REMOTE}.patch"
    if ! [ -e "${REDECLIPSE_TEMP}/base.patch" ]; then
        echo "Failed to retrieve base update package. Downloading full zip instead."
        redeclipse_update_baseblob
        return $?
    fi
    redeclipse_update_basepatchdeploy
    return $?
}

redeclipse_update_basepatchdeploy() {
    echo "${REDECLIPSE_GITAPPLY} --directory=\"${REDECLIPSE_PATH}\" \"${REDECLIPSE_TEMP}/base.patch\" && (" >> "${REDECLIPSE_TEMP}/install.sh"
    echo "    echo \"${REDECLIPSE_BASE_REMOTE}\" > \"${REDECLIPSE_PATH}/bin/base.txt\"" >> "${REDECLIPSE_TEMP}/install.sh"
    echo ") || (" >> "${REDECLIPSE_TEMP}/install.sh"
    echo "    echo \"none\" > \"${REDECLIPSE_PATH}/bin/base.txt\"" >> "${REDECLIPSE_TEMP}/install.sh"
    echo "    rm -f \"${REDECLIPSE_TEMP}/base.txt\"" >> "${REDECLIPSE_TEMP}/install.sh"
    echo "    REDECLIPSE_ERROR=\"true\"" >> "${REDECLIPSE_TEMP}/install.sh"
    echo ")" >> "${REDECLIPSE_TEMP}/install.sh"
    REDECLIPSE_DEPLOY="true"
    redeclipse_update_data
    return $?
}

redeclipse_update_baseblob() {
    if [ -e "${REDECLIPSE_TEMP}/base.${REDECLIPSE_ARCHEXT}" ]; then
        if [ "${REDECLIPSE_BASE_CACHED}" = "${REDECLIPSE_BASE_REMOTE}" ]; then
            echo "[F] base: Using cached file \"${REDECLIPSE_TEMP}/base.${REDECLIPSE_ARCHEXT}\""
            redeclipse_update_baseblobdeploy
            return $?
        else
            rm -f "${REDECLIPSE_TEMP}/base.${REDECLIPSE_ARCHEXT}"
        fi
    fi
    echo "[D] base: ${REDECLIPSE_GITHUB}/base/${REDECLIPSE_BLOB}/${REDECLIPSE_BASE_REMOTE}"
    echo ""
    ${REDECLIPSE_CURL} --output "${REDECLIPSE_TEMP}/base.${REDECLIPSE_ARCHEXT}" "${REDECLIPSE_GITHUB}/base/${REDECLIPSE_BLOB}/${REDECLIPSE_BASE_REMOTE}"
    if ! [ -e "${REDECLIPSE_TEMP}/base.${REDECLIPSE_ARCHEXT}" ]; then
        echo "Failed to retrieve base update package."
        redeclipse_update_data
        return $?
    fi
    redeclipse_update_baseblobdeploy
    return $?
}

redeclipse_update_baseblobdeploy() {
    if [ "${REDECLIPSE_BLOB}" = "zipball" ]; then
        echo "${REDECLIPSE_UNZIP} -o \"${REDECLIPSE_TEMP}/base.${REDECLIPSE_ARCHEXT}\" -d \"${REDECLIPSE_TEMP}\" && (" >> "${REDECLIPSE_TEMP}/install.sh"
    else
        echo "${REDECLIPSE_TAR} --file=\"${REDECLIPSE_TEMP}/base.${REDECLIPSE_ARCHEXT}\" --directory=\"${REDECLIPSE_TEMP}\" && (" >> "${REDECLIPSE_TEMP}/install.sh"
    fi
    echo "   copy --recursive --force --verbose \"${REDECLIPSE_TEMP}/red-eclipse-base-$(echo "$REDECLIPSE_DATA_REMOTE" | cut -b 1-7)/*\" \"${REDECLIPSE_PATH}\"" >> "${REDECLIPSE_TEMP}/install.sh"
    echo "   rm -rf \"${REDECLIPSE_TEMP}/red-eclipse-base-$(echo "$REDECLIPSE_DATA_REMOTE" | cut -b 1-7)\"" >> "${REDECLIPSE_TEMP}/install.sh"
    echo "   echo \"${REDECLIPSE_BASE_REMOTE}\" > \"${REDECLIPSE_PATH}/bin/base.txt\"" >> "${REDECLIPSE_TEMP}/install.sh"
    echo ") || (" >> "${REDECLIPSE_TEMP}/install.sh"
    echo "    rm -f \"${REDECLIPSE_TEMP}/base.txt\"" >> "${REDECLIPSE_TEMP}/install.sh"
    echo "    REDECLIPSE_ERROR=\"true\"" >> "${REDECLIPSE_TEMP}/install.sh"
    echo ")" >> "${REDECLIPSE_TEMP}/install.sh"
    REDECLIPSE_DEPLOY="true"
    redeclipse_update_data
    return $?
}

redeclipse_update_data() {
    echo ""
    if  [ -e "${REDECLIPSE_PATH}/data/readme.txt" ]; then
        redeclipse_update_dataver
        return $?
    fi
    echo "Unable to find \"data/readme.txt\". Will start from scratch."
    REDECLIPSE_DATA="none"
    echo "mkdir -p \"${REDECLIPSE_PATH}/data\"" >> "${REDECLIPSE_TEMP}/install.sh"
    redeclipse_update_dataget
    return $?
}

redeclipse_update_dataver() {
    echo ""
    if [ -e "${REDECLIPSE_PATH}/bin/data.txt" ]; then REDECLIPSE_DATA=`cat "${REDECLIPSE_PATH}/bin/data.txt"`; fi
    if [ -z "${REDECLIPSE_DATA}" ]; then REDECLIPSE_DATA="none"; fi
    echo "[I] data: ${REDECLIPSE_DATA}"
    REDECLIPSE_DATA_CACHED="none"
    if ! [ -e "${REDECLIPSE_TEMP}/data.txt" ]; then
        redeclipse_update_dataget
        return $?
    fi
    REDECLIPSE_DATA_CACHED=`cat "${REDECLIPSE_TEMP}/data.txt"`
    if [ -z "${REDECLIPSE_DATA_CACHED}" ]; then REDECLIPSE_DATA_CACHED="none"; fi
    echo "[C] data: ${REDECLIPSE_DATA_CACHED}"
    rm -f "${REDECLIPSE_TEMP}/data.txt"
    redeclipse_update_dataget
    return $?
}

redeclipse_update_dataget() {
    ${REDECLIPSE_CURL} --silent --output "${REDECLIPSE_TEMP}/data.txt" "${REDECLIPSE_SOURCE}/${REDECLIPSE_UPDATE}/data.txt"
    if ! [ -e "${REDECLIPSE_TEMP}/data.txt" ]; then
        echo "Failed to retrieve data update information."
        redeclipse_update_bins
        return $?
    fi
    REDECLIPSE_DATA_REMOTE=`cat "${REDECLIPSE_TEMP}/data.txt"`
    if [ -z "${REDECLIPSE_DATA_REMOTE}" ]; then
        echo "Failed to read data update information."
        redeclipse_update_bins
        return $?
    fi
    echo "[R] data: ${REDECLIPSE_DATA_REMOTE}"
    if [ "${REDECLIPSE_DATA_REMOTE}" = "${REDECLIPSE_DATA}" ]; then
        redeclipse_update_bins
        return $?
    fi
    if [ "${REDECLIPSE_DATA}" = "none" ]; then
        redeclipse_update_datablob
        return $?
    fi
    redeclipse_update_datapatch
    return $?
}

redeclipse_update_datapatch() {
    if [ -e "${REDECLIPSE_TEMP}/data.patch" ]; then rm -f "${REDECLIPSE_TEMP}/data.patch"; fi
    if [ -e "${REDECLIPSE_TEMP}/data.${REDECLIPSE_ARCHEXT}" ]; then rm -f "${REDECLIPSE_TEMP}/data.${REDECLIPSE_ARCHEXT}"; fi
    echo "[D] data: ${REDECLIPSE_GITHUB}/data/compare/${REDECLIPSE_DATA}...${REDECLIPSE_DATA_REMOTE}.patch"
    echo ""
    ${REDECLIPSE_CURL} --output "${REDECLIPSE_TEMP}/data.patch" "${REDECLIPSE_GITHUB}/data/compare/${REDECLIPSE_DATA}...${REDECLIPSE_DATA_REMOTE}.patch"
    if ! [ -e "${REDECLIPSE_TEMP}/data.patch" ]; then
        echo "Failed to retrieve data update package. Downloading full zip instead."
        redeclipse_update_datablob
        return $?
    fi
    redeclipse_update_datapatchdeploy
    return $?
}

redeclipse_update_datapatchdeploy() {
    echo "${REDECLIPSE_GITAPPLY} --directory=\"${REDECLIPSE_PATH}/data\" \"${REDECLIPSE_TEMP}/data.patch\" && (" >> "${REDECLIPSE_TEMP}/install.sh"
    echo "    echo \"${REDECLIPSE_DATA_REMOTE}\" > \"${REDECLIPSE_PATH}/bin/data.txt\"" >> "${REDECLIPSE_TEMP}/install.sh"
    echo ") || (" >> "${REDECLIPSE_TEMP}/install.sh"
    echo "    echo \"none\" > \"${REDECLIPSE_PATH}/bin/data.txt\"" >> "${REDECLIPSE_TEMP}/install.sh"
    echo "    rm -f \"${REDECLIPSE_TEMP}/data.txt\"" >> "${REDECLIPSE_TEMP}/install.sh"
    echo "    REDECLIPSE_ERROR=\"true\"" >> "${REDECLIPSE_TEMP}/install.sh"
    echo ")" >> "${REDECLIPSE_TEMP}/install.sh"
    REDECLIPSE_DEPLOY="true"
    redeclipse_update_bins
    return $?
}

redeclipse_update_datablob() {
    if [ -e "${REDECLIPSE_TEMP}/data.${REDECLIPSE_ARCHEXT}" ]; then
        if [ "${REDECLIPSE_DATA_CACHED}" = "${REDECLIPSE_DATA_REMOTE}" ]; then
            echo "[F] data: Using cached file \"${REDECLIPSE_TEMP}/data.${REDECLIPSE_ARCHEXT}\""
            redeclipse_update_datablobdeploy
            return $?
        else
            rm -f "${REDECLIPSE_TEMP}/data.${REDECLIPSE_ARCHEXT}"
        fi
    fi
    echo "[D] data: ${REDECLIPSE_GITHUB}/data/${REDECLIPSE_BLOB}/${REDECLIPSE_DATA_REMOTE}"
    echo ""
    ${REDECLIPSE_CURL} --output "${REDECLIPSE_TEMP}/data.${REDECLIPSE_ARCHEXT}" "${REDECLIPSE_GITHUB}/data/${REDECLIPSE_BLOB}/${REDECLIPSE_DATA_REMOTE}"
    if ! [ -e "${REDECLIPSE_TEMP}/data.${REDECLIPSE_ARCHEXT}" ]; then
        echo "Failed to retrieve data update package."
        redeclipse_update_bins
        return $?
    fi
    redeclipse_update_datablobdeploy
    return $?
}

redeclipse_update_datablobdeploy() {
    if [ "${REDECLIPSE_BLOB}" = "zipball" ]; then
        echo "${REDECLIPSE_UNZIP} -o \"${REDECLIPSE_TEMP}/data.${REDECLIPSE_ARCHEXT}\" -d \"${REDECLIPSE_TEMP}\" && (" >> "${REDECLIPSE_TEMP}/install.sh"
    else
        echo "${REDECLIPSE_TAR} --file=\"${REDECLIPSE_TEMP}/data.${REDECLIPSE_ARCHEXT}\" --directory=\"${REDECLIPSE_TEMP}\" && (" >> "${REDECLIPSE_TEMP}/install.sh"
    fi
    echo "   copy --recursive --force --verbose \"${REDECLIPSE_TEMP}/red-eclipse-data-$(echo "$REDECLIPSE_DATA_REMOTE" | cut -b 1-7)/*\" \"${REDECLIPSE_PATH}/data\"" >> "${REDECLIPSE_TEMP}/install.sh"
    echo "   rm -rf \"${REDECLIPSE_TEMP}/red-eclipse-data-$(echo "$REDECLIPSE_DATA_REMOTE" | cut -b 1-7)\"" >> "${REDECLIPSE_TEMP}/install.sh"
    echo "   echo \"${REDECLIPSE_DATA_REMOTE}\" > \"${REDECLIPSE_PATH}/bin/data.txt\"" >> "${REDECLIPSE_TEMP}/install.sh"
    echo ") || (" >> "${REDECLIPSE_TEMP}/install.sh"
    echo "    rm -f \"${REDECLIPSE_TEMP}/data.txt\"" >> "${REDECLIPSE_TEMP}/install.sh"
    echo "    REDECLIPSE_ERROR=\"true\"" >> "${REDECLIPSE_TEMP}/install.sh"
    echo ")" >> "${REDECLIPSE_TEMP}/install.sh"
    REDECLIPSE_DEPLOY="true"
    redeclipse_update_bins
    return $?
}

redeclipse_update_bins() {
    echo ""
    if [ -e "${REDECLIPSE_PATH}/bin/bins.txt" ]; then REDECLIPSE_BINS=`cat "${REDECLIPSE_PATH}/bin/bins.txt"`; fi
    if [ -z "${REDECLIPSE_BINS}" ]; then REDECLIPSE_BINS="none"; fi
    echo "[I] bins: ${REDECLIPSE_BINS}"
    REDECLIPSE_BINS_CACHED="none"
    if ! [ -e "${REDECLIPSE_TEMP}/bins.txt" ]; then
        redeclipse_update_binsget
        return $?
    fi
    REDECLIPSE_BINS_CACHED=`cat "${REDECLIPSE_TEMP}/bins.txt"`
    if [ -z "${REDECLIPSE_BINS_CACHED}" ]; then REDECLIPSE_BINS_CACHED="none"; fi
    echo "[C] bins: ${REDECLIPSE_BINS_CACHED}"
    rm -f "${REDECLIPSE_TEMP}/bins.txt"
    redeclipse_update_binsget
    return $?
}

redeclipse_update_binsget() {
    ${REDECLIPSE_CURL} --silent --output "${REDECLIPSE_TEMP}/bins.txt" "${REDECLIPSE_SOURCE}/${REDECLIPSE_UPDATE}/bins.txt"
    if ! [ -e "${REDECLIPSE_TEMP}/bins.txt" ]; then
        echo "Failed to retrieve bins update information."
        redeclipse_update_deploy
        return $?
    fi
    REDECLIPSE_BINS_REMOTE=`cat "${REDECLIPSE_TEMP}/bins.txt"`
    if [ -z "${REDECLIPSE_BINS_REMOTE}" ]; then
        echo "Failed to read bins update information."
        redeclipse_update_deploy
        return $?
    fi
    echo "[R] bins: ${REDECLIPSE_BINS_REMOTE}"
    if [ "${REDECLIPSE_TRYUPDATE}" != "true" ] && [ "${REDECLIPSE_BINS_REMOTE}" = "${REDECLIPSE_BINS}" ]; then
        redeclipse_update_deploy
        return $?
    fi
    redeclipse_update_binsblob
    return $?
}

redeclipse_update_binsblob() {
    if [ -e "${REDECLIPSE_TEMP}/${REDECLIPSE_ARCHIVE}" ]; then
        if [ "${REDECLIPSE_BINS_CACHED}" = "${REDECLIPSE_BINS_REMOTE}" ]; then
            echo "[F] bins: Using cached file \"${REDECLIPSE_TEMP}/${REDECLIPSE_ARCHIVE}\""
            redeclipse_update_binsdeploy
            return $?
        else
            rm -f "${REDECLIPSE_TEMP}/${REDECLIPSE_ARCHIVE}"
        fi
    fi
    echo "[D] bins: ${REDECLIPSE_SOURCE}/${REDECLIPSE_UPDATE}/${REDECLIPSE_ARCHIVE}"
    echo ""
    ${REDECLIPSE_CURL} --output "${REDECLIPSE_TEMP}/${REDECLIPSE_ARCHIVE}" "${REDECLIPSE_SOURCE}/${REDECLIPSE_UPDATE}/${REDECLIPSE_ARCHIVE}"
    if ! [ -e "${REDECLIPSE_TEMP}/${REDECLIPSE_ARCHIVE}" ]; then
        echo "Failed to retrieve bins update package."
        redeclipse_update_deploy
        return $?
    fi
    redeclipse_update_binsdeploy
    return $?
}

redeclipse_update_binsdeploy() {
    if [ "${REDECLIPSE_TARGET}" = "windows" ]; then
        echo "${REDECLIPSE_UNZIP} -o \"${REDECLIPSE_TEMP}/${REDECLIPSE_ARCHIVE}\" -d \"${REDECLIPSE_PATH}\" && (" >> "${REDECLIPSE_TEMP}/install.sh"
    else
        echo "${REDECLIPSE_TAR} --file=\"${REDECLIPSE_TEMP}/${REDECLIPSE_ARCHIVE}\" --directory=\"${REDECLIPSE_PATH}\" && (" >> "${REDECLIPSE_TEMP}/install.sh"
    fi
    echo "    echo \"${REDECLIPSE_BINS_REMOTE}\" > \"${REDECLIPSE_PATH}/bin/bins.txt\"" >> "${REDECLIPSE_TEMP}/install.sh"
    echo ") || (" >> "${REDECLIPSE_TEMP}/install.sh"
    echo "    rm -f \"${REDECLIPSE_TEMP}/bins.txt\"" >> "${REDECLIPSE_TEMP}/install.sh"
    echo "    REDECLIPSE_ERROR=\"true\"" >> "${REDECLIPSE_TEMP}/install.sh"
    echo ")" >> "${REDECLIPSE_TEMP}/install.sh"
    REDECLIPSE_DEPLOY="true"
    redeclipse_update_deploy
    return $?
}

redeclipse_update_deploy() {
    echo ""
    if [ "${REDECLIPSE_DEPLOY}" != "true" ]; then
        echo "Everything is already up to date."
        return 0
    fi
    echo "if [ \"\${REDECLIPSE_ERROR}\" = \"true\" ]; then exit 1; else exit 0; fi" >> "${REDECLIPSE_TEMP}/install.sh"
    echo "Deploying: \"${REDECLIPSE_TEMP}/install.sh\""
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
        echo "Updated successfully."
        echo "${REDECLIPSE_BRANCH}" > "${REDECLIPSE_PATH}/bin/branch.txt"
        return 0
    ) || (
        echo ""
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
