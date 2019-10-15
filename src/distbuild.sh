#! /bin/bash

DISTBUILD_PWD=`pwd`
DISTBUILD_BRANCH=$1
if [ -z "${DISTBUILD_BRANCH}" ]; then
    DISTBUILD_BRANCH="master"
fi
DISTBUILD_BUILD="${HOME}/deploy"
DISTBUILD_MODULES=`curl --connect-timeout 30 -L -k -f https://raw.githubusercontent.com/redeclipse/deploy/master/${DISTBUILD_BRANCH}/mods.txt` || exit 1
DISTBUILD_ALLMODS="base ${DISTBUILD_MODULES}"
DISTBUILD_DIST="bz2 combined win zip mac"

rm -rfv "${DISTBUILD_BUILD}"
mkdir -pv "${DISTBUILD_BUILD}" || exit 1

for i in ${DISTBUILD_ALLMODS}; do
    if [ "${i}" = "base" ]; then
        DISTBUILD_MODDIR="${DISTBUILD_BUILD}"
        DISTBUILD_GITDIR="${DISTBUILD_PWD}"
    else
        DISTBUILD_MODDIR="${DISTBUILD_BUILD}/data/${i}"
        DISTBUILD_GITDIR="${DISTBUILD_PWD}/data/${i}"
    fi
    mkdir -pv "${DISTBUILD_MODDIR}" || exit 1
    pushd "${DISTBUILD_GITDIR}" || exit 1
    DISTBUILD_ARCHBR=`git rev-parse HEAD`
    (git archive --verbose ${DISTBUILD_ARCHBR} | tar -x -C "${DISTBUILD_MODDIR}") || exit 1
    popd || exit 1
done

DISTBUILD_UNAME=`sed -n 's/.define VERSION_UNAME *"\([^"]*\)"/\1/p' "${DISTBUILD_PWD}/src/engine/version.h"`
DISTBUILD_VERSION=`sed -n 's/.define VERSION_STRING *"\([^"]*\)"/\1/p' "${DISTBUILD_PWD}/src/engine/version.h"`

for i in ${DISTBUILD_DIST}; do
    pushd "${DISTBUILD_BUILD}/src" || exit 1
    make APPBRANCH="${DISTBUILD_BRANCH}" dist-${i} dist-torrent-${i} || exit 1
    popd || exit 1
    pushd "${DISTBUILD_BUILD}" || exit 1
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
    o="${DISTBUILD_UNAME}_${DISTBUILD_VERSION}_${m}"
    p="${o}.${n} ${o}.${n}.torrent"
    for q in ${p}; do
        mv -vf "${q}" "releases/${q}" || exit 1
        shasum "releases/${q}" > "releases/${q}.shasum" || exit 1
        md5sum "releases/${q}" > "releases/${q}.md5sum" || exit 1
    done
    popd || exit 1
done
