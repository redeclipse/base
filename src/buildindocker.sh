#!/bin/bash

IMAGE_NAME=${IMAGE_NAME:-"q009/redeclipse_build"}
IMAGE_TAG=${IMAGE_TAG:-"v1.0.0"}

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
WORK_DIR=${HOME}/.redeclipse_build
SYS_DIR=${WORK_DIR}/sys

CACHE_DIR=${CACHE_DIR:-"${WORK_DIR}/cache"}
IMAGE_DIR="${CACHE_DIR}/image"
CACHE_IMAGE="${IMAGE_DIR}/image.tar.gz"

GAME_DIR=${GAME_DIR:-"${SCRIPT_DIR}/.."}
OUTPUT_DIR=${OUTPUT_DIR:-"${SCRIPT_DIR}/../build"}

IMAGE_CACHE_ENABLED=0
INSTALL_BINARIES_ENABLED=0

NEEDS_CACHE=0

get_branch() {
    pushd "${GAME_DIR}" &>/dev/null || return 1

    type git &>/dev/null && \
        git rev-parse --abbrev-ref HEAD 2> /dev/null ||
        echo ""

    popd &>/dev/null || return 1
}

get_revision() {
    pushd "${GAME_DIR}" &>/dev/null || return 1

    type git &>/dev/null && \
        git rev-parse HEAD 2> /dev/null ||
        echo ""

    popd &>/dev/null || return 1
}

PLATFORM_BUILD=${PLATFORM_BUILD:-""}
PLATFORM_BRANCH=${PLATFORM_BRANCH:-$(get_branch)}
PLATFORM_REVISION=${PLATFORM_REVISION:-$(get_revision)}

cleanup() {
    sudo umount ${SYS_DIR} 2> /dev/null
    rm -rf ${WORK_DIR}/sys.img 2> /dev/null
}

fail() {
    echo "Build in Docker failure: $1"

    cleanup
    exit 1
}

check_image_cache() {
    if [ $IMAGE_CACHE_ENABLED == 0 ]; then
        # Caching disabled
        return 0
    fi

    echo "Checking for Docker image ${IMAGE_NAME}:${IMAGE_TAG}..."

    if [[ ! -f "${CACHE_IMAGE}" ]]; then
        echo "Cached image not found, will pull."
        NEEDS_CACHE=1
        return 0
    fi

    # Check if the cached image name and tag match the ones we want
    if [ $(cat "${CACHE_IMAGE}.ver") != "${IMAGE_NAME}:${IMAGE_TAG}" ]; then
        # No match, we'll pull a new image
        echo "Cached image name and tag do not match, will pull."
        NEEDS_CACHE=1
        return 0
    fi

    pushd "${IMAGE_DIR}"  || return 1

    echo "Loading cached Docker image..."
    docker load -i image.tar.gz || return 1

    popd || return 1

    return 0
}

save_image_cache() {
    if [ ${IMAGE_CACHE_ENABLED} == 0 ]; then
        # Caching disabled
        return 0
    fi

    if [ ${NEEDS_CACHE} == 0 ]; then
        echo "Image cache is up to date"
        return 0
    fi

    echo "Saving Docker image..."

    mkdir -p "${IMAGE_DIR}" || return 1

    pushd "${IMAGE_DIR}"  || return 1

    rm -f image*

    docker save -o image.tar "${IMAGE_NAME}:${IMAGE_TAG}" || return 1

    echo "Compressing image..."

    # Repackage the image to save space
    mkdir image              || return 1
    mv image.tar image       || return 1
    cd image                 || return 1
    tar xf image.tar         || return 1
    rm image.tar             || return 1
    tar czf image.tar.gz ./* || return 1
    mv image.tar.gz ..       || return 1
    cd ..                    || return 1
    rm -rf image             || return 1

    echo "${IMAGE_NAME}:${IMAGE_TAG}" > "image.tar.gz.ver" || return 1

    popd || return 1

    echo "Image cache saved for ${IMAGE_NAME}:${IMAGE_TAG}"

    return 0
}

# Windows libraries require a case-insensitive filesystem
prep_sys_fs() {
    echo "Creating case-insensitive FS..."

    mkdir -p ${SYS_DIR} || return 1
    truncate -s 2G ${WORK_DIR}/sys.img || return 1
    mkfs.vfat ${WORK_DIR}/sys.img || return 1
    sudo mount -o loop,rw,users,uid=1000,umask=000 ${WORK_DIR}/sys.img ${SYS_DIR} || return 1

    return 0
}

build() {
    docker run \
        -v "${GAME_DIR}:/ci/game" \
        -v "${SYS_DIR}:/ci/sys" \
        --mount type=bind,src="${OUTPUT_DIR}",dst=/ci/output \
        -e PLATFORM_BUILD="${PLATFORM_BUILD}" \
        -e PLATFORM_BRANCH="${PLATFORM_BRANCH}" \
        -e PLATFORM_REVISION="${PLATFORM_REVISION}" \
        --rm \
        "${IMAGE_NAME}:${IMAGE_TAG}" || return 1
}

install_binaries() {
    if [ ${INSTALL_BINARIES_ENABLED} == 0 ]; then
        return 0
    fi

    echo "Installing binaries..."

    cp -rf "${OUTPUT_DIR}/windows/"* "${GAME_DIR}" || return 1
    cp -rf "${OUTPUT_DIR}/linux/"* "${GAME_DIR}" || return 1
}

help() {
    echo "Usage: buildindocker.sh [-i] [-h] [-c]"
    echo "Options:"
    echo "  -i - Install binaries"
    echo "  -h - Help"
    echo "  -c - Enable image cache"
    exit 0
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

while getopts "ihc" opt; do
    case ${opt} in
        i)
            INSTALL_BINARIES_ENABLED=1
            ;;
        h)
            help
            ;;
        c)
            IMAGE_CACHE_ENABLED=1
            ;;
        \?)
            echo "Invalid option: -${OPTARG}" 1>&2
            exit 1
            ;;
    esac
done

prep_sys_fs ||
    fail "Unable to create a filesystem for Windows libraries"

check_image_cache ||
    fail "Unable to check Docker image"

build ||
    fail "Unable to build the game"

install_binaries ||
    fail "Unable to copy game binaries"

save_image_cache ||
    fail "Unable to save Docker image for building"

cleanup
