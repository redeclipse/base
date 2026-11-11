# Red Eclipse Web API: a guide for web developers

Red Eclipse game servers and the master server answer HTTP and WebSocket requests with JSON. You can use this to build server lists, live scoreboards, match trackers, Discord bots and moderation tools.

This guide covers what you can call and what comes back. How it is built inside the engine is in `doc/web-api.md`.

## At a glance

- **The master server** is `play.redeclipse.net`, port **28888**, for HTTP and HTTPS alike. It lists every public server, pushes changes to that list live, and relays requests to game servers.
- **Game servers** each run their own copy of the API on a port of their own. The master's list tells you which port (`webport`).
- **Every route is below `/api/v1`.** The rest of each server's address space is left for web pages.
- **Most sites only need the master.** A browser page on https can't reach a game server that has no CA-signed certificate, and most don't have one. The master's relay reaches every server holding a link to it, over https, with one code path.
- **Reading is open to everyone.** Anything above that, such as moderation, needs a token and an encrypted connection.

## Quick start

All of these work from a browser page on any site (see [CORS](#cors)).

### List the servers

```js
const res = await fetch("https://play.redeclipse.net:28888/api/v1/servers");
const list = await res.json();
for(const s of list.servers)
    console.log(s.description, s.host, s.port, s.status ? s.status.map : "(no status)");
```

### See who is playing on one, through the relay

The relay address uses the server's **game port** (`port`), not its `webport`.

```js
const s = list.servers[0];
const base = `https://play.redeclipse.net:28888/relay/${encodeURIComponent(s.host)}/${s.port}/api/v1`;
const players = await (await fetch(`${base}/players`)).json();
```

`s.relay` says what the relay will carry for that server: 0 means nothing, 1 means public routes and the Event feed, and 2 means everything.

### Watch a match live

```js
const ws = new WebSocket(`wss://play.redeclipse.net:28888/relay/${encodeURIComponent(s.host)}/${s.port}/api/v1/events`);
ws.onmessage = (e) =>
{
    const ev = JSON.parse(e.data);
    if(ev.event === "frag") console.log(`${ev.killer.name} fragged ${ev.victim.name} with the ${ev.weapon}`);
};
```

### Keep a server list up to date

```js
const live = new WebSocket("wss://play.redeclipse.net:28888/api/v1/servers/live");
live.onmessage = (e) =>
{
    const ev = JSON.parse(e.data);
    switch(ev.event)
    {
        case "snapshot": /* ev.servers is the whole list */ break;
        case "add": case "update": /* ev.server is one entry */ break;
        case "remove": /* ev.host and ev.port name it */ break;
    }
};
```

## Reaching a server

| Way | URL | When to use it |
|---|---|---|
| Master | `https://play.redeclipse.net:28888/api/v1/...` | The server list and its live updates |
| Relay | `https://play.redeclipse.net:28888/relay/<host>/<port>/api/v1/...` | Any game server, from a browser |
| Direct | `https://<host>:<webport>/api/v1/...` | Tools, bots and servers with a CA-signed certificate |

### Direct connections

A game server's entry in the master's list has a `webport` when its API is on, and a `fingerprint` when it serves HTTPS. Most servers use a self-signed certificate:

- **Browsers** will refuse a self-signed certificate. They also block plain `http://` requests from an https page, so a browser page should use the relay.
- **Tools** can trust a self-signed certificate by its fingerprint. The fingerprint is the SHA-256 of the server's DER certificate in lowercase hex, the same as `openssl x509 -noout -fingerprint -sha256` prints without the colons. Only trust a fingerprint you fetched from the master over https. Note that curl's `--pinnedpubkey` hashes the public key instead, which gives a different value.
- **Plain `http://`** works too, but tokens are ignored over it (see [Signing in](#signing-in)).

### The relay

`/relay/<host>/<port>/<path>` asks that server for `/<path>` exactly as given, query string included. `<host>` is the `host` from the list and `<port>` is its game `port`. They are separate path segments because an IPv6 address has colons of its own. Requests and the Event feed both work.

What the relay passes on:

- **Request headers:** only `Content-Type`, `Accept`, `Origin` and `If-None-Match`. `Authorization` is passed on only when the server's relay level is 2 and you reached the master over https. A token sent to the master over plain http is dropped, and the server sees an anonymous request.
- **Request bodies:** text only, such as JSON or a form.
- **Response headers:** only `Content-Type`, `ETag`, `Cache-Control`, `Allow`, `WWW-Authenticate` and `Retry-After`. The master adds `X-Content-Type-Options: nosniff` and `Content-Security-Policy: sandbox allow-scripts allow-forms allow-popups`, so anything a server returns is sandboxed away from the master's origin.
- **Caching:** anonymous `GET`s that succeed are cached for 2 seconds per server, path and query string. Polling more often than that gets the same answer back.

Errors that come from the relay itself rather than from the server:

| Status | Meaning |
|---|---|
| 403 | The server's relay level is 0 |
| 404 | No server with that host and port holds a link to the master |
| 414 | The path is over 511 characters |
| 502 | The master lost its link to the server |
| 503 | The server's relay bandwidth is used up for now; retry after `Retry-After` (1 second) |
| 504 | The server didn't answer within 30 seconds |

## Conventions

### JSON and text

- Request and response bodies are UTF-8 JSON. `POST` routes also accept `application/x-www-form-urlencoded`, and a few fields can go in the query string.
- Numbers that are counts, ids and scores are integers. `time`, `ends` and `stateends` are unix times in seconds.
- Names of things (privileges, teams, states, weapons) are plain strings, listed under [Reference](#reference).

### Text markup

Strings are plain text, with one exception: a server's `description` in the master's list is stored as the server sent it, and may carry Red Eclipse's colour codes. Each code starts with a form feed (`\f`, U+000C). This strips them:

```js
const plain = (s) => s.replace(/\f(?:z..|\[[^\]]*\]|\([^)]*\)|\{[^}]*\}|.)/g, "");
```

Player names and chat are plain everywhere: in the master's list, in `/status` and `/players`, and on the Event feed. Where the game shows a player with colours and icons, the list gives you the parts as fields of their own (see [GET /api/v1/servers](#get-apiv1servers)).

### Errors

Every error has a JSON body of the form `{"error": "what went wrong"}`, with a matching status:

| Status | Meaning |
|---|---|
| 400 | The request is malformed or a field is missing or invalid |
| 401 | The route needs more than Player and you sent no token (`WWW-Authenticate: Bearer`) |
| 403 | Your Privilege is too low, the route needs an encrypted connection, the origin isn't allowed, or your address is banned |
| 404 | No such route, player or entry |
| 405 | The path exists but not for that method (`Allow` lists the ones it takes) |
| 408 | The request took too long to arrive |
| 409 | The action can't apply, such as banning a player with no address |
| 413 | The body is over 1 MB |
| 426 | `/events` was asked for without a WebSocket upgrade |
| 429 | Too many failed sign-ins from your address, or too many messages to `POST /say`; retry after `Retry-After` |
| 431 | Too many headers, or a header line that is too long |
| 503 | The server isn't connected to the master, so it can't check a sign-in |
| 504 | An answer the server waited on, such as a sign-in, took over 30 seconds |

### CORS

Browsers may call the API from other origins:

- Responses carry `Access-Control-Allow-Origin`.
- Preflight `OPTIONS` requests are answered for the methods `GET`, `HEAD`, `POST`, `PUT`, `PATCH`, `DELETE` and `OPTIONS`, and the headers `Authorization` and `Content-Type`. They are cached for a day.
- No cookies are used, so don't send credentials. Tokens go in `Authorization`.

**The official master, `play.redeclipse.net`, allows every origin.** Any site can use its list, live list and relay from the browser, which is what the Web API is for.

Game servers allow every origin by default too. An operator can limit their own server with `httpalloworigin`, and then only the origins listed can read its responses from a browser or open its WebSockets. That affects direct connections only, since the relay and the live list follow the master's setting.

### Caching

- `GET /servers` on the master has an `ETag` and `Cache-Control: public, max-age=5`. Send `If-None-Match` to get `304 Not Modified` when nothing changed.
- Relayed anonymous `GET`s are cached for 2 seconds, or as long as the server's `Cache-Control` says, up to an hour, as for [`/mapshot`](#get-mapshot) asked for by crc.

Compressed responses are sent when you accept gzip, which browsers do by default.

## Signing in

### Privileges

Every request has a **Privilege**, from lowest to highest: `none`, `player`, `supporter`, `moderator`, `administrator`, `developer`, `creator`. An anonymous request is `none`, as an unauthenticated player is in the game. `player` means a signed in account with no higher standing.

Each route needs at least a set Privilege. `GET /api/v1` lists the routes your Privilege allows, and what each needs.

### Getting a Privilege

There are two ways:

- **API tokens** are set by a server's operator in its config, each with a fixed Privilege. Ask the operator for one. A token works only on the server that issued it.
- **Signing in with a Red Eclipse account** gives a session token at that account's standing on the server, through a challenge the master checks (below).

Send a token as `Authorization: Bearer <token>`. On the Event feed, where browsers can't set headers, use `?token=<token>` on the URL or a message (see [Signing in on the feed](#signing-in-on-the-feed)).

### Encryption is required

Tokens and sign-ins only count over an encrypted connection. That means one of:

- https or wss directly to the server,
- the relay over https or wss, when the server's link to the master is itself encrypted (it is by default),
- or localhost, or a reverse proxy the operator trusts.

Over plain http a token is ignored: the request is answered as anonymous, and routes that need an account return 403. This keeps tokens and the addresses moderators can see from crossing the network in the clear.

### Sessions from an account

Signing in is a challenge and response:

1. `POST /api/v1/auth` with `{"name": "<account name>"}`. The answer is `{"id": <number>, "challenge": "<string>"}`.
2. Compute the answer to the challenge with the account's private key.
3. `POST /api/v1/auth` with `{"id": <number>, "answer": "<string>"}`. The answer is:

```json
{"token": "4f3a...", "name": "someone", "privilege": "moderator", "expires": 1800}
```

About the session:

- `expires` is the session's idle lifetime in seconds. Each use renews it, and a server restart ends it.
- The Privilege is what the master grants the account, raised by any local standing the server's operator gives it.
- Step 3 must come from the same address as step 1.

Computing the answer:

- **From tools,** the `genkey` program that ships with the game prints it:

  ```
  genkey answer <private key> <challenge>
  ```

- **The private key** is in the player's `auth.cfg`, as the second word after `authkey <name>`. It is a secret, so it must never leave the player's machine.
- **A browser** has to do the maths itself. The challenge is a point on NIST P-192 in compressed form: a sign (`+` or `-`, for the parity of y), then x in hex. The answer is the x coordinate, in hex, of that point multiplied by the private key. See `answerchallenge` in `src/shared/crypto.cpp`.

Errors from `POST /auth`:

| Status | Error |
|---|---|
| 400 | `bad name`, or neither a `name` nor an `id` and `answer` were sent |
| 403 | `identity verification failed` (wrong answer or unknown account), or the connection isn't encrypted, or the relay level is under 2 |
| 404 | `no such challenge` (expired after a minute, or from another address) |
| 429 | Too many attempts from your address, or more than 4 challenges in progress |
| 503 | The server isn't connected to the master |

### Failed attempts

Each unknown token, wrong answer or expired session counts as a failure, **even on public routes**. So drop a token once it stops working rather than resending it.

- An address with 10 failures within a minute gets 429 for the rest of that minute.
- An address that hits that limit 3 times within an hour is banned from the server, game included, for the server's usual ban time (4 hours by default).
- Through the relay, this counts your visitor's address, not the master's.

## Game server routes

The paths below leave out `/api/v1`. All `GET` routes also answer `HEAD`.

| Method and path | Privilege | What it does |
|---|---|---|
| `GET /` (no `/api/v1`) | none | Says where the API is: `{"name": "Red Eclipse", "api": "/api/v1"}` |
| `GET /api/v1` | none | What the server is and the routes you may use |
| `GET /status` | none | The match: map, mode, counts, time left |
| `GET /players` | none | Everyone connected, with scores |
| `GET /teams` | none | Team scores |
| `GET /mapshot` | none | The map's preview, a PNG |
| `GET /events` | none | The Event feed (WebSocket) |
| `POST /auth` | none | Sign in with an account |
| `GET /controls` | player | Bans, mutes and the like |
| `POST /controls` | player | Kick, or add a ban, mute, limit, allow or except |
| `DELETE /controls/<id>` | player | Remove a Control entry |
| `POST /map` | player | Change the map and mode |
| `POST /say` | player | Send a message to everyone in the game |

The Privileges listed are the minimum to reach the route. Each action then checks the server's own in-game minimum for it, the same as a player in the game would face. For example, banning needs at least the server's `banlock`.

Operators can add routes of their own outside `/api`, and those appear in the index.

### GET /api/v1

```json
{
    "name": "Red Eclipse",
    "version": "Red Eclipse 2.0.9 ...",
    "type": "server",
    "routes": [
        {"path": "/api/v1/status", "methods": {"GET": "none"}},
        {"path": "/api/v1/events", "methods": {"WS": "none"}},
        {"path": "/api/v1/controls", "methods": {"GET": "player", "POST": "player"}}
    ]
}
```

- `type` is `server` or `master`.
- Each route maps its methods to the Privilege they need. `WS` is a WebSocket.
- Only the routes your Privilege reaches are listed.

### GET /status

```json
{
    "description": "My Server",
    "version": "Red Eclipse 2.0.9 ...",
    "map": "bloodlust",
    "mode": "Deathmatch",
    "modeid": 2,
    "mutators": 0,
    "players": 6,
    "spectators": 1,
    "bots": 4,
    "maxplayers": 16,
    "mastermode": "open",
    "state": "Playing",
    "timeleft": 412,
    "relay": 1
}
```

| Field | Meaning |
|---|---|
| `maptitle`, `mapauthor` | The map's title and author, present when the map has them and the server has its files |
| `mapcrc` | The map's crc, present when the server has its preview (see [GET /mapshot](#get-mapshot)) |
| `mode` | The mode's display name, with mutators |
| `players`, `spectators`, `bots` | Human players in the match, spectators, and bots |
| `modeid`, `mutators` | The mode's number, and its mutators as bit flags (see [Modes and mutators](#modes-and-mutators)) |
| `mastermode` | One of `open`, `veto`, `locked`, `private`, `password` |
| `state` | A [game state](#game-states) |
| `timeleft` | Seconds left in the match, or -1 with no time limit |
| `relay` | The server's relay level |
| `you` | Your Privilege, present only when signed in |

### GET /players

An array with one entry for each player, bot and spectator:

```json
[
    {
        "cn": 0, "name": "someone", "team": "alpha", "state": "Alive", "bot": false,
        "privilege": "player", "handle": "someone", "points": 12, "frags": 10, "deaths": 4, "ping": 38
    }
]
```

| Field | Meaning |
|---|---|
| `cn` | The client number, which `POST /controls` takes |
| `team` | A [team](#teams) |
| `state` | A [player state](#player-states) |
| `handle` | The player's account, present only when signed in |
| `ip` | Present only at the server's `iphostlock` and above (moderator by default), and only for human players |

### GET /mapshot

The current map's preview, as a PNG. Add `?crc=` with the `mapcrc` from `/status` or the master's list: the answer is then cached for a day by browsers and the master's relay, and is 404 once the map has changed. Without it the answer is `no-cache` with an `ETag`, so send `If-None-Match` to get `304 Not Modified`.

It's 404 when the server doesn't have the map's files, which is the case in editing, or until a player has sent a map the server didn't have. The previews of the official maps are on the website too, so try those first.

```js
const img = new Image();
img.src = `https://play.redeclipse.net:28888/relay/${encodeURIComponent(s.host)}/${s.port}/api/v1/mapshot?crc=${s.status.mapcrc}`;
```

### GET /teams

```json
[{"team": "alpha", "score": 3}, {"team": "omega", "score": 5}]
```

The array is empty in modes without teams. A team only appears once it has a score.

### GET /controls

The Control entries your Privilege may see:

```json
[
    {"id": 7, "type": "ban", "scope": "local", "source": "player", "name": "someone", "by": "admin", "reason": "cheating", "ip": "1.2.3.4"}
]
```

| Field | Meaning |
|---|---|
| `type` | `allow`, `ban`, `mute`, `limit` or `except` |
| `scope` | `temporary`, `internal`, `local` or `global`. Global entries come from the master and can't be removed here |
| `source` | `server`, `player`, `automatic` or `master` |
| `name`, `handle`, `by`, `reason` | Present when known |
| `ip`, `mask` | At the server's `iphostlock` and above (moderator by default). `mask` is the prefix length, and is present only for ranges |

### POST /controls

```json
{"cn": 3, "type": "ban", "reason": "cheating"}
```

- `type` is `kick`, `allow`, `ban`, `mute`, `limit` or `except`. `reason` is optional.
- Success is `201` with `{"ok": true}`.
- You can't act on a player whose Privilege is higher than yours. A player protected by an `except` entry can only be acted on by an Administrator.

### DELETE /controls/<id>

Removes the entry. The answer is `{"ok": true}`, or 404 when there is no such entry.

### POST /map

```json
{"map": "bloodlust", "mode": 2, "mutators": 0}
```

- `mode` and `mutators` are optional and default to what is playing now.
- They are checked as an in-game vote would be: mutators the server forces are added, combinations that can't go together are fixed, and demo playback is refused.
- The map changes straight away, with no vote, so it needs the server's `httpmaplock` (moderator by default).
- The server's vote, mode and map locks still apply, as they do to a forced vote in the game: a recently played map, a locked mode, or a map outside the rotation, gets 403 unless your Privilege meets the lock.

### POST /say

```json
{"text": "Server restarting in 5 minutes"}
```

The message is shown in game under your account's name, marked as from the web. It needs the server's `httpsaylock` (player by default) and follows the server's rules for chat: muted addresses and Privileges below `messagelock` get 403, text is cut to `messagelength` and censored, and sending more than the server's flood limit (5 messages in 10 seconds by default) gets 429. Repeated flooding mutes your address, as it would in the game.

## The Event feed

Open a WebSocket to `/api/v1/events`, directly or through the relay. Each message is a JSON object with an `event` name and a `time`.

- **No snapshot comes first.** Fetch `/status` and `/players` when you connect, then apply events as they arrive.
- **The server pings every 30 seconds.** Browsers answer by themselves. A connection that stays silent for about a minute is closed.
- **Your messages must be text frames,** and on the relay at most 4 KB.

### Signing in on the feed

A feed starts as `none`. To see more, such as Control changes, sign in with one of:

- `?token=<token>` on the URL,
- a message `{"token": "<token>"}`,
- or the challenge: send `{"name": "<account>"}`, receive `{"event": "challenge", "id": ..., "challenge": "..."}`, then send `{"id": ..., "answer": "..."}`.

The answer is one of:

```json
{"event": "auth", "privilege": "moderator", "name": "someone"}
{"event": "auth", "error": "unknown token"}
```

How long it lasts:

- A feed signed in with a challenge keeps its standing for as long as it stays open.
- One signed in with a token drops back to `none` when that token is removed or its session expires. You get another `auth` event when that happens.
- Signing in needs `wss://` (see [Encryption is required](#encryption-is-required)). Through the relay it also needs the server's relay level to be 2.

### Events

Players in events are described the same way throughout:

```json
{"cn": 3, "name": "someone", "team": "alpha", "bot": false, "handle": "someone", "privilege": "player"}
```

| Event | Fields | When |
|---|---|---|
| `join` | `client` | A player joined |
| `leave` | `client`, `reason` | A player left; `reason` is one of the [leave reasons](#leave-reasons) |
| `team` | `client` | A player changed team |
| `frag` | `victim`, `killer`, `weapon`, `flags` | One player fragged another |
| `suicide` | `victim`, `weapon`, `flags` | A player died by their own hand or the world |
| `chat` | `client`, `text` | Public chat, from the game or `POST /say`; team chat and whispers are never sent |
| `action` | `client`, `text` | A `/me` action |
| `map` | `map`, `mode`, `modeid`, `mutators` | A new match started |
| `score` | `teams`, `players` | Scores changed |
| `control` | `action`, `type`, `id`, `name`, `reason`, `by`, `ip`, `mask` | A Control entry was added or removed |

- **`weapon`** is a [weapon](#weapons) name, and missing when none applies. **`flags`** is the game's own hit flags; treat it as opaque.
- **`score`** comes at most once a second. `teams` lists every team's score, in team modes only. `players` lists only the players whose `points`, `frags` or `deaths` changed, as `{"cn", "points", "frags", "deaths"}`. The first `score` after you connect lists everyone.
- **`control`:** `action` is `add` or `remove`. These go only to feeds whose Privilege may see that type of entry, and `ip` and `mask` only to those at the server's `iphostlock` and above (moderator by default).
- **`chat`** from `POST /say` has a `client` with `cn` -1, the sender's `name`, `handle` and `privilege`, and `"web": true`.
- A server's operator can turn `chat` events off.

The feed also passes on the game's broadcast messages, the ones every player sees in their console. These are named for their kind, such as `match`, `balance`, `duel`, `capture`, `defend`, `bomber`, `speedrun` and `servmsg`. Each has:

- a `text` field with the console line, markup removed,
- fields that vary by kind,
- and arrays of the players involved, each with `cn`, `name`, `team`, `actortype` and `privilege` (a number here).

Use `text` for display, and the events above for anything you compute.

Positions, health, loadouts and anything else a player couldn't already see in the match are never sent.

## The master server

The master also answers the game server routes above for itself, and its `/api/v1` index says `"type": "master"`.

### GET /api/v1/servers

```json
{
    "version": {"main": 209, "game": 289},
    "servers": [
        {
            "host": "1.2.3.4", "port": 28801, "priority": 0,
            "description": "My Server", "handle": "someone", "flags": "s", "branch": "stable",
            "version": 209, "webport": 28888, "relay": 1, "fingerprint": "9f86d0...",
            "status": { ... }
        }
    ]
}
```

| Field | Meaning |
|---|---|
| `version` (top level) | The versions the master expects of current clients: `main` as in 209 for 2.0.9, and `game`, the game protocol |
| `script` | Present when the master has a script for game clients; you can ignore it |
| `host`, `port` | The server's address and game port, which are also its relay address |
| `description` | May contain [markup](#text-markup) |
| `handle`, `flags` | The account that registered the server and its standing, when there is one |
| `version` | The server's version, such as 209 for 2.0.9 |
| `webport` | Present when the server's API is on |
| `relay` | 0 when the server isn't linked to the master |
| `fingerprint` | Present when the server serves HTTPS |
| `status` | What the server is playing, when the master knows; see below |

For servers linked to the master, `status` is what the server reports about its match:

```json
{
    "gameversion": 289, "map": "bloodlust", "maptitle": "Bloodlust", "mapauthor": "someone", "mapcrc": 1324137294,
    "modeid": 2, "mutators": 0,
    "ends": 1762201234, "maxplayers": 16, "mastermode": 0, "gamevars": 0, "gamemods": 0,
    "build": [2, 0, 9, 0, 1], "state": 5, "stateends": 1762201234,
    "spectators": 1, "bots": 4,
    "players": [
        {"cn": 0, "name": "someone", "handle": "someone", "team": "alpha", "privilege": "moderator", "local": true, "colour": 3368703},
        {"cn": 1, "name": "watcher", "team": "neutral", "privilege": "none", "spectator": true, "colour": 0}
    ]
}
```

Differences from `/status` on the server itself:

- **`mastermode` and `state` are numbers here:** the index into the [master modes](#master-modes) and [game states](#game-states).
- **Times are end times, not times left.** `ends` is when the match ends (-1 with no time limit), and `stateends` when the current state does. Subtract the current unix time to count down.
- **Players are every human player, spectators included;** bots are left out. There are fewer fields than in `/players`:

| Field | Meaning |
|---|---|
| `cn` | The client number |
| `name` | Plain text |
| `handle` | The player's account, present only when signed in |
| `team` | A [team](#teams) |
| `privilege` | A [Privilege](#privileges) |
| `local` | Present and `true` when that standing was given by the server's operator rather than the master |
| `spectator` | Present and `true` when the player is spectating |
| `colour` | The colour the player chose, as `0xRRGGBB` in a number, or 0 for none (the game then uses the team's) |

#### Polled status

Servers that aren't linked, including older versions of the game, don't report anything. The master asks them itself every 30 seconds, the way the game's server browser does, and lists what they answer as a `status` with `"polled": true`:

```json
{
    "polled": true, "gameversion": 230, "map": "enyo", "modeid": 2, "mutators": 2,
    "ends": 1762201234, "maxplayers": 16, "mastermode": 0, "gamevars": 0, "gamemods": 0,
    "build": [1, 6, 0, 2, 64], "state": 5, "stateends": 1762201234,
    "players": [
        {"name": "someone", "handle": "someone", "privilege": "moderator", "local": true, "colour": 6316287, "teamcolour": 6252287},
        {"name": "watcher", "privilege": "none", "colour": 16731972, "teamcolour": 9478288}
    ]
}
```

- **Fewer fields.** There is no `spectators` or `bots`, and players have no `cn`, `team` or `spectator`. Spectators are listed as players, as the server doesn't say who is spectating. Players come best score first.
- **Players carry what the game's server browser shows.** That is `name`, `handle` (when signed in), `privilege` and `local`, and two colours as `0xRRGGBB` in a number:

| Field | Meaning |
|---|---|
| `privilege` | The word on the player's Privilege icon in the server's version. It is mostly a [Privilege](#privileges), but every version says `founder` for `creator`, and v1.6 also has `operator`, between `moderator` and `administrator` |
| `local` | Present and `true` when that standing was given by the server's operator |
| `colour` | The colour the game shows the player in. Unlike a reported status, this is never 0: a player who chose none gets their weapon's colour |
| `teamcolour` | Their team's colour. No colour has ever been the default for two teams, so a default names the team. Game mods can change them, which is rare |

Any of these is left out when the server's answer doesn't have it.
- **Numbers follow the server's `gameversion`.** `modeid`, `mutators`, `mastermode` and `state` are passed on exactly as the server sent them. Modes, master modes and game states are numbered the same in every version, but the mutator bits moved: v1.6 (game version 230) and 2.0.0 (245) number them differently from the [current list](#modes-and-mutators). Check `gameversion` before naming them.
- **Up to a poll old.** `ends` and `stateends` are worked out from the times left when the master asked. A server that misses three polls in a row loses its `status` but stays listed, and one that starts reporting over the link replaces it with its own.

A server that has never answered has no `status`, and you only have what the list gives.

### GET /api/v1/servers/<address>/<port>/history

How busy a server has been: the most players it had in each 15 minutes, oldest first, over the last `days` (default 7, up to a year).

```json
{"bucket": 900, "start": 1762128000, "peaks": [0, 0, null, 3, 8, 12]}
```

- **`start`** is the unix time the first peak's 15 minutes begin, and each one after is `bucket` seconds later; the last is the 15 minutes in progress.
- **`null`** is a time the master knew nothing: the server wasn't listed, or didn't say how many were on.
- **Players** are human players, spectators included and bots left out, as in `status`.
- A server the master has never listed gets all `null`. The answer may be a minute old.

To show when a server is usually busy, average the peaks for each hour of the day, in the viewer's own time zone, over the last few weeks:

```js
const res = await fetch(`https://play.redeclipse.net:28888/api/v1/servers/${encodeURIComponent(s.host)}/${s.port}/history?days=28`);
const h = await res.json(), sums = new Array(24).fill(0), counts = new Array(24).fill(0);
h.peaks.forEach((p, i) => {
    if(p === null) return;
    const hour = new Date((h.start + i * h.bucket) * 1000).getHours();
    sums[hour] += p; counts[hour]++;
});
const usual = sums.map((s, i) => counts[i] ? s / counts[i] : null);
```

### WebSocket /api/v1/servers/live

The live list sends these messages, and expects nothing from you:

- **On connect:** a `snapshot` with the same fields as `GET /servers`.
- **When a server appears or changes:** `{"event": "add" | "update", "server": {...}}`, with one entry as in the list.
- **When a server goes:** `{"event": "remove", "host": "...", "port": ...}`.

Changes are checked once a second.

If your visitors' networks block port 28888, fall back to polling `GET /servers` with `If-None-Match`, no more often than every 5 seconds.

## Limits

| Limit | Value |
|---|---|
| Open connections per address | 16 |
| Request body | 1 MB |
| Headers per request | 64 |
| Header line | 8 KB |
| Idle HTTP connection | closed after 15 s |
| Request arriving | must finish within 30 s of its first byte |
| TLS handshakes per address | 30 every 10 s |
| Messages to the master's sockets | 4 KB |
| Relayed bandwidth per server | 256 KB/s (shared by everyone using that server's relay) |
| Session idle lifetime | 30 min |

These are defaults. Operators can change them, except on the master's own sockets.

## Reference

### Privileges

`none`, `player`, `supporter`, `moderator`, `administrator`, `developer`, `creator`, lowest to highest.

### Teams

`neutral` (no team), `alpha`, `omega`, `enemy` (non-player enemies), `environment`.

### Player states

`Alive`, `Dead`, `Editing`, `Spectator`, `Waiting`.

### Game states

Numbers are for `state` in the master's `status`, and strings are for `/status`.

| Number | String |
|---|---|
| 0 | `Waiting` |
| 1 | `Getting Map` |
| 2 | `Sending Map` |
| 3 | `Readying` |
| 4 | `Syncing Info` |
| 5 | `Playing` |
| 6 | `Overtime` |
| 7 | `Intermission` |
| 8 | `Voting` |

### Master modes

| Number | String |
|---|---|
| 0 | `open` |
| 1 | `veto` |
| 2 | `locked` |
| 3 | `private` |
| 4 | `password` |

### Modes and mutators

| `modeid` | Mode |
|---|---|
| 0 | Demo |
| 1 | Editing |
| 2 | Deathmatch |
| 3 | Capture the Flag |
| 4 | Defend and Control |
| 5 | Bomber Ball |
| 6 | Speedrun |

`mutators` is a bit field: bit *n* set means mutator *n* is on.

| Bit | Mutator |
|---|---|
| 0 | Free for All |
| 1 | Coop |
| 2 | Instagib |
| 3 | Medieval |
| 4 | Kaboom |
| 5 | Duel |
| 6 | Survivor |
| 7 | Classic |
| 8 | Onslaught |
| 9 | Vampire |
| 10 | Resize |
| 11 | Hard |
| 12 | Arena |
| 13 | Dark |
| 14 | Game specific 1 |
| 15 | Game specific 2 |
| 16 | Game specific 3 |

The game-specific mutators mean something different in each mode:

| Mode | Bit 14 | Bit 15 | Bit 16 |
|---|---|---|---|
| Deathmatch | Gladiator | Old School | |
| Capture the Flag | Quick | Defend | Protect |
| Defend and Control | Quick | King | |
| Bomber Ball | Hold | Basket | Assault |
| Speedrun | Lapped | Endurance | Gauntlet |

### Weapons

`claw`, `pistol`, `sword`, `shotgun`, `smg`, `flamer`, `plasma`, `zapper`, `rifle`, `corroder`, `trident`, `grenade`, `mine`, `rocket`, `minigun`, `jetsaw`, `eclipse`, `melee`.

### Leave reasons

`End of packet`, `Invalid client number`, `Kicked from server`, `Communication error`, `Address is banned`, `Server is in private mode`, `Incorrect password`, `AI client removed`, `Server is at maximum capacity`, `Server and client are incompatible`, `Connection timed out`, `Packet overflow`, `Server shutting down`, `Hostname lookup failure`, `Client with same handle authenticated`.

A player who simply quits leaves with no `reason`.

## Versions

Everything here is version 1 of the API, below `/api/v1`. A future change that breaks clients will move to a new prefix, with `/api/v1` kept alongside it for a while. New fields and events may appear in version 1 at any time, so ignore anything you don't recognise.
