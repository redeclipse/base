#! /bin/bash

SEMABUILD_PWD=`pwd`
SEMABUILD_BUILD="${HOME}/deploy"
SEMABUILD_STEAM="${HOME}/depot"
SEMABUILD_DIR="${SEMABUILD_BUILD}/${BRANCH_NAME}"
SEMABUILD_APT='DEBIAN_FRONTEND=noninteractive apt-get'
SEMABUILD_DEST="https://${GITHUB_TOKEN}:x-oauth-basic@github.com/redeclipse/deploy.git"
SEMABUILD_MODULES=`cat "${SEMABUILD_PWD}/.gitmodules" | grep '\[submodule "[^.]' | sed -e 's/^.submodule..//;s/..$//' | tr "\n" " " | sed -e 's/ $//'`
SEMABUILD_ALLMODS="base ${SEMABUILD_MODULES}"
SEMABUILD_DEPLOY="false"

semabuild_setup() {
    echo "setting up ${BRANCH_NAME}.."
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
    echo "archiving ${BRANCH_NAME}.."
    # ensure updater and scripts updated at least if base fails
    cp -fv "redeclipse.bat" "${SEMABUILD_DIR}/windows/redeclipse.bat" || return 1
    cp -fv "redeclipse_server.bat" "${SEMABUILD_DIR}/windows/redeclipse_server.bat" || return 1
    cp -fv "redeclipse.sh" "${SEMABUILD_DIR}/linux/redeclipse.sh" || return 1
    cp -fv "redeclipse_server.sh" "${SEMABUILD_DIR}/linux/redeclipse_server.sh" || return 1
    # create the archives
    pushd "${SEMABUILD_DIR}/windows" || return 1
    zip -r "${SEMABUILD_DIR}/windows.zip" . || return 1
    popd || return 1
    pushd "${SEMABUILD_DIR}/linux" || return 1
    tar -zcvf "${SEMABUILD_DIR}/linux.tar.gz" . || return 1
    popd || return 1
    rm -rfv "${SEMABUILD_DIR}/windows" "${SEMABUILD_DIR}/linux" || return 1
    SEMABUILD_DEPLOY="true"
    return 0
}

semabuild_test() {
    echo "testing ${BRANCH_NAME}.."
    sudo ${SEMABUILD_APT} update || return 1
    sudo ${SEMABUILD_APT} -fy install build-essential multiarch-support gcc-multilib g++-multilib zlib1g-dev libsdl2-dev libsndfile1-dev libalut-dev libopenal-dev libsdl2-image-dev libfreetype6-dev binutils-mingw-w64 g++-mingw-w64 || return 1
    sudo ${SEMABUILD_APT} clean || return 1
    make PLATFORM="crossmingw64" PLATFORM_BIN="amd64" PLATFORM_BUILD="${SEMAPHORE_BUILD_NUMBER}" PLATFORM_BRANCH="${BRANCH_NAME}" PLATFORM_REVISION="${REVISION}" WANT_DISCORD=1 WANT_STEAM=1 INSTDIR="${SEMABUILD_DIR}/windows/bin/amd64" CFLAGS="-m64 -O3 -fomit-frame-pointer -ffast-math" CXXFLAGS="-m64 -O3 -fomit-frame-pointer -ffast-math" LDFLAGS="-m64" -C src clean install || return 1
    make PLATFORM="linux64" PLATFORM_BIN="amd64" PLATFORM_BUILD="${SEMAPHORE_BUILD_NUMBER}" PLATFORM_BRANCH="${BRANCH_NAME}" PLATFORM_REVISION="${REVISION}" WANT_DISCORD=1 WANT_STEAM=1 INSTDIR="${SEMABUILD_DIR}/linux/bin/amd64" CFLAGS="-m64 -O3 -fomit-frame-pointer -ffast-math" CXXFLAGS="-m64 -O3 -fomit-frame-pointer -ffast-math" LDFLAGS="-m64" -C src clean install || return 1
    return 0
}

semabuild_build() {
    echo "building ${BRANCH_NAME}.."

    CACHE_DIR="${SEMAPHORE_CACHE_DIR}" \
        GAME_DIR="$(pwd)" \
        OUTPUT_DIR="${SEMABUILD_DIR}" \
        PLATFORM_BUILD="${SEMAPHORE_BUILD_NUMBER}" \
        PLATFORM_BRANCH="${BRANCH_NAME}" \
        PLATFORM_REVISION="${REVISION}" \
        src/buildindocker.sh || return 1

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
        if [ ! -e "${SEMABUILD_DIR}/${i}.txt" ]; then
            echo "0" > "${SEMABUILD_DIR}/${i}.txt"
        fi
        SEMABUILD_LAST=`cat "${SEMABUILD_DIR}/${i}.txt"`
        echo "module ${i} compare: ${SEMABUILD_LAST} -> ${SEMABUILD_HASH}"
        if [ "${i}" = "base" ] && [ "${SEMAPHORE_TRIGGER_SOURCE}" = "manual" ]; then
            SEMABUILD_LAST="0"
        fi
        if [ -n "${SEMABUILD_HASH}" ] && [ "${SEMABUILD_HASH}" != "${SEMABUILD_LAST}" ]; then
            echo "module ${i} updated, syncing.."
            echo "${SEMABUILD_HASH}" > "${SEMABUILD_DIR}/${i}.txt"
            if [ "${SEMABUILD_LAST}" = "0" ]; then
                pushd "${SEMABUILD_BUILD}" || return 1
                git add "${BRANCH_NAME}/${i}.txt" || return 1
                popd || return 1
            fi
            SEMABUILD_DEPLOY="true"
            if [ "${i}" = "base" ]; then
                echo "module ${i} checking for source modifications.."
                SEMABUILD_CHANGES=""
                SEMABUILD_BINS=`cat "${SEMABUILD_DIR}/bins.txt"` || return 1
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

semabuild_deploy() {
    echo "deploying ${BRANCH_NAME}.."
    echo "${SEMABUILD_ALLMODS}" > "${SEMABUILD_DIR}/mods.txt"
    pushd "${SEMABUILD_BUILD}" || return 1
    git status || return 1
    git commit -a -m "Build ${BRANCH_NAME}:${SEMAPHORE_BUILD_NUMBER} from ${REVISION}" || return 1
    git pull --rebase || return 1
    git push -u origin master || return 1
    popd || return 1
    return 0
}

semabuild_setup || exit 1
semabuild_process || exit 1
if [ "${SEMABUILD_DEPLOY}" = "true" ]; then
    semabuild_deploy || exit 1
fi
echo "done."
