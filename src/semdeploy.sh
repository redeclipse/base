#!/bin/sh
SEMABUILD_PWD=`pwd`
SEMABUILD_BUILD="${HOME}/deploy"
SEMABUILD_SCP='scp -BC -o StrictHostKeyChecking=no'
SEMABUILD_TARGET='qreeves@icculus.org:/webspace/redeclipse.net/files'
SEMABUILD_APT='DEBIAN_FRONTEND=noninteractive apt-get'
SEMABUILD_MODULES=`curl --silent --fail http://redeclipse.net/files/stable/modules.txt` || (echo "failed to retrieve modules"; exit 1)
SEMABUILD_ALLMODS="base ${SEMABUILD_MODULES}"

sudo ${SEMABUILD_APT} update || exit 1
sudo ${SEMABUILD_APT} -fy install build-essential unzip zip nsis nsis-common mktorrent || exit 1

rm -rfv "${SEMABUILD_BUILD}"
mkdir -pv "${SEMABUILD_BUILD}" || exit 1
git submodule init || exit 1
git submodule update || exit 1

for i in ${SEMABUILD_ALLMODS}; do
    if [ "${i}" = "base" ]; then
        SEMABUILD_MODDIR="${SEMABUILD_BUILD}"
        SEMABUILD_GITDIR="${SEMABUILD_PWD}"
        SEMABUILD_ARCHBR="stable"
    else
        SEMABUILD_MODDIR="${SEMABUILD_BUILD}/data/${i}"
        SEMABUILD_GITDIR="${SEMABUILD_PWD}/data/${i}"
        SEMABUILD_ARCHBR="master"
    fi
    mkdir -pv "${SEMABUILD_MODDIR}" || exit 1
    pushd "${SEMABUILD_GITDIR}" || exit 1
    (git archive ${SEMABUILD_ARCHBR} | tar -x -C "${SEMABUILD_MODDIR}") || exit 1
    popd
done

pushd "${SEMABUILD_BUILD}/src" || exit 1
make dist dist-torrents || (popd; exit 1)
popd

pushd "${SEMABUILD_BUILD}" || exit 1
mkdir -p releases || (popd; exit 1)
mv -vf redeclipse_*.*_*.tar.bz2 releases/ || (popd; exit 1)
mv -vf redeclipse_*.*_*.exe releases/ || (popd; exit 1)
mv -vf redeclipse_*.*_*.torrent releases/ || (popd; exit 1)
${SEMABUILD_SCP} -r "releases" "${SEMABUILD_TARGET}" || (popd; exit 1)
popd
