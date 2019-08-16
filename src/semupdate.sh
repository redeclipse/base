#! /bin/bash

SEMUPDATE_PWD=`pwd`
SEMUPDATE_BUILD="${HOME}/deploy"
SEMUPDATE_DEPOT="${HOME}/depot"
SEMUPDATE_DIR="${SEMUPDATE_BUILD}/${BRANCH_NAME}"
SEMUPDATE_APT='DEBIAN_FRONTEND=noninteractive apt-get'
SEMUPDATE_DEST="https://${GITHUB_TOKEN}:x-oauth-basic@github.com/red-eclipse/deploy.git"
SEMUPDATE_APPIMAGE="https://github.com/red-eclipse/appimage-builder.git"
SEMUPDATE_APPIMAGE_GH_DEST="red-eclipse/deploy"
SEMUPDATE_MODULES=`cat "${SEMUPDATE_PWD}/.gitmodules" | grep '\[submodule "[^.]' | sed -e 's/^.submodule..//;s/..$//' | tr "\n" " " | sed -e 's/ $//'`
SEMUPDATE_ALLMODS="base ${SEMUPDATE_MODULES}"

semupdate_setup() {
    echo "setting up ${BRANCH_NAME}..."
    git config --global user.email "noreply@redeclipse.net" || return 1
    git config --global user.name "Red Eclipse" || return 1
    git config --global credential.helper store || return 1
    echo "https://${GITHUB_TOKEN}:x-oauth-basic@github.com" > "${HOME}/.git-credentials"
    rm -rf "${SEMUPDATE_BUILD}" || return 1
    rm -rf "${SEMUPDATE_PWD}/data" || return 1
    pushd "${HOME}" || return 1
    git clone --depth 1 "${SEMUPDATE_DEST}" || return 1
    popd || return 1
    mkdir -pv "${SEMUPDATE_DIR}" || return 1
    for i in ${SEMUPDATE_ALLMODS}; do
        if [ "${i}" != "base" ]; then
            git submodule update --init --depth 1 "data/${i}" || return 1
        fi
    done
    return 0
}

semupdate_wait() {
    pushd "${SEMUPDATE_BUILD}" || return 1
    SEMUPDATE_CURPRC=1
    echo "Waiting for macOS build to complete..." # Will wait up to 20 minutes before failing
    for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40; do
        SEMUPDATE_CURBIN=`cat "${SEMUPDATE_DIR}/bins.txt"`
        SEMUPDATE_CURMAC=`cat "${SEMUPDATE_DIR}/macos.txt"`
        echo ""
        echo "Binaries: ${SEMUPDATE_CURBIN} macOS: ${SEMUPDATE_CURMAC}"
        if [ "${SEMUPDATE_CURBIN}" != "${SEMUPDATE_CURMAC}" ]; then
            echo "[${i}] Sleep for 30 seconds..."
            sleep 30s || return 1
            git pull || return 1
        else
            SEMUPDATE_CURPRC=0
            break
        fi
    done
    popd || return 1
    return ${SEMUPDATE_CURPRC}
}

semupdate_appimage() {
    git clone --depth 1 "${SEMUPDATE_APPIMAGE}" appimage || return 1
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
    export BUILD="${SEMUPDATE_PWD}"
    bash build-appimages.sh || return 1
    export GITHUB_TOKEN="${GITHUB_TOKEN}"
    export REPO_SLUG="${SEMUPDATE_APPIMAGE_GH_DEST}"
    export COMMIT=$(git rev-parse ${REVISION})
    bash github-release.sh || return 1
    popd || return 1
    # Clear the appimage building directory to save space.
    rm -rf appimage
    return 0
}

semupdate_steam() {
    echo "building Steam depot..."
    cp -Rv "${SEMUPDATE_PWD}/src/install/steam" "${SEMUPDATE_DEPOT}" || return 1
    mkdir -pv "${SEMAPHORE_CACHE_DIR}/Steam-dot" || return 1
    ln -sv "${SEMAPHORE_CACHE_DIR}/Steam-dot" "${HOME}/.steam" || return 1
    mkdir -pv "${SEMAPHORE_CACHE_DIR}/Steam" || return 1
    ln -sv "${SEMAPHORE_CACHE_DIR}/Steam" "${HOME}/Steam" || return 1
    for i in output package public; do
        mkdir -pv "${SEMAPHORE_CACHE_DIR}/Steam-${i}" || return 1
        ln -sv "${SEMAPHORE_CACHE_DIR}/Steam-${i}" "${SEMUPDATE_DEPOT}/${i}" || return 1
    done
    for i in ${SEMUPDATE_ALLMODS}; do
        if [ "${i}" = "base" ]; then
            SEMUPDATE_MODDIR="${SEMUPDATE_DEPOT}/content"
            SEMUPDATE_GITDIR="${SEMUPDATE_PWD}"
            SEMUPDATE_ARCHBR="${BRANCH_NAME}"
        else
            SEMUPDATE_MODDIR="${SEMUPDATE_DEPOT}/content/data/${i}"
            SEMUPDATE_GITDIR="${SEMUPDATE_PWD}/data/${i}"
            git submodule update --init --depth 1 "data/${i}" || return 1
            pushd "${SEMUPDATE_GITDIR}" || return 1
            SEMUPDATE_ARCHBR=`git rev-parse HEAD`
            popd || return 1
        fi
        mkdir -pv "${SEMUPDATE_MODDIR}" || return 1
        pushd "${SEMUPDATE_GITDIR}" || return 1
        (git archive ${SEMUPDATE_ARCHBR} | tar -x -C "${SEMUPDATE_MODDIR}") || return 1
        if [ "${i}" = "base" ]; then
            # Steam build on Windows HATES SYMLINKS
            rm -rfv "${SEMUPDATE_MODDIR}/bin/redeclipse.app" "${SEMUPDATE_MODDIR}/readme.md" "${SEMUPDATE_MODDIR}/doc/commands.txt" || return 1
            cp -RLfv "${SEMUPDATE_GITDIR}/config/usage.cfg" "${SEMUPDATE_MODDIR}/doc/commands.txt" || return 1
            cp -RLfv "${SEMUPDATE_GITDIR}/bin/redeclipse.app" "${SEMUPDATE_MODDIR}/bin/redeclipse.app" || return 1
        fi
        popd || return 1
    done
    echo "steam" > "${SEMUPDATE_DEPOT}/content/branch.txt" || return 1
    unzip -o "${SEMUPDATE_DIR}/windows.zip" -d "${SEMUPDATE_DEPOT}/content" || return 1
    tar --gzip --extract --verbose --overwrite --file="${SEMUPDATE_DIR}/linux.tar.gz" --directory="${SEMUPDATE_DEPOT}/content"
    tar --gzip --extract --verbose --overwrite --file="${SEMUPDATE_DIR}/macos.tar.gz" --directory="${SEMUPDATE_DEPOT}/content"
    pushd "${SEMUPDATE_DEPOT}" || return 1
    curl -sqL "https://steamcdn-a.akamaihd.net/client/installer/steamcmd_linux.tar.gz" | tar zxvf -
    chmod --verbose +x linux32/steamcmd || return 1
    export LD_LIBRARY_PATH="${SEMUPDATE_DEPOT}/linux32:${SEMUPDATE_DEPOT}/linux64:${LD_LIBRARY_PATH}"
    STEAM_ARGS="+login redeclipsenet ${STEAM_TOKEN} +run_app_build_http app_build_967460.vdf +quit"
    if [ "${STEAM_GUARD}" != "0" ]; then
        STEAM_ARGS="+set_steam_guard_code ${STEAM_GUARD} ${STEAM_ARGS}"
    fi
    ./linux32/steamcmd ${STEAM_ARGS}
    if [ $? -eq 42 ]; then
        ./linux32/steamcmd ${STEAM_ARGS}
    fi
    popd || return 1
    return 0
}

if [ "${BRANCH_NAME}" = master ]; then
    semupdate_setup || exit 1
    sudo ${SEMUPDATE_APT} update || exit 1
    sudo ${SEMUPDATE_APT} -fy install build-essential multiarch-support gcc-multilib g++-multilib zlib1g-dev libsdl2-dev libsdl2-mixer-dev libsdl2-image-dev jq zsync || exit 1
    pushd "${HOME}" || exit 1
    semupdate_appimage || exit 1
    popd || exit 1
    semupdate_wait || exit 1
    semupdate_steam || exit 1
fi
echo "done."
