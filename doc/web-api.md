# Web API

The **Web API**: HTTP/1.1, WebSockets and JSON, over TLS in both directions, on game servers, the master server and the client. The code is in `src/engine/httpnet.cpp` (the network thread), `src/engine/http.cpp` (routes, replies, requests and the CubeScript commands), `src/engine/json.cpp`, `src/game/webapi.h` (the game's routes, access and Event feed) and `src/engine/master.cpp` (the master's list, link and relay). "Differences from the plan" at the end lists where it departs from what was planned. For what web developers can call and what comes back, see `doc/web-api-guide.md`.

## Scope

In scope:

- **Client fetches**: news, the server list, map and asset downloads, account, achievement and stats APIs.
- **Server sends**: match results and stats posted to web backends, and webhooks such as Discord.
- **Server answers**: public status and player endpoints, plus moderation routes for tools and websites.
- **Event feed**: live match events over WebSockets.
- **Master server**: HTTPS endpoints, browser viewers for the website's live server list, and a `wss://` link for game servers alongside the text protocol, which stays for v1.6.
- **Relay**: the master carries each game server's Web API to websites, so servers without certificates are still reachable.
- **CubeScript**: requests, WebSockets, JSON, downloads and server routes from any script except **Map script**.

Out of scope: HTTP/2, and JSONP (CORS replaces it).

## Architecture

- **TLS**: mbedTLS 3.6 LTS, source vendored in `src/support/mbedtls/` with a trimmed config, built by our Makefile into every client and server. `MBEDTLS_SSL_VARIABLE_BUFFER_LENGTH` keeps idle connections small, and session tickets let returning browsers resume instead of doing a full handshake.
- **Connection layer**: one interface over plain TCP or TLS. The server checks the first byte of each connection (`0x16` is a TLS ClientHello), so HTTP and HTTPS share one port.
- **Network thread**: owns every socket and the TLS records, using `poll`/`WSAPoll` rather than ENet's `select`. A worker pool (`httphandshakethreads`, default 2, at least one) does handshakes and name lookups. Locked queues carry work to and from the main thread, and a loopback UDP socket wakes the thread when work arrives. Handlers and callbacks run only on the main thread, during `serverslice`. The threads are native (pthreads or Win32), as the dedicated server has no SDL, and mbedTLS is built thread safe the same way.
- **HTTP/1.1**: one parser for both directions, with keep-alive and pipelining (server), chunked transfer, `Expect: 100-continue`, redirects (client, up to 5, never from https to plain http, and dropping `Authorization`, `Cookie` and the pinned fingerprint when the host, port or scheme changes), gzip through zlib both ways, CORS (server), and proper status codes. A request with both `Transfer-Encoding` and `Content-Length` is refused, as that is how requests get smuggled past proxies. So is a header whose name isn't a token or whose value has control characters other than tabs, a `Content-Length` that isn't digits alone and a chunk size that isn't hex digits alone.
- **WebSockets**: our own framing and handshake for both directions (`ws://` and `wss://`), with pings, close codes, and fragmented messages reassembled up to the size cap. SHA-1 comes from mbedTLS.
- **JSON**: our own value type with a parser and writer on engine containers. There are no size caps, any value can be the top level, and nesting depth is limited. It replaces jsmn and `jsobj`.
- **Downloads**: small responses (≤1 MB) stay in memory. Larger ones stream to a temporary file in the home directory, with a size cap per request and progress the main thread can query. Resuming with Range headers comes later.
- **Outgoing certificate checks**: always on. Root certificates come from the system bundle on Linux, `CertOpenSystemStore("ROOT")` on Windows, or a Mozilla bundle shipped in `config/`, and `httpcafile` overrides all of these. A request can pin a certificate fingerprint, so a client can talk to a server's self-signed HTTPS when it already knows the fingerprint, for example from the master's list. The pinned certificate alone decides, so one that chains to a root nobody trusts still works.

### Updating mbedTLS

`src/support/mbedtls/` holds an unmodified mbedTLS 3.6 LTS release (3.6.7), apart from `include/mbedtls/mbedtls_config.h`. To update, download `mbedtls-3.6.x.tar.bz2` from the Mbed-TLS/mbedtls GitHub releases and check it against the release's sha256 file. Replace `include/mbedtls`, `include/psa`, `library/*.c`, `library/*.h` and `LICENSE` with the new release's, then carry over the config changes, which are the lines our copy comments out or enables compared with the release's own `mbedtls_config.h`. Bump the version above and run the three CI builds.

### C++ interface

All in `engine/http.h`:

- `http::newfetch(method, url, callback, ctx)` makes an `httpfetch`. Set `reqheaders`, `reqbody`, `maxsize`, `file` (stream to that path) or `pin` (a certificate's SHA-256), then `http::submit(f)` returns an id for `http::cancel` and `http::fetchprogress`. The callback runs on the main thread with `status`, `headers`, `body` and `error` filled in. Whoever owns a pending request cancels it in their own cleanup, for example on map change or disconnect.
- `http::addroute(method, pattern, minpriv, fun)` adds a route; `:name` matches one path segment and a final `*` the rest, read with `req.param()`. `req.var()` reads the query or a form body. A handler fills in the `httpresponse` (`res.json()`, `res.text()`, `res.error()`), or calls `res.defer()` and keeps the id from `http::deferred(req)` to answer later with `http::reply(id, res)`.
- `http::addsocket(pattern, minpriv, fun, ctx, maxmessage)` adds a route answered with a WebSocket. `http::wsopen(url, fun, ctx, pin)` opens one to elsewhere. Both report `WSEV_OPEN`, `WSEV_MESSAGE` and `WSEV_CLOSE` to `fun`, and `http::wssend`, `http::wsbroadcast` (encoded once for all) and `http::wsclose` take the connection id.
- `jsonwriter` builds JSON text without a tree, taking the engine's own string encoding; `json::parse` and `jsonvalue` read it.

Callbacks use function pointers and a `void *` context, not `std::function`, to match the codebase.

### CubeScript interface

Available to all trusted script: your own config and console, the shipped `config/` and UIs, `servinit.cfg`, and scripts the master sends. None of these commands is marked **Safe**, so the **Sandbox** that limits map script denies them to **Map script** with no extra checks. Everything goes through the same C++ client, with the same certificate checks and size caps.

- `httprequest <method> <url> [headers] [body] [callback] [pin]` runs in the background and returns a handle. `headers` is a list of names and values. The callback gets the status, the body, the headers as a list and an error as `$arg1` to `$arg4`. `httpcancel <handle>` cancels it and `httpprogress <handle>` gives the bytes so far and the total. A UI that is torn down cancels its handles.
- `httpdownload <url> <file> [callback]` writes only below the home directory. It refuses absolute paths, `..` and `temp/`. The callback gets the status, the file and an error.
- `wsopen <url> [onmessage] [onclose] [onopen] [pin]` returns a handle, and `wssend`/`wsclose` use it. `onmessage` gets the text, `onclose` the close code and any error.
- `jsonget <json> <path>` (for example `servers.3.name`), `jsontype`, `jsonkeys` and `jsonlength` read values, and `jsonstring`, `jsonobject` and `jsonarray` build JSON from lists with proper escaping. A value that is JSON already is kept as it is, so wrap anything else in `jsonstring` to be sure. None of them runs anything. Never `execute` remote content.
- `httproute <method> <path> <privilege> [body]` in `servinit.cfg` adds a route whose body's result is the reply, as JSON if it parses as JSON. Paths below `/api` and `/relay` are kept for the Web API and the relay. While it runs, `httpreqmethod`, `httpreqpath`, `httpreqaddr`, `httpreqprivilege`, `httpreqhandle`, `httpreqbody`, `httpreqvar` and `httpreqheader` describe the request, and `httpresstatus` (200 to 599) and `httpresheader` shape the reply. It follows the same Privilege rules as C++ routes and can't replace a built-in route. `delhttproute` removes one.
- Strings cross into scripts in the engine's own encoding and leave as UTF-8, so JSON text inside a script is ordinary script text.

## Access

- **Paths**: every route is below `/api/v1`, and the master's relay is below `/relay`, which leaves the rest of each server free for web pages. `GET /` only says where the API is, as `{"name": "Red Eclipse", "api": "/api/v1"}`. The paths below leave out `/api/v1`.
- Every request has a **Privilege**. An anonymous request is `none`, as an unauthenticated player is in the game; `player` needs an account, which the master's `u` flag gives.
- **API tokens**: `addhttptoken <name> <token> <privilege>` in `servinit.cfg`.
- **Challenge**: the client sends `POST /auth` with `name` (JSON or a form) and receives `id` and `challenge`. It then sends `POST /auth` with `id` and `answer` and receives a session `token` at the master-granted Privilege plus any **Local operator** standing. The challenge always goes through the master. Session tokens expire after `httpsessiontime` (default 30 min) without use, and are dropped when the server restarts. `genkey answer <privkey> <challenge>` prints the answer, for tools. Like the routes that need an account, `POST /auth` needs https, localhost, a trusted proxy or `httpallowinsecure`, as the session token it hands out works for whoever sees it.
- **WebSockets**: browsers can't send headers on a WebSocket, so a token can also be given as `?token=` on the upgrade or sent as `{"token": "..."}`. The challenge works there too: send `{"name": "..."}`, receive `{"event": "challenge", "id": ..., "challenge": ...}`, send `{"id": ..., "answer": "..."}` and receive `{"event": "auth", "privilege": ...}`. The socket keeps that standing while it stays open, with no session, but one let in with a token drops back to `none` when the token is cleared or its session expires. Signing in, by token or challenge, needs `wss://`, localhost, a trusted proxy or `httpallowinsecure`, as a signed in viewer is sent addresses; a feed over plain `ws://` from elsewhere stays Public.
- **Guessing**: an address that fails `httpauthfails` (default 10) tokens or challenges in a minute is told 429 until the minute is up. An address locked out like this for `httpauthban` (default 3) minutes within an hour gets a temporary ban, an ordinary **Control** entry lasting `bantimeout` (default 4 hours) that also keeps it out of the game, closes its Event feed sockets, and shows on the feed and in `GET /controls`, so moderators can lift it. `httpauthban 0` never bans, and addresses with an except or trust entry are never banned. Through the relay this counts the viewer's address, not the master's.
- Routes reuse the server's existing per-action minimum Privileges. Routes that need an account (above `none`) are refused over plain HTTP unless the request comes from localhost or a trusted proxy, or the operator allows it explicitly. A token sent that way isn't looked at, so even a public route such as `GET /players` answers it as anonymous and sends no addresses in the clear.
- **Trusted proxies**: `httptrustproxy` lists proxy addresses whose `X-Forwarded-For`/`X-Forwarded-Proto` headers are believed. The real client address then goes through the **Control** checks and rate limits.
- **Origins**: WebSocket upgrades that carry an `Origin` header are accepted only from `httpalloworigin`, and responses carry CORS headers only for those origins. Empty, the default, allows every origin, and the official master stays that way, as letting any site use the list, live list and relay from the browser is the point of the Web API. Non-browser clients send no `Origin` header and aren't affected.

## Game server Web API

- Game servers serve HTTP and HTTPS exactly as the master does. They run the same code; only the defaults below differ.
- On by default for dedicated servers and off for the client's local listen server.
- **Port**: tries `httpserverport` (28888), then the following ports up to `httpserverportrange` (default 16), and reports the port it got to the master. A range of 0 fixes the port.
- **Index**: `GET /api/v1` on game servers and the master names what it is (`name`, `version`, `type` of `server` or `master`) and lists the routes the caller's Privilege allows, read from the route table, so the master's own routes and script routes appear too. Each entry has a `path` and its `methods`, each with the Privilege it needs; a WebSocket's method is `WS`.
- **Routes**: `GET /status` (map with its title, author and crc, mode, players, time left, relay level), `GET /mapshot` (the map's preview from the server's own copy of its files, cacheable for a day when asked for by crc), `GET /players` (addresses only at `iphostlock` and above, moderator by default), `GET /teams`, the Event feed at `/events`, and `POST /auth`. Moderation: `GET /controls`, `POST /controls` (`cn`, `type` of kick, allow, ban, mute, limit or except, and `reason`), `DELETE /controls/<id>`, `POST /map` (`map`, `mode`, `mutators`, with `mutslockforce` added and demo playback refused; it changes the map without a vote, so it needs `httpmaplock`, moderator by default, and is still held to `votelock`, `modelock` and `mapslock` as a vote is) and `POST /say` (`text`, needing `httpsaylock`, player by default, and held to `messagelock`, mutes, the flood rule, `messagelength` and `censorwords` as in-game chat is). They need an account, at `player` and above, and then each checks the same minimum as in game, such as `kicklock` or `banlock`, and the Control entries it adds name the web user as their Source.
- **Certificates**: `httpcertfile`/`httpkeyfile` turn HTTPS on. The server checks both files' modification times about once a minute and reloads them when they change. Plain HTTP on the same port still works from localhost and trusted proxies.
- **Event feed**: only what any player in the match can already see, as JSON messages with an `event` and a `time`: `join`, `leave`, `team`, `frag` and `suicide` (with the weapon), `chat` and `action` (public chat only, including `POST /say` with a `web` sender, `httpfeedchat 0` turns it off), `map`, `score` (teams and players, compared once a second, so every mode's scoring is covered), and the game log's broadcast lines such as `match`, `duel`, `balance` and `servmsg`. Positions, health and loadouts are never published. `control` events, when Control entries change, go to viewers who may see that type of entry (`banlock` and so on), and carry the entry's address and mask for those at `iphostlock` and above.

## Master server Web API

- The master keeps its legacy text protocol on 28800, unchanged and for good, so v1.6 clients and servers keep working, and serves the Web API on exactly 28888. The port is fixed because the website relies on it.
- `GET /servers` is a cacheable list with an ETag (`If-None-Match` gets 304). It has the `version` the master hands out (`main` and `game`, as `setversion` did), `script` when `masterscriptclient` is set, and `servers`. Each entry has the host, port, priority, description, account handle and flags, branch, version, and, for servers on the link, the Web API port, relay level, certificate fingerprint and `status`.
- **Status**: what the query reply has, reported by each linked server over the link as `{"status": {...}}`. It is checked once a second and sent only when it changes. It holds `gameversion`, `map`, `modeid`, `mutators`, `maxplayers`, `mastermode` (password and private as the query reply works them out), `gamevars`, `gamemods`, `build` (major, minor, patch, platform, arch), `state`, `spectators`, `bots`, and `players`, each with `cn`, a plain `name`, `handle`, `team`, `privilege`, `local`, `spectator` and the `colour` the player chose. Spectators are listed, as the query reply lists them, and `local` and `spectator` are only there when true. The game client decorates each name as the query reply's are (`client::serverplayername`), and websites get the parts without markup; the chosen colour is sent rather than the one shown, as that falls back to the weapon's and would change with it. Times are unix end times, `ends` for the match (-1 without a limit) and `stateends` for the current state, so a countdown isn't a change; only a drift over two seconds is. The master parses it and writes it again before keeping it, and drops anything over 16 KB.
- **Polled status**: a listed server that reports no status, being older or on the text protocol, is asked the query the browser sends every `masterpollinterval` seconds (default 30, 0 never) on the master's ping socket, and its reply listed as `status` with `"polled": true`. The query's time is a random token, and a reply counts only when it echoes it and every string in it ends. The reply's layout hasn't changed since v1.6, so the master reads it without game knowledge and passes on `modeid`, `mutators`, `mastermode` and `state` as that version numbers them, with `gameversion` saying which; it strips the markup from player names and keeps their handles. There's no `stateends`, spectators or bots. `ends` is the time left plus now, -1 when the server says there's no limit (older servers send -1, newer ones 0 while playing), and is kept while the server isn't playing, as the time left doesn't count down then, or drifts by 2 seconds or less. The reply to the ping that confirms a listing is the first poll. Three unanswered polls in a row drop the status but not the listing, and a reported status replaces a polled one for good.
- Browser viewers fetch `/servers`, then open `wss://play.redeclipse.net:28888/api/v1/servers/live`. They get a `snapshot` first and then `add`, `update` and `remove` events; `remove` names the server by `host` and `port`. Each change is encoded once and shared by every viewer. Visitors on networks that block ports other than 80 and 443 fall back to polling `/servers`.
- Game servers try `wss://<servermaster>:28888/api/v1/link` first (`servermasterlink`, `servermasterlinkport`, and `servermasterlinktls` to use `ws://`). Each WebSocket message carries the same lines as the text protocol, so registration, auth challenges, Control pushes and pings all work as before, plus a `webapi` line with the server's Web API port, relay level and fingerprint. A link that carries an `Origin` header is refused, as only a browser sends one. If the link can't open, the server uses the text protocol on 28800 straight away and tries the link again after ten minutes. The legacy protocol isn't going away, as v1.6 clients and servers still use it.
- At startup the master raises its soft open-file limit to the hard limit, and warns if that is below `httpmaxconns`.
- The master host has no root, and a reverse proxy can't be installed there. The master handles TLS itself (see Certificates below).

## Server browser

- **List**: the client fetches `/servers` in the background, from `servermaster` on `servermasterlinkport`, over https, then plain http, then the text protocol's `update` on `servermasterport`. The way that works is kept for the session; "Refresh list", a new `servermaster`, or every way failing starts again from https, and `servermasterlinktls 0` starts at plain http. `serverlistfetching` is 1 while a fetch is under way.
- **Downgrades**: every failure falls back, including a certificate that doesn't check out, which is always reported. Anyone in the way could force the fallback by breaking the connection anyway, so the protection is that `script` only runs when the list came over https that checked out; a forged plain list can only show servers that are fake or missing. The text protocol's reply is read for `setversion`, `clearservers` and `addserver`, never run.
- **Live list**: while the browser is open it keeps `/servers/live` open (`wss://` or `ws://`, as the list was fetched), and closes it two seconds after the browser stops asking. If it can't open, the list is fetched again every `serverlistinterval` seconds (default 60, 0 only on request), and the socket is tried again no more often than that.
- **Status and pings**: rows with a status are filled from it, laid out as the query reply's attrs, so `getserver` and the menus read it unchanged, and they read "No ping" until a ping comes back. A polled status fills the row too, players by name alone, but those servers still get the full query, as they may ignore the ping only one and their own answer is fresher; the row keeps what it has when the query goes unanswered. Their queries send a `-1` after the client's time, which a server answers with only the echo; older servers ignore it and answer in full, and a player count is never negative, so the client tells the two apart. Every other server gets the full query, as before.
- **servers.cfg**: each server remembers where it came from (`addserver ... manual|master|lan`), and the file starts with `serverscfgversion 2`. The master's servers are kept as a cache for when it can't be reached, and a list that arrives replaces them, dropping the ones it no longer has. A file without the version line counts all its servers as the master's. Servers found on the local network aren't saved.
- **Details**: `serverdetails <n>` fetches a server's `/players` and `/teams`, directly over https pinned to its fingerprint (or plain http without a certificate, or when the list came over plain http, as its fingerprints can't be trusted then), then through the relay when it allows one, and again every five seconds while it's asked. It returns -1 when neither works, 1 while fetching and 2 when ready. `getserverdetail players|teams [index] [field]` reads them, `clearserverdetails` stops. The browser's Details button shows them as Server details.

## Relay

- **Addresses**: `https://play.redeclipse.net:28888/relay/<address>/<port>/<path>` asks that server for `/<path>` exactly, such as `/relay/1.2.3.4/28801/api/v1/players`, so the relay carries any version of the API and any web page a server serves. The address and port are separate segments because an IPv6 address has colons of its own. The Event feed is `wss://…/relay/<address>/<port>/api/v1/events`. Only servers holding a link to the master can be reached, each through its own link, so the relay never connects anywhere itself. Over the link, a request travels as `{"relay": "request", ...}` and comes back as `{"relay": "response", ...}`, with a body that isn't text, such as a PNG, sent after that head and a NUL in a binary frame instead of in the JSON; the feed is switched on only while someone watches.
- **Transport**: the game server's `wss://` link to the master (stage 8). Servers on the legacy protocol have no relay.
- **`httprelay`** (game server): 0 off, 1 public routes and Event feed, 2 everything (default), including API tokens, challenges and `POST /auth` (whose session token would otherwise cross the master), moderation, signing in on the Event feed and the events that needs, such as Control changes with addresses. The listing on the master shows each server's level.
- **Event feed**: the server sends each public event to the master once, and the master builds the frame once and fans it out to every viewer of that server. At level 2 a viewer can sign in as on a direct feed; the master forwards its messages, and the game server names which viewers get each event above Public, so the master delivers but never decides who sees what. At level 1 the master answers a sign-in attempt itself and forwards nothing.
- **Caching**: anonymous `GET` responses are cached per server and path for `httprelaycache` (default 2 s), or as long as the server's `Cache-Control` `max-age` allows up to `httprelaycachemax` (default an hour), within `httprelaycachesize` bytes in all (default 32 MB, oldest out first). Anything carrying a token or session is never cached.
- **Addresses and bans**: the master applies its global bans and per-address limits, then passes the viewer's address over the link. The game server treats the link as a trusted proxy and applies its own Control checks.
- **Security**: relayed requests count as secure only while the game server's link is `wss://`; with `servermasterlinktls 0` they don't, so tokens, `POST /auth`, moderation and signing in on the feed are refused through the relay and only public routes work. A token a browser sends the master over plain http or ws (other than from localhost) is never passed on, in a request or the feed's `?token=`, so the server sees it as anonymous, and a viewer on plain `ws://` can't sign in through the relay at all. What comes back keeps only `Content-Type`, `ETag`, `Cache-Control`, `Allow`, `WWW-Authenticate` and `Retry-After`, with a status from 200 to 599, and gets `X-Content-Type-Options: nosniff` and `Content-Security-Policy: sandbox allow-scripts allow-forms allow-popups`, so a server's pages can't set cookies or CORS for the master's origin or reach anything stored there.
- **Limits**: `httprelaymaxrate` caps each server's relayed bandwidth, counting requests, answers and relayed feed viewers' messages. Requests whose cache key would pass 512 characters aren't cached, and a path past 511 characters is refused with 414.
- **Privacy**: the master logs only server, path, status, size and viewer address. It never logs headers, bodies, query strings, tokens or challenge answers, and writes nothing relayed to disk.
- **The website** uses the relay for every server, so it has one code path. Direct HTTPS is for tools, bots and operators with their own certificates.

## Limits

| Variable | Game server | Master | Meaning |
|---|---|---|---|
| `httpmaxconns` | 256 | 8192 | Open connections in total |
| `httpmaxperaddr` | 16 | 16 | Open connections per address |
| `httpmaxheaders` | 64 | 64 | Headers per request |
| `httpmaxline` | 8 KB | 8 KB | Bytes per header line |
| `httpmaxbody` | 1 MB | 1 MB | Request body (routes may raise it for JSON uploads) |
| `httpmaxmessage` | 16 MB | 4 KB from viewers | WebSocket message received |
| `httpidletime` | 15 s | 15 s | HTTP idle timeout |
| `httppingtime` | 30 s | 30 s | WebSocket ping interval |
| `httphandshakerate` | 30 | 30 | TLS handshakes per address every ten seconds |
| `httpdefertime` | 30 s | 30 s | How long a deferred answer may take before 504 |
| `httprelaycache` | | 2 s | How long anonymous relayed GETs are kept |
| `httprelaycachemax` | | 3600 s | The longest a server's `Cache-Control` can keep one |
| `httprelaycachesize` | | 32 MB | Bytes the relay cache may hold |
| `httprelaymaxrate` | | 256 KB/s | Relayed bytes a second for each server |

`httpmaxconns` rises to 8192 by itself on a master. Viewers of `/servers/live` and relayed feeds may send at most 4 KB. A connection whose unsent output passes `httpmaxmessage` plus 1 MB is dropped, so a slow reader can't hold memory. Addresses on the ban list are refused at their first request. `httpallowinsecure` allows routes that need an account over plain HTTP from anywhere, and `httpverbose` logs requests (1) and also refused connections and failed handshakes (2).

Measured on a local master: an idle `ws://` viewer costs about 0.6 KB, and an idle `wss://` viewer about 39 KB, which is mbedTLS's 16 KB input and output buffers. 8192 TLS viewers would need about 320 MB.

## Stages

All ten are in, one commit per problem, and every commit builds in CI with no pipeline changes.

1. Vendor mbedTLS into `src/support/mbedtls/` and build it into the client and server from the Makefile, for Linux, mingw and MSVC.
2. The network thread and connection layer, plain and TLS, with `poll`/`WSAPoll`.
3. The HTTP/1.1 parser, client and server. Remove the old code, the debug page and `httptest`.
4. JSON.
5. WebSockets.
6. Access: Privilege per request, API tokens, challenge and session tokens, trusted proxies, origins.
7. Game server routes, the Event feed, and port selection reported to the master.
8. Master: `/servers`, browser viewers, and the `wss://` link for game servers, with the legacy protocol kept.
9. Relay: `/relay/<address>/<port>/…` (first `/servers/<address>:<port>/…`) over the link, `httprelay` levels, Event feed fan-out, the anonymous cache, passing on viewer addresses, and the per-server rate cap.
10. CubeScript: `httprequest`, `httpdownload`, `ws*`, `json*` and `httproute`, none marked Safe.

## Testing

- `src/scripts/webapi/run.sh` starts a local dedicated server with a self-signed certificate and an API token, then checks routes, script routes, access rules (no token, a bad token, plain HTTP from another address), HTTPS and TLS 1.2 on the same port, CORS, gzip, the parser (`parser.py` throws malformed and awkward requests at it) and the Event feed (`ws.py`, a small WebSocket client).
- `src/scripts/sandbox/run.sh a15-webapi`: Map script starting requests, downloads, sockets and routes is denied and logged, so their callbacks can't later run as the user.
- The server browser, by hand: a local master with a linked server, a server on the text protocol and a linked server with its Web API off. A scripted client fetched the list over each way (https with the master's script, an untrusted certificate refused, plain http, and the text protocol when the Web API port is dead, replacing cached servers), saw status rows and ping only replies, got a join on the live list within a second, and fetched details directly, through the relay, and from neither.
- By hand, so far: a scripted client using `httprequest`, `httpdownload`, `wsopen`, pins and the `json*` commands against a local server and the internet; a local master with a game server on the link and on the fallback; the challenge with an account from `addauth` and `genkey answer`; relayed requests and a relayed Event feed at each `httprelay` level; 4000 `ws://` and 2000 `wss://` viewers on a master; and the Windows build under Wine (HTTPS, the system certificate store, the link and the relay).

## Operator guide

For a game server operator's walkthrough of HTTPS, self-signed certificates and `acme.sh`, see `doc/server-https.md`.

### Certificates

The server only reads `httpcertfile`/`httpkeyfile` and reloads them when they change, so any source of certificates works. Use an ECDSA certificate, which makes handshakes much cheaper than RSA. Without root, Let's Encrypt's HTTP-01 check (port 80) isn't available, so use DNS-01 through `acme.sh`. For example, for the official master on name.com DNS:

```
export Namecom_Username=<name.com user>
export Namecom_Token=<name.com API token>
acme.sh --issue --dns dns_namecom -d play.redeclipse.net --keylength ec-256
acme.sh --install-cert -d play.redeclipse.net --ecc --fullchain-file <dir>/fullchain.pem --key-file <dir>/key.pem
```

`acme.sh` stores the credentials in `~/.acme.sh/account.conf` and installs its own crontab entry for renewals. To test without Let's Encrypt's rate limits, add `--staging` (browsers won't trust the result).

### Reverse proxies

An operator who prefers to use nginx or Caddy for TLS can leave the certificate variables empty and list the proxy in `httptrustproxy`.

### Without a certificate

A server that doesn't set a certificate is still reachable from websites through the master's relay. `httprelay 1` relays public routes and the Event feed. `httprelay 2` (the default) also relays moderation, which means your API tokens and challenges pass through play.redeclipse.net in plain text (the master doesn't log or store them). `httprelay 0` turns the relay off.

### API tokens

`addhttptoken <name> <token> <privilege>` in `servinit.cfg`, with a privilege name such as `moderator` or the letters `addauth` uses, and a token of at least 16 characters. A token carries that Privilege on every request that presents it, so treat it like a password. `clearhttptokens` removes them all; put it before the tokens, since a rehash re-adds them but keeps any you've taken out of the file.

## Differences from the plan

- The client sends `Connection: close` and doesn't pool connections; the server side does keep-alive.
- Banned addresses are refused at their first request, not when they connect, as the ban list lives on the main thread.
- The master's own browser routes and the relay use one fixed set of headers each way, rather than passing headers through.
- Routes moved below `/api/v1`, and the relay to `/relay/<address>/<port>/<path>` passing the path on as it is, before anything shipped, so web pages can be added later and the API can change version.
