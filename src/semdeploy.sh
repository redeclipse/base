#!/bin/sh
SEMABUILD_PWD=`pwd`
SEMABUILD_SCP='scp -BC -o StrictHostKeyChecking=no'
SEMABUILD_TARGET='qreeves@icculus.org:/webspace/redeclipse.net/files/releases'
SEMABUILD_APT='DEBIAN_FRONTEND=noninteractive apt-get'

sudo ${SEMABUILD_APT} update || exit 1
sudo ${SEMABUILD_APT} install build-essential nsis nsis-common || exit 1
pushd src || exit 1
make dist || exit 1
popd
mkdir releases || exit 1
mv -vf redeclipse_*.*_*.tar.bz2 releases/ || exit 1
mv -vf redeclipse_*.*_*.exe releases/ || exit 1
${SEMABUILD_SCP} -r "releases" "${SEMABUILD_TARGET}" || exit 1
exit 0
