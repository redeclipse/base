# Red Eclipse Web API: a guide for web developers

Red Eclipse game servers and the master server answer HTTP and WebSocket requests with JSON. You can use this to build server lists, live scoreboards, match trackers, Discord bots and moderation tools.

This guide covers what you can call and what comes back. How it is built inside the engine is in `doc/web-api.md`.

## At a glance

- **The master server** is `play.redeclipse.net`, port **28888**, for HTTP and HTTPS alike. It lists every public server, pushes changes to that list live, and relays requests to game servers.
- **Game servers** each run their own copy of the API on a port of their own. The master's list tells you which port (`webport`).
- **Every route is below `/api/v1`.** The rest of each server's address space is left for web pages, such as the master's [bounce links](#join-links-and-bounce-links).
- **Most sites only need the master.** A browser page on https can't reach a game server that has no CA-signed certificate, and most don't have one. The master's relay reaches every server holding a link to it, over https, with one code path.
- **Reading is open to everyone.** Anything above that, such as moderation, needs a token and an encrypted connection.
- **The master holds the Matchmaking Queue,** where people with an account wait for enough others to play with, and sends them to a server together. See [The Matchmaking Queue](#the-matchmaking-queue).
- **People can stay signed in for weeks** through a remembered sign-in on the master, which gets them a session on any server that takes one, without your site keeping their private key. See [Remembered sign-ins](#remembered-sign-ins).
- **The master registers accounts,** from the game or a website, and lets their owners reset a lost key, change their email, link Steam and Discord and pick a display name. See [Accounts](#accounts).

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
    if(ev.event === "frag") console.log(`${ev.killer.name} fragged ${ev.victim.name} with the ${ev.weaponname || ev.weapon}`);
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
| 401 | The route needs more than Player and you sent no token (`WWW-Authenticate: Bearer`), or your token is unknown or its session expired (`unknown or expired token`), so sign in again |
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
- No cookies are used, so don't send credentials. Tokens go in `Authorization`. The one exception is the master's [remembered sign-ins](#remembered-sign-ins), below `/api/v1/signin`, whose cookie only pages from the master's own list of origins may send.

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

A Privilege can be **local**: standing the server's operator gives of their own, rather than the master's, as for players in the game. Local Privileges go no higher than `administrator`, and wherever a Privilege is given, a local one comes with `"local": true`, so it can be drawn with the game's local icon.

### Getting a Privilege

There are two ways:

- **API tokens** are set by a server's operator in its config, each with a fixed Privilege, which is local. Ask the operator for one. A token works only on the server that issued it.
- **Signing in with a Red Eclipse account** gives a session token at that account's standing on the server, through a challenge the master checks (below): what the master grants it, or what the server's operator gives it locally when that is higher.

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
{"token": "4f3a...", "name": "someone", "privilege": "moderator", "local": true, "expires": 1800}
```

About the session:

- `expires` is the session's idle lifetime in seconds. Each use renews it, and a server restart ends it. A route that needs an account answers an ended session's token with 401 `unknown or expired token`, so sign in again then.
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

### Remembered sign-ins

A session lasts half an hour unused and ends when its server restarts, and a site that talks to many servers would need one on each. A **remembered sign-in** lasts weeks instead. It lives on the master, which gets you a session on whichever server you need, so neither your site nor the person's browser has to keep their private key.

Sign in on the master with the same challenge as above, at `/api/v1/signin` rather than `/api/v1/auth`:

1. `POST /api/v1/signin` with `{"name": "<account name>"}` gives `{"id": <number>, "challenge": "<string>"}`.
2. `POST /api/v1/signin` with `{"id": <number>, "answer": "<string>", "remember": true}` gives:

```json
{"id": "5cbf5ba653fbe075", "name": "someone", "flags": "u", "expires": 1810350612}
```

Options on the second step:

- **`remember`** keeps the sign-in for weeks. Without it, it ends after a day unused, and a browser's cookie ends when the browser closes.
- **`label`** names the device in the person's list, up to 64 characters. Without one, it's the browser's User-Agent.
- **`cookie`**: see below.

**How long it lasts:**

| Account | At most | Unused |
|---|---|---|
| player or supporter | 26 weeks | 13 weeks |
| moderator and above | 4 weeks | 2 weeks |

Each account holds at most 10; signing in an 11th time ends the one used longest ago. The master's operator can change these, or turn remembered sign-ins off for moderators and above, who then get 403 and sign in on each server instead. `expires` is when it ends at the latest, as a Unix time.

**Where the token goes:**

- **Browsers on the master's own list of origins** (on `play.redeclipse.net`, `https://www.redeclipse.net`) get it as an `HttpOnly` cookie, which page scripts can never read, so a script injected into the page can't take it away. Send every request below `/api/v1/signin` with `credentials: "include"`. The answer has no `token`, unless you send `"cookie": false`.
- **Everyone else, such as tools and bots,** gets `token` in the answer, and sends it as `Authorization: Bearer <token>` to the routes below. Keep it secret: it is the account, until it ends.
- The cookie only counts from those origins. Sent from anywhere else, it's ignored.
- Safari keeps a cookie at most 7 days when the server that set it has a different address from the page's, which is the case for the website and the master. So in Safari, people who don't come back within 7 days sign in again.

**Routes**, all below `/api/v1/signin`, each needing the sign-in:

| Route | Does |
|---|---|
| `GET /api/v1/signin` | Lists the account's own sign-ins: `{"name", "flags", "signins": [...]}`, each with `id`, `label`, `created`, `lastused`, `expires`, `firstaddr`, `lastaddr`, `remember`, and `current` for the one you used |
| `DELETE /api/v1/signin` | Signs out: ends the one you used, and clears the cookie |
| `DELETE /api/v1/signin/<id>` | Ends one of the account's others |
| `DELETE /api/v1/signin/all` | Ends all of them, everywhere |
| `POST /api/v1/signin/session/<address>/<port>` | A session on that server, below |
| `POST /api/v1/signin/session` | A session on the master itself, below |

Each `DELETE` answers `{"ok": true, "ended": <count>}`. A sign-in also ends when the account's key or standing changes on the master.

#### Sessions from a remembered sign-in

`POST /api/v1/signin/session/<address>/<port>`, with the address and port as the master's list gives them, answers as [signing in on that server](#sessions-from-an-account) would:

```json
{"token": "4f3a...", "name": "someone", "privilege": "moderator", "local": true, "expires": 1800}
```

- The master vouches for the account over that server's link, so the server never sees the sign-in token. The session is an ordinary one: use it on the server directly or through the relay, and get another when it answers 401.
- **It ends with the remembered sign-in** that made it, at once. It also ends when the server's link to the master closes, as when either restarts.
- The standing is the account's, raised by any local standing the server's operator gives it, as for any session.

Errors:

| Status | Error |
|---|---|
| 401 | `unknown or expired token`: the sign-in ended, so sign in again. A cookie that ended is cleared |
| 403 | The server's link to the master isn't encrypted, or the server has no certificate and its relay level is under 2, so the session couldn't be used anywhere |
| 404 | No such server holds a link to the master |
| 429 | Too many failed attempts, or more than 8 sessions waiting on servers for one sign-in |
| 501 | `this server doesn't support sessions from the master`: an older server. Sign in on it with a challenge |

`POST /api/v1/signin/session`, with no server named, gives a session on the master's own Web API, in the same form without `local`. Open the [live list](#websocket-apiv1serverslive) with it to queue as the account in the [Matchmaking Queue](#the-matchmaking-queue). It ends with the remembered sign-in that made it, and lapses unused like any other session.
| 502, 504 | The server's link closed, or it didn't answer within 10 seconds |

### Failed attempts

Each unknown token, wrong answer or expired session counts as a failure, **even on public routes**. So drop a token once it stops working rather than resending it.

- An address with 10 failures within a minute gets 429 for the rest of that minute.
- An address that hits that limit 3 times within an hour is banned from the server, game included, for the server's usual ban time (4 hours by default).
- Through the relay, this counts your visitor's address, not the master's.
- On the master's `/api/v1/signin`, an unknown token or cookie and a wrong answer count the same way and give 429 for the rest of the minute, but never a ban.

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
| `GET /matches` | none | The results of the last few matches |
| `GET /objectives` | none | Flags, points and the ball, as every player's HUD shows them |
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
    "url": "https://example.org/",
    "motd": "Welcome! Match nights are on Saturdays.",
    "map": "bloodlust",
    "mode": "Deathmatch",
    "modeid": 2,
    "mutators": 0,
    "players": 6,
    "spectators": 1,
    "bots": 4,
    "maxplayers": 16,
    "playerslots": 16,
    "mastermode": "open",
    "state": "Playing",
    "timeleft": 412,
    "relay": 1,
    "bridge": 1
}
```

| Field | Meaning |
|---|---|
| `url` | The operator's web address, such as their community's site, present when set; always `http://` or `https://` with no spaces, quotes or angle brackets, but open it as a link away from your page all the same |
| `motd` | The server's message of the day as plain text, present when set |
| `maptitle`, `mapauthor` | The map's title and author, present when the map has them and the server has its files |
| `mapcrc` | The map's crc, present when the server has its preview (see [GET /mapshot](#get-mapshot)) |
| `mode` | The mode's display name, with mutators |
| `players`, `spectators`, `bots` | Human players in the match, spectators, and bots |
| `maxplayers` | The player slots and the spectator slots the server sets aside, together; one with no spectator slots set aside still lets a few more connect to watch |
| `playerslots` | How many may play at once; spectators don't take a player slot. Older servers don't send it |
| `modeid`, `mutators` | The mode's number, and its mutators as bit flags (see [Modes and mutators](#modes-and-mutators)) |
| `mastermode` | One of `open`, `veto`, `locked`, `private`, `password` |
| `state` | A [game state](#game-states) |
| `timeleft` | Seconds left in the match, or -1 with no time limit |
| `relay` | The server's relay level |
| `bridge` | 1 when the server takes chat bridged from outside the game (see [Bridging chat](#bridging-chat)), 0 when its operator turned that off; older servers don't send it |
| `you` | Your Privilege, present only when signed in, with `local` when it is local |

### GET /players

An array with one entry for each player, bot and spectator. The map's own enemies and hazards are left out here and on the Event feed:

```json
[
    {
        "cn": 0, "name": "someone", "team": "alpha", "state": "Alive", "bot": false,
        "privilege": "player", "colour": 3368703, "handle": "someone", "points": 12, "frags": 10, "deaths": 4, "ping": 38
    }
]
```

| Field | Meaning |
|---|---|
| `cn` | The client number, which `POST /controls` takes |
| `team` | A [team](#teams) |
| `state` | A [player state](#player-states) |
| `handle` | The player's account, present only when signed in |
| `privilege`, `local` | Their [Privilege](#privileges), and `local` present and `true` when the server's operator gave it |
| `colour` | The colour they chose, as `0xRRGGBB` in a number, or 0 for none. The game draws a player's Privilege icon in it before their name in their team's colour, and so should you |
| `ip` | Present only at the server's `iphostlock` and above (moderator by default), and only for human players |

### GET /mapshot

The current map's preview, as a PNG. Add `?crc=` with the `mapcrc` from `/status` or the master's list: the answer is then cached for a day by browsers and the master's relay, and is 404 once the map has changed. Without it the answer is `no-cache` with an `ETag`, so send `If-None-Match` to get `304 Not Modified`.

It's 404 when the server doesn't have the map's files, which is the case in editing, or until a player has sent a map the server didn't have. The previews of the official maps are on the website too, so try those first.

```js
const img = new Image();
img.src = `https://play.redeclipse.net:28888/relay/${encodeURIComponent(s.host)}/${s.port}/api/v1/mapshot?crc=${s.status.mapcrc}`;
```

### GET /matches

The results of the last matches played there, newest first, kept until the server stops (`httpmatches` of them, 10 by default). A match is kept when play ends with a winner, by the time or score limit, and only when a human player was playing at the end; demos and editing aren't matches.

```json
[
    {
        "ended": 1762201234, "duration": 600, "map": "bloodlust", "maptitle": "Bloodlust", "modeid": 2, "mutators": 0,
        "teams": [{"team": "alpha", "score": 5}, {"team": "omega", "score": 3}],
        "players": [
            {"name": "someone", "handle": "someone", "team": "alpha", "points": 12, "frags": 9, "deaths": 4},
            {"name": "Ken", "team": "omega", "points": 7, "frags": 6, "deaths": 8, "bot": true}
        ]
    }
]
```

| Field | Meaning |
|---|---|
| `ended` | When play ended, a unix time |
| `duration` | How long it was played, in seconds |
| `map`, `maptitle` | The map, and its title when it has one |
| `modeid`, `mutators` | The [mode and mutators](#modes-and-mutators) |
| `timed` | Present and `true` in a timed speedrun, where scores are lap times in milliseconds and the shortest wins |
| `draw` | Present and `true` when the first two teams, or players in a mode without teams, are level |
| `teams` | Team modes only: every team's score, best first, so the winner is the first |
| `players` | Everyone who played, bots too, best first as the scoreboard has them; `team` is present in team modes |

### GET /objectives

What every player in the match already sees of its objectives: who carries each flag or ball, and who holds each point and who is taking it. It's `{}` in modes without objectives and outside play, and only the mode's own list is present.

```json
{
    "flags": [
        {"team": "alpha", "state": "taken", "carrier": {"cn": 3, "name": "someone", "team": "omega"}},
        {"team": "omega", "state": "home"}
    ],
    "points": [
        {"id": "A", "name": "Point", "owner": "alpha", "progress": 100},
        {"id": "B", "name": "Courtyard", "owner": "neutral", "enemy": "omega", "progress": 45}
    ],
    "balls": [{"state": "dropped"}]
}
```

| Field | Meaning |
|---|---|
| `flags` | Capture the Flag: each flag and its [team](#teams) |
| `balls` | Bomber Ball: the ball |
| `state` | A flag's or ball's `home`, `taken` or `dropped` |
| `carrier` | Who has it, when `taken`: their `cn`, `name` and `team` |
| `points` | Defend and Control: each point |
| `id` | The letter the game's HUD shows the point by, `A` onwards |
| `name` | The map's name for it, or `Point` when it has none |
| `owner` | The team holding it, `neutral` for none |
| `enemy` | The team taking it, present only while one is |
| `progress` | How far the point is taken, 0 to 100: 100 for one held and not under attack |

The Event feed sends an [`objectives`](#events) event with the same fields whenever any of this changes, at most once a second.

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

### DELETE /controls/\<id\>

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

The message is shown in game under your account's name, marked as from the web. It needs the server's `httpsaylock` (player by default) and follows the server's rules for chat: muted addresses and Privileges below `messagelock` get 403, text is censored and cut to fit `messagelength` (256 by default) ending with `...`, in which case the answer is `{"ok": true, "cut": <characters kept>}` so you can tell whoever wrote it, and sending more than the server's flood limit (5 messages in 10 seconds by default) gets 429. Repeated flooding mutes your address, as it would in the game. Add `"action": true` to send it as an action, as `/me` does in the game.

#### Bridging chat

A bridge carries chat between a server and somewhere outside the game, such as a Discord channel, so a message can name the person who wrote it there rather than your account:

```json
{"text": "anyone up for a duel?", "from": "Someone", "via": "discord"}
```

- **`from`** is the sender's name, filtered as a player's name is. **`via`** is where they are, up to 32 lowercase letters, digits, `-` or `_`; players see its icon when their game knows it, and the web's icon when it doesn't. Without `via` it is `web`, and without `from` the name is your account's.
- Sending either needs the server's `httpbridgelock` (developer by default, and never lower), and the server's `bridge` in [GET /status](#get-status) or the master's list to be 1; a server with it at 0 refuses them with 403.
- **Bridged chat skips the flood rule,** as everyone you carry would otherwise share your address's limit. Keep your own senders in check.
- The Event feed's `chat` carries your account's `handle` along with `from` as its `name`, so drop events with your own handle to keep your messages from coming back to you.
- **`account`** is the bridged sender's own Red Eclipse account, when they've linked one where they are (see [Linking Steam and Discord](#linking-steam-and-discord)). Players see it after their name as `Someone (handle)`, drawn by the game so no name can pass for one, and the Event feed's `chat` carries it as `account`. Send it with `privilege` set to the account's own standing, which the bot's lookup gives. It is refused without `from` or `via`.
- **`privilege`** is the standing a bridged sender has where they are, such as a Discord moderator, as one of `none`, `player`, `supporter`, `moderator`, `administrator`, `developer` or `creator`. It only chooses the icon players and websites see by their name and gives them nothing; without it they show none, as a bridged sender never wears your account's. It is refused without `from` or `via`.
- A bridged sender can't be muted or banned in the game, as they have no player or address there; players can hide them with `ignorebridge <via> <name>`. Moderate them where they are.

## The Event feed

Open a WebSocket to `/api/v1/events`, directly or through the relay. Each message is a JSON object with an `event` name and a `time`.

- **No snapshot comes first.** Fetch `/status` and `/players` when you connect, then apply events as they arrive.
- **Choose your events** with `?events=` on the URL, such as `?events=chat,action,join,leave`. See [Choosing events](#choosing-events).
- **The server sends a WebSocket ping every 30 seconds.** Browsers answer it by themselves. A connection that stays silent for about a minute is closed.
- **Your messages must be text frames,** and on the relay at most 4 KB.

### Choosing events

`?events=` takes event names separated by commas, and `default`, which stands for every event except `ping`. The game's broadcast messages are chosen by their own names, such as `servmsg` or `capture`.

- **Without `?events=`** a feed gets `default`. Pings come only to feeds that name `ping`, so a scoreboard asks for `?events=default,ping`.
- **The answers to your own messages** (`auth`, `challenge` and `said`) always come, whatever you chose.
- **A name the server doesn't know is ignored,** so you can ask for an event a server doesn't have yet.
- **Choose once,** when the feed opens. To change your mind, open another.

The server works out an event only when some feed asked for it, so asking only for what you use spares the server and the relay.

### Signing in on the feed

A feed starts as `none`. To see more, such as Control changes, sign in with one of:

- `?token=<token>` on the URL,
- a message `{"token": "<token>"}`,
- or the challenge: send `{"name": "<account>"}`, receive `{"event": "challenge", "id": ..., "challenge": "..."}`, then send `{"id": ..., "answer": "..."}`.

The answer is one of:

```json
{"event": "auth", "privilege": "moderator", "local": true, "name": "someone"}
{"event": "auth", "error": "unknown token"}
```

How long it lasts:

- A feed signed in with a challenge keeps its standing for as long as it stays open.
- One signed in with a token drops back to `none` when that token is removed or its session expires. You get another `auth` event when that happens.
- Signing in needs `wss://` (see [Encryption is required](#encryption-is-required)). Through the relay it also needs the server's relay level to be 2.

### Chatting on the feed

A feed signed in as an account can chat, under the same rules as [POST /say](#post-say): `httpsaylock`, `messagelock`, mutes, censoring, cutting and the flood limit. Send:

```json
{"say": "anyone for a duel?", "id": 7}
```

Add `"action": true` to send it as an action. `id` is optional, and comes back so you can match the answer:

```json
{"event": "said", "ok": true, "id": 7}
{"event": "said", "ok": true, "cut": 255}
{"event": "said", "status": 429, "error": "too many messages, slow down"}
```

`status` and `error` are what `POST /say` would have answered. The message itself comes back as a `chat` or `action` event, as everyone's does. The game's server browser and the website chat this way.

### Events

Players in events are described the same way throughout:

```json
{"cn": 3, "name": "someone", "team": "alpha", "bot": false, "handle": "someone", "privilege": "player"}
```

| Event | Fields | When |
|---|---|---|
| `join` | `client`, `players` | A player or bot joined; `players` is how many people are on the server now, spectators included and bots left out |
| `leave` | `client`, `reason`, `players` | A player or bot left; `reason` is one of the [leave reasons](#leave-reasons), missing for a bot, and `players` how many people are still on |
| `team` | `client` | A player changed team |
| `identify` | `client`, `text` | A player signed in to an account; `client` has its `handle` and the `privilege` it gave them, and `text` is the game's line saying so |
| `frag` | `victim`, `killer`, `weapon`, `weaponname`, `flags` | One player fragged another |
| `suicide` | `victim`, `weapon`, `weaponname`, `flags` | A player died by their own hand or the world |
| `chat` | `client`, `text` | Public chat, from the game or `POST /say`; team chat and whispers are never sent |
| `action` | `client`, `text` | A `/me` action |
| `map` | `map`, `mode`, `modeid`, `mutators` | A new match started |
| `result` | `match` | A match ended with a winner; `match` is as in [GET /matches](#get-matches) |
| `objectives` | `flags`, `points`, `balls` | The objectives changed, as in [GET /objectives](#get-objectives); compared once a second |
| `score` | `teams`, `players` | Scores changed |
| `ping` | `players` | Pings changed; only for feeds that ask for it (see [Choosing events](#choosing-events)) |
| `control` | `action`, `type`, `id`, `name`, `reason`, `by`, `ip`, `mask` | A Control entry was added or removed |

- **`weapon`** is a [weapon](#weapons) name, and missing when none applies, and **`weaponname`** the name the server gives it to players, such as `Sidearm Pistol`, which a game mod can change; show that one, and the `weapon` from servers too old to send it. **`flags`** is the game's own hit flags; treat it as opaque.
- **`score`** comes at most once a second. `teams` lists every team's score, in team modes only. `players` lists only the players whose `points`, `frags` or `deaths` changed, as `{"cn", "points", "frags", "deaths"}`. The first `score` after you connect lists everyone.
- **`ping`** comes at most every 30 seconds. `players` lists only the players whose `ping` changed, as `{"cn", "ping"}`, which is nearly everyone each time, as pings shift a little all the time. The first `ping` after a feed that asks for it connects lists everyone. A bot has its owner's ping, as in `/players`.
- **`control`:** `action` is `add` or `remove`. These go only to feeds whose Privilege may see that type of entry, and `ip` and `mask` only to those at the server's `iphostlock` and above (moderator by default).
- **`client`, `victim`, `killer`** and a flag's or ball's `carrier` are players as in [GET /players](#get-players), with `cn`, `name`, `team`, `bot`, `handle`, `privilege`, `local` and `colour`. So are a match result's `players`, without `cn`.
- **`chat`** and **`action`** from `POST /say` have a `client` with `cn` -1, the sender's `name` (`from` when it was bridged), the bridged sender's own `account` when it was sent, the account's `handle`, the sender's `privilege` and `local` (the account's own, or for someone bridged the `privilege` sent for them), `"web": true` and `via` (see [Bridging chat](#bridging-chat)).
- A server's operator can turn `chat` events off.
- **`join`** and **`leave`** come for bots too, with `bot` true in `client`, so a scoreboard kept from the feed has everyone `/players` lists. The game and the website keep them off their match feeds.
- **`identify`** takes the place of the game's `servmsg` line saying the same, and comes only for a player already on the server. One who signs in as they connect has no `identify`, and their `join` carries the `handle`. On joins and leaves, show a player's standing and `handle` after their name as the game's own join line does, such as `(global moderator: someone)`, `(player account: someone)`, or `(local administrator)` for a standing without an account, so nobody can pass for someone with an account.

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
    "time": 1762201000,
    "version": {"main": 209, "game": 289},
    "servers": [
        {
            "host": "1.2.3.4", "port": 28801, "priority": 0,
            "description": "My Server", "handle": "someone", "flags": "s", "branch": "stable",
            "version": 209, "webport": 28888, "relay": 1, "bridge": 1, "fingerprint": "9f86d0...",
            "status": { ... }
        }
    ]
}
```

| Field | Meaning |
|---|---|
| `time` | The master's clock when it sent the list, a unix time; see below |
| `version` (top level) | The versions the master expects of current clients: `main` as in 209 for 2.0.9, and `game`, the game protocol |
| `script` | Present when the master has a script for game clients; you can ignore it |
| `host`, `port` | The server's address and game port, which are also its relay address |
| `description` | May contain [markup](#text-markup) |
| `handle`, `flags` | The account that registered the server and its standing, when there is one |
| `version` | The server's version, such as 209 for 2.0.9 |
| `webport` | Present when the server's API is on |
| `relay` | 0 when the server isn't linked to the master |
| `bridge` | As in [GET /status](#get-status), present only for linked servers new enough to say; treat it missing as 0 |
| `fingerprint` | Present when the server serves HTTPS |
| `channel` | Present when the server takes [Matchmaking Queue](#the-matchmaking-queue) pops: `devel` or `release` |
| `status` | What the server is playing, when the master knows; see below |

For servers linked to the master, `status` is what the server reports about its match:

```json
{
    "gameversion": 289, "map": "bloodlust", "maptitle": "Bloodlust", "mapauthor": "someone", "mapcrc": 1324137294,
    "modeid": 2, "mutators": 0,
    "ends": 1762201234, "maxplayers": 16, "playerslots": 16, "mastermode": 0, "gamevars": 0, "gamemods": 0,
    "build": [2, 0, 9, 0, 1], "state": 5, "stateends": 1762201234,
    "spectators": 1, "bots": 4,
    "lastmatch": {"ended": 1762200634, "duration": 600, "map": "bloodlust", "maptitle": "Bloodlust", "modeid": 2, "mutators": 0,
        "teams": [{"team": "alpha", "score": 5}, {"team": "omega", "score": 3}], "players": [{"name": "someone", "team": "alpha", "points": 12, "frags": 9, "deaths": 4}]},
    "players": [
        {"cn": 0, "name": "someone", "handle": "someone", "team": "alpha", "privilege": "moderator", "local": true, "colour": 3368703},
        {"cn": 1, "name": "watcher", "team": "neutral", "privilege": "none", "spectator": true, "colour": 0}
    ]
}
```

Differences from `/status` on the server itself:

- **`mastermode` and `state` are numbers here:** the index into the [master modes](#master-modes) and [game states](#game-states).
- **`url` and `motd`** are as in [GET /status](#get-status).
- **`lastmatch`** is the newest of [GET /matches](#get-matches), present once a match has ended there, with only the best `httpmatchstatus` players (3 by default). Show it when nobody is on, such as "Alpha won 5-3 in Deathmatch on Bloodlust, 20 minutes ago".
- **Times are end times, not times left, by the master's clock.** `ends` is when the match ends (-1 with no time limit), `stateends` when the current state does, and `lastmatch`'s `ended` when it ended. The master moves them off each server's clock onto its own, and neither need agree with your visitor's, so count down from the list's `time`: work out how far your clock is ahead of it as the list arrives, and take that off your clock from then on. A list your browser revalidated with `If-None-Match` comes back with its old `time` and its old end times together, so keep the smallest difference you've seen rather than the latest.
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

### GET /api/v1/servers/\<address\>/\<port\>/history

How busy a server has been: the most players it had in each 15 minutes, oldest first, over the last `days` (default 7, up to a year).

```json
{"bucket": 900, "start": 1762128000, "peaks": [0, 0, null, 3, 8, 12]}
```

- **`start`** is the unix time the first peak's 15 minutes begin, and each one after is `bucket` seconds later; the last is the 15 minutes in progress.
- **`null`** is a time the master knew nothing: the server wasn't listed, or didn't say how many were on.
- **Players** are human players, spectators included and bots left out, as in `status`.
- A server the master has never listed gets all `null`. The answer may be a minute old.

### GET /api/v1/history

The same for the whole game: every listed server's players added together, then the most in each 15 minutes. That's more than adding up each server's peaks would say, as their busiest moments needn't be at the same time. `null` is a time no listed server said how many were on.

To show when people usually play, average the peaks for each weekday and hour, in the viewer's own time zone. A small game is quiet on weekdays, so eight weeks give each weekday enough to go on:

```js
const res = await fetch("https://play.redeclipse.net:28888/api/v1/history?days=56");
const h = await res.json();
const sums = [...Array(7)].map(() => new Array(24).fill(0)), counts = [...Array(7)].map(() => new Array(24).fill(0));
h.peaks.forEach((p, i) => {
    if(p === null) return;
    const t = new Date((h.start + i * h.bucket) * 1000), day = (t.getDay() + 6) % 7; // Monday first
    sums[day][t.getHours()] += p; counts[day][t.getHours()]++;
});
const usual = sums.map((row, d) => row.map((s, hr) => counts[d][hr] ? s / counts[d][hr] : null));
```

### WebSocket /api/v1/servers/live

The live list sends these messages:

- **On connect:** a `snapshot` with the same fields as `GET /servers`, its `time` included, then a `queue` event (see [The Matchmaking Queue](#the-matchmaking-queue)).
- **When a server appears or changes:** `{"event": "add" | "update", "time": ..., "server": {...}}`, with one entry as in the list.
- **When a server goes:** `{"event": "remove", "host": "...", "port": ...}`.

Changes are checked once a second. All you can say to it is about the Matchmaking Queue, below; anything else is ignored. A token, as `?token=` on the URL, is read as the socket opens and sets, for as long as it stays open, which account the socket queues as and what it sees of the queue. A session on the master, from `POST /api/v1/auth` there or [from a remembered sign-in](#sessions-from-a-remembered-sign-in), is one.

If your visitors' networks block port 28888, fall back to polling `GET /servers` with `If-None-Match`, no more often than every 5 seconds.

### Join links and bounce links

A join link, `redeclipse://<host>:<port>`, opens the game and joins that server, or hands the server to the game that's already open. The game registers the scheme itself whenever it starts. Join links never carry a password. Anything after the port is ignored, so later links can carry more. Link a listed server's `host` and `port` from `GET /servers` straight to its join link when your page is in a browser.

Some places, such as Discord, only make `https` links clickable. For those, link the master's bounce link instead:

```
https://play.redeclipse.net:28888/join/<host>/<port>
```

It's a web page, not part of `/api/v1`. For a server on the list it shows the server's description, sends the browser on to the join link, and offers a download link for anyone without the game. For anything else it answers `404` with a page that says so, so the master's address can't be used to send people to an unlisted server.

### The Matchmaking Queue

The Matchmaking Queue is for people who'd play if enough others turned up. It has a **channel** for each version the master's operator names, `devel` with `masterverdevel` and `release` with `masterverrelease`, and a channel set to `""` doesn't exist. A channel's servers are the linked ones on its version that run the newest protocol any of them does, which is what an up to date game of that version plays, and each lists that `channel`. Everything below happens within one channel. Each entry has a **Threshold**: the fewest people it will play with, 2, 4, 6 or 8, counting everyone already on the server it would be sent to, spectators included. Once some group's Thresholds are all met, the master **pops** them: it sends them all to one server and takes them out of the queue.

- **Who goes:** the largest group, lowest Thresholds first, whose Thresholds are all met by the group and those already on the server.
- **Where:** a listed server linked to the master, on the current game version, open, and with a player slot for each of them. Spectators don't take a player slot; a server too old to report its `playerslots` counts everyone on it against `maxplayers` instead. Of those, the one taking the largest group, then the one with the most people on, then the highest `priority`.
- **When:** whenever the queue changes, and once a second as the list does.
- **On their way:** popped entries count as on their server for 5 minutes, or until they arrive, so the next pop doesn't fill the same slots.

Every entry belongs to an **account**, and queuing needs one: there is one entry for each account, however many places it joined from. Linked servers report the handle of each player the master signed in there, which is how the master knows:

- **Arrived:** a popped entry's account turns up on its server, and the entry ends.
- **Held:** an entry's account is on a server without having been popped there. A held entry doesn't count again, isn't popped, and picks up again when they leave. It ends once its server has as many on as its Threshold.

So a player who joins without signing in to the server isn't seen there. The game signs in by itself once its auth.cfg holds the account's key.

Every entry lasts **60 minutes** from when it last joined or was renewed, wherever from, whether or not any socket stays open. A renewal adds 30 minutes, up to 90 from now. An entry also ends when its account is deleted.

#### GET /api/v1/queue

```json
{"time": 1762201000, "queue": {
    "channels": [{"name": "devel", "version": "2.0.9", "servers": 2, "ready": 3, "held": 1, "travelling": 0, "next": {"threshold": 4, "needed": 1}}],
    "ready": 3, "held": 1, "travelling": 0, "thresholds": [2, 4, 6, 8], "next": {"threshold": 4, "needed": 1},
    "entries": [{"handle": "someone", "displayname": "Some One", "privilege": "player", "channel": "devel", "threshold": 4,
        "state": "ready", "added": 1762200400, "expires": 1762204000}]}}
```

`channels` gives each channel's counts, its `version` and how many `servers` it has; the fields beside it add every channel up, with the `next` closest to a pop of them all. A page or bot shows each channel, as a player can only join the servers of their own version.

| Field | Meaning |
|---|---|
| `ready` | Entries waiting to be popped, neither held nor on their way |
| `held` | Entries whose account is on a server |
| `travelling` | Entries popped and not yet arrived |
| `thresholds` | The Thresholds an entry may have |
| `next` | How close the queue is: the Threshold that needs the fewest more people, and how many, on the server they'd go to; `null` when nobody is waiting or no server could take them |
| `entries` | Everyone queued, ready ones first by channel and Threshold, then held, then travelling |

Show it as the game does: "3 waiting, 1 more needed for a 4+ match".

Each entry has its account's `handle`, `displayname` (`""` for none) and `privilege` (a name from the [privilege table](#privileges)), its `channel`, `threshold`, `state` (`ready`, `held` or `travelling`), `added` and `expires`, and when they apply `held` or `popped` (`host`, `port` and, for `popped`, `time`). Show a player as the game does: the privilege's icon, then the display name, or the handle when there's none. At `masterqueuelock` and above (moderator by default) each also has `sources` (`web`, `game`, `discord`) and a game's `protocol`, and at `masterqueuediscordlock` (developer) the account's linked `discord` ID.

#### Queuing over the live list

Open the live list with a session on the master as `?token=`, from `POST /api/v1/auth` there or [from a remembered sign-in](#sessions-from-a-remembered-sign-in), and send text messages on it for that account:

| Message | What it does |
|---|---|
| `{"queue": "join", "source": "web", "channel": "devel", "threshold": 4}` | Joins that channel, or changes the account's Threshold or channel |
| `{"queue": "renew"}` | Adds 30 minutes, up to 90 from now |
| `{"queue": "leave"}` | Leaves, wherever the account joined from |

A socket may send one queue message every 250 ms. Without an account, offer the buttons greyed out with a way to register one, as the master answers `sign in with an account to queue`.

What comes back:

| Event | Fields | When |
|---|---|---|
| `queue` | `queue` | What anyone sees of the queue changed; viewers at `masterqueuelock` get every change |
| `queued` | `threshold`, `channel`, `expires`, `host` and `port` once popped, or `error` | The account's entry was taken or changed, from anywhere, and on connecting when it has one; or why a message wasn't taken |
| `unqueued` | `reason` | The account's entry ended: `left`, `arrived`, `satisfied` (held where enough are on), `popped` (sent but didn't arrive in time), `expired`, `deleted` (the account was) or `closed` (its channel was taken away) |
| `pop` | `host`, `port`, `count`, `entries`, and `"mine": true` when it's yours | A group was sent to a server |

The account's events go to every socket signed in as it. On a `pop` that's `mine`, open `redeclipse://<host>:<port>` when the visitor asks, never by itself.

#### Queuing Discord members

A viewer signed in at `masterqueuediscordlock` (developer, the Discord bot's) queues members by their Discord ID, with no `source`, into the account that ID is linked to:

| Message | What it does |
|---|---|
| `{"queue": "join", "discord": "<id>", "channel": "devel", "threshold": 4}` | Queues the member's account in that channel, or changes its Threshold or channel |
| `{"queue": "renew", "discord": "<id>"}` | Adds 30 minutes, up to 90 from now |
| `{"queue": "leave", "discord": "<id>"}` | Takes the account out |

Each is answered with `queued` and the member's `discord`, the account's `handle`, `threshold`, `channel` and `expires`, or `queued` with their `discord` and an `error`: `no account` when no account is linked to that ID, so tell them how to register one and link it, or `not queued`. A `leave` that takes them out is answered by its `unqueued`. Those viewers also hear `unqueued` with `handle`, the linked `discord` when there is one, and `reason` whenever any entry ends.

The queue is saved to `masterqueuefile` (`masterqueue.dat`) as it changes, so it outlives a restart.

## Accounts

The master holds every account. Anyone can register one, from the game or the website, with a **handle** and an email address. The master mails a link to activate it, and only an activated account works. All of these routes are on the master and need https (or localhost), like signing in.

### Registering

`GET /api/v1/accounts` says whether registration is open:

```json
{"register": true, "website": true, "days": 7}
```

`website` is false when the master has no captcha set up, so only the game can register. `days` is how long a registration has to be activated.

`POST /api/v1/accounts` registers one:

| Field | |
|---|---|
| `handle` | 3 to 24 letters, numbers, `-` or `_`. It's stored in lowercase, and no two accounts share one, whatever their case |
| `email` | Where the link goes. No two accounts, or registrations waiting, share one |
| `captcha` | From a website: the token Cloudflare Turnstile gave the page. The master checks it with Cloudflare |
| `game` | From the game: `true` in place of a captcha |

It answers 202 with `{"handle": "<handle>", "expires": <Unix time>}`, and from the game, also `pickup`, a secret the game keeps (see [The game collects its key](#the-game-collects-its-key)).

The registration is now **pending**. It holds its handle and email so nobody else can take them, but it can't sign in. The email has two links to the website's account page:

- **`/account/activate#<token>`** activates it. The page posts the token to `POST /api/v1/accounts/activate` with `{"token": "<token>"}`, which makes the account's key pair and answers once with:

  ```json
  {"handle": "someone", "privkey": "1f3a...", "pubkey": "+8c2d...", "authkey": "authkey someone 1f3a..."}
  ```

  The master keeps only the public key. The private key is in this answer and nowhere else, so show it to the person straight away with how to save it: the `authkey` line typed in the game's console, or saved as `auth.cfg` in their Red Eclipse home folder. The same token again gets 410.
- **`/account/cancel#<token>`**, "this wasn't me", posts to `POST /api/v1/accounts/cancel` and removes the registration at once (204).

A registration not activated within `days` is removed. Registering again with the same handle and email replaces it with a fresh one and a new email, in case the first never arrived; its earlier links stop working. The tokens are after `#` so they never reach a server's logs, and a mail scanner fetching the links does nothing, as only the page's script posts them.

Errors:

| Status | Error |
|---|---|
| 400 | A bad handle or email, or no captcha |
| 403 | The captcha wasn't solved |
| 404 | The link is unknown, ran out or was removed |
| 409 | The handle or email is taken |
| 410 | The account is already activated |
| 429 | Too many registrations from your address today (3 by default) |
| 503 | Registration is closed, or the website can't register (no captcha set up) |

### Lost keys

`POST /api/v1/accounts/reset` with `{"handle": "<handle>"}` or `{"email": "<address>"}`, and a `captcha` or `"game": true` as above, asks for a new key. It always answers 202 the same way, so it never says whether an account exists. If one does and has an email address, that address gets a link to `/account/reset#<token>`, lasting 24 hours. Only the newest link works.

The page posts the token to `POST /api/v1/accounts/reset/confirm`, which answers like activation, with the new key. The old key stops working, and the account's remembered sign-ins end.

### The game collects its key

A registration or reset from the game gets a `pickup` secret. The game posts it to `POST /api/v1/accounts/pickup` as `{"pickup": "<secret>"}` every few seconds:

- `{"ready": false, "expires": <Unix time>}` while the link hasn't been followed. A registration's answer also has its `handle`.
- `{"ready": true, "handle", "privkey", "pubkey", "authkey"}` once it has, which the game saves to `auth.cfg`. This works once.
- 404 once it ran out or was collected.

The page that followed the link still shows the key too, in case the game was closed.

### The owner's own account

These are below `/api/v1/signin`, so they take a [remembered sign-in](#remembered-sign-ins), as a cookie or a Bearer token.

| Route | Does |
|---|---|
| `GET /api/v1/signin/account` | `{"handle", "flags", "email", "steamid", "discord", "displayname"}`, and `newemail` while a change waits |
| `PATCH /api/v1/signin/account` | `{"displayname": "<name>"}` sets the display name, once an hour (429 sooner). `""` clears it |
| `POST /api/v1/signin/account/email` | `{"email": "<address>"}` mails that address a link to `/account/email#<token>`, and tells the old address, with a link to `/account/email-cancel#<token>` that stops it. Answers 202 |
| `DELETE /api/v1/signin/account` | `{"confirm": "<handle>"}` deletes the account (204). Its handle is then free to register again. Accounts at moderator and above are removed by an administrator instead (403) |

The email links post their token to `POST /api/v1/accounts/email/confirm`, which answers `{"handle", "email"}`, or `POST /api/v1/accounts/email/cancel` (204).

**Display names** are what the website shows for an account, always with its handle beside it, as `Display Name (handle)`. They're free-form and not unique: up to 32 characters, filtered as names in game are, without colour codes or any of `(){}<>:`. An account that hasn't chosen one takes the first name it's seen playing under on a server the master signed it in on, or failing that its Discord name. In game, players are known by their in-game names, and on Discord by their Discord names.

### Linking Steam and Discord

An account can link one Steam account and one Discord account, each proven through its own sign-in. A Steam or Discord account links to one account at most (409 otherwise). Links are only shown to the owner, administrators and the Discord bot.

**Steam**, through Steam's OpenID:

1. `GET /api/v1/signin/account/steam` gives `{"url", "state"}`. Send the person to `url`.
2. Steam sends them back to `/account/steam?state=<state>` with `openid.*` fields in the query. Post them all, as they came, to `POST /api/v1/signin/account/steam` as `{"openid": {"openid.ns": "...", ...}}`.
3. The master asks Steam whether it made that answer, and answers with the account as `GET /api/v1/signin/account` does. The state works once, within 10 minutes.

`DELETE /api/v1/signin/account/steam` unlinks it.

**Discord**, two ways:

- Through Discord's sign-in: `GET /api/v1/signin/account/discord` gives `{"url", "state"}`. Discord sends the person back to `/account/discord?code=<code>&state=<state>`; post both to `POST /api/v1/signin/account/discord`. 503 when the master has no Discord application set up.
- With a code: `POST /api/v1/signin/account/discord/code` gives `{"code": "H6SEHTCQ", "expires": <Unix time>}`, good once for 10 minutes. The person types `/link <code>` to the Discord bot.

`DELETE /api/v1/signin/account/discord` unlinks it.

**The bot's routes** need a session at the master's `masterdiscordlock` (developer by default), as the bot's account has:

| Route | Does |
|---|---|
| `POST /api/v1/accounts/discord/link` | `{"code", "discord": "<member's ID>", "name": "<their Discord name>"}` links the member to the code's account, answering `{"handle", "displayname"}` |
| `GET /api/v1/accounts/discord/<id>` | Which account the member is: `{"handle", "displayname", "privilege"}`, `privilege` being the account's own standing, or 404 |
| `DELETE /api/v1/accounts/discord/<id>` | Unlinks the member (204) |

### Administration

The master has no console, so its administrators manage it here, signed in with a remembered sign-in at the master's `masteradminlock` (administrator by default). Nobody can change an account at or above their own standing, or give one, except a creator.

| Route | Does |
|---|---|
| `GET /api/v1/signin/admin/accounts?q=<text>&limit=<n>` | Accounts whose handle or email holds `q`: `{"total", "accounts": [{"handle", "flags", "email", "steamid", "discord", "displayname", "pubkey"}]}` |
| `PATCH /api/v1/signin/admin/accounts/<handle>` | `{"flags": "<letters>"}` changes its standing, which ends its sign-ins. `{"displayname": "<name>"}` replaces or clears its display name, which takes `masterdisplaylock` (moderator by default) |
| `DELETE /api/v1/signin/admin/accounts/<handle>` | Removes it |
| `GET /api/v1/signin/admin/servers` | Server accounts, the same way |
| `POST /api/v1/signin/admin/servers` | `{"handle", "flags", "pubkey", "email"}` adds one |
| `DELETE /api/v1/signin/admin/servers/<handle>` | Removes one |
| `GET /api/v1/signin/admin/controls` | The master's own control entries, which it sends to every linked server: `{"controls": [{"id", "type", "address", "reason"}]}` |
| `POST /api/v1/signin/admin/controls` | `{"type": "ban", "address": "1.2.3", "reason": "..."}` adds one. `type` is `allow`, `ban`, `mute`, `limit`, `except` or `trust`; `address` is up to four numbers, fewer for a range |
| `DELETE /api/v1/signin/admin/controls/<id>` | Removes one |
| `POST /api/v1/signin/admin/mail` | `{"to": "<address>"}` sends a test email, to check the master's mail settings (202) |

Flags are letters: `u` player, `s` supporter, `m` or `o` moderator, `a` administrator, `d` developer, `c` creator.

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
| Remembered sign-ins | 26 weeks, 13 unused; 4 and 2 for moderators and above; 10 per account |

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

`mutators` is a bit field: bit *n* set means mutator *n* is on, including any the mode implies (Editing implies Free for All and Arena).

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
