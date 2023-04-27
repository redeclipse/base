#!/bin/bash

WORK_DIR=${HOME}/.redeclipse_build
SYS_DIR=${WORK_DIR}/sys

CACHE_DIR=${CACHE_DIR:-"${WORK_DIR}/cache"}
CACHE_IMAGE="${CACHE_DIR}/image.tar"

cleanup() {
    sudo umount ${SYS_DIR} 2> /dev/null
    rm -rf ${WORK_DIR}/sys.img 2> /dev/null
}

fail() {
    echo "Build in Docker failure: $1"

    cleanup
    exit 1
}

get_image() {
    echo "Getting Docker image..."

    mkdir -p "${CACHE_DIR}"

    sudo apt update && \
        sudo apt install -y git-lfs

    pushd "${CACHE_DIR}"

    if git rev-parse --is-inside-work-tree > /dev/null 2>&1; then
        git pull || return 1
    else
        git clone -b dockerimg https://github.com/redeclipse/deploy.git . --depth 1 || return 1
    fi

    unzip image.tar.zip || return 1
    docker load -i image.tar || return 1
    rm image.tar || return 1

    popd

    return 0
}

# Windows libraries require a case-insensitive filesystem
prep_sys_fs() {
    echo "Creating case-insensitive FS..."

    mkdir -p ${SYS_DIR} || return 1
    truncate -s 4G ${WORK_DIR}/sys.img || return 1
    mkfs.vfat ${WORK_DIR}/sys.img || return 1
    sudo mount -o loop,rw,users,uid=1000,umask=000 ${WORK_DIR}/sys.img ${SYS_DIR} || return 1

    return 0
}

cleanup

echo "Preparing to build Red Eclipse..."
echo "Cache directory: ${CACHE_DIR}"

prep_sys_fs ||
    fail "Unable to create a filesystem for Windows libraries"

get_image ||
    fail "Unable to get Docker image for building"

docker run \
    -v /mnt/f/Red\ Eclipse:/ci/game \
    -v /mnt/f/Red\ Eclipse/build:/ci/output \
    -v ${SYS_DIR}:/ci/sys \
    --rm \
    redeclipse/build

cleanup
