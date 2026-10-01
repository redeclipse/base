# Eclipse Recoil

*Be kind, reload*

Eclipse Recoil is a multiplayer first-person shooter built from a fork of Red Eclipse.
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
git clone --recurse-submodules https://github.com/agea/eclipse-recoil.git eclipse-recoil
cd eclipse-recoil
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
pistol and one selected primary are assigned at spawn. A separate HE grenade is granted at each spawn. The expanded loadout also offers AK-47, AWP, MP9, XM1014 and M249 profiles
on existing Red Eclipse weapon slots. Other weapons remain disabled. Primary
fire is available; secondary input is reserved for AWP zoom/scoped fire. Upstream respawn requires primary fire or jump input after
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
client preference `clipspread` enables it in Eclipse Recoil and defaults to off in
the original profile.

### Weapon reference: Desert Eagle and PP-Bizon

The pistol and SMG now use the [supplied weapon statistics sheet](https://docs.google.com/spreadsheets/d/11tDzUNBq9zIX6_9Rel__fdAUezAQzSnh5AVYzCP060c/edit?gid=0)
for torso damage, headshot multiplier, fire interval and magazine/reserve size.
The selected source rows are saved in `config/csgopen/weapon-reference.json`.

| Parameter | Pistol (Desert Eagle) | SMG (PP-Bizon) |
| --- | --- | --- |
| Torso damage, without armor | 53 | 27 |
| Headshot multiplier | 3.9 | 4 |
| Fire interval | 225 ms | 80 ms |
| Magazine / reserve | 7 / 21 | 64 / 128 |
| Hold to fire | No | Yes |

This is the first calibration step. Spread and burst recovery retain the
previously tested prototype values. Source recoil, inaccuracy and mobility use
different units and need a separate conversion. Armor, exponential distance
falloff, wall penetration and fixed recoil patterns are not implemented.
Limb damage and reload times still use upstream behavior. Existing projectile
weapons and models remain; this is not yet a full CS:GO weapon simulation.

### Expanded primary loadout

The loadout menu is enabled in the Eclipse Recoil client profile. Choosing a primary immediately saves the selection for the next spawn; the Desert Eagle is always granted
separately. The menu shows the sidearm as fixed and validates only the selected primary.
The chosen primary is preserved across client restarts. The Eclipse Recoil selection overlay includes only enabled primaries and
Random. Weapon names identify the reference; models, icons and sounds reuse
Red Eclipse assets.

| Reference | Red Eclipse slot | Torso damage | Interval | Magazine / reserve |
| --- | --- | --- | --- | --- |
| AK-47 | Zapper | 36 | 100 ms | 30 / 90 |
| AWP | Rifle | 115 | 1455 ms | 5 / 10 |
| MP9 | Plasma | 26 | 70 ms | 30 / 60 |
| XM1014 | Shotgun | 20 per pellet, 6 pellets | 350 ms | 7 / 32 |
| M249 | Minigun | 32 | 80 ms | 100 / 200 |
| PP-Bizon | SMG | 27 | 80 ms | 64 / 128 |

Each new profile uses a 4x headshot multiplier. AK-47, MP9, XM1014 and M249
are automatic; AWP is semiautomatic. Secondary input operates the AWP scope
without the original charging attack; scoped and unscoped shots share damage,
ammunition and cadence. Other alternate attacks remain blocked on client and
server. Eclipse Recoil allows Minigun in the loadout, whereas the original profile
retains its special-weapon classification.

New primary shots use impact projectiles without ricochet, splash damage,
residual status effects or fragments. They still travel at finite speed. Pistol, SMG and converted energy slots use the Bizon bullet
trail, muzzle and impact effects, including scoped AWP shots. Shotgun and
Minigun retain their original conventional muzzle, projectile and color
effects. All enabled shots stop on impact without ricochet or wall penetration. Energy weapon slots also use
the SMG firing sound; the original profile retains its own effects and sounds. Spread is provisional and recoil/reload timing
remain upstream. The shotgun currently has no CS:GO distance falloff; armor,
penetration and Source recoil patterns remain pending. This is a functional
arsenal prototype rather than a complete weapon simulation.

For all-primary spawn and permission checks, run the dedicated server and:

```sh
scripts/csgopen/dev.sh tdm '-xexec "config/csgopen/arsenal-smoke.cfg"'
```

Expected result remains `SMOKE_DONE FAILURES 0`. This test also runs the
existing respawn and map-change checks after visiting every primary loadout.

### HE grenade with fuse cooking

Each player and bot receives one HE independently of the primary loadout.
Press **G** to select it. Hold primary fire to start the **3-second fuse**;
release to throw. Time spent holding it is deducted from the remaining fuse.
Holding it for the full three seconds detonates it at the holder, rather than
throwing it automatically. Switching, dropping and pickups are blocked while
cooking. Dying with an armed HE triggers immediate detonation at the death
position; carrying an unarmed grenade does not cause a death explosion.

The HE bounces off surfaces and players, and detonates immediately when hit by a bullet. The firearms
retain their no-bounce impacts. HE damage has no burn, status effects or extra
fragment projectiles. Self-damage and friendly fire remain active. Initial
tuning is 180 maximum base damage (scaled from 1800 engine damage) with a
72-unit blast radius and distance attenuation. These are prototype values,
not a verified CS:GO HE reproduction. One grenade is consumed per throw or
in-hand detonation, with no reserve; respawn grants another. The map's grenade
pickups can replenish the single-grenade capacity. The Mine slot supplies the separate circular proximity mine.

### Smoke grenade

Each player and bot receives one smoke grenade in addition to the HE. Press
**H** to select it, hold primary fire to cook the **3-second fuse**, then
release to throw. Holding it to the fuse limit deploys smoke at the holder.
Switching, dropping and pickups are blocked while cooking. Smoke causes no
damage, does not stick, and cannot be detonated by shooting it.

The cloud builds up over one second, lasts **18 seconds** including a two-second
fade, and has a **68-unit radius**. It persists after the thrower's death and
clears on map reset. Bullets pass through the cloud. Bots cannot acquire sight
through a dense cloud. Other player models, attachments and status effects
are also hidden when the viewing line crosses dense smoke, for both teams.
Player halos are disabled in Eclipse Recoil. Labels above weapons, pickups and
dropped loot are hidden; player labels are shown only for teammates.
Teammate labels and player radar indicators require sight without a wall or
dense smoke in between. Bots retain their existing last-seen memory, which can still cause shots. Inside the cloud, a gray overlay obscures the world; outside it,
dense alpha-blended particles provide the visual screen, with several
vertical layers and a central puff to conceal silhouettes. This initial spherical particle
implementation does not simulate smoke filling rooms or flowing around walls;
its shape, opacity and visibility at the edges require manual testing.
Clouds use live projectile replication; joining after deployment currently
does not reconstruct existing clouds. Models and inventory icons still use
the existing Grenade models, with gray tint for smoke and orange for HE.
The smoke inventory uses the otherwise unused Corroder slot. Smoke uses
zero damage and no blast, fragments, status effects or original mine explosion
FX. Respawn replenishes the single smoke; no reserve is granted.

### Circular proximity mine

Press **J** to select the mine and fire toward the ground to place it with a
short throw. Each player and bot gets one at spawn, separately from HE (**G**)
and smoke (**H**). It attaches to geometry, arms fully **1.5 seconds after
landing**, then detects enemies within **32 units** in all directions with
an unobstructed line of sight. The owner and teammates do not trigger it.
Shooting the mine detonates it, including before it arms. Once triggered,
it explodes after 100 ms. Initial tuning is 180 base damage with a 64-unit
blast radius, with self-damage and friendly fire. It has no burn or fragments.
A mine also expires by exploding after 60 seconds; active mines persist after
the owner's death and clear on map reset. These are initial prototype values.

Smoke uses Corroder only as its internal inventory slot; it retains grenade
models, throwing physics, cooking and smoke behavior. Primary loadout selection
excludes this utility. Rocket remains disabled and reserved for a future
grenade launcher. The original Red Eclipse profile retains its own weapons.

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
per profile; do not launch two Eclipse Recoil clients sharing the same profile.

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
