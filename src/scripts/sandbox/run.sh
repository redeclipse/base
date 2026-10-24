#!/usr/bin/env bash
# run.sh <case> [outdir]
# Loads <case>.cfg as the Map script of a scratch map, then runs launch.cfg as the user, which runs <case>.user.cfg
# if there is one and reports whether the Map script got out of the Sandbox. Prints the SANDBOX lines from the log.
# <case>.autoexec.cfg, if there is one, is added to the player's autoexec.cfg.
# <case>.next.cfg, if there is one, is the config of a second scratch map, sandboxnext, for the user config to load.
set -u
here="$(cd "$(dirname "$0")" && pwd)"; repo="$(cd "$here/../../.." && pwd)"
case=${1:?case}; out="$(mkdir -p "${2:-${TMPDIR:-/tmp}/sandbox-$case}" && cd "${2:-${TMPDIR:-/tmp}/sandbox-$case}" && pwd)"
[ -f "$here/$case.cfg" ] || { echo "no case $here/$case.cfg"; exit 1; }
home="$out/home"; log="$home/log.txt"; bin=redeclipse_linu
if pgrep -x $bin >/dev/null; then echo "another client is running"; exit 1; fi
rm -rf "${home:?}"; mkdir -p "$home/maps"
printf 'playername "tester"\nplayerloadweap "0 0"\n' > "$home/autoexec.cfg"
[ -f "$here/$case.autoexec.cfg" ] && cat "$here/$case.autoexec.cfg" >> "$home/autoexec.cfg"
cp "$repo/data/maps/challenges-port-01.mpz" "$home/maps/sandbox.mpz"
cp "$here/$case.cfg" "$home/maps/sandbox.cfg"
if [ -f "$here/$case.next.cfg" ]; then
    cp "$repo/data/maps/challenges-port-02.mpz" "$home/maps/sandboxnext.mpz"
    cp "$here/$case.next.cfg" "$home/maps/sandboxnext.cfg"
fi
usercfg=""; [ -f "$here/$case.user.cfg" ] && usercfg="$here/$case.user.cfg"
args=(-h"$home" -dw1280 -dh720 -df0 "-xsandboxuser = \"$usercfg\"; exec $here/launch.cfg")
"$repo/.claude/re-test.sh" launch "${args[@]}" >/dev/null
for _ in $(seq 120); do grep -q "SANDBOX-DONE" "$log" 2>/dev/null && break; pgrep -x $bin >/dev/null || break; sleep 0.5; done
for _ in $(seq 20); do pgrep -x $bin >/dev/null || break; sleep 0.5; done
"$repo/.claude/re-test.sh" quit >/dev/null 2>&1
grep -E "SANDBOX|Sandbox:|Cannot override|sandboxchat|sandboxpayload" "$log"
