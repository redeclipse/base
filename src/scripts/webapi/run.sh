#!/usr/bin/env bash
# run.sh [outdir]: starts a local dedicated server with a self-signed certificate and an API token,
# then checks routes, access rules, HTTPS on the same port, the parser and the Event feed. Prints PASS or FAIL lines.
set -u
here="$(cd "$(dirname "$0")" && pwd)"; repo="$(cd "$here/../../.." && pwd)"
out="$(mkdir -p "${1:-${TMPDIR:-/tmp}/webapi-test}" && cd "${1:-${TMPDIR:-/tmp}/webapi-test}" && pwd)"
home="$out/home"; port=28960; web=28961; token=0123456789abcdef0123; member=fedcba9876543210fedc
rm -rf "${home:?}"; mkdir -p "$home"
openssl req -x509 -newkey ec -pkeyopt ec_paramgen_curve:prime256v1 -nodes -keyout "$home/key.pem" -out "$home/cert.pem" \
    -days 2 -subj "/CN=localhost" -addext "subjectAltName=DNS:localhost,IP:127.0.0.1" 2>/dev/null
cat > "$home/servinit.cfg" <<CFG
servermaster ""
httpserverport $web
httpserverportrange 0
httpcertfile "$home/cert.pem"
httpkeyfile "$home/key.pem"
addhttptoken tester $token administrator
addhttptoken member $member player
addexcept 127.0.0.1
httpauthban 1
httproute GET /api/nope none [ result "squatting" ]
httproute GET /members player [ result "welcome" ]
httproute GET /hello none [ concatword "{^"greeting^":^"hi" (httpreqvar who) "^"}" ]
CFG
"$repo/redeclipse_server.sh" -h"$home" -sp$port > "$home/out.log" 2>&1 &
pid=$!
trap 'kill -9 $pid 2>/dev/null' EXIT
for _ in $(seq 40); do grep -q "HTTP server started" "$home/out.log" && break; sleep 0.25; done
url=http://127.0.0.1:$web
api=$url/api/v1
check() { if [ "$2" = "$3" ]; then echo "PASS $1"; else echo "FAIL $1: got '$2', wanted '$3'"; fi; }
check "root" "$(curl -s $url/)" '{"name":"Red Eclipse","api":"/api/v1"}'
check "status" "$(curl -s -o /dev/null -w '%{http_code}' $api/status)" 200
check "index" "$(curl -s $api | grep -o '"type":"server"')" '"type":"server"'
check "index hides moderation" "$(curl -s $api | grep -c '/controls')" 0
check "index with a token" "$(curl -s -H "Authorization: Bearer $token" $api | grep -o '"path":"/api/v1/controls","methods":{"GET":"player","POST":"player"}')" '"path":"/api/v1/controls","methods":{"GET":"player","POST":"player"}'
check "index has script routes" "$(curl -s $api | grep -c '"path":"/hello"')" 1
check "script routes keep out of /api" "$(curl -s -o /dev/null -w '%{http_code}' $url/api/nope)" 404
check "players" "$(curl -s $api/players)" "[]"
check "script route" "$(curl -s "$url/hello?who=you")" '{"greeting":"hiyou"}'
check "player route needs an account" "$(curl -s -o /dev/null -w '%{http_code}' $url/members)" 401
check "index hides player routes" "$(curl -s $api | grep -c '"path":"/members"')" 0
check "not found" "$(curl -s -o /dev/null -w '%{http_code}' $url/nope)" 404
check "wrong method" "$(curl -s -o /dev/null -w '%{http_code}' -X DELETE $url/)" 405
check "needs a token" "$(curl -s -o /dev/null -w '%{http_code}' $api/controls)" 401
check "token" "$(curl -s -H "Authorization: Bearer $token" $api/controls)" '[{"id":1,"type":"except","scope":"local","source":"server","ip":"127.0.0.1"}]'
check "a player may chat" "$(curl -s -o /dev/null -w '%{http_code}' -H "Authorization: Bearer $member" -d '{"text":"hello"}' $api/say)" 200
check "a player sees no controls" "$(curl -s -H "Authorization: Bearer $member" $api/controls)" "[]"
check "a player may not change the map" "$(curl -s -o /dev/null -w '%{http_code}' -H "Authorization: Bearer $member" -d '{"map":"bath"}' $api/map)" 403
check "chat needs some text" "$(curl -s -o /dev/null -w '%{http_code}' -H "Authorization: Bearer $member" -d '{"text":""}' $api/say)" 400
flood=""; for _ in 1 2 3 4 5 6; do flood="$flood$(curl -s -o /dev/null -w '%{http_code} ' -H "Authorization: Bearer $member" -d '{"text":"spam"}' $api/say)"; done
check "chat floods like in game" "$(echo "$flood" | grep -o 429 | head -1)" 429
check "bad token" "$(curl -s -o /dev/null -w '%{http_code}' -H "Authorization: Bearer nope" $api/controls)" 403
check "https" "$(curl -s -o /dev/null -w '%{http_code}' --cacert "$home/cert.pem" https://localhost:$web/)" 200
check "tls 1.2" "$(curl -s -o /dev/null -w '%{http_code}' --tls-max 1.2 --cacert "$home/cert.pem" https://localhost:$web/)" 200
check "preflight" "$(curl -s -o /dev/null -w '%{http_code}' -X OPTIONS -H 'Origin: https://example.org' $url/)" 204
check "gzip" "$(curl -s -o /dev/null -w '%{http_code}' --compressed $url/)" 200
lan=$(ip -4 route get 1.1.1.1 2>/dev/null | grep -o 'src [0-9.]*' | cut -d' ' -f2)
if [ -n "$lan" ]; then
    check "plain http from afar" "$(curl -s -o /dev/null -w '%{http_code}' -H "Authorization: Bearer $token" http://$lan:$web/api/v1/controls)" 403
    check "a token in the clear is ignored" "$(curl -s -H "Authorization: Bearer $token" http://$lan:$web/api/v1/status | grep -c '"you"')" 0
    check "a token over https from afar" "$(curl -s --resolve localhost:$web:$lan --cacert "$home/cert.pem" -H "Authorization: Bearer $token" https://localhost:$web/api/v1/status | grep -o '"you":"[a-z]*"')" '"you":"administrator"'
fi
python3 -I "$here/parser.py" $web | sed 's/^/PARSER /'
feed="$(python3 -I "$here/ws.py" ws://127.0.0.1:$web/api/v1/events $token 1)"
check "feed upgrade" "$(echo "$feed" | head -1)" "HTTP/1.1 101 Switching Protocols"
check "feed auth" "$(echo "$feed" | grep -c '"privilege":"administrator"')" 1
if [ -n "$lan" ]; then
    python3 -I "$here/ws.py" ws://127.0.0.1:$web/api/v1/events $token 3 > "$home/admin.log" &
    p1=$!
    python3 -I "$here/ws.py" ws://$lan:$web/api/v1/events "" 3 > "$home/viewer.log" &
    p2=$!
    sleep 1
    # over https, as a token in the clear is never looked at
    for _ in $(seq 10); do curl -s -o /dev/null --resolve localhost:$web:$lan --cacert "$home/cert.pem" -H "Authorization: Bearer guess$RANDOM$RANDOM" https://localhost:$web/api/v1/controls; done
    wait $p1 $p2
    check "banned for guessing" "$(curl -s -o /dev/null -w '%{http_code}' http://$lan:$web/)" 403
    check "ban closes the feed" "$(grep -c '^op8' "$home/viewer.log")" 1
    check "ban on the feed" "$(grep -c '"type":"ban".*"reason":"too many failed sign-in attempts"' "$home/admin.log")" 1
    check "except is never banned" "$(curl -s -o /dev/null -w '%{http_code}' $url/)" 200
fi
for _ in $(seq 10); do curl -s -o /dev/null -H "Authorization: Bearer guess$RANDOM$RANDOM" $api/controls; done
check "guessing tokens" "$(curl -s -o /dev/null -w '%{http_code}' -H "Authorization: Bearer $token" $api/controls)" 429
check "server alive" "$(kill -0 $pid 2>/dev/null && echo yes)" yes
