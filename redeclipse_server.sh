#!/bin/sh
if [ -z "${REDECLIPSE_PATH+isset}" ]; then REDECLIPSE_PATH="$(cd "$(dirname "$0")" && pwd)"; fi
REDECLIPSE_BINARY="redeclipse_server"
REDECLIPSE_SOURCED="true"
. "${REDECLIPSE_PATH}/redeclipse.sh"
