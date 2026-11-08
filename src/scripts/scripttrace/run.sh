#!/usr/bin/env bash
# run.sh [outdir] [server|client]
# Makes CubeScript errors from every script origin, in a dedicated server and in the client, and checks that each is
# followed by the script trace expected.txt gives for it. Uses the installed binaries in bin/amd64.
set -u
here="$(cd "$(dirname "$0")" && pwd)"; repo="$(cd "$here/../../.." && pwd)"
out="$(mkdir -p "${1:-${TMPDIR:-/tmp}/scripttrace}" && cd "${1:-${TMPDIR:-/tmp}/scripttrace}" && pwd)"; only=${2:-}
bin=redeclipse_linu; fails=0

sethome()
{
    rm -rf "${1:?}"; mkdir -p "$1/scripttrace" "$1/maps"
    cp "$here"/*.cfg "$1/scripttrace/"
    printf 'playername "tester"\nplayerloadweap "0 0"\n' > "$1/autoexec.cfg"
    printf 'servermaster ""\n' > "$1/servinit.cfg"
}

# each expected error line must be followed by its trace line
check()
{
    local where=$1 log=$2
    sed -E 's/^[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9:.]+ //' "$log" > "$out/$where.txt"
    while IFS='|' read -r w msg trace; do
        [ "$w" = "$where" ] || continue
        if grep -Fx -A1 -- "$msg" "$out/$where.txt" | grep -Fxq -- "$trace"; then echo "PASS $where: $msg"
        else echo "FAIL $where: $msg"; echo "     expected: $trace"; grep -Fx -A1 -- "$msg" "$out/$where.txt" | sed 's/^/     got: /' | head -2; fails=$((fails+1)); fi
    done < <(grep -v '^#' "$here/expected.txt")
    if grep -q "nosuchquiet" "$out/$where.txt"; then echo "FAIL $where: nodebug printed an error"; fails=$((fails+1)); fi
}

if [ "$only" != client ]; then
    home="$out/server"; sethome "$home"
    cd "$repo"
    LD_LIBRARY_PATH=bin/amd64 bin/amd64/redeclipse_server_linux -h"$home" "-xexec scripttrace/server.cfg" > "$out/server-stdout.txt" 2>&1 &
    pid=$!
    for _ in $(seq 40); do grep -q "SCRIPTTRACE-DONE" "$out/server-stdout.txt" 2>/dev/null && break; sleep 0.5; done
    kill -9 $pid 2>/dev/null; wait $pid 2>/dev/null
    check server "$out/server-stdout.txt"
fi

if [ "$only" != server ]; then
    if pgrep -x $bin >/dev/null; then echo "another client is running"; exit 1; fi
    home="$out/client"; sethome "$home"
    cp "$repo/data/maps/challenges-port-01.mpz" "$home/maps/scripttrace.mpz"
    cp "$here/map.cfg" "$home/maps/scripttrace.cfg"
    "$repo/.claude/re-test.sh" launch -h"$home" -dw1280 -dh720 -df0 "-xexec scripttrace/client.cfg" >/dev/null
    for _ in $(seq 60); do grep -q "SCRIPTTRACE-DONE" "$home/log.txt" 2>/dev/null && break; pgrep -x $bin >/dev/null || break; sleep 0.5; done
    for _ in $(seq 20); do pgrep -x $bin >/dev/null || break; sleep 0.5; done
    "$repo/.claude/re-test.sh" quit >/dev/null 2>&1
    check client "$home/log.txt"
fi

echo "$fails failed"
[ $fails -eq 0 ]
