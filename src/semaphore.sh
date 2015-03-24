#!/bin/sh
SEMABUILD_PWD=`pwd`
SEMABUILD_SCP='scp -BC -o StrictHostKeyChecking=no'
SEMABUILD_TARGET='qreeves@icculus.org:/webspace/redeclipse.net/files'
SEMABUILD_APT='DEBIAN_FRONTEND=noninteractive apt-get'
SEMABUILD_SOURCE="http://redeclipse.net/files"
SEMABUILD_BUILD="${HOME}/build"
SEMABUILD_DIR="${SEMABUILD_BUILD}/${BRANCH_NAME}"

semabuild_setup() {
    echo "Setting up ${BRANCH_NAME}..."
    rm -rfv "${SEMABUILD_BUILD}" || return 1
    mkdir -pv "${SEMABUILD_DIR}" || return 1
    SEMABUILD_BASE=`git rev-parse HEAD` || return 1
    if [ "${BRANCH_NAME}" != "master" ]; then
        SEMABUILD_BASE_ANCESTOR=`git merge-base origin/master ${SEMABUILD_BASE}` || return 1
    fi
    git submodule init || return 1
    git submodule update || return 1
    cd "${SEMABUILD_PWD}/data" || return 1
    SEMABUILD_DATA=`git rev-parse HEAD` || return 1
    #SEMABUILD_DATA=`git submodule status -- data | sed -e 's/^.\(.*\) .*/\1/'`
    if [ "${BRANCH_NAME}" != "master" ]; then
        SEMABUILD_DATA_ANCESTOR=`git merge-base origin/master ${SEMABUILD_DATA}` || return 1
    fi
    cd "${SEMABUILD_PWD}" || return 1
    SEMABUILD_BASE_LAST=`curl --fail --silent "${SEMABUILD_SOURCE}/${BRANCH_NAME}/base.txt"`
    SEMABUILD_DATA_LAST=`curl --fail --silent "${SEMABUILD_SOURCE}/${BRANCH_NAME}/data.txt"`
    if [ "${BRANCH_NAME}" != "master" ]; then
        SEMABUILD_BASE_ANCESTOR_LAST=`curl --fail --silent "${SEMABUILD_SOURCE}/${BRANCH_NAME}/base.ancestor.txt"`
        SEMABUILD_DATA_ANCESTOR_LAST=`curl --fail --silent "${SEMABUILD_SOURCE}/${BRANCH_NAME}/data.ancestor.txt"`
    fi
    if [ -n "${SEMABUILD_BASE_LAST}" ]; then
        SEMABUILD_SRC_HASH="${SEMABUILD_BASE_LAST}"
    elif [ -n "${SEMABUILD_BASE_ANCESTOR}" ]; then
        SEMABUILD_SRC_HASH="${SEMABUILD_BASE_ANCESTOR}"
    else
        echo "Unable to determine a proper ancestor hash!"
        return 1
    fi
    SEMABUILD_BINS_LAST=`curl --fail --silent "${SEMABUILD_SOURCE}/${BRANCH_NAME}/bins.txt"`
    if [ -n "${SEMABUILD_BINS_LAST}" ]; then
        SEMABUILD_SRC_CHANGES=`git diff --name-only HEAD ${SEMABUILD_SRC_HASH} -- src` || return 1
        if [ -z "${SEMABUILD_SRC_CHANGES}" ]; then
            echo "No source files have been modified"
            SEMABUILD_DEPLOY="sync"
        else
            echo "Source files modified:"
            echo "${SEMABUILD_SRC_CHANGES}"
        fi
    fi
    return 0
}

semabuild_build() {
    echo "Building ${BRANCH_NAME}..."
    sudo dpkg --add-architecture i386 || return 1
    sudo ${SEMABUILD_APT} update || return 1
    sudo ${SEMABUILD_APT} -fy install build-essential zlib1g-dev libsdl-mixer1.2-dev libsdl-image1.2-dev || return 1
    make PLATFORM=linux64 PLATFORM_BIN=amd64 INSTDIR=${SEMABUILD_DIR}/linux/bin/amd64 CFLAGS=-m64 CXXFLAGS=-m64 LDFLAGS=-m64 -C src clean install || return 1
    sudo ${SEMABUILD_APT} -fy install binutils-mingw-w64 g++-mingw-w64 || return 1
    make PLATFORM=crossmingw64 PLATFORM_BIN=amd64 INSTDIR=${SEMABUILD_DIR}/windows/bin/amd64 CFLAGS=-m64 CXXFLAGS=-m64 LDFLAGS=-m64 -C src clean install || return 1
    make PLATFORM=crossmingw32 PLATFORM_BIN=x86 INSTDIR=${SEMABUILD_DIR}/windows/bin/x86 CFLAGS=-m32 CXXFLAGS=-m32 LDFLAGS=-m32 -C src clean install || return 1
    sudo ${SEMABUILD_APT} -fy remove zlib1g-dev libsdl1.2-dev libsdl-mixer1.2-dev libsdl-image1.2-dev libpng-dev || return 1
    sudo ${SEMABUILD_APT} -fy autoremove || return 1
    sudo ${SEMABUILD_APT} -fy install build-essential multiarch-support g++-multilib zlib1g-dev:i386 libsdl1.2-dev:i386 libsdl-mixer1.2-dev:i386 libsdl-image1.2-dev:i386 libpng-dev:i386 || return 1
    make PLATFORM=linux32 PLATFORM_BIN=x86 INSTDIR=${SEMABUILD_DIR}/linux/bin/x86 CFLAGS=-m32 CXXFLAGS=-m32 LDFLAGS=-m32 -C src clean install || return 1
    return 0
}

semabuild_sync() {
    echo "Syncing ${BRANCH_NAME} as no source files have changed."
    echo "base data" > "${SEMABUILD_DIR}/modules.txt"
    if [ -n "${SEMABUILD_BASE}" ] && [ "${SEMABUILD_BASE}" != "${SEMABUILD_BASE_LAST}" ]; then
        echo "Module 'base' commit updated, syncing that: ${SEMABUILD_BASE} -> ${SEMABUILD_BASE_LAST}"
        echo "${SEMABUILD_BASE}" > "${SEMABUILD_DIR}/base.txt"
    fi
    if [ -n "${SEMABUILD_DATA}" ] && [ "${SEMABUILD_DATA}" != "${SEMABUILD_DATA_LAST}" ]; then
        echo "Module 'data' commit updated, syncing that: ${SEMABUILD_DATA} -> ${SEMABUILD_DATA_LAST}"
        echo "${SEMABUILD_DATA}" > "${SEMABUILD_DIR}/data.txt"
    fi
    if [ "${BRANCH_NAME}" != "master" ]; then
        if [ -n "${SEMABUILD_BASE_ANCESTOR}" ] && [ "${SEMABUILD_BASE_ANCESTOR}" != "${SEMABUILD_BASE_ANCESTOR_LAST}" ]; then
            echo "Module 'base' ancestor updated, syncing that: ${SEMABUILD_BASE_ANCESTOR} -> ${SEMABUILD_BASE_ANCESTOR_LAST}"
            echo "${SEMABUILD_BASE_ANCESTOR}" > "${SEMABUILD_DIR}/base.ancestor.txt"
        fi
        if [ -n "${SEMABUILD_DATA_ANCESTOR}" ] && [ "${SEMABUILD_DATA_ANCESTOR}" != "${SEMABUILD_DATA_ANCESTOR_LAST}" ]; then
            echo "Module 'data' ancestor updated, syncing that: ${SEMABUILD_DATA_ANCESTOR} -> ${SEMABUILD_DATA_ANCESTOR_LAST}"
            echo "${SEMABUILD_DATA_ANCESTOR}" > "${SEMABUILD_DIR}/data.ancestor.txt"
        fi
    fi
    return 0
}

semabuild_deploy() {
    echo "Deploying ${BRANCH_NAME}..."
    # windows
    cd "${SEMABUILD_DIR}/windows" || return 1
    zip -r "${SEMABUILD_DIR}/windows.zip" . || return 1
    # linux
    cd "${SEMABUILD_DIR}/linux" || return 1
    tar -zcvf "${SEMABUILD_DIR}/linux.tar.gz" . || return 1
    # env
    cd "${SEMABUILD_PWD}" || return 1
    # sha
    rm -rfv "${SEMABUILD_DIR}/windows" "${SEMABUILD_DIR}/linux" || return 1
    echo "base data" > "${SEMABUILD_DIR}/modules.txt"
    echo "Module 'base' commit, syncing: ${SEMABUILD_BASE}"
    echo "${SEMABUILD_BASE}" > "${SEMABUILD_DIR}/bins.txt"
    echo "${SEMABUILD_BASE}" > "${SEMABUILD_DIR}/base.txt"
    echo "Module 'data' commit, syncing: ${SEMABUILD_DATA}"
    echo "${SEMABUILD_DATA}" > "${SEMABUILD_DIR}/data.txt"
    if [ "${BRANCH_NAME}" != "master" ]; then
        echo "Module 'base' ancestor, syncing: ${SEMABUILD_BASE_ANCESTOR}"
        echo "${SEMABUILD_BASE_ANCESTOR}" > "${SEMABUILD_DIR}/base.ancestor.txt"
        echo "Module 'data' ancestor, syncing: ${SEMABUILD_DATA_ANCESTOR}"
        echo "${SEMABUILD_DATA_ANCESTOR}" > "${SEMABUILD_DIR}/data.ancestor.txt"
    fi
    return 0
}

semabuild_send() {
    echo "Sending ${BRANCH_NAME}..."
    cd "${SEMABUILD_BUILD}" || return 1
    if [ -e "${BRANCH_NAME}" ]; then
        ${SEMABUILD_SCP} -r "${BRANCH_NAME}" "${SEMABUILD_TARGET}" || return 1
    else
        echo "Failed to send ${BRANCH_NAME} as the folder doesn't exist!"
        return 1
    fi
    cd "${SEMABUILD_PWD}" || return 1
    return 0
}

semabuild_setup
if [ $? -ne 0 ]; then
    echo "Failed to setup ${BRANCH_NAME}!"
    exit 1
fi
if [ "${SEMABUILD_DEPLOY}" = "sync" ]; then
    semabuild_sync
    if [ $? -ne 0 ]; then
        echo "Failed to sync ${BRANCH_NAME}!"
        exit 1
    fi
    semabuild_send
    if [ $? -ne 0 ]; then
        cd "${SEMABUILD_PWD}"
        echo "Failed to deploy ${BRANCH_NAME}!"
        exit 1
    fi
else
    semabuild_build
    if [ $? -ne 0 ]; then
        echo "Failed to build ${BRANCH_NAME}!"
        exit 1
    fi
    semabuild_deploy
    if [ $? -ne 0 ]; then
        cd "${SEMABUILD_PWD}"
        echo "Failed to deploy ${BRANCH_NAME}!"
        exit 1
    fi
    semabuild_send
    if [ $? -ne 0 ]; then
        cd "${SEMABUILD_PWD}"
        echo "Failed to deploy ${BRANCH_NAME}!"
        exit 1
    fi
fi
