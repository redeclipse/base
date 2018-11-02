#! /bin/bash

SEMABUILD_PWD=`pwd`
SEMABUILD_BUILD="${HOME}/deploy"
SEMABUILD_STEAM="${HOME}/steam"
SEMABUILD_DIR="${SEMABUILD_BUILD}/${BRANCH_NAME}"
SEMABUILD_APT='DEBIAN_FRONTEND=noninteractive apt-get'
SEMABUILD_DEST="https://${GITHUB_TOKEN}:x-oauth-basic@github.com/red-eclipse/deploy.git"
SEMABUILD_SOURCE="https://raw.githubusercontent.com/red-eclipse/deploy/master"
SEMABUILD_APPIMAGE="https://github.com/red-eclipse/appimage-builder.git"
SEMABUILD_APPIMAGE_GH_DEST="red-eclipse/deploy"
SEMABUILD_MODULES=`cat "${SEMABUILD_PWD}/.gitmodules" | grep '\[submodule "[^.]' | sed -e 's/^.submodule..//;s/..$//' | tr "\n" " " | sed -e 's/ $//'`
SEMABUILD_ALLMODS="base ${SEMABUILD_MODULES}"
SEMABUILD_DEPLOY="false"

semabuild_setup() {
    echo "setting up ${BRANCH_NAME}..."
    git config --global user.email "noreply@redeclipse.net" || return 1
    git config --global user.name "Red Eclipse" || return 1
    git config --global credential.helper store || return 1
    echo "https://${GITHUB_TOKEN}:x-oauth-basic@github.com" > "${HOME}/.git-credentials"
    rm -rf "${SEMABUILD_BUILD}" || return 1
    rm -rf "${SEMABUILD_PWD}/data" || return 1
    pushd "${HOME}" || return 1
    git clone --depth 1 "${SEMABUILD_DEST}" || return 1
    popd || return 1
    mkdir -pv "${SEMABUILD_DIR}" || return 1
    return 0
}

semabuild_archive() {
    echo "archiving ${BRANCH_NAME}..."
    pushd "${SEMABUILD_DIR}/windows" || return 1
    zip -r "${SEMABUILD_DIR}/windows.zip" . || return 1
    popd || return 1
    pushd "${SEMABUILD_DIR}/linux" || return 1
    tar -zcvf "${SEMABUILD_DIR}/linux.tar.gz" . || return 1
    popd || return 1
    rm -rf "${SEMABUILD_DIR}/windows" "${SEMABUILD_DIR}/linux" || return 1
    SEMABUILD_DEPLOY="true"
    return 0
}

semabuild_test() {
    echo "testing ${BRANCH_NAME}..."
    sudo ${SEMABUILD_APT} update || return 1
    sudo ${SEMABUILD_APT} -fy install build-essential multiarch-support gcc-multilib g++-multilib zlib1g-dev libsdl2-dev libsdl2-mixer-dev libsdl2-image-dev binutils-mingw-w64 g++-mingw-w64 || return 1
    make PLATFORM=crossmingw64 PLATFORM_BIN=amd64 PLATFORM_BUILD=${SEMAPHORE_BUILD_NUMBER} PLATFORM_BRANCH=${BRANCH_NAME} PLATFORM_REVISION=${REVISION} INSTDIR=${SEMABUILD_DIR}/windows/bin/amd64 CFLAGS=-m64 CXXFLAGS=-m64 LDFLAGS=-m64 -C src clean install || return 1
    make PLATFORM=linux64 PLATFORM_BIN=amd64 PLATFORM_BUILD=${SEMAPHORE_BUILD_NUMBER} PLATFORM_BRANCH=${BRANCH_NAME} PLATFORM_REVISION=${REVISION} INSTDIR=${SEMABUILD_DIR}/linux/bin/amd64 CFLAGS=-m64 CXXFLAGS=-m64 LDFLAGS=-m64 -C src clean install || return 1
    return 0
}

semabuild_build() {
    echo "building ${BRANCH_NAME}..."
    sudo dpkg --add-architecture i386 || return 1
    sudo ${SEMABUILD_APT} update || return 1
    sudo ${SEMABUILD_APT} -fy install build-essential multiarch-support gcc-multilib g++-multilib zlib1g-dev libsdl2-dev libsdl2-mixer-dev libsdl2-image-dev binutils-mingw-w64 g++-mingw-w64 || return 1
    make PLATFORM=crossmingw64 PLATFORM_BIN=amd64 PLATFORM_BUILD=${SEMAPHORE_BUILD_NUMBER} PLATFORM_BRANCH=${BRANCH_NAME} PLATFORM_REVISION=${REVISION} INSTDIR=${SEMABUILD_DIR}/windows/bin/amd64 CFLAGS=-m64 CXXFLAGS=-m64 LDFLAGS=-m64 -C src clean install || return 1
    make PLATFORM=crossmingw32 PLATFORM_BIN=x86 PLATFORM_BUILD=${SEMAPHORE_BUILD_NUMBER} PLATFORM_BRANCH=${BRANCH_NAME} PLATFORM_REVISION=${REVISION} INSTDIR=${SEMABUILD_DIR}/windows/bin/x86 CFLAGS=-m32 CXXFLAGS=-m32 LDFLAGS=-m32 -C src clean install || return 1
    make PLATFORM=linux64 PLATFORM_BIN=amd64 PLATFORM_BUILD=${SEMAPHORE_BUILD_NUMBER} PLATFORM_BRANCH=${BRANCH_NAME} PLATFORM_REVISION=${REVISION} INSTDIR=${SEMABUILD_DIR}/linux/bin/amd64 CFLAGS=-m64 CXXFLAGS=-m64 LDFLAGS=-m64 -C src clean install || return 1
    sudo ${SEMABUILD_APT} purge -fy sbt || return 1
    sudo ${SEMABUILD_APT} -o Dpkg::Options::="--force-overwrite" -fy --no-install-recommends install pkg-config:i386 gcc:i386 g++:i386 cpp:i386 g++-4.8:i386 gcc-4.8:i386 cpp-4.8:i386 binutils:i386 zlib1g-dev:i386 libsdl2-dev:i386 libsdl2-mixer-dev:i386 libsdl2-image-dev:i386 libpng12-dev:i386 || return 1
    PKG_CONFIG_PATH=/usr/lib/i386-linux-gnu/pkgconfig make PLATFORM=linux32 PLATFORM_BIN=x86 PLATFORM_BUILD=${SEMAPHORE_BUILD_NUMBER} PLATFORM_BRANCH=${BRANCH_NAME} PLATFORM_REVISION=${REVISION} INSTDIR=${SEMABUILD_DIR}/linux/bin/x86 CFLAGS=-m32 CXXFLAGS=-m32 LDFLAGS=-m32 -C src clean install || return 1
    sudo ${SEMABUILD_APT} purge -fy ".*:i386" || return 1
    sudo dpkg --remove-architecture i386 || return 1
    sudo ${SEMABUILD_APT} update || return 1
    return 0
}

semabuild_integrate() {
    for i in ${SEMABUILD_ALLMODS}; do
        if [ "${i}" = "base" ]; then
            SEMABUILD_MODDIR="${SEMABUILD_PWD}"
        else
            SEMABUILD_MODDIR="${SEMABUILD_PWD}/data/${i}"
            echo "module ${i} updating.."
            git submodule update --init "data/${i}" || return 1
        fi
        pushd "${SEMABUILD_MODDIR}" || return 1
        echo "module ${i} processing.."
        SEMABUILD_HASH=`git rev-parse HEAD` || return 1
        SEMABUILD_LAST=`curl --connect-timeout 30 -L -k -f "${SEMABUILD_SOURCE}/${BRANCH_NAME}/${i}.txt"`
        echo "module ${i} compare: ${SEMABUILD_LAST} -> ${SEMABUILD_HASH}"
        if [ "${i}" = "base" ] && [ "${SEMAPHORE_TRIGGER_SOURCE}" = "manual" ]; then
            SEMABUILD_LAST="0"
        fi
        if [ -n "${SEMABUILD_HASH}" ] && [ "${SEMABUILD_HASH}" != "${SEMABUILD_LAST}" ]; then
            echo "module ${i} updated, syncing.."
            echo "${SEMABUILD_HASH}" > "${SEMABUILD_DIR}/${i}.txt"
            SEMABUILD_DEPLOY="true"
            if [ "${i}" = "base" ]; then
                echo "module ${i} checking for source modifications.."
                SEMABUILD_CHANGES=""
                SEMABUILD_BINS=`curl --connect-timeout 30 -L -k -f "${SEMABUILD_SOURCE}/${BRANCH_NAME}/bins.txt"` || return 1
                if [ "${SEMAPHORE_TRIGGER_SOURCE}" = "manual" ]; then
                    SEMABUILD_CHANGES="<manual rebuild forced>"
                else
                    SEMABUILD_CHANGES=`git diff --name-only HEAD ${SEMABUILD_BINS} -- src | egrep '\.h$|\.c$|\.cpp$|Makefile$'`
                fi
                if [ -n "${SEMABUILD_CHANGES}" ]; then
                    echo "module ${i} has modified source files:"
                    echo "${SEMABUILD_CHANGES}"
                    semabuild_build || return 1
                    semabuild_archive || return 1
                    echo "binary archive updated, syncing: ${SEMABUILD_HASH} -> ${SEMABUILD_BINS}"
                    echo "${SEMABUILD_HASH}" > "${SEMABUILD_DIR}/bins.txt"
                    echo "${SEMAPHORE_BUILD_NUMBER}" > "${SEMABUILD_DIR}/build.txt"
                fi
            fi
        fi
        popd || return 1
    done
    return 0
}

semabuild_process() {
    if [ "${BRANCH_NAME}" = master ] || [ "${BRANCH_NAME}" = stable ]; then
        semabuild_integrate || return 1
    else
        semabuild_test || return 1
    fi
    return 0
}

semabuild_appimage() {
    git clone --depth 1 "${SEMABUILD_APPIMAGE}" appimage || return 1
    pushd appimage || return 1
    export BRANCH="${BRANCH_NAME}"
    export ARCH=x86_64
    export COMMIT=${REVISION}
    export BUILD_SERVER=1
    export BUILD_CLIENT=1
    export PLATFORM_BUILD=${SEMAPHORE_BUILD_NUMBER}
    export PLATFORM_BRANCH="${BRANCH_NAME}"
    export PLATFORM_REVISION="${REVISION}"
    export NO_UPDATE=true
    export BUILD="${SEMABUILD_PWD}"
    bash build-appimages.sh || return 1
    export GITHUB_TOKEN="${GITHUB_TOKEN}"
    export REPO_SLUG="${SEMABUILD_APPIMAGE_GH_DEST}"
    export COMMIT=$(git rev-parse ${REVISION})
    bash github-release.sh || return 1
    popd || return 1
    return 0
}

semabuild_deploy() {
    echo "deploying ${BRANCH_NAME}..."
    echo "${SEMABUILD_ALLMODS}" > "${SEMABUILD_DIR}/mods.txt"
    pushd "${SEMABUILD_BUILD}" || return 1
    git commit -a -m "Build ${BRANCH_NAME}:${SEMAPHORE_BUILD_NUMBER}" || return 1
    git pull --rebase || return 1
    git push -u origin master || return 1
    popd || return 1
    return 0
}

semabuild_steam() {
    echo "building Steam depot..."
    sudo ${SEMABUILD_APT} install multiarch-support libc6-i386 || return 1
    cp -Rv "${SEMABUILD_PWD}/src/install/steam" "${SEMABUILD_STEAM}" || return 1
    mkdir -p "${SEMABUILD_STEAM}/content" || return 1
    mkdir -p "${SEMABUILD_STEAM}/output" || return 1
    for i in ${SEMABUILD_ALLMODS}; do
        if [ "${i}" = "base" ]; then
            SEMABUILD_MODDIR="${SEMABUILD_STEAM}/content"
            SEMABUILD_GITDIR="${SEMABUILD_PWD}"
            SEMABUILD_ARCHBR="${BRANCH_NAME}"
        else
            SEMABUILD_MODDIR="${SEMABUILD_STEAM}/content/data/${i}"
            SEMABUILD_GITDIR="${SEMABUILD_PWD}/data/${i}"
            git submodule update --init --depth 1 "data/${i}" || return 1
            pushd "${SEMABUILD_GITDIR}" || return 1
            SEMABUILD_ARCHBR=`git rev-parse HEAD`
            popd || return 1
        fi
        mkdir -pv "${SEMABUILD_MODDIR}" || return 1
        pushd "${SEMABUILD_GITDIR}" || return 1
        (git archive ${SEMABUILD_ARCHBR} | tar -x -C "${SEMABUILD_MODDIR}") || return 1
        popd || return 1
    done
    echo "steam" > "${SEMABUILD_STEAM}/content/branch.txt" || return 1
	unzip -o "${SEMABUILD_DIR}/windows.zip" -d "${SEMABUILD_STEAM}/content" || return 1
    tar --gzip --extract --verbose --overwrite --file="${SEMABUILD_DIR}/linux.tar.gz" --directory="${SEMABUILD_STEAM}/content"
    tar --gzip --extract --verbose --overwrite --file="${SEMABUILD_DIR}/macos.tar.gz" --directory="${SEMABUILD_STEAM}/content"
    pushd "${SEMABUILD_STEAM}" || return 1
    chmod --verbose +x builder_linux/linux32/steamcmd || return 1
    export LD_LIBRARY_PATH="${SEMABUILD_STEAM}/builder_linux/linux32:${LD_LIBRARY_PATH}"
    ./builder_linux/linux32/steamcmd +login redeclipsebuild ${STEAM_TOKEN} +run_app_build_http ../app_build_967460.vdf +quit
    if [ $? -eq 42 ]; then
        ./builder_linux/linux32/steamcmd +login redeclipsebuild ${STEAM_TOKEN} +run_app_build_http ../app_build_967460.vdf +quit
    fi
    popd || return 1
    return 0
}

semabuild_setup || exit 1
#semabuild_process || exit 1
#if [ "${SEMABUILD_DEPLOY}" = "true" ]; then
#    semabuild_deploy || exit 1
    if [ "${BRANCH_NAME}" = master ]; then
        semabuild_steam || exit 1
    #    echo "building ${BRANCH_NAME} appimages..."
    #    sudo ${SEMABUILD_APT} update || return 1
    #    sudo ${SEMABUILD_APT} -fy install build-essential multiarch-support gcc-multilib g++-multilib zlib1g-dev libsdl2-dev libsdl2-mixer-dev libsdl2-image-dev jq zsync || exit 1
    #    pushd "${HOME}" || return 1
    #    semabuild_appimage || exit 1
    #    popd || return 1
    fi
#fi
echo "done."
