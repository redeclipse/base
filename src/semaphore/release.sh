#! /bin/bash

SEMUPDATE_GIT="${HOME}/${SEMAPHORE_GIT_DIR}"
SEMABUILD_BUILD="${HOME}/deploy"
SEMABUILD_BINS="${HOME}/bins"
SEMABUILD_KEY="/home/runner/.ssh/public_rsa"
SEMABUILD_SCP="scp -BC -i ${SEMABUILD_KEY} -o StrictHostKeyChecking=no"
SEMABUILD_TARGET='qreeves@icculus.org:/webspace/redeclipse.net/files'
SEMABUILD_APT='DEBIAN_FRONTEND=noninteractive apt-get'
SEMABUILD_MODULES=`cat "${SEMUPDATE_GIT}/.gitmodules" | grep '\[submodule "[^.]' | sed -e 's/^.submodule..//;s/..$//' | tr "\n" " " | sed -e 's/ $//'`
SEMABUILD_ALLMODS="base ${SEMABUILD_MODULES}"
SEMABUILD_DIST="bz2 combined win zip" # mac
SEMABUILD_GHREPO="redeclipse/base"
# GitHub refuses release assets of 2GiB or more
SEMABUILD_GHMAX=2147483648

if [ ! -f "${SEMABUILD_KEY}" ]; then
    echo "No SSH key at ${SEMABUILD_KEY}, check the Icculus SSH secret"
    ls -la "${HOME}/.ssh" /home/runner/.ssh 2>/dev/null
    exit 1
fi
if ! grep -q "PRIVATE KEY" "${SEMABUILD_KEY}"; then
    echo "${SEMABUILD_KEY} is not a private key, check the Icculus SSH secret"
    exit 1
fi
chmod 600 "${SEMABUILD_KEY}" || exit 1
export GH_TOKEN="${GITHUB_TOKEN}"

sudo ${SEMABUILD_APT} update || exit 1
sudo ${SEMABUILD_APT} -fy install build-essential unzip zip nsis mktorrent lbzip2 || exit 1
if ! command -v gh > /dev/null; then sudo ${SEMABUILD_APT} -fy install gh || exit 1; fi
sudo ${SEMABUILD_APT} clean || exit 1

rm -rfv "${SEMABUILD_BUILD}" "${SEMABUILD_BINS}"
rm -rfv "${SEMUPDATE_GIT}/data"
mkdir -pv "${SEMABUILD_BUILD}" "${SEMABUILD_BINS}" || exit 1

pushd "${SEMABUILD_BINS}" || exit 1
artifact pull workflow "windows.zip" || exit 1
artifact pull workflow "linux.tar.gz" || exit 1
popd || exit 1

pushd "${SEMUPDATE_GIT}" || exit 1
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
    (git archive ${SEMABUILD_ARCHBR} | tar -x -C "${SEMABUILD_MODDIR}") || exit 1
    popd || exit 1
done
popd || exit 1

rm -rfv "${SEMUPDATE_GIT}/data" "${SEMUPDATE_GIT}/.git"
df -h "${HOME}"

SEMABUILD_NAME=`sed -n 's/.define VERSION_NAME *"\([^"]*\)"/\1/p' "${SEMUPDATE_GIT}/src/engine/version.h"`
SEMABUILD_UNAME=`sed -n 's/.define VERSION_UNAME *"\([^"]*\)"/\1/p' "${SEMUPDATE_GIT}/src/engine/version.h"`
SEMABUILD_RELEASE=`sed -n 's/.define VERSION_RELEASE *"\([^"]*\)"/\1/p' "${SEMUPDATE_GIT}/src/engine/version.h"`
SEMABUILD_VERSION_MAJOR=`sed -n 's/.define VERSION_MAJOR \([0-9]*\)/\1/p' "${SEMUPDATE_GIT}/src/engine/version.h"`
SEMABUILD_VERSION_MINOR=`sed -n 's/.define VERSION_MINOR \([0-9]*\)/\1/p' "${SEMUPDATE_GIT}/src/engine/version.h"`
SEMABUILD_VERSION_PATCH=`sed -n 's/.define VERSION_PATCH \([0-9]*\)/\1/p' "${SEMUPDATE_GIT}/src/engine/version.h"`
SEMABUILD_VERSION="${SEMABUILD_VERSION_MAJOR}.${SEMABUILD_VERSION_MINOR}.${SEMABUILD_VERSION_PATCH}"
SEMABUILD_TAG="v${SEMABUILD_VERSION}"

# a re-run reuses the draft left by the last one
if ! gh release view "${SEMABUILD_TAG}" --repo "${SEMABUILD_GHREPO}" > /dev/null 2>&1; then
    gh release create "${SEMABUILD_TAG}" --repo "${SEMABUILD_GHREPO}" --draft --target "${SEMAPHORE_GIT_SHA}" --title "${SEMABUILD_TAG} (${SEMABUILD_RELEASE})" --notes "${SEMABUILD_NAME} ${SEMABUILD_TAG} (${SEMABUILD_RELEASE}) has been released!" || exit 1
fi

for i in ${SEMABUILD_DIST}; do
    pushd "${SEMABUILD_BUILD}/src" || exit 1
    make APPBRANCH="${SEMAPHORE_GIT_BRANCH}" DISTBINS="${SEMABUILD_BINS}" DISTBZIP2=lbzip2 dist-${i} dist-torrent-${i} || exit 1
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
            # combined (and mac)
            n="tar.bz2"
            ;;
    esac
    o="${SEMABUILD_UNAME}_${SEMABUILD_VERSION}_${m}"
    p="${o}.${n} ${o}.${n}.torrent"
    for q in ${p}; do
        mv -vf "${q}" "releases/${q}" || exit 1
        shasum "releases/${q}" > "releases/${q}.shasum" || exit 1
        md5sum "releases/${q}" > "releases/${q}.md5sum" || exit 1
    done
    ls -la releases
    df -h "${HOME}"
    ${SEMABUILD_SCP} -r "releases" "${SEMABUILD_TARGET}" || exit 1
    for q in releases/*; do
        if [ `stat -c %s "${q}"` -ge ${SEMABUILD_GHMAX} ]; then
            echo "Skipping GitHub upload of ${q}, it is over 2GiB"
            continue
        fi
        gh release upload "${SEMABUILD_TAG}" "${q}" --repo "${SEMABUILD_GHREPO}" --clobber || exit 1
    done
    rm -rfv releases || exit 1
    popd || exit 1
done

echo "done."
