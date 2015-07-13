#!/bin/sh
SEMABUILD_PWD=`pwd`
SEMABUILD_BUILD="${HOME}/build"
SEMABUILD_DIR="${SEMABUILD_BUILD}/${BRANCH_NAME}"
SEMABUILD_SCP='scp -BC -o StrictHostKeyChecking=no'
SEMABUILD_TARGET='qreeves@icculus.org:/webspace/redeclipse.net/files'
SEMABUILD_APT='DEBIAN_FRONTEND=noninteractive apt-get'
SEMABUILD_SOURCE="http://redeclipse.net/files"
#SEMABUILD_MODULES=`cat "${SEMABUILD_PWD}/.gitmodules" | grep '\[submodule' | sed -e 's/^.submodule..//;s/..$//' | tr "\n" " "`
SEMABUILD_ALLMODS="base"
SEMABUILD_DEPLOY="false"

semabuild_setup() {
    echo "setting up ${BRANCH_NAME}..."
    rm -rfv "${SEMABUILD_DIR}"
    mkdir -pv "${SEMABUILD_DIR}" || return 1
    git submodule init || return 1
    git submodule update || return 1
    return 0
}

semabuild_archive() {
    echo "archiving ${BRANCH_NAME}..."
    # windows
    pushd "${SEMABUILD_DIR}/windows" || return 1
    zip -r "${SEMABUILD_DIR}/windows.zip" . || (popd; return 1)
    popd
    # linux
    pushd "${SEMABUILD_DIR}/linux" || return 1
    tar -zcvf "${SEMABUILD_DIR}/linux.tar.gz" . || (popd; return 1)
    popd
    # cleanup
    rm -rfv "${SEMABUILD_DIR}/windows" "${SEMABUILD_DIR}/linux" || return 1
    SEMABUILD_DEPLOY="true"
    return 0
}

semabuild_build() {
    echo "building ${BRANCH_NAME}..."
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

semabuild_process() {
    for i in "${SEMABUILD_ALLMODS}"; do
        if [ "${i}" = "base" ]; then
            SEMABUILD_MODDIR="${SEMABUILD_PWD}"
        else
            SEMABUILD_MODDIR="${SEMABUILD_PWD}/${i}"
        fi
        pushd "${SEMABUILD_MODDIR}" || return 1
        SEMABUILD_HASH=`git rev-parse HEAD` || (popd; return 1)
        SEMABUILD_LAST=`curl --fail --silent "${SEMABUILD_SOURCE}/${BRANCH_NAME}/${i}.txt"` || (popd; return 1)
        if [ -n "${SEMABUILD_HASH}" ] && [ -n "${SEMABUILD_LAST}" ] && [ "${SEMABUILD_HASH}" != "${SEMABUILD_LAST}" ]; then
            echo "module '${i}' updated, syncing: ${SEMABUILD_HASH} -> ${SEMABUILD_LAST}"
            echo "${SEMABUILD_HASH}" > "${SEMABUILD_DIR}/${i}.txt"
            SEMABUILD_DEPLOY="true"
            if [ "${i}" = "base" ]; then
                SEMABUILD_BINS=`curl --fail --silent "${SEMABUILD_SOURCE}/${BRANCH_NAME}/bins.txt"` || (popd; return 1)
                SEMABUILD_CHANGES=`git diff --name-only HEAD ${SEMABUILD_BINS} -- src` || (popd; return 1)
                if [ -n "${SEMABUILD_CHANGES}" ]; then
                    echo "source files modified:"
                    echo "${SEMABUILD_CHANGES}"
                    semabuild_build || (echo "build failed."; popd; return 1)
                    semabuild_archive || (echo "archive failed."; popd; return 1)
                    echo "archive 'bins' updated, syncing: ${SEMABUILD_HASH} -> ${SEMABUILD_BINS}"
                    echo "${SEMABUILD_HASH}" > "${SEMABUILD_DIR}/bins.txt"
                fi
            fi
        fi
        popd
    done
    return 0
}

semabuild_deploy() {
    echo "deploying ${BRANCH_NAME}..."
    echo "${SEMABUILD_ALLMODS}" > "${SEMABUILD_DIR}/modules.txt"
    pushd "${SEMABUILD_BUILD}" || return 1
    if [ -e "${BRANCH_NAME}" ]; then
        ${SEMABUILD_SCP} -r "${BRANCH_NAME}" "${SEMABUILD_TARGET}" || (popd; return 1)
    else
        echo "failed to send ${BRANCH_NAME} as the folder doesn't exist!"
        popd
        return 1
    fi
    popd
    return 0
}

semabuild_setup
if [ $? -ne 0 ]; then
    echo "failed to setup ${BRANCH_NAME}!"
    exit 1
fi
semabuild_process
if [ $? -ne 0 ]; then
    echo "failed to process ${BRANCH_NAME}!"
    exit 1
fi
if [ "${SEMABUILD_DEPLOY}" = "true" ]; then
    semabuild_deploy
    if [ $? -ne 0 ]; then
        echo "failed to deploy ${BRANCH_NAME}!"
        exit 1
    fi
else
    echo "nothing to deploy!"
fi
echo "done."
