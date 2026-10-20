#!/usr/bin/env bash
# drive.sh <checkout> <outdir> [mode]
# Runs test.cfg in the client built in <checkout>, with a fresh home in <outdir>/home.
# mode 0: counters only; 1: perf record per scene (PERFCG and PERFSEC override the call graph and length); 2: perf stat per scene
set -u
here="$(cd "$(dirname "$0")" && pwd)"
repo="$(cd "${1:?checkout}" && pwd)"; out="$(mkdir -p "${2:?outdir}" && cd "$2" && pwd)"; mode=${3:-2}
home="$out/home"; log="$home/log.txt"; bin=redeclipse_linu
if pgrep -x $bin >/dev/null; then echo "another client is running, which would skew the numbers"; exit 1; fi
rm -rf "${home:?}"; mkdir -p "$home"
printf 'playername "tester"\nplayerloadweap "0 0"\n' > "$home/autoexec.cfg"
args=(-h"$home" -dw1600 -dh900 -df0 "-xexec $here/test.cfg")
if [ -x "$repo/.claude/re-test.sh" ]; then "$repo/.claude/re-test.sh" launch "${args[@]}"
else (cd "$repo" && nohup ./redeclipse.sh "${args[@]}" > "$out/stdout.txt" 2>&1 &); fi
for _ in $(seq 60); do pid=$(pgrep -x $bin | head -1); [ -n "$pid" ] && break; sleep 0.5; done
[ -n "${pid:-}" ] || { echo "the client did not start"; exit 1; }
echo "pid $pid"
for scene in menu player maps match; do
    for _ in $(seq 240); do grep -q "CSPERF-START $scene" "$log" 2>/dev/null && break; sleep 0.5; done
    echo "scene $scene"
    case $mode in
        1) perf record -q -F 999 ${PERFCG:--g} -p "$pid" -o "$out/$scene.data" -- sleep "${PERFSEC:-10}" 2>/dev/null ;;
        2) perf stat --per-thread -x, -e instructions:u,cycles:u -p "$pid" -o "$out/$scene.stat" -- sleep 10 2>/dev/null ;;
    esac
done
# the client quits by itself; a client still running after a minute is hung, usually in a crash on exit
for _ in $(seq 120); do pgrep -x $bin >/dev/null || break; sleep 0.5; done
if pgrep -x $bin >/dev/null; then echo "EXIT: the client did not quit by itself"; pkill -9 -x $bin; else echo "EXIT: clean"; fi
[ -x "$repo/.claude/re-test.sh" ] && "$repo/.claude/re-test.sh" quit >/dev/null 2>&1
for f in /tmp/re-test.log "$out/stdout.txt"; do [ -f "$f" ] && grep -E "Fatal|double free" "$f" | sed 's/^/EXIT: /'; done
grep -oE "(CSPERF|CSFPS|CSIDENTS|CSLIST|CSOPS) .*" "$log" > "$out/report.txt"
cat "$out/report.txt"
