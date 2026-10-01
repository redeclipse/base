# CSGOpen

CSGOpen is a multiplayer first-person shooter built from a fork of Red Eclipse.
The project's goal is a two-team Deathmatch experience inspired by
Counter-Strike: Global Offensive, with deliberate ground movement and weapons
that gradually move toward that style of play. Development proceeds in small,
testable steps while keeping the original Red Eclipse gameplay available as a
reference.

The longer-term scope includes community map conversion and a dedicated Linux
server. The first milestone focuses on a native macOS development baseline and
a separate TDM prototype using existing maps and assets. Its movement and
weapon values are starting points, not a faithful reproduction of CS:GO.

## Current milestone: native macOS baseline and TDM v0.1

Starting commit: `faf378d12558addc700d0e464e7e8c3a39fbceee`. Both the client
and dedicated server have been built and launched natively on the Mac, and the
local network smoke test passes. Visual inspection and keyboard/mouse gameplay
testing remain open. See the [results and checklist](validation.md) and the
[gameplay rules and units](gameplay.md).

## Setup

You need an active Xcode/Command Line Tools installation, Homebrew matching
the architecture reported by `uname -m`, and a checkout with its recorded
submodule revisions:

```sh
git clone --recurse-submodules https://github.com/agea/csgopen-base.git csgopen-base
cd csgopen-base
git submodule update --init --recursive
brew install pkgconf sdl2 sdl2_image openal-soft libsndfile
scripts/csgopen/dev.sh check
scripts/csgopen/dev.sh build
```

For an already prepared checkout, run `check` and `build`. Do not use
`--remote` for the asset submodules. Dependencies are SDL2, SDL2_image,
OpenAL Soft, libsndfile, the SDK's zlib, and OpenGL.framework. ENet is built
from the included source. XQuartz and Rosetta are not required. `check`
discovers prefixes through Homebrew and pkg-config and verifies library
architectures with `lipo`.

If the toolchain is missing, install Xcode or run `xcode-select --install`;
select the appropriate installation without arbitrarily changing the system's
selection. The build has been tested on arm64; Intel has not been tested.

The script uses the upstream Makefile with four parallel jobs by default
(`CSGOPEN_JOBS=8 scripts/csgopen/dev.sh build` changes this). It runs exactly
`src/redeclipse_native` and `src/redeclipse_server_native`, with no fallback
to another installation. Repository paths and arguments are quoted; you can
invoke the script by its absolute path from another directory. Binaries are
not installed globally. The upstream launcher now recognizes Darwin and looks
for the `_native` suffix in `bin/<arch>/`, where the Makefile's install targets
place the binaries. Use the development script for this milestone.

## Local gameplay

```sh
scripts/csgopen/dev.sh original
scripts/csgopen/dev.sh tdm
```

The second command starts TDM on **Echo**, with a minimum of two participants
in total, including humans. A bot makes an initial solo test possible. Close
the client before switching profiles. Choose a player name when prompted,
then leave spectator mode through the menu or `/spectate 0` in the console.
`/tdm echo` is an alias present in this version. To use another included map:

```sh
scripts/csgopen/dev.sh tdm dutility
```

Echo is a native map with Alpha/Omega spawn points and a courtyard derived
from Cube 2. `bath` is absent from this checkout's submodules. The checklist
still needs to confirm which areas and routes are usable without parkour.
No map or asset files have been modified.

The preset sets actual spawn health to 100, disables regeneration, enables
friendly fire for humans and bots with a team damage multiplier of 1, and
starts with a three-second respawn delay. Normal jumping and
crouching remain enabled; parkour capabilities are disabled. A semiautomatic
pistol and an automatic SMG are assigned at spawn. Other weapons are disabled
for both loadouts and pickups. Primary fire remains available; arena alternate
fire is disabled. Upstream respawn requires primary fire or jump input after
death; the delay does not imply automatic respawn without input.

Pistol and SMG primary fire now have nonzero projectile spread. Relative to
standing still, running triples spread and crouching halves it. Moving while
crouched returns to the standing spread; airborne fire adds a further penalty.
These are initial tuning values. Compare single shots or short bursts at the
same wall and distance, standing, moving, and crouched. Existing recoil remains.

Sustained primary fire also builds additional spread after each shot. The first
shot starts at normal accuracy; buildup is capped at an extra multiplier of
1.5 (up to 2.5x posture spread). Recovery is linear and takes 1.2 seconds from
the cap. Crouching still improves accuracy during a burst. Buildup is tracked
per weapon and cleared on spawn/reset; switching weapons does not clear the
previous weapon's buildup. Compare short bursts, a full magazine, and shots
after a pause. Server settings are `sv_spreadburstadd`, `sv_spreadburstmax`,
and `sv_spreadburstrecovery`. The pistol uses `sv_pistolspreadburstscale 2`
(0.7 buildup per shot) so repeated semiautomatic shots visibly lose accuracy
despite recovery between shots. The SMG retains 0.35 buildup per shot.

The ammunition ring around the crosshair expands with current primary-fire
spread and contracts during recovery or crouching. Bullet glyphs still show
remaining ammunition. Its radius uses the same posture and burst calculation
as firing, with a bounded square-root scale (standing SMG is the reference).
It is an indicative accuracy display, not a projected impact boundary. The
client preference `clipspread` enables it in CSGOpen and defaults to off in
the original profile.

## Dedicated server on loopback

In one terminal:

```sh
scripts/csgopen/dev.sh server
```

In another terminal, run `scripts/csgopen/dev.sh tdm`, then use the game console:

```text
/connect 127.0.0.1 28801
/spectate 0
```

The dedicated server uses the same preset, binds to **127.0.0.1**, and uses
UDP port 28801 for gameplay and 28802 for information queries. LAN discovery,
public master registration, and HTTP are disabled. No router or cloud setup is
needed. Use exactly `127.0.0.1`: only this literal address is exempt from the
public-server guidelines prompt, without storing agreement to those terms.

For another port, use `CSGOPEN_PORT=28811 scripts/csgopen/dev.sh server` and
the same port in `connect`. Stop the server with Ctrl-C. Run only one instance
per profile; do not launch two CSGOpen clients sharing the same profile.

Repeatable smoke test, with the server already running on the default port:

```sh
scripts/csgopen/dev.sh tdm '-xexec "config/csgopen/smoke.cfg"'
rg 'CHECK_FAIL|RESPAWN_ELAPSED|SMOKE_DONE|SMOKE_TIMEOUT' .csgopen/logs/tdm-client.log
```

The test chooses a name, connects, checks synchronized rules and actual weapon
state, triggers suicide, requests another spawn through spectator/rejoin,
checks the delay, and changes the server map to Dutility. It closes the client;
the server keeps running. Expected result: `SMOKE_DONE FAILURES 0`, with no
`CHECK_FAIL` or timeout. The client's exit code alone is insufficient. This
test does not verify physical input, aiming, friendly fire from actual shots,
regeneration after damage, or rendering quality.

## Profiles, tuning, and troubleshooting

All local state is ignored by Git:

| Path | Purpose |
| --- | --- |
| `.csgopen/original/` | Original configuration and cache |
| `.csgopen/csgopen-client/` | Preset client, integrated server, and cache |
| `.csgopen/server/` | Dedicated server |
| `.csgopen/logs/` | Build and launch logs; launch logs are overwritten on restart |

Launchers regenerate the managed initialization files for their own profiles.
Edit `config/csgopen/tdm.cfg` for server settings, then restart the sessions.
`config/csgopen/client.cfg` contains only client preferences. Server examples:
`sv_playerspawndelay` and `sv_botspawndelay` are in milliseconds;
`sv_movespeed` is a multiplier; `sv_moveaccelscale` and `sv_movebrakescale`
control ground velocity response. The new coefficients are needed because
upstream uses one coast parameter for both acceleration and braking.
`sv_savevars` preserves preset defaults across cleanup and map changes.
To return to the reference gameplay, quit and run `original`, which retains
arena defaults of 1000 health, speed 1, and the full set of impulse capabilities.

- Missing assets (`asset mancanti`) or mismatched submodules: complete the
  update to the recorded revisions. A partial clone cannot launch correctly.
- pkg-config errors: run `check` and inspect the native Homebrew installation.
  OpenAL Soft is keg-only; the script adds its pkg-config path.
- Linux X11/GL symbols: use this Makefile with the Darwin toolchain, without
  manually setting `PLATFORM` to a Linux target. X11 libraries are not needed.
- Missing or non-native binary: run `build` again. Do not use the prebuilt
  Linux executables included in the assets.
- Connection failure: check that the dedicated server is still running,
  verify the port, and inspect `server.log`. To inspect sockets, run
  `lsof -nP -iUDP:28801 -iUDP:28802`.
- Initial spawn blocked: set a name and leave spectator mode. Executing
  `primary 1` in CubeScript does not simulate a physical key press.
- Observed runtime warnings: PNG iCCP profiles with invalid CRCs and some
  missing upstream IQM animations. An Apple driver sampler warning also
  appears on the baseline. No features were disabled to hide these warnings;
  visual inspection remains necessary. Dutility also reports a rail outside
  the map; the smoke test uses it only to exercise map changes.

No automatic commits or pushes. Linux and Windows retain their existing build
branches, but have not been compiled on this Mac.
