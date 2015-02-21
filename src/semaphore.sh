#!/bin/sh
SEMABUILD_PWD=`pwd`
SEMABUILD_SCP='scp -BCv -i "${HOME}/.ssh/public_rsa" -o StrictHostKeyChecking=no'
SEMABUILD_TARGET='qreeves@icculus.org:/webspace/redeclipse.net/files'
SEMABUILD_APT='DEBIAN_FRONTEND=noninteractive apt-get'
SEMABUILD_SOURCE="http://redeclipse.net/files"
SEMABUILD_BUILD="${HOME}/build"
SEMABUILD_DIR="${SEMABUILD_BUILD}/${BRANCH_NAME}"

semabuild_setup() {
    echo "Setting up ${BRANCH_NAME}..."
    rm -rfv "${SEMABUILD_BUILD}" || return 1
    mkdir -pv "${SEMABUILD_DIR}" || return 1
    mkdir -pv "${SEMAPHORE_CACHE_DIR}/apt/archives/partial" || return 1
    sudo cp -ruv "/var/cache/apt" "${SEMAPHORE_CACHE_DIR}/apt" || return 1
    sudo rm -rfv "/var/cache/apt" || return 1
    sudo ln -sv "${SEMAPHORE_CACHE_DIR}/apt" "/var/cache/apt" || return 1
    sudo dpkg --add-architecture i386 || return 1
    sudo ${SEMABUILD_APT} update || return 1
    return 0
}

semabuild_parse() {
    echo "Parsing ${BRANCH_NAME}..."
    SEMABUILD_BASE=`git rev-parse HEAD` || return 1
    git submodule init data || return 1
    git submodule update data || return 1
    cd "${SEMABUILD_PWD}/data" || return 1
    SEMABUILD_DATA=`git rev-parse HEAD` || return 1
    cd "${SEMABUILD_PWD}" || return 1
    SEMABUILD_BUILD_LAST=`curl --fail --silent "${SEMABUILD_SOURCE}/${BRANCH_NAME}/base.txt"` || return 1
    if [ -n "${SEMABUILD_BUILD_LAST}" ]; then
        SEMABUILD_DATA_LAST=`curl --fail --silent "${SEMABUILD_SOURCE}/${BRANCH_NAME}/data.txt"` || return 1
        SEMABUILD_SRC_CHANGES=`git diff --name-only HEAD ${SEMABUILD_BUILD_LAST} -- src` || return 1
        if [ -z "${SEMABUILD_SRC_CHANGES}" ]; then SEMABUILD_DEPLOY="sync"; fi
    fi
    return 0
}

semabuild_build() {
    echo "Building ${BRANCH_NAME}..."
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
    if [ -n "${SEMABUILD_BASE}" ] && [ -n "${SEMABUILD_BASE_LAST}" ] && [ "${SEMABUILD_BASE}" != "${SEMABUILD_BUILD_LAST}" ]; then
        echo "Module 'base' commit updated, syncing that: ${SEMABUILD_BASE} -> ${SEMABUILD_BUILD_LAST}"
        echo "${SEMABUILD_BASE}" > "${SEMABUILD_DIR}/base.txt"
    fi
    if [ -n "${SEMABUILD_DATA}" ] && [ -n "${SEMABUILD_DATA_LAST}" ] && [ "${SEMABUILD_DATA}" != "${SEMABUILD_DATA_LAST}" ]; then
        echo "Module 'data' commit updated, syncing that: ${SEMABUILD_DATA_LAST} -> ${SEMABUILD_DATA}"
        echo "${SEMABUILD_DATA}" > "${SEMABUILD_DIR}/data.txt"
    fi
    cd "${SEMABUILD_BUILD}" || return 1
    ${SEMABUILD_SCP} -r "${BRANCH_NAME}" "${SEMABUILD_TARGET}" || return 1
    cd "${SEMABUILD_PWD}" || return 1
    return 0
}

semabuild_deploy() {
    # windows
    cd "${SEMABUILD_DIR}/windows" || return 1
    zip -r "${SEMABUILD_DIR}/windows.zip" . || return 1
    # linux
    cd "${SEMABUILD_DIR}/linux" || return 1
    tar -zcvf "${SEMABUILD_DIR}/linux.tar.gz" . || return 1
    # env
    cd "${SEMABUILD_DIR}" || return 1
    rm -rfv "${SEMABUILD_DIR}/windows" "${SEMABUILD_DIR}/linux" || return 1
    echo "${SEMABUILD_BASE}" > "${SEMABUILD_DIR}/bins.txt"
    echo "${SEMABUILD_BASE}" > "${SEMABUILD_DIR}/base.txt"
    echo "${SEMABUILD_DATA}" > "${SEMABUILD_DIR}/data.txt"
    # deploy
    cd "${SEMABUILD_BUILD}" || return 1
    ${SEMABUILD_SCP} -r "${BRANCH_NAME}" "${SEMABUILD_TARGET}" || return 1
    cd "${SEMABUILD_PWD}" || return 1
    return 0
}

semabuild_setup
if [ $? -ne 0 ]; then
    echo "Failed to setup ${BRANCH_NAME}!"
    exit 1
fi
semabuild_parse
if [ $? -ne 0 ]; then
    echo "Failed to parse ${BRANCH_NAME}!"
    exit 1
fi
if [ "${SEMABUILD_DEPLOY}" = "sync" ]; then
    echo "Syncing ${BRANCH_NAME} as no source files have changed."
    semabuild_sync
    if [ $? -ne 0 ]; then
        echo "Failed to sync ${BRANCH_NAME}!"
        exit 1
    fi
    exit 0
fi
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
exit 0
