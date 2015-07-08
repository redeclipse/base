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
    git submodule init || return 1
    git submodule update || return 1
    #pushd "${SEMABUILD_PWD}/data" || return 1
    #SEMABUILD_DATA=`git rev-parse HEAD` || (popd; return 1)
    #popd
    SEMABUILD_BINS_LAST=`curl --fail --silent "${SEMABUILD_SOURCE}/${BRANCH_NAME}/bins.txt"`
    if [ -z "${SEMABUILD_BINS_LAST}" ]; then
        echo "Unable to determine previous hash for comparison!"
        return 1
    fi
    SEMABUILD_SRC_CHANGES=`git diff --name-only HEAD ${SEMABUILD_BINS_LAST} -- src` || return 1
    if [ -z "${SEMABUILD_SRC_CHANGES}" ]; then
        echo "No source files have been modified"
        SEMABUILD_DEPLOY="sync"
    else
        echo "Source files modified:"
        echo "${SEMABUILD_SRC_CHANGES}"
    fi
    return 0
}

semabuild_build() {
    echo "Building ${BRANCH_NAME}..."
    sudo dpkg --add-architecture i386 || return 1
    sudo ${SEMABUILD_APT} update || return 1
    sudo ${SEMABUILD_APT} -fy install build-essential multiarch-support gcc-multilib g++-multilib zlib1g-dev libsdl-mixer1.2-dev libsdl-image1.2-dev binutils-mingw-w64 g++-mingw-w64 || return 1
    make PLATFORM=crossmingw64 PLATFORM_BIN=amd64 INSTDIR=${SEMABUILD_DIR}/windows/bin/amd64 CFLAGS=-m64 CXXFLAGS=-m64 LDFLAGS=-m64 -C src clean install || return 1
    make PLATFORM=crossmingw32 PLATFORM_BIN=x86 INSTDIR=${SEMABUILD_DIR}/windows/bin/x86 CFLAGS=-m32 CXXFLAGS=-m32 LDFLAGS=-m32 -C src clean install || return 1
    make PLATFORM=linux64 PLATFORM_BIN=amd64 INSTDIR=${SEMABUILD_DIR}/linux/bin/amd64 CFLAGS=-m64 CXXFLAGS=-m64 LDFLAGS=-m64 -C src clean install || return 1
    sudo apt-get -o Dpkg::Options::="--force-overwrite" -fy install zlib1g-dev:i386 libsdl1.2-dev:i386 libsdl-mixer1.2-dev:i386 libsdl-image1.2-dev:i386 libpng12-dev:i386 libcaca-dev:i386 libglu1-mesa-dev:i386 libgl1-mesa-dev:i386 || sudo apt-get -o Dpkg::Options::="--force-overwrite" -fy install || return 1
    sudo apt-get -o Dpkg::Options::="--force-overwrite" -fy install gcc:i386 g++:i386 || sudo apt-get -o Dpkg::Options::="--force-overwrite" -fy install || return 1
    make PLATFORM=linux32 PLATFORM_BIN=x86 INSTDIR=${SEMABUILD_DIR}/linux/bin/x86 CFLAGS=-m32 CXXFLAGS=-m32 LDFLAGS=-m32 -C src clean install || return 1
    return 0
}

semabuild_sync() {
    echo "Syncing ${BRANCH_NAME} as no source files have changed."
    echo "base" > "${SEMABUILD_DIR}/modules.txt"
    if [ -n "${SEMABUILD_BASE}" ] && [ "${SEMABUILD_BASE}" != "${SEMABUILD_BASE_LAST}" ]; then
        echo "Module 'base' commit updated, syncing that: ${SEMABUILD_BASE} -> ${SEMABUILD_BASE_LAST}"
        echo "${SEMABUILD_BASE}" > "${SEMABUILD_DIR}/base.txt"
    fi
    #if [ -n "${SEMABUILD_DATA}" ] && [ "${SEMABUILD_DATA}" != "${SEMABUILD_DATA_LAST}" ]; then
    #    echo "Module 'data' commit updated, syncing that: ${SEMABUILD_DATA} -> ${SEMABUILD_DATA_LAST}"
    #    echo "${SEMABUILD_DATA}" > "${SEMABUILD_DIR}/data.txt"
    #fi
    return 0
}

semabuild_deploy() {
    echo "Deploying ${BRANCH_NAME}..."
    # windows
    pushd "${SEMABUILD_DIR}/windows" || return 1
    zip -r "${SEMABUILD_DIR}/windows.zip" . || (popd; return 1)
    popd
    # linux
    pushd "${SEMABUILD_DIR}/linux" || return 1
    tar -zcvf "${SEMABUILD_DIR}/linux.tar.gz" . || (popd; return 1)
    popd
    # env
    rm -rfv "${SEMABUILD_DIR}/windows" "${SEMABUILD_DIR}/linux" || return 1
    # sha
    pushd "${SEMABUILD_PWD}" || return 1
    echo "base" > "${SEMABUILD_DIR}/modules.txt"
    echo "Module 'base' commit, syncing: ${SEMABUILD_BASE}"
    echo "${SEMABUILD_BASE}" > "${SEMABUILD_DIR}/bins.txt"
    echo "${SEMABUILD_BASE}" > "${SEMABUILD_DIR}/base.txt"
    #echo "Module 'data' commit, syncing: ${SEMABUILD_DATA}"
    #echo "${SEMABUILD_DATA}" > "${SEMABUILD_DIR}/data.txt"
    popd
    return 0
}

semabuild_send() {
    echo "Sending ${BRANCH_NAME}..."
    pushd "${SEMABUILD_BUILD}" || return 1
    if [ -e "${BRANCH_NAME}" ]; then
        ${SEMABUILD_SCP} -r "${BRANCH_NAME}" "${SEMABUILD_TARGET}" || (popd; return 1)
    else
        echo "Failed to send ${BRANCH_NAME} as the folder doesn't exist!"
        popd
        return 1
    fi
    popd
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
        echo "Failed to deploy ${BRANCH_NAME}!"
        exit 1
    fi
    semabuild_send
    if [ $? -ne 0 ]; then
        echo "Failed to deploy ${BRANCH_NAME}!"
        exit 1
    fi
fi
