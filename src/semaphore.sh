#/bin/sh
case $1 in
    setup)
        mkdir -pv ${SEMAPHORE_CACHE_DIR}/${BRANCH_NAME}/windows/bin ${SEMAPHORE_CACHE_DIR}/${BRANCH_NAME}/linux/bin ${SEMAPHORE_CACHE_DIR}/apt/partial
        sudo cp -ruv /var/cache/apt/archives ${SEMAPHORE_CACHE_DIR}/apt
        sudo rm -rfv /var/cache/apt/archives
        sudo ln -sv ${SEMAPHORE_CACHE_DIR}/apt /var/cache/apt/archives
        ;;
    build-1)
        sudo apt-get update
        sudo apt-get -fy install build-essential zlib1g-dev libsdl-mixer1.2-dev libsdl-image1.2-dev
        make PLATFORM=linux64 PLATFORM_BIN=amd64 INSTDIR=${SEMAPHORE_CACHE_DIR}/${BRANCH_NAME}/linux/bin/amd64 CFLAGS=-m64 CXXFLAGS=-m64 LDFLAGS=-m64 -C src clean install
        if [ "${BRANCH_NAME}" = "master" ]; then
            sudo apt-get -fy install binutils-mingw-w64 g++-mingw-w64
            make PLATFORM=crossmingw64 PLATFORM_BIN=amd64 INSTDIR=${SEMAPHORE_CACHE_DIR}/${BRANCH_NAME}/windows/bin/amd64 CFLAGS=-m64 CXXFLAGS=-m64 LDFLAGS=-m64 -C src clean install
            make PLATFORM=crossmingw32 PLATFORM_BIN=x86 INSTDIR=${SEMAPHORE_CACHE_DIR}/${BRANCH_NAME}/windows/bin/x86 CFLAGS=-m32 CXXFLAGS=-m32 LDFLAGS=-m32 -C src clean install
        fi
        ;;
    build-2)
        if [ "${BRANCH_NAME}" = "master" ]; then
            sudo dpkg --add-architecture i386
            sudo apt-get update
            sudo apt-get -fy remove zlib1g-dev libsdl1.2-dev libsdl-mixer1.2-dev libsdl-image1.2-dev libpng-dev
            sudo apt-get -fy autoremove
            sudo apt-get -fy install build-essential multiarch-support g++-multilib zlib1g-dev:i386 libsdl1.2-dev:i386 libsdl-mixer1.2-dev:i386 libsdl-image1.2-dev:i386 libpng-dev:i386
            make PLATFORM=linux32 PLATFORM_BIN=x86 INSTDIR=${SEMAPHORE_CACHE_DIR}/${BRANCH_NAME}/linux/bin/x86 CFLAGS=-m32 CXXFLAGS=-m32 LDFLAGS=-m32 -C src clean install
        fi
        ;;
    deploy)
        if [ "${BRANCH_NAME}" = "master" ]; then
            sudo apt-get update
            sudo apt-get -fy install openssh-client zip
            pushd ${SEMAPHORE_CACHE_DIR}/${BRANCH_NAME}
            rm -fv version.txt windows.zip linux.tar.bz2
            pushd windows
            zip -r ../windows.zip .
            popd
            pushd linux
            tar -jcvf ../linux.tar.bz2 .
            popd
            date +%Y%m%d%H%M%S > version.txt
            scp -BC -i ${HOME}/.ssh/public_rsa -o StrictHostKeyChecking=no windows.zip qreeves@icculus.org:/webspace/redeclipse.net/files/devel/windows.zip
            scp -BC -i ${HOME}/.ssh/public_rsa -o StrictHostKeyChecking=no linux.tar.bz2 qreeves@icculus.org:/webspace/redeclipse.net/files/devel/linux.tar.bz2
            scp -BC -i ${HOME}/.ssh/public_rsa -o StrictHostKeyChecking=no version.txt qreeves@icculus.org:/webspace/redeclipse.net/files/devel/version.txt
            rm -rfv *
            popd
        fi
        ;;
    *)
        exit 1
        ;;
esac
