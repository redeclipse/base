#/bin/sh
ORIGPWD=`pwd`
# check
if [ "${BRANCH_NAME}" = "master" ]; then
    BASE_COMMIT=`git rev-parse HEAD`
    git submodule init data || exit 1
    git submodule update data || exit 1
    cd data || exit 1
    DATA_COMMIT=`git rev-parse HEAD`
    cd "${ORIGPWD}" || exit 1
    DEPLOY_COMMIT=`curl --fail --silent http://redeclipse.net/files/devel/base.txt`
    if [ -n "${DEPLOY_COMMIT}" ]; then
        DEPLOY_SRCFILES=`git diff --name-only HEAD ${DEPLOY_COMMIT} -- src`
        if [ -z "${DEPLOY_SRCFILES}" ]; then
            echo "No source files modified, skipping..."
            if [ -n "${BASE_COMMIT}" ] && [ "${BASE_COMMIT}" != "${DEPLOY_COMMIT}" ]; then
                echo "Module 'base' commit updated, syncing that: ${BASE_COMMIT} -> ${DEPLOY_COMMIT}"
                echo "${BASE_COMMIT}" > base.txt
                scp -BC -i ${HOME}/.ssh/public_rsa -o StrictHostKeyChecking=no base.txt qreeves@icculus.org:/webspace/redeclipse.net/files/devel/base.txt
            fi
            DEPLOY_DATA=`curl --fail --silent http://redeclipse.net/files/devel/data.txt`
            if [ -n "${DATA_COMMIT}" ] && [ -n "${DEPLOY_DATA}" ] && [ "${DATA_COMMIT}" != "${DEPLOY_DATA}" ]; then
                echo "Module 'data' commit updated, syncing that: ${DEPLOY_DATA} -> ${DATA_COMMIT}"
                echo "${DATA_COMMIT}" > data.txt
                scp -BC -i ${HOME}/.ssh/public_rsa -o StrictHostKeyChecking=no data.txt qreeves@icculus.org:/webspace/redeclipse.net/files/devel/data.txt
            fi
            exit 0
        else
            echo "Source files modified, proceeding with build..."
            echo ""
        fi
    fi
fi
#setup
CMD_APT="DEBIAN_FRONTEND=noninteractive apt-get"
mkdir -pv ${SEMAPHORE_CACHE_DIR}/apt/archives/partial
sudo cp -ruv /var/cache/apt ${SEMAPHORE_CACHE_DIR}/apt
sudo rm -rfv /var/cache/apt
sudo ln -sv ${SEMAPHORE_CACHE_DIR}/apt /var/cache/apt
sudo dpkg --add-architecture i386
sudo ${CMD_APT} update
#build
sudo ${CMD_APT} -fy install build-essential zlib1g-dev libsdl-mixer1.2-dev libsdl-image1.2-dev
make PLATFORM=linux64 PLATFORM_BIN=amd64 INSTDIR=${HOME}/build/${BRANCH_NAME}/linux/bin/amd64 CFLAGS=-m64 CXXFLAGS=-m64 LDFLAGS=-m64 -C src clean install || exit 1
if [ "${BRANCH_NAME}" = "master" ]; then
    sudo ${CMD_APT} -fy install binutils-mingw-w64 g++-mingw-w64
    make PLATFORM=crossmingw64 PLATFORM_BIN=amd64 INSTDIR=${HOME}/build/${BRANCH_NAME}/windows/bin/amd64 CFLAGS=-m64 CXXFLAGS=-m64 LDFLAGS=-m64 -C src clean install || exit 1
    make PLATFORM=crossmingw32 PLATFORM_BIN=x86 INSTDIR=${HOME}/build/${BRANCH_NAME}/windows/bin/x86 CFLAGS=-m32 CXXFLAGS=-m32 LDFLAGS=-m32 -C src clean install || exit 1
    sudo ${CMD_APT} -fy remove zlib1g-dev libsdl1.2-dev libsdl-mixer1.2-dev libsdl-image1.2-dev libpng-dev
    sudo ${CMD_APT} -fy autoremove
    sudo ${CMD_APT} -fy install build-essential multiarch-support g++-multilib zlib1g-dev:i386 libsdl1.2-dev:i386 libsdl-mixer1.2-dev:i386 libsdl-image1.2-dev:i386 libpng-dev:i386
    make PLATFORM=linux32 PLATFORM_BIN=x86 INSTDIR=${HOME}/build/${BRANCH_NAME}/linux/bin/x86 CFLAGS=-m32 CXXFLAGS=-m32 LDFLAGS=-m32 -C src clean install || exit 1
    cd ${HOME}/build/${BRANCH_NAME} || exit 1
    rm -fv windows.zip linux.tar.gz || exit 1
    cd windows || exit 1
    zip -r ../windows.zip . || exit 1
    cd ../linux || exit 1
    tar -zcvf ../linux.tar.gz . || exit 1
    cd .. || exit 1
    echo "${BASE_COMMIT}" > bins.txt
    echo "${BASE_COMMIT}" > base.txt
    echo "${DATA_COMMIT}" > data.txt
    scp -BC -i ${HOME}/.ssh/public_rsa -o StrictHostKeyChecking=no windows.zip qreeves@icculus.org:/webspace/redeclipse.net/files/devel/windows.zip
    scp -BC -i ${HOME}/.ssh/public_rsa -o StrictHostKeyChecking=no linux.tar.gz qreeves@icculus.org:/webspace/redeclipse.net/files/devel/linux.tar.gz
    scp -BC -i ${HOME}/.ssh/public_rsa -o StrictHostKeyChecking=no bins.txt qreeves@icculus.org:/webspace/redeclipse.net/files/devel/bins.txt
    scp -BC -i ${HOME}/.ssh/public_rsa -o StrictHostKeyChecking=no base.txt qreeves@icculus.org:/webspace/redeclipse.net/files/devel/base.txt
    scp -BC -i ${HOME}/.ssh/public_rsa -o StrictHostKeyChecking=no data.txt qreeves@icculus.org:/webspace/redeclipse.net/files/devel/data.txt
    cd "${ORIGPWD}" || exit 1
fi
exit 0
