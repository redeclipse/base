#! /bin/bash

SEMUPDATE_GIT="${HOME}/${SEMAPHORE_GIT_DIR}"
SEMABUILD_BUILD="${HOME}/deploy"
SEMABUILD_SCP='scp -BC -o StrictHostKeyChecking=no'
SEMABUILD_TARGET='qreeves@icculus.org:/webspace/redeclipse.net/files'
SEMABUILD_APT='DEBIAN_FRONTEND=noninteractive apt-get'
SEMABUILD_MODULES=`curl --connect-timeout 30 -L -k -f https://raw.githubusercontent.com/redeclipse/deploy/master/${SEMAPHORE_GIT_BRANCH}/mods.txt` || exit 1
SEMABUILD_ALLMODS="base ${SEMABUILD_MODULES}"
SEMABUILD_DIST="bz2 combined win zip mac"

sudo ${SEMABUILD_APT} update || exit 1
sudo ${SEMABUILD_APT} -fy install build-essential unzip zip nsis nsis-common mktorrent golang || exit 1
sudo ${SEMABUILD_APT} clean || exit 1

#export GOPATH="${HOME}/gofiles"
#go get "github.com/itchio/gothub"
#SEMABUILD_GHR="${GOPATH}/bin/gothub"

rm -rfv "${SEMABUILD_BUILD}"
rm -rfv "${SEMUPDATE_GIT}/data"
mkdir -pv "${SEMABUILD_BUILD}" || exit 1

for i in ${SEMABUILD_ALLMODS}; do
    if [ "${i}" = "base" ]; then
        SEMABUILD_MODDIR="${SEMABUILD_BUILD}"
        SEMUPDATE_GITDIR="${SEMUPDATE_GIT}"
    else
        SEMABUILD_MODDIR="${SEMABUILD_BUILD}/data/${i}"
        SEMUPDATE_GITDIR="${SEMUPDATE_GIT}/data/${i}"
        git submodule update --init --depth 5 "data/${i}" || exit 1
    fi
    mkdir -pv "${SEMABUILD_MODDIR}" || exit 1
    pushd "${SEMUPDATE_GITDIR}" || exit 1
    SEMABUILD_ARCHBR=`git rev-parse HEAD`
    (git archive --verbose ${SEMABUILD_ARCHBR} | tar -x -C "${SEMABUILD_MODDIR}") || exit 1
    popd || exit 1
done

rm -rfv "${SEMUPDATE_GIT}/data" "${SEMUPDATE_GIT}/.git"

SEMABUILD_NAME=`sed -n 's/.define VERSION_NAME *"\([^"]*\)"/\1/p' "${SEMUPDATE_GIT}/src/engine/version.h"`
SEMABUILD_UNAME=`sed -n 's/.define VERSION_UNAME *"\([^"]*\)"/\1/p' "${SEMUPDATE_GIT}/src/engine/version.h"`
SEMABUILD_RELEASE=`sed -n 's/.define VERSION_RELEASE *"\([^"]*\)"/\1/p' "${SEMUPDATE_GIT}/src/engine/version.h"`
SEMABUILD_VERSION_MAJOR=`sed -n 's/.define VERSION_MAJOR \([0-9]*\)/\1/p' src/engine/version.h`
SEMABUILD_VERSION_MINOR=`sed -n 's/.define VERSION_MINOR \([0-9]*\)/\1/p' src/engine/version.h`
SEMABUILD_VERSION_PATCH=`sed -n 's/.define VERSION_PATCH \([0-9]*\)/\1/p' src/engine/version.h`
SEMABUILD_VERSION="${SEMABUILD_VERSION_MAJOR}.${SEMABUILD_VERSION_MINOR}.${SEMABUILD_VERSION_PATCH}"

#${SEMABUILD_GHR} release --user "redeclipse" --repo "base" --tag "v${SEMABUILD_VERSION}" --name "v${SEMABUILD_VERSION} (${SEMABUILD_RELEASE})" --description "${SEMABUILD_NAME} v${SEMABUILD_VERSION} (${SEMABUILD_RELEASE}) has been released!" --target "${SEMAPHORE_GIT_BRANCH}" --draft

for i in ${SEMABUILD_DIST}; do
    pushd "${SEMABUILD_BUILD}/src" || exit 1
    make APPBRANCH="${SEMAPHORE_GIT_BRANCH}" dist-${i} dist-torrent-${i} || exit 1
    popd || exit 1
    pushd "${SEMABUILD_BUILD}" || exit 1
    mkdir -p releases || exit 1
    m="${i}"
    n=""
    case "${i}" in
        bz2)
            m="nix"
            n="tar.bz2"
            ;;
        win)
            n="exe"
            ;;
        zip)
            m="win"
            n="zip"
            ;;
        *)
            n="tar.bz2"
            ;;
    esac
    o="${SEMABUILD_UNAME}_${SEMABUILD_VERSION}_${m}"
    p="${o}.${n} ${o}.${n}.torrent"
    for q in ${p}; do
        mv -vf "${q}" "releases/${q}" || exit 1
        shasum "releases/${q}" > "releases/${q}.shasum" || exit 1
        md5sum "releases/${q}" > "releases/${q}.md5sum" || exit 1
        #${SEMABUILD_GHR} upload --user "redeclipse" --repo "base" --tag "v${SEMABUILD_VERSION}" --name "${q}" --file "releases/${q}" || exit 1
    done
    ${SEMABUILD_SCP} -r "releases" "${SEMABUILD_TARGET}" || exit 1
    rm -rfv releases || exit 1
    popd || exit 1
done
