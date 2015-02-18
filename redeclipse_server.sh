#!/bin/sh
if [ -z "${REDECLIPSE_PATH+isset}" ]; then REDECLIPSE_PATH="$(cd "$(dirname "$0")" && pwd)"; fi
REDECLIPSE_BINARY="redeclipse_server"
BEING_SOURCED=1
. "${REDECLIPSE_PATH}/redeclipse.sh"
