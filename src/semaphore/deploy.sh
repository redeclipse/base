#! /bin/bash

SEMUPDATE_GIT="${HOME}/${SEMAPHORE_GIT_DIR}"
SEMUPDATE_DIR="${HOME}/deploy"
SEMUPDATE_DEPOT="${HOME}/depot"

SEMUPDATE_APT='DEBIAN_FRONTEND=noninteractive apt-get'

SEMUPDATE_APPIMAGE="https://github.com/redeclipse/appimage-builder.git"
SEMUPDATE_APPIMAGE_GH_DEST="redeclipse/deploy"

SEMUPDATE_MODULES=`cat "${SEMUPDATE_GIT}/.gitmodules" | grep '\[submodule "[^.]' | sed -e 's/^.submodule..//;s/..$//' | tr "\n" " " | sed -e 's/ $//'`
SEMUPDATE_ALLMODS="base ${SEMUPDATE_MODULES}"

SEMUPDATE_VERSION_MAJOR=`sed -n 's/.define VERSION_MAJOR \([0-9]*\)/\1/p' src/engine/version.h`
SEMUPDATE_VERSION_MINOR=`sed -n 's/.define VERSION_MINOR \([0-9]*\)/\1/p' src/engine/version.h`
SEMUPDATE_VERSION_PATCH=`sed -n 's/.define VERSION_PATCH \([0-9]*\)/\1/p' src/engine/version.h`
SEMUPDATE_VERSION="${SEMUPDATE_VERSION_MAJOR}.${SEMUPDATE_VERSION_MINOR}.${SEMUPDATE_VERSION_PATCH}"
SEMUPDATE_STEAM_APPID=`sed -n 's/.define VERSION_STEAM_APPID \([0-9]*\)/\1/p' src/engine/version.h`
SEMUPDATE_STEAM_DEPOT=`sed -n 's/.define VERSION_STEAM_DEPOT \([0-9]*\)/\1/p' src/engine/version.h`
SEMUPDATE_DESCRIPTION="${SEMAPHORE_GIT_BRANCH}:${SEMAPHORE_WORKFLOW_NUMBER} from ${SEMAPHORE_GIT_SHA} for v${SEMUPDATE_VERSION}"

SEMUPDATE_BRANCH="${SEMAPHORE_GIT_BRANCH}"
if [ "${SEMUPDATE_BRANCH}" = "master" ]; then SEMUPDATE_BRANCH="devel"; fi

semupdate_setup() {
    echo "########## SETTING UP ${SEMAPHORE_GIT_BRANCH} ##########"

    rm -rfv "${SEMUPDATE_DIR}" || return 1
    rm -rfv "${SEMUPDATE_GIT}/data" || return 1
    mkdir -pv "${SEMUPDATE_DIR}" || return 1

    echo "Getting artifacts..."
    pushd "${SEMUPDATE_DIR}" || return 1
    artifact pull workflow "windows.zip" || return 1
    artifact pull workflow "linux.tar.gz" || return 1
    popd || return 1

    for i in ${SEMUPDATE_ALLMODS}; do
        if [ "${i}" != "base" ]; then
            git submodule update --init --depth 2 "data/${i}" || return 1
        fi
    done

    echo "--------------------------------------------------------------------------------"
    return 0
}

semupdate_appimage() {
    echo "########## BUILDING APPIMAGE ##########"
    sudo ${SEMUPDATE_APT} update || return 1
    sudo ${SEMUPDATE_APT} -fy install build-essential multiarch-support gcc-multilib g++-multilib zlib1g-dev libsdl2-dev libsndfile1-dev libalut-dev libopenal-dev libsdl2-image-dev jq zsync || return 1
    sudo ${SEMUPDATE_APT} clean || return 1
    echo "--------------------------------------------------------------------------------"

    echo "########## CLONING REPOSITORY ##########"
    pushd "${HOME}" || return 1
    git clone --depth 1 "${SEMUPDATE_APPIMAGE}" appimage || return 1
    pushd appimage || return 1
    echo "--------------------------------------------------------------------------------"

    echo "########## BUILDING APPIMAGE ##########"
    export BRANCH="${SEMAPHORE_GIT_BRANCH}"
    export ARCH=x86_64
    export COMMIT=${SEMAPHORE_GIT_SHA}
    export BUILD_SERVER=1
    export BUILD_CLIENT=1
    export PLATFORM_BUILD=${SEMAPHORE_WORKFLOW_NUMBER}
    export PLATFORM_BRANCH="${SEMAPHORE_GIT_BRANCH}"
    export PLATFORM_REVISION="${SEMAPHORE_GIT_SHA}"
    export NO_UPDATE=true
    export BUILD="${SEMUPDATE_GIT}"
    bash build-appimages.sh || return 1
    echo "--------------------------------------------------------------------------------"

    echo "########## DEPLOYING GITHUB RELEASE ##########"
    export GITHUB_TOKEN="${GITHUB_TOKEN}"
    export REPO_SLUG="${SEMUPDATE_APPIMAGE_GH_DEST}"
    export COMMIT=$(git rev-parse ${SEMAPHORE_GIT_SHA})
    bash github-release.sh || return 1
    echo "--------------------------------------------------------------------------------"

    popd || return 1
    # Clear the appimage building directory to save space. (not needed with separated deploy servers)
    # rm -rf appimage
    popd || return 1
    return 0
}

semupdate_steamdbg() {
    echo "########## HOME DIRECTORY LISTING ##########"
    date || return 1
    find "${HOME}" -maxdepth 1 -printf "%c | %M | %u:%g | %Y | %p | %l\n" || return 1
    echo "--------------------------------------------------------------------------------"

    echo "########## FREE DISK SPACE CHECK ##########"
    df -h || return 1
    echo "--------------------------------------------------------------------------------"

    if [ $1 != 0 ]; then
        echo "########## PRINTING LOGS ##########"
        cat "${HOME}/Steam/logs/stderr.txt"
        echo "--------------------------------------------------------------------------------"
    fi
    return 0
}

semupdate_steamguard() {
    # Steam Guard code from the authenticator's shared secret at time $1
    STEAM_GUARDTIME="$1" python3 -c '
import base64, hashlib, hmac, os, struct
key = base64.b64decode(os.environ["STEAM_SHARED_SECRET"])
mac = hmac.new(key, struct.pack(">Q", int(os.environ["STEAM_GUARDTIME"]) // 30), hashlib.sha1).digest()
off = mac[19] & 0x0f
num = struct.unpack(">I", mac[off:off + 4])[0] & 0x7fffffff
code = ""
for i in range(5):
    code += "23456789BCDFGHJKMNPQRTVWXY"[num % 26]
    num //= 26
print(code)
'
}

semupdate_steamcmd() {
    STEAM_GUARDARGS=""
    if [ -n "${STEAM_SHARED_SECRET}" ]; then
        # Steam refuses a code that has already been used, so wait for the next window on retries
        while [ $(( $(date +%s) / 30 )) -le ${STEAM_GUARDWIN} ]; do sleep 1; done
        STEAM_GUARDTIME=`date +%s`
        STEAM_GUARDWIN=$(( STEAM_GUARDTIME / 30 ))
        STEAM_GUARDCODE=`semupdate_steamguard ${STEAM_GUARDTIME}` || return 1
        STEAM_GUARDARGS="+set_steam_guard_code ${STEAM_GUARDCODE}"
    fi
    ./linux32/steamcmd ${STEAM_GUARDARGS} +login redeclipsenet ${STEAM_TOKEN} +run_app_build_http app_build.vdf +quit
}

semupdate_steam() {
    echo "########## BUILDING STEAM DEPOT ##########"

    sudo ${SEMUPDATE_APT} update || exit 1
    sudo ${SEMUPDATE_APT} -fy install libc6-i386 || exit 1
    #sudo ${SEMUPDATE_APT} clean || exit 1

    mkdir -pv "${SEMUPDATE_DEPOT}" || return 1
    echo "########## RESTORING CACHE ##########"
    cache restore .steam || mkdir -pv "${HOME}/.steam" || return 1
    cache restore Steam || mkdir -pv "${HOME}/Steam" || return 1
    for i in output package public; do
        cache restore "${i}" || mkdir -pv "${SEMUPDATE_DEPOT}/${i}" || return 1
    done
    echo "--------------------------------------------------------------------------------"

    pushd "${SEMUPDATE_GIT}/src/install/steam" || return 1
    for i in *; do
        if [ ! -d "${i}" ] && [ -e "${i}" ]; then
            sed -e "s/~REPAPPID~/${SEMUPDATE_STEAM_APPID}/g;s/~REPDESC~/${SEMUPDATE_DESCRIPTION}/g;s/~REPBRANCH~/${SEMUPDATE_BRANCH}/g;s/~REPDEPOT~/${SEMUPDATE_STEAM_DEPOT}/g" "${i}" > "${SEMUPDATE_DEPOT}/${i}" || return 1
        fi
    done
    popd || return 1
    echo "--------------------------------------------------------------------------------"

    echo "########## ARCHIVING REPOSITORIES ##########"
    for i in ${SEMUPDATE_ALLMODS}; do
        if [ "${i}" = "base" ]; then
            SEMUPDATE_MODDIR="${SEMUPDATE_DEPOT}/content"
            SEMUPDATE_GITDIR="${SEMUPDATE_GIT}"
            SEMUPDATE_ARCHBR="${SEMAPHORE_GIT_BRANCH}"
        else
            SEMUPDATE_MODDIR="${SEMUPDATE_DEPOT}/content/data/${i}"
            SEMUPDATE_GITDIR="${SEMUPDATE_GIT}/data/${i}"
            git submodule update --init --depth 5 "data/${i}" || return 1
            pushd "${SEMUPDATE_GITDIR}" || return 1
            SEMUPDATE_ARCHBR=`git rev-parse HEAD`
            popd || return 1
        fi
        mkdir -pv "${SEMUPDATE_MODDIR}" || return 1
        pushd "${SEMUPDATE_GITDIR}" || return 1
        (git archive ${SEMUPDATE_ARCHBR} | tar -x -C "${SEMUPDATE_MODDIR}") || return 1
        if [ "${i}" = "base" ]; then
            # Steam build on Windows HATES SYMLINKS
            rm -rfv "${SEMUPDATE_MODDIR}/readme.md" "${SEMUPDATE_MODDIR}/doc/commands.txt" || return 1
            cp -RLfv "${SEMUPDATE_GITDIR}/config/usage.cfg" "${SEMUPDATE_MODDIR}/doc/commands.txt" || return 1
        fi
        popd || return 1
    done
    echo "--------------------------------------------------------------------------------"

    echo "########## GRABBING DEPLOYMENT BINARIES ##########"
    echo "steam" > "${SEMUPDATE_DEPOT}/content/branch.txt" || return 1
    unzip -o "${SEMUPDATE_DIR}/windows.zip" -d "${SEMUPDATE_DEPOT}/content" || return 1
    tar --gzip --extract --verbose --overwrite --file="${SEMUPDATE_DIR}/linux.tar.gz" --directory="${SEMUPDATE_DEPOT}/content"
    echo "--------------------------------------------------------------------------------"

    echo "########## SETTING UP STEAMCMD ##########"
    pushd "${SEMUPDATE_DEPOT}" || return 1
    curl -sqL "https://steamcdn-a.akamaihd.net/client/installer/steamcmd_linux.tar.gz" | tar zxvf -
    chmod --verbose +x "linux32/steamcmd" || return 1
    export LD_LIBRARY_PATH="${SEMUPDATE_DEPOT}/linux32:${LD_LIBRARY_PATH}"
    STEAM_GUARDWIN=0
    echo "--------------------------------------------------------------------------------"

    semupdate_steamdbg 0 || return 1

    echo "########## RUNNING STEAMCMD ##########"
    STEAM_EXECS=0
    semupdate_steamcmd
    while [ $? -eq 42 ] && [ ${STEAM_EXECS} -lt 2 ]; do
        echo "--------------------------------------------------------------------------------"
        semupdate_steamdbg 1 || return 1

        STEAM_EXECS=$(( STEAM_EXECS + 1 ))
        echo "########## RUNNING STEAMCMD [RETRY: ${STEAM_EXECS}] ##########"
        semupdate_steamcmd
    done
    echo "--------------------------------------------------------------------------------"

    semupdate_steamdbg 1 || return 1

    echo "########## SAVING CACHE ##########"
    # cache store skips keys that already exist, so drop the old copies first (steamcmd's saved login is in Steam/config)
    cache delete .steam
    cache store .steam "${HOME}/.steam" || return 1
    cache delete Steam
    cache store Steam "${HOME}/Steam" || return 1
    for i in output package public; do
        cache delete "${i}"
        cache store "${i}" "${SEMUPDATE_DEPOT}/${i}" || return 1
    done
    echo "--------------------------------------------------------------------------------"

    popd || return 1
    return 0
}

if [ "${SEMAPHORE_GIT_BRANCH}" = master ]; then
    semupdate_setup || exit 1
    case $1 in
        appimage)
            semupdate_appimage || exit 1
            ;;
        steam)
            semupdate_steam || exit 1
            ;;
        *)
            echo "Usage: $0 <appimage|steam>"
            ;;
    esac
fi
echo "done."
