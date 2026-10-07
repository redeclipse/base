#!/bin/bash

MANIFEST_DIR=${HOME}/sys_manifests
ARCHIVE_DIR=${SEMAPHORE_CACHE_DIR}/sys_archives
SDK_ARCHIVE_DIR=${ARCHIVE_DIR}/sdk
MSVC_ARCHIVE_DIR=${ARCHIVE_DIR}/msvc
SYS_DIR=${HOME}/sys
SDK_VER_FILE=${ARCHIVE_DIR}/SDK_VER

msvc_pkgs=()
msvc_needs_fetch=0

# DESIRED VERSION DEFINITIONS
SDK_VER=10.0.22000.196
VS_VER=16
# LLVM 13 results in a crashing game binary
LLVM_VER=12

install_llvm() {
    wget https://apt.llvm.org/llvm.sh || return 1

    # Patch for non-interactive install
    sed -i 's/add-apt-repository "\${REPO_NAME}"/add-apt-repository "${REPO_NAME}" -y/g' llvm.sh || return 1

    chmod +x llvm.sh || return 1
    sudo ./llvm.sh ${LLVM_VER}

    # Check installed packages instead of return code from the script
    dpkg -l clang-${LLVM_VER} || return 1
    dpkg -l llvm-${LLVM_VER} || return 1

    return 0
}

fetch_sdk() {
    echo "Fetching Windows SDK..."

    wget -O ${SDK_ARCHIVE_DIR}/winsdk.nupkg https://www.nuget.org/api/v2/package/Microsoft.Windows.SDK.CPP/${SDK_VER} || return 1
    wget -O ${SDK_ARCHIVE_DIR}/winsdk_x86.nupkg https://www.nuget.org/api/v2/package/Microsoft.Windows.SDK.CPP.x86/${SDK_VER} || return 1
    wget -O ${SDK_ARCHIVE_DIR}/winsdk_x64.nupkg https://www.nuget.org/api/v2/package/Microsoft.Windows.SDK.CPP.x64/${SDK_VER} || return 1

    echo "${SDK_VER}" > ${SDK_VER_FILE}

    return 0
}

install_sdk() {
    touch ${SDK_VER_FILE}

    cached_ver=`cat ${SDK_VER_FILE}`
    if [ "$cached_ver" != "${SDK_VER}" ]; then
        echo "Purging old SDK cache..."
        rm -rf ${SDK_ARCHIVE_DIR}/*
        fetch_sdk || return 1
    fi

    unzip ${SDK_ARCHIVE_DIR}/winsdk.nupkg -d ${SYS_DIR}/winsdk || return 1
    unzip ${SDK_ARCHIVE_DIR}/winsdk_x86.nupkg -d ${SYS_DIR}/winsdk_lib || return 1
    unzip -n ${SDK_ARCHIVE_DIR}/winsdk_x64.nupkg -d ${SYS_DIR}/winsdk_lib || return 1

    return 0
}

get_msvc_manifest() {
    wget -O ${MANIFEST_DIR}/vsrel.json https://aka.ms/vs/${VS_VER}/release/channel || return 1
    wget -O ${MANIFEST_DIR}/vspkgs.json `jq '.channelItems[] | select(.type == "Manifest") | .payloads[0].url' ${MANIFEST_DIR}/vsrel.json -r` || return 1

    return 0
}

find_msvc_package() {
    deps=`jq ".packages[] | select(.id == \"$1\") | .dependencies | select(. != null) | keys[]" ${MANIFEST_DIR}/vspkgs.json -r` 2> /dev/null

    for dep in $deps; do
        find_msvc_package "$dep" "$2" || return 1
    done

    if [[ ! "${msvc_pkg_names[*]}" =~ "$1" ]]; then
        echo "Needs package: $1"
        msvc_pkgs+=("$1")
        if [[ ! -f "${MSVC_ARCHIVE_DIR}/$1.vsix" ]]; then
            msvc_needs_fetch=1
        fi
    fi

    return 0
}

install_msvc_package() {
    echo "Installing $1..."

    if [[ $msvc_needs_fetch -eq 1 ]]; then
        wget -O "${MSVC_ARCHIVE_DIR}/$1.vsix" `jq ".packages[] | select(.id == \"$1\") | .payloads[0].url" ${MANIFEST_DIR}/vspkgs.json -r` || return 1
    fi

    unzip -n "${MSVC_ARCHIVE_DIR}/$1.vsix" -d ${SYS_DIR}/msvc || return 1

    return 0
}

install_msvc_packages() {
    if [ $msvc_needs_fetch -eq 1 ]; then
        echo "Purging old MSVC cache..."
        rm -rf ${MSVC_ARCHIVE_DIR}/*
    fi

    for pkg in ${msvc_pkgs[@]}; do
        install_msvc_package "$pkg" || return 1
    done

    return 0
}

prep_sys_fs() {
    echo "Creating case-insensitive FS..."

    mkdir -p ${SYS_DIR} || return 1
    truncate -s 8G sys.img || return 1
    mkfs.vfat sys.img || return 1
    sudo mount -o loop,rw,users,uid=1000,umask=000 sys.img ${SYS_DIR} || return 1

    return 0
}

sys_install() {
    mkdir -p ${MANIFEST_DIR} || return 1
    mkdir -p ${SDK_ARCHIVE_DIR} || return 1
    mkdir -p ${MSVC_ARCHIVE_DIR} || return 1

    prep_sys_fs || return 1

    rm -rf ${MANIFEST_DIR}/* || return 1
    rm -rf ${SYS_DIR}/* || return 1

    sudo DEBIAN_FRONTEND=noninteractive apt-get install -y jq || return 1
    sudo DEBIAN_FRONTEND=noninteractive apt-get clean || return 1

    install_llvm || return 1
    install_sdk || return 1

    get_msvc_manifest || return 1

    find_msvc_package "Microsoft.VisualCpp.CRT.Headers" || return 1
    find_msvc_package "Microsoft.VisualCpp.CRT.x86.Desktop" || return 1
    find_msvc_package "Microsoft.VisualCpp.CRT.x64.Desktop" || return 1

    install_msvc_packages || return 1

    return 0
}

sys_install
