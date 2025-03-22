#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
DEPLOY_REPO_DIR=$DIR/deploy

NEEDS_CLONE=0

if [ ! -d "${DEPLOY_REPO_DIR}" ]; then
    echo "Deploy repo not found, cloning"
    NEEDS_CLONE=1
else
    cd "${DEPLOY_REPO_DIR}"
    if [ -z "$(git remote -v | grep 'https://github.com/redeclipse/deploy.git')" ]; then
        echo "Deploy repo invalid, re-cloning"
        NEEDS_CLONE=1
    fi
fi

if [ $NEEDS_CLONE -eq 1 ]; then
    echo "Cloning deploy repo"
    rm -rf "${DEPLOY_REPO_DIR}"
    git clone --single-branch -b master --depth 1 https://github.com/redeclipse/deploy.git "${DEPLOY_REPO_DIR}"
else
    echo "Updating deploy repo"
    cd "${DEPLOY_REPO_DIR}"
    git fetch origin --depth 1 && git clean -xfd && git reset --hard origin/master
fi

cd "${DIR}/../.."
tar xf "${DEPLOY_REPO_DIR}/master/linux.tar.gz"
