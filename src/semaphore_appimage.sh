#! /bin/bash

SEMABUILD_APT='DEBIAN_FRONTEND=noninteractive apt-get'

semabuild_appimage() {
    git clone https://github.com/red-eclipse/appimage-builder.git

    pushd appimage-builder
    export BRANCH=${BRANCH_NAME}
    # Build parameters.
    export ARCH=x86_64
    export COMMIT=$(git rev-parse HEAD)
    export BUILD_SERVER=1
    export BUILD_CLIENT=1
    # Build the appimages.
    bash build-with-docker.sh || exit 1
    # Release parameters.
    export GITHUB_TOKEN=${APPIMAGE_GITHUB_TOKEN}
    # Release the appimages
    bash github-release.sh || exit 1
    popd
    return 0
}

if [ "${BRANCH_NAME}" = master ] || [ "${BRANCH_NAME}" = stable ]; then
    # Install jq for Github API and zsync for AppImage update information.
    sudo ${SEMABUILD_APT} -fy install jq zsync || exit 1
    semabuild_appimage || exit 1
fi
