#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
case "$(uname -s)" in
    Darwin)
        PROFILE=${ECLIPSE_RECOIL_HOME:-"$HOME/Library/Application Support/Eclipse Recoil"}
        BINARY="$ROOT/../../MacOS/eclipse-recoil-bin"
        ;;
    Linux)
        PROFILE=${ECLIPSE_RECOIL_HOME:-"${XDG_DATA_HOME:-$HOME/.local/share}/eclipse-recoil"}
        BINARY="$ROOT/bin/eclipse-recoil"
        export LD_LIBRARY_PATH="$ROOT/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
        ;;
    *) echo "Unsupported platform" >&2; exit 1 ;;
esac

mkdir -p "$PROFILE"
cat > "$PROFILE/localinit.cfg" <<'EOF'
exec "config/csgopen/tdm.cfg"
sv_defaultmap "maps/echo"
servermaster ""
serverlanport 0
httpserver 0
EOF
cat > "$PROFILE/autoexec.cfg" <<'EOF'
exec "config/csgopen/client.cfg"
EOF

# Keep the release independent of any other Red Eclipse installation.
unset REDECLIPSE_DATADIR REDECLIPSE_EXTRADIRS REDECLIPSE_PATH
export REDECLIPSE_HOME="$PROFILE"
cd "$ROOT"
exec "$BINARY" "-h$PROFILE" "-g$PROFILE/game.log" \
    -bconfig/csgopen/branding.cfg -sm -ss0 "-xtdm echo" "$@"
