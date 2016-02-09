#!/bin/sh
if [ -z "${REDECLIPSE_PATH+isset}" ]; then REDECLIPSE_PATH="$(cd "$(dirname "$0")" && pwd)"; fi
REDECLIPSE_BINARY="redeclipse_server"
export REDECLIPSE_BINARY
REDECLIPSE_CALLED="true" . "${REDECLIPSE_PATH}/redeclipse.sh"
