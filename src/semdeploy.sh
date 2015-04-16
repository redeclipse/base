#!/bin/sh
SEMABUILD_PWD=`pwd`
SEMABUILD_SCP='scp -BC -o StrictHostKeyChecking=no'
SEMABUILD_TARGET='qreeves@icculus.org:/webspace/redeclipse.net/files'
SEMABUILD_APT='DEBIAN_FRONTEND=noninteractive apt-get'

sudo ${SEMABUILD_APT} update || exit 1
sudo ${SEMABUILD_APT} -fy install build-essential unzip zip nsis nsis-common mktorrent || exit 1

mkdir -p "${HOME}/deploy/data" || exit 1
( git archive stable | tar -x -C "${HOME}/deploy" ) || exit 1
pushd "${SEMABUILD_PWD}/data" || exit 1
( git archive master | tar -x -C "${HOME}/deploy/data" ) || exit 1
popd || exit 1

pushd "${HOME}/deploy/src" || exit 1
make dist-clean dist-$1 dist-torrent-$1 || exit 1
popd || exit 1

pushd "${HOME}/deploy" || exit 1
mkdir -p releases || exit 1
case "$1" in
    osx|nix|bz2)
        mv -vf redeclipse_*.*_*.tar.bz2 releases/ || exit 1
        ;;
    win)
        mv -vf redeclipse_*.*_*.exe releases/ || exit 1
        ;;
esac
mv -vf redeclipse_*.*_*.torrent releases/ || exit 1
${SEMABUILD_SCP} -r "releases" "${SEMABUILD_TARGET}" || exit 1
popd || exit 1
