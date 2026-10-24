#!/usr/bin/env bash
# allmaps.sh [outdir]
# Loads every shipped map in turn and prints what the Sandbox denied their Map script; a clean run prints nothing else.
set -u
here="$(cd "$(dirname "$0")" && pwd)"; repo="$(cd "$here/../../.." && pwd)"
out="$(mkdir -p "${1:-${TMPDIR:-/tmp}/sandbox-allmaps}" && cd "${1:-${TMPDIR:-/tmp}/sandbox-allmaps}" && pwd)"
home="$out/home"; log="$home/log.txt"; bin=redeclipse_linu
if pgrep -x $bin >/dev/null; then echo "another client is running"; exit 1; fi
rm -rf "${home:?}"; mkdir -p "$home"
printf 'playername "tester"\nplayerloadweap "0 0"\n' > "$home/autoexec.cfg"
maps=$(cd "$repo/data/maps" && ls *.mpz | sed 's/\.mpz$//' | tr '\n' ' ')
"$repo/.claude/re-test.sh" launch -h"$home" -dw1280 -dh720 -df0 "-xsandboxmaps = [$maps]; exec $here/allmaps.cfg" >/dev/null
for _ in $(seq 600); do grep -q "SANDBOX-DONE" "$log" 2>/dev/null && break; pgrep -x $bin >/dev/null || break; sleep 1; done
for _ in $(seq 20); do pgrep -x $bin >/dev/null || break; sleep 0.5; done
"$repo/.claude/re-test.sh" quit >/dev/null 2>&1
grep -E "SANDBOX-MAP|Sandbox:|Not loading|SANDBOX-DONE" "$log" | cut -c21-
