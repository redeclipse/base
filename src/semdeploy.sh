#!/bin/sh
SEMABUILD_PWD=`pwd`
SEMABUILD_BUILD="${HOME}/deploy"
SEMABUILD_APT='DEBIAN_FRONTEND=noninteractive apt-get'
SEMABUILD_MODULES=`curl --connect-timeout 30 -L -k -f https://raw.githubusercontent.com/red-eclipse/deploy/master/stable/mods.txt` || exit 1
SEMABUILD_ALLMODS="base ${SEMABUILD_MODULES}"
SEMABUILD_DIST="bz2 combined win zip mac"

sudo ${SEMABUILD_APT} update || exit 1
sudo ${SEMABUILD_APT} -fy install build-essential unzip zip nsis nsis-common mktorrent golang || exit 1

export GOPATH="${HOME}/gofiles"
go get "github.com/aktau/github-release"
SEMABUILD_GHR="${GOPATH}/bin/github-release"

rm -rfv "${SEMABUILD_BUILD}"
rm -rfv "${SEMABUILD_PWD}/data"
mkdir -pv "${SEMABUILD_BUILD}" || exit 1

for i in ${SEMABUILD_ALLMODS}; do
    if [ "${i}" = "base" ]; then
        SEMABUILD_MODDIR="${SEMABUILD_BUILD}"
        SEMABUILD_GITDIR="${SEMABUILD_PWD}"
        SEMABUILD_ARCHBR="stable"
    else
        SEMABUILD_MODDIR="${SEMABUILD_BUILD}/data/${i}"
        SEMABUILD_GITDIR="${SEMABUILD_PWD}/data/${i}"
        SEMABUILD_ARCHBR="master"
        git submodule init "data/${i}"
        git submodule update "data/${i}"
    fi
    mkdir -pv "${SEMABUILD_MODDIR}" || exit 1
    pushd "${SEMABUILD_GITDIR}" || exit 1
    (git archive ${SEMABUILD_ARCHBR} | tar -x -C "${SEMABUILD_MODDIR}") || exit 1
    popd
done

rm -rfv "${SEMABUILD_PWD}/data" "${SEMABUILD_PWD}/.git"

SEMABUILD_NAME=`sed -n 's/.define VERSION_NAME *"\([^"]*\)"/\1/p' "${SEMABUILD_PWD}/src/engine/version.h"`
SEMABUILD_UNAME=`sed -n 's/.define VERSION_UNAME *"\([^"]*\)"/\1/p' "${SEMABUILD_PWD}/src/engine/version.h"`
SEMABUILD_VERSION=`sed -n 's/.define VERSION_STRING *"\([^"]*\)"/\1/p' "${SEMABUILD_PWD}/src/engine/version.h"`
SEMABUILD_RELEASE=`sed -n 's/.define VERSION_RELEASE *"\([^"]*\)"/\1/p' "${SEMABUILD_PWD}/src/engine/version.h"`

${SEMABUILD_GHR} --verbose release --user "red-eclipse" --repo "base" --tag "v${SEMABUILD_VERSION}" --name "v${SEMABUILD_VERSION} (${SEMABUILD_RELEASE})" --description "${SEMABUILD_NAME} v${SEMABUILD_VERSION} (${SEMABUILD_RELEASE}) has been released!" --target "stable" --draft

for i in ${SEMABUILD_DIST}; do
    pushd "${SEMABUILD_BUILD}/src" || exit 1
    make dist-${i} dist-torrent-${i} || exit 1
    popd
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
            ;;
        *)
            n="tar.bz2"
            ;;
    esac
    o="${SEMABUILD_UNAME}_${SEMABUILD_VERSION}_${m}"
    p="${o}.${n} ${o}.${n}.torrent"
    for q in ${p}; do
        mv -vf "${q}" "releases/${q}"
        shasum "releases/${q}" > "releases/${q}.shasum"
        md5sum "releases/${q}" > "releases/${q}.md5sum"
        ${SEMABUILD_GHR} --verbose upload --user "red-eclipse" --repo "base" --tag "v${SEMABUILD_VERSION}" --name "${q}" --file "releases/${q}"
    done
    rm -rfv releases
    popd
done
