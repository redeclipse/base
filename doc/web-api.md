# Web API

The plan for replacing `src/engine/http.cpp` and `http.h` with a **Web API**: HTTP/1.1, WebSockets and JSON, over TLS in both directions, on game servers, the master server and the client. This is future work. Names of variables, commands and routes below are planned names, so check the source once a stage has landed.

## Scope

In scope:

- **Client fetches**: news, the server list, map and asset downloads, account, achievement and stats APIs.
- **Server sends**: match results and stats posted to web backends, and webhooks such as Discord.
- **Server answers**: public status and player endpoints, plus moderation routes for tools and websites.
- **Event feed**: live match events over WebSockets.
- **Master server**: HTTPS endpoints, browser viewers for the website's live server list, and a `wss://` link for game servers that replaces the text protocol over time.
- **Relay**: the master carries each game server's Web API to websites, so servers without certificates are still reachable.
- **CubeScript**: requests, WebSockets, JSON, downloads and server routes from any script except **Map script**.

Out of scope: HTTP/2, and JSONP (CORS replaces it).

The current code has no users apart from its debug `/` page and `httptest`, which fetches from `hq.redeclipse.net`, a host that no longer exists. Both are removed, and nothing old needs to stay compatible.

## Architecture

- **TLS**: mbedTLS 3.6 LTS, source vendored in `src/support/mbedtls/` with a trimmed config, built by our Makefile. `WANT_TLS=0` leaves it out, and it's on by default. `MBEDTLS_SSL_VARIABLE_BUFFER_LENGTH` keeps idle connections small, and session tickets let returning browsers resume instead of doing a full handshake.
- **Connection layer**: one interface over plain TCP or TLS. The server checks the first byte of each connection (`0x16` is a TLS ClientHello), so HTTP and HTTPS share one port.
- **Network thread**: owns every socket, DNS lookups and TLS records, using `poll`/`WSAPoll` rather than ENet's `select`. A handshake worker pool (`httphandshakethreads`, default 2) does handshakes. Locked queues carry work to and from the main thread. Handlers and callbacks run only on the main thread, during `serverslice`.
- **HTTP/1.1**: one parser for both directions, with keep-alive, chunked transfer, redirects (client), gzip through zlib, CORS (server), and proper status codes.
- **WebSockets**: our own framing and handshake for both directions (`ws://` and `wss://`), with pings, close codes, and fragmented messages reassembled up to the size cap. SHA-1 comes from mbedTLS, with a small fallback when it is built without TLS.
- **JSON**: our own value type with a parser and writer on engine containers. There are no size caps, any value can be the top level, and nesting depth is limited. It replaces jsmn and `jsobj`.
- **Downloads**: small responses (≤1 MB) stay in memory. Larger ones stream to a temporary file in the home directory, with a size cap per request and progress the main thread can query. Resuming with Range headers comes later.
- **Outgoing certificate checks**: always on. Root certificates come from the system bundle on Linux, `CertOpenSystemStore("ROOT")` on Windows, or a Mozilla bundle shipped in `config/`, and `httpcafile` overrides all of these. A request can pin a certificate fingerprint, so a client can talk to a server's self-signed HTTPS when it already knows the fingerprint, for example from the master's list.

### C++ interface

A request is built (`http::get`, `http::post`, …), given headers, a body or JSON, a size cap and a callback plus a `void *` context, then submitted. Submitting returns a handle that can be cancelled. The callback runs on the main thread. Whoever owns a pending request cancels it in their own cleanup, for example on map change or disconnect. Server routes are registered with a method, a path pattern and a minimum Privilege, replacing the `HTTP(...)` macro. Callbacks use function pointers, not `std::function`, to match the codebase.

### CubeScript interface

Available to all trusted script: your own config and console, the shipped `config/` and UIs, `servinit.cfg`, and scripts the master sends. None of these commands is marked **Safe**, so the **Sandbox** that limits map script denies them to **Map script** with no extra checks. Everything goes through the same C++ client, with the same certificate checks and size caps.

- `httprequest <method> <url> [headers] [body] [callback]` runs in the background and returns a handle. `httpcancel <handle>` cancels it. The callback runs on the main thread with the status, body and headers as arguments. A UI that is torn down cancels its handles.
- `httpdownload <url> <file> [callback]` writes only below the home directory. It refuses absolute paths, `..` and `temp/`.
- `wsopen <url> [onmessage] [onclose]` returns a handle, and `wssend`/`wsclose` use it.
- `jsonget <json> <path>` (for example `servers.3.name`), `jsonkeys` and `jsonlength` read values, and `jsonobject`/`jsonarray` build JSON from key/value lists with proper escaping. None of them runs anything. Never `execute` remote content.
- `httproute <method> <path> <privilege> [body]` in `servinit.cfg` adds a route whose body returns the JSON response. It follows the same Privilege rules as C++ routes and can't replace a built-in route.

## Access

- Every request has a **Privilege**. An anonymous request is Player.
- **API tokens**: `addhttptoken <name> <token> <privilege>` in `servinit.cfg`.
- **Challenge**: the client sends `POST /auth` with an account name and receives a challenge. It then sends `POST /auth` with the answer and receives a session token at the master-granted Privilege plus any **Local operator** standing. The challenge always goes through the master. Session tokens expire after `httpsessiontime` (default 30 min) without use, and are dropped when the server restarts. A WebSocket does the same exchange once on its open connection.
- Routes reuse the server's existing per-action minimum Privileges. Routes above Player are refused over plain HTTP unless the request comes from localhost or a trusted proxy, or the operator allows it explicitly.
- **Trusted proxies**: `httptrustproxy` lists proxy addresses whose `X-Forwarded-For`/`X-Forwarded-Proto` headers are believed. The real client address then goes through the **Control** checks and rate limits.
- **Origins**: WebSocket upgrades that carry an `Origin` header are accepted only from `httpalloworigin`, for example `https://www.redeclipse.net` on the master. Non-browser clients send no `Origin` header and aren't affected.

## Game server Web API

- Game servers serve HTTP and HTTPS exactly as the master does. They run the same code; only the defaults below differ.
- On by default for dedicated servers and off for the client's local listen server.
- **Port**: tries `httpserverport` (28888), then the following ports up to `httpserverportrange` (default 16), and reports the port it got to the master. A range of 0 fixes the port.
- **Certificates**: `httpcertfile`/`httpkeyfile` turn HTTPS on. The server checks both files' modification times about once a minute and reloads them when they change. Plain HTTP on the same port still works from localhost and trusted proxies.
- **Event feed**: only what any player in the match can already see. That covers match start and end, map and mode, joins and leaves, team changes, frags with weapon, score changes, flags and objectives, and chat (which an operator can turn off). Positions, health and loadouts are never published. Higher Privileges also receive moderation events such as Control changes.

## Master server Web API

- The master keeps its legacy text protocol on 28800, unchanged, and serves the Web API on exactly 28888. The port is fixed because the website relies on it.
- `GET /servers` is a cacheable list with an ETag. Each entry includes the server's Web API port.
- Browser viewers fetch `/servers`, then open `wss://play.redeclipse.net:28888` and receive changes only. Each change is encoded once and shared by every viewer. Visitors on networks that block ports other than 80 and 443 fall back to polling `/servers`.
- Game servers that support it use a `wss://` link for heartbeats, auth challenges and Control pushes, in place of the text protocol. The legacy protocol stays until the cutoff is decided (see `doc/todo-list.md`).
- At startup the master raises its soft open-file limit to the hard limit, and warns if that is below `httpmaxconns`.
- The master host has no root, and a reverse proxy can't be installed there. The master handles TLS itself (see Certificates below).

## Relay

- **Addresses**: `https://play.redeclipse.net:28888/servers/<address>:<port>/<path>`, and `wss://…/servers/<address>:<port>/events` for the Event feed. Paths are the same as on the server itself.
- **Transport**: the game server's `wss://` link to the master (stage 8). Servers on the legacy protocol have no relay.
- **`httprelay`** (game server): 0 off, 1 public routes and Event feed (default), 2 everything, including API tokens, challenges and moderation. The listing on the master shows each server's level.
- **Event feed**: the server sends each event to the master once, and the master builds the frame once and fans it out to every viewer of that server.
- **Caching**: anonymous `GET` responses are cached per server and path for `httprelaycache` (default 2 s). Anything carrying a token or session is never cached.
- **Addresses and bans**: the master applies its global bans and per-address limits, then passes the viewer's address over the link. The game server treats the link as a trusted proxy and applies its own Control checks.
- **Security**: relayed requests count as secure, since each leg uses TLS.
- **Limits**: `httprelaymaxrate` caps each server's relayed bandwidth.
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

The master also limits the TLS handshake rate per address and caps each viewer's send queue. Addresses on the ban list are refused when they connect.

## Stages

One commit per problem, and every commit builds in CI with no pipeline changes.

1. Vendor mbedTLS into `src/support/mbedtls/` and add `WANT_TLS` (default on) to the Makefile, for Linux, mingw and MSVC.
2. The network thread and connection layer, plain and TLS, with `poll`/`WSAPoll`.
3. The HTTP/1.1 parser, client and server. Remove the old code, the debug page and `httptest`.
4. JSON.
5. WebSockets.
6. Access: Privilege per request, API tokens, challenge and session tokens, trusted proxies, origins.
7. Game server routes, the Event feed, and port selection reported to the master.
8. Master: `/servers`, browser viewers, and the `wss://` link for game servers, with the legacy protocol kept.
9. Relay: `/servers/<address>:<port>/…` over the link, `httprelay` levels, Event feed fan-out, the anonymous cache, passing on viewer addresses, and the per-server rate cap.
10. CubeScript: `httprequest`, `httpdownload`, `ws*`, `json*` and `httproute`, none marked Safe.

## Testing

- Malformed-input scenes for the HTTP, WebSocket and JSON parsers in `src/scripts/`, in the style of `csperf/` and `sandbox/`.
- A local dedicated server with a self-signed certificate, exercised by `curl`, a browser page and the game client: HTTP and HTTPS on one port, a `wss://` Event feed, the challenge, and API tokens.
- A connection soak against a local master to check the viewer limits and memory per connection.
- A scene in `src/scripts/sandbox/` where Map script calls each new CubeScript command and each call is denied and logged, plus a scripted client using `httprequest`, `wsopen` and the `json*` commands against a local server.
- A local master and a game server without a certificate. A browser page reaches the server only through the relay, at each `httprelay` level. Check that a banned viewer is refused at the server, and that the master's logs contain no tokens.

## Operator guide

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

A server that doesn't set a certificate is still reachable from websites through the master's relay. `httprelay 1` (the default) relays public routes and the Event feed. `httprelay 2` also relays moderation, which means your API tokens and challenges pass through play.redeclipse.net in plain text (the master doesn't log or store them). `httprelay 0` turns the relay off.

### API tokens

`addhttptoken <name> <token> <privilege>` in `servinit.cfg`. A token carries that Privilege on every request that presents it, so treat it like a password.
