## THE RED ECLIPSE PRIVACY POLICY

This Privacy Policy explains how the Red Eclipse project ("Red Eclipse", "Red
Eclipse Team", "we", "us") collects, uses, and protects information when you use
the Red Eclipse game client, connect to the play.redeclipse.net master server or
connected game servers, interact with our Discord integration or bot, or visit
https://www.redeclipse.net.

Red Eclipse is an open-source, non-profit hobby project operated by Quinton
Reeves and the Red Eclipse Team. We do not sell, rent, or trade personal data,
and we do not use your information for advertising or commercial profiling.

### Offline and Local Play

When you run Red Eclipse offline or on private servers not connected to the
Red Eclipse master server, the game stores your configuration, logs, demos, and
screenshots locally on your device. No gameplay or personal data is sent to the
Red Eclipse Team during offline play.

### Discord Integration

Red Eclipse integrates with Discord in two ways:

* Game Client Rich Presence (Application ID 506825464946360321):
  - When Discord support (`discordenabled`) and presence (`presence`) are
    enabled in the game client, Red Eclipse communicates locally with the
    Discord client running on your computer using the Discord RPC library.
  - Depending on your `presence` setting (`0` = off, `1` = activity, `2` =
    activity and Join button), the game sends your current activity (such as
    game mode, map, server description, player count, and match timer) and,
    while on a listed, open server with room, party and Join link information
    (the server's address is never shown as text) to your local Discord client
    so it can be displayed on your Discord profile according to your Discord
    privacy settings.
  - The game client receives your Discord username and user ID locally from your
    Discord client to log connection status and handle join requests. It sends
    your Discord user ID, and nothing else of your Discord account, to the game
    servers you join and to the master server, only for the Matchmaking Queue (see
    below). It never transmits your Discord credentials.
  - You can disable Discord Rich Presence at any time in the game settings
    (`presence 0` or `discordenabled 0`).

* Official Discord Server and Chat Bridge (Redroid):
  - In the official Red Eclipse Discord server, our bot (Redroid) processes
    slash and context-menu commands, user IDs, display names, roles, and server
    events (joins, leaves, bans) for community features (such as FAQs, quotes,
    and dictionary entries) and moderation logs.
  - On game servers that enable chat bridging (`httpbridge 1`) through the
    master server relay, in-game chat messages are posted to a public thread in
    the Red Eclipse Discord server under the player's in-game display name, and
    messages sent in that Discord thread are relayed into the game server with
    the sender's Discord display name and role badge.

### Steam Integration

* When running Red Eclipse with Steam enabled (`steamclient 1`), the game uses
  the Steamworks SDK to read your Steam persona name and Steam ID locally, show
  your current match status to Steam friends (`presence`), and generate a
  session authentication ticket when connecting to a Steam-authenticated game
  server (`steamserver`) so the server can verify your Steam ID.
* You can disable Steam presence or client integration via `presence 0` or
  `steamclient 0`.

### Online Play, Master Server, and Web API

When you connect to the `play.redeclipse.net` master server or any game server
connected to it, certain information is processed for server operation,
authentication, public server listings, and moderation in accordance with our
Multiplayer Guidelines (https://www.redeclipse.net/guidelines):

* Connection and Moderation Data:
  - In-game username, authentication handle (if you use a master server
    account), Steam ID and handle (if using Steam authentication), Discord
    handle (if bridged or provided), IP address, client version, operating
    system, and hardware configuration.
  - IP addresses and account identifiers are used to route game traffic, enforce
    bans and trusts, and assist moderators and administrators in maintaining
    fair play.

* Public Server Browser and Web API:
  - Listed game servers publish their status (server name, address, map, mode,
    match time, and active players' display names, scores, pings, teams, and
    privilege badges) to the master server (`play.redeclipse.net:28888`), where
    it is viewable in the in-game server browser and on the web server browser
    at https://www.redeclipse.net/servers.
  - The master server Web API relay logs only the target server, request path,
    HTTP status, response size, and viewer IP address; it never logs headers,
    bodies, query strings, authentication tokens, or challenge answers, and
    writes no relayed payloads to disk.

* Matchmaking Queue:
  - The Matchmaking Queue lets you wait for enough players for a match from the game,
    the website or Discord. Each entry holds how many players you want, when it
    was made, a random ticket identifying it, the IP address of the game or
    browser that made it, and your Discord user ID when you queued through
    Discord or the game has Discord connected.
  - So that the master server can tell when you've joined a server, the game
    sends your ticket and Discord user ID to each game server it joins, and
    those servers pass them only to the master server. Servers don't show or
    log them, and the master server never publishes them: the public queue shows
    counts only, and only moderators and above see individual entries.
  - The master server saves the queue to disk so it survives a restart. An entry
    is removed, with its Discord user ID, when it ends: when you leave the queue,
    join the match you were sent to, or its time runs out.
  - With `discordenabled 0` the game sends no Discord user ID.

* Game Statistics:
  - On servers with statistics reporting enabled (`sv_serverstats`), completed
    match results and timed race laps are sent to the master server's statistics
    database and displayed at https://www.redeclipse.net/stats.
  - Statistics records store only players' in-game nicknames, authentication
    handles (for authenticated users), and match performance metrics. IP
    addresses are not stored in the statistics database.

### Website

* Our website at https://www.redeclipse.net is hosted via GitHub Pages. Standard
  web server access logs (such as IP address and browser user-agent) are handled
  by GitHub in accordance with the GitHub Privacy Statement.
* When you open the Server Browser page (`https://www.redeclipse.net/servers`),
  your browser connects directly to `play.redeclipse.net:28888` to retrieve live
  server and match status.

### Data Sharing and Disclosure

* We do not sell or share your personal information with third-party marketers
  or advertisers.
* Public match information (active player names and scores on listed servers,
  bridged chat messages, and match statistics) is publicly accessible as part of
  the multiplayer service.
* Administrative data (such as IP addresses, server logs, and moderation
  records) is restricted to authorized Red Eclipse Team members and server
  operators for legitimate administration, security, and moderation purposes, or
  when required by law.

### Data Retention and Deletion

* Server and relay logs are rotated and pruned automatically on a regular
  schedule.
* Matchmaking Queue entries are kept only while they last, and removed from the saved
  queue as soon as they end.
* Account authentication entries, moderation records (such as global bans),
  match statistics, and community database entries (such as Discord quotes or
  FAQs) are retained for as long as needed to operate the service and maintain
  historical records.
* You may request access to, correction of, or deletion of personal data
  associated with your Red Eclipse authentication handle, Steam ID, or Discord
  User ID at any time by contacting us.

### Contact Information

If you have questions about this Privacy Policy or wish to make a data request,
please contact the Red Eclipse Team:

* Email: contact@redeclipse.net
* Discussions: https://www.redeclipse.net/discuss
* Issues: https://www.redeclipse.net/issues

### Attribution

This text is Copyright (C) 2011-2026, the Red Eclipse Team
and is available under a Creative Commons Attribution-ShareAlike 4.0
License <http://creativecommons.org/licenses/by-sa/4.0/>
