#!/bin/bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
STATE="$ROOT/.csgopen"
fail() { echo "CSGOpen: $*" >&2; exit 1; }

prerequisites() {
    [[ $(uname -s) == Darwin ]] || fail "questo script richiede macOS"
    for tool in xcrun clang clang++ make git pkg-config brew; do
        command -v "$tool" >/dev/null || fail "manca $tool (installare Xcode e Homebrew)"
    done
    xcrun --show-sdk-path >/dev/null || fail "SDK macOS non disponibile"
    ARCH=$(uname -m)
    case "$ARCH" in arm64|x86_64) ;; *) fail "architettura non supportata: $ARCH" ;; esac
    [[ $(clang -dumpmachine) == "$ARCH"-apple-darwin* ]] || fail "compilatore non nativo per $ARCH"
    local alprefix
    alprefix=$(HOMEBREW_NO_AUTO_UPDATE=1 brew --prefix openal-soft) || fail "manca openal-soft"
    export PKG_CONFIG_PATH="$alprefix/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
    pkg-config --exists sdl2 SDL2_image openal sndfile zlib || fail "dipendenze mancanti: brew install pkgconf sdl2 sdl2_image openal-soft libsndfile"
    local package libdir library
    for package in sdl2 SDL2_image openal sndfile; do
        libdir=$(pkg-config --variable=libdir "$package")
        case "$package" in
            sdl2) library=libSDL2.dylib ;;
            SDL2_image) library=libSDL2_image.dylib ;;
            openal) library=libopenal.dylib ;;
            sndfile) library=libsndfile.dylib ;;
        esac
        lipo -verify_arch "$ARCH" "$libdir/$library" || fail "$package non contiene $ARCH: $libdir/$library"
    done
}

content() {
    local status
    status=$(git -C "$ROOT" submodule status --recursive)
    if printf '%s\n' "$status" | /usr/bin/grep -Eq '^[-+U]'; then
        fail "submodule non allineati: eseguire git submodule update --init --recursive (senza --remote)"
    fi
    [[ -f "$ROOT/data/maps/readme.txt" ]] || fail "asset mancanti"
}

binary() {
    local path="$ROOT/src/$1"
    [[ -x "$path" ]] || fail "binario mancante: eseguire scripts/csgopen/dev.sh build"
    lipo -verify_arch "$ARCH" "$path" || fail "binario non nativo: $path"
}

profile() {
    RUNTIME="$STATE/$1"
    mkdir -p "$RUNTIME" "$STATE/logs"
    export REDECLIPSE_HOME="$RUNTIME"
    # Avoid inherited installation/content overrides from an unrelated launcher.
    unset REDECLIPSE_DATADIR REDECLIPSE_EXTRADIRS REDECLIPSE_PATH
    cd "$ROOT"
}

testmap() {
    MAP=echo
    if [[ $# -gt 0 && "$1" != -* ]]; then MAP=$1; fi
    [[ "$MAP" =~ ^[A-Za-z0-9_-]+$ ]] || fail "nome mappa non valido: $MAP"
    [[ -f "$ROOT/data/maps/$MAP.mpz" ]] || fail "mappa mancante: $MAP"
}

tdmprofile() {
    cat > "$RUNTIME/localinit.cfg" <<EOF
exec "config/csgopen/tdm.cfg"
sv_defaultmap "maps/$MAP"
servermaster ""
serverlanport 0
httpserver 0
EOF
    cat > "$RUNTIME/autoexec.cfg" <<'EOF'
exec "config/csgopen/client.cfg"
EOF
}

command=${1:-help}
if [[ $# -gt 0 ]]; then shift; fi
case "$command" in
    check)
        prerequisites
        uname -m
        sw_vers
        xcode-select -p
        clang --version
        pkg-config --modversion sdl2 SDL2_image openal sndfile zlib
        echo "Prerequisiti nativi disponibili."
        ;;
    build)
        prerequisites
        content
        mkdir -p "$STATE/logs"
        jobs=${CSGOPEN_JOBS:-4}
        [[ "$jobs" =~ ^[1-9][0-9]*$ ]] || fail "CSGOPEN_JOBS deve essere un intero positivo"
        make -C "$ROOT/src" -j"$jobs" client server 2>&1 | tee "$STATE/logs/build.log"
        binary redeclipse_native
        binary redeclipse_server_native
        ;;
    original)
        prerequisites
        content
        binary redeclipse_native
        profile original
        exec "$ROOT/src/redeclipse_native" "-h$RUNTIME" "-g$STATE/logs/original.log" -sm -ss0 -dw1280 -dh720 -df0 "$@"
        ;;
    tdm)
        prerequisites
        content
        binary redeclipse_native
        testmap "$@"
        if [[ $# -gt 0 && "$1" != -* ]]; then shift; fi
        profile csgopen-client
        tdmprofile
        exec "$ROOT/src/redeclipse_native" "-h$RUNTIME" "-g$STATE/logs/tdm-client.log" -bconfig/csgopen/branding.cfg -sm -ss0 -dw1280 -dh720 -df0 "-xtdm $MAP" "$@"
        ;;
    server)
        prerequisites
        content
        binary redeclipse_server_native
        testmap "$@"
        if [[ $# -gt 0 && "$1" != -* ]]; then shift; fi
        port=${CSGOPEN_PORT:-28801}
        [[ "$port" =~ ^[0-9]{1,5}$ ]] || fail "CSGOPEN_PORT non valido"
        port=$((10#$port))
        [[ "$port" -ge 1024 && "$port" -le 65534 ]] || fail "CSGOPEN_PORT deve essere tra 1024 e 65534"
        profile server
        cat > "$RUNTIME/servinit.cfg" <<EOF
exec "config/csgopen/tdm.cfg"
sv_defaultmap "maps/$MAP"
serverip "127.0.0.1"
serverport $port
serverlanport 0
servermaster ""
masterserver 0
httpserver 0
EOF
        echo "Server locale: nella console client usare connect 127.0.0.1 $port"
        exec "$ROOT/src/redeclipse_server_native" "-h$RUNTIME" "-g$STATE/logs/server.log" -si127.0.0.1 -sm -ss1 "-sp$port" "$@"
        ;;
    *)
        echo "Uso: $0 {check|build|original [opzioni]|tdm [mappa] [opzioni]|server [mappa] [opzioni]}"
        [[ "$command" == help ]] || exit 1
        ;;
esac
