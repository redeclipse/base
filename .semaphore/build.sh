#! /bin/bash

SEMABUILD_GIT="${HOME}/${SEMAPHORE_GIT_DIR}"
SEMABUILD_BUILD="${HOME}/deploy"
SEMABUILD_DIR="${SEMABUILD_BUILD}/${SEMAPHORE_GIT_BRANCH}"
SEMABUILD_DEST="https://${GITHUB_TOKEN}:x-oauth-basic@github.com/redeclipse/deploy.git"
SEMABUILD_APT='DEBIAN_FRONTEND=noninteractive apt-get'

semabuild_setup() {
    echo "setting up ${SEMAPHORE_GIT_BRANCH}.."

    git config --global user.email "noreply@redeclipse.net" || return 1
    git config --global user.name "Red Eclipse" || return 1
    git config --global credential.helper store || return 1
    echo "https://${GITHUB_TOKEN}:x-oauth-basic@github.com" > "${HOME}/.git-credentials"

    rm -rfv "${SEMABUILD_BUILD}" || return 1
    rm -rfv "${SEMABUILD_PWD}/data" || return 1

    pushd "${HOME}" || return 1
    git clone --depth 1 "${SEMABUILD_DEST}" "${SEMABUILD_BUILD}" || return 1
    popd || return 1

    mkdir -pv "${SEMABUILD_DIR}" || return 1

    return 0
}

semabuild_archive() {
    echo "archiving ${SEMAPHORE_GIT_BRANCH}.."

    pushd "${SEMABUILD_DIR}/windows" || return 1
    zip -r "${SEMABUILD_DIR}/windows.zip" . || return 1
    popd || return 1

    pushd "${SEMABUILD_DIR}/linux" || return 1
    tar -zcvf "${SEMABUILD_DIR}/linux.tar.gz" . || return 1
    popd || return 1

    rm -rfv "${SEMABUILD_DIR}/windows" "${SEMABUILD_DIR}/linux" || return 1

    pushd "${SEMABUILD_DIR}" || return 1
    artifact push workflow "windows.zip" || return 1
    artifact push workflow "linux.tar.gz" || return 1
    popd || return 1

    pushd "${SEMABUILD_BUILD}" || return 1
    git status || return 1
    git commit -a -m "Build ${SEMAPHORE_GIT_BRANCH}:${SEMAPHORE_WORKFLOW_NUMBER} from ${SEMAPHORE_GIT_SHA}" || return 1
    git pull --rebase || return 1
    git push -u origin master || return 1
    popd || return 1

    return 0
}

semabuild_test() {
    echo "testing ${SEMAPHORE_GIT_BRANCH}.."

    sudo ${SEMABUILD_APT} update || return 1
    sudo ${SEMABUILD_APT} -fy install build-essential multiarch-support gcc-multilib g++-multilib zlib1g-dev libsdl2-dev libsndfile1-dev libalut-dev libopenal-dev libsdl2-image-dev libfreetype6-dev binutils-mingw-w64 g++-mingw-w64 || return 1
    sudo ${SEMABUILD_APT} clean || return 1
    make PLATFORM="crossmingw64" PLATFORM_BIN="amd64" PLATFORM_BUILD="${SEMAPHORE_WORKFLOW_NUMBER}" PLATFORM_BRANCH="${SEMAPHORE_GIT_BRANCH}" PLATFORM_REVISION="${SEMAPHORE_GIT_SHA}" WANT_DISCORD=1 WANT_STEAM=1 INSTDIR="${SEMABUILD_DIR}/windows/bin/amd64" CFLAGS="-m64 -O3 -fomit-frame-pointer -ffast-math" CXXFLAGS="-m64 -O3 -fomit-frame-pointer -ffast-math" LDFLAGS="-m64" -C src clean install || return 1
    make PLATFORM="linux64" PLATFORM_BIN="amd64" PLATFORM_BUILD="${SEMAPHORE_WORKFLOW_NUMBER}" PLATFORM_BRANCH="${SEMAPHORE_GIT_BRANCH}" PLATFORM_REVISION="${SEMAPHORE_GIT_SHA}" WANT_DISCORD=1 WANT_STEAM=1 INSTDIR="${SEMABUILD_DIR}/linux/bin/amd64" CFLAGS="-m64 -O3 -fomit-frame-pointer -ffast-math" CXXFLAGS="-m64 -O3 -fomit-frame-pointer -ffast-math" LDFLAGS="-m64" -C src clean install || return 1

    return 0
}

semabuild_build() {
    echo "building ${SEMAPHORE_GIT_BRANCH}.."

    cache restore .buildindocker || mkdir -pv "${HOME}/.buildindocker" || return 1

    # remove now irrelevant cache
    rm -rfv "${HOME}/.buildindocker/sys_archives"

    src/buildindocker.sh \
        -c "${HOME}/.buildindocker" \
        -g "$(pwd)" \
        -o "${SEMABUILD_DIR}" \
        -n "${SEMAPHORE_WORKFLOW_NUMBER}" \
        -r "${SEMAPHORE_GIT_SHA}" \
        -b "${SEMAPHORE_GIT_BRANCH}" || return 1

    sudo chmod -R a+rw "${SEMABUILD_DIR}" || return 1
    cache store .buildindocker "${HOME}/.buildindocker" || return 1

    return 0
}

semabuild_integrate() {
    semabuild_build || return 1
    semabuild_archive || return 1
    return 0
}

semabuild_process() {
    if [ "${SEMAPHORE_GIT_BRANCH}" = master ] || [ "${SEMAPHORE_GIT_BRANCH}" = stable ]; then
        semabuild_integrate || return 1
    else
        semabuild_test || return 1
    fi
    return 0
}

semabuild_setup || exit 1
semabuild_process || exit 1

echo "done."
