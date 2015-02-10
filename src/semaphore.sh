#/bin/sh
case $1 in
    setup)
        export BUILDDIR=${HOME}/build/${BRANCH_NAME}
        mkdir -pv ${SEMAPHORE_CACHE_DIR}/apt/archives/partial
        sudo cp -ruv /var/cache/apt ${SEMAPHORE_CACHE_DIR}/apt
        sudo rm -rfv /var/cache/apt
        sudo ln -sv ${SEMAPHORE_CACHE_DIR}/apt /var/cache/apt
        ;;
    build)
        sudo apt-get update
        sudo apt-get -fy install build-essential zlib1g-dev libsdl-mixer1.2-dev libsdl-image1.2-dev
        make PLATFORM=linux64 PLATFORM_BIN=amd64 INSTDIR=linux/bin/amd64 CFLAGS=-m64 CXXFLAGS=-m64 LDFLAGS=-m64 -C src clean install || exit 1
        if [ "${BRANCH_NAME}" = "master" ]; then
            sudo apt-get -fy install binutils-mingw-w64 g++-mingw-w64
            make PLATFORM=crossmingw64 PLATFORM_BIN=amd64 INSTDIR=${BUILDDIR}/windows/bin/amd64 CFLAGS=-m64 CXXFLAGS=-m64 LDFLAGS=-m64 -C src clean install || exit 1
            make PLATFORM=crossmingw32 PLATFORM_BIN=x86 INSTDIR=${BUILDDIR}/windows/bin/x86 CFLAGS=-m32 CXXFLAGS=-m32 LDFLAGS=-m32 -C src clean install || exit 1
            sudo dpkg --add-architecture i386
            sudo apt-get update
            sudo apt-get -fy remove zlib1g-dev libsdl1.2-dev libsdl-mixer1.2-dev libsdl-image1.2-dev libpng-dev
            sudo apt-get -fy autoremove
            sudo apt-get -fy install build-essential multiarch-support g++-multilib zlib1g-dev:i386 libsdl1.2-dev:i386 libsdl-mixer1.2-dev:i386 libsdl-image1.2-dev:i386 libpng-dev:i386
            make PLATFORM=linux32 PLATFORM_BIN=x86 INSTDIR=${BUILDDIR}/linux/bin/x86 CFLAGS=-m32 CXXFLAGS=-m32 LDFLAGS=-m32 -C src clean install || exit 1
        fi
        ;;
    deploy)
        if [ "${BRANCH_NAME}" = "master" ] && [ "${SEMAPHORE_THREAD_RESULT}" = "passed" ]; then
            pushd ${BUILDDIR} || exit 1
            rm -fv version.txt windows.zip linux.tar.bz2
            pushd windows || exit 1
            zip -r ../windows.zip .
            popd
            pushd linux || exit 1
            tar -jcvf ../linux.tar.bz2 .
            popd
            date +%Y%m%d%H%M%S > version.txt
            scp -BC -i ${HOME}/.ssh/public_rsa -o StrictHostKeyChecking=no windows.zip qreeves@icculus.org:/webspace/redeclipse.net/files/devel/windows.zip
            scp -BC -i ${HOME}/.ssh/public_rsa -o StrictHostKeyChecking=no linux.tar.bz2 qreeves@icculus.org:/webspace/redeclipse.net/files/devel/linux.tar.bz2
            scp -BC -i ${HOME}/.ssh/public_rsa -o StrictHostKeyChecking=no version.txt qreeves@icculus.org:/webspace/redeclipse.net/files/devel/version.txt
            popd
        fi
        ;;
    *)
        exit 1
        ;;
esac
