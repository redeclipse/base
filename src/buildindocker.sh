#!/bin/bash

WORK_DIR=${HOME}/.redeclipse_build
SYS_DIR=${WORK_DIR}/sys

CACHE_DIR=${CACHE_DIR:-"${WORK_DIR}/cache"}
IMAGE_DIR="${CACHE_DIR}/image"
CACHE_IMAGE="${IMAGE_DIR}/image.tar"

GAME_DIR=${GAME_DIR:-""}
OUTPUT_DIR=${OUTPUT_DIR:-""}

PLATFORM_BUILD=${PLATFORM_BUILD:-""}
PLATFORM_BRANCH=${PLATFORM_BRANCH:-""}
PLATFORM_REVISION=${PLATFORM_REVISION:-""}

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

    mkdir -p "${IMAGE_DIR}"

    sudo apt update && \
        sudo apt install -y git-lfs

    pushd "${IMAGE_DIR}"

    if [ "$(git rev-parse --show-toplevel)" == "$(pwd)" ]; then
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

cat << EOF



#####################################
#                                   #
# Preparing to build Red Eclipse... #
#                                   #
#####################################

Directories:
* Game: ${GAME_DIR}
* Output: ${OUTPUT_DIR}
* Cache: ${CACHE_DIR}

Build information:
* Number: ${PLATFORM_BUILD}
* Branch: ${PLATFORM_BRANCH}
* Commit: ${PLATFORM_REVISION}



EOF



prep_sys_fs ||
    fail "Unable to create a filesystem for Windows libraries"

get_image ||
    fail "Unable to get Docker image for building"

docker run \
    -v "${GAME_DIR}:/ci/game" \
    -v "${SYS_DIR}:/ci/sys" \
    --mount type=bind,src="${OUTPUT_DIR}",dst=/ci/output \
    -e PLATFORM_BUILD=${PLATFORM_BUILD} \
    -e PLATFORM_BRANCH=${PLATFORM_BRANCH} \
    -e PLATFORM_REVISION=${PLATFORM_REVISION} \
    redeclipse/build

cleanup
