# To-do List

## General Stuff

- Ability to stack shots to make them bigger, etc
- Occupation map to displace things like grass, etc? (rewrite grass first?)
- Fix triplanar shader activation method
- Proper detail texture support for all textures
- Redo crosshairs (currently aliased and poorly mipmaps) [SDF?] - https://opengameart.org/content/some-hud-elements-icons-and-crosshairs
- Rail grinding system
- Underwater bubbles
- Better moderator console in-game
- Graph widget for UI
- Refactor sound mapping system to no longer need unmapped sounds
- Cleanup editmode helpers (partially done)
- Fix dropped items? (hotfix with virtual entities currently)
- Navmesh for AI based on octree?
- Minimap entity with selectable areas that gets switched between?
- Ability for UI to access internal engine textures (FBOs etc) (replace `debug*` stuff)
- Blendmap mirror command
- Add more texture controls to modelstate
- Password prompt for the server list
- Support for wind on players, replace wavepush with it
- Documentation - start with textures/composites, models, and variables
- Team swapping from spectator does a request rather than choosing properly
- A way to programmatically cull/rehash unused mapmodels/sounds/decals
- Variant of noclip that works only on projectiles?
- Rewrite master server with HTTP and JSON
- Support for saving server passwords?
- Replace "on_*" aliases with proper event system
- Better bounding checker with more than just spheres and cubes
- Cubescript accessors for models
- Explore replacing `fogintensity` with map/volume specific settings.

## Actors

- Shooting range target?
- Ability to destroy mapmodels (break/explode/etc)
    - Allow linking actor to mapmodel to control it?
    - Passive actor?

## Modes / Mutators

- Lobby
- Campaign
- Sandbox
- "Gun game" mutator where you cycle through weapons until the winner gets a melee kill
- Attack bomber-ball switching mechanics (after each possession?)
- FFA defend and control?

## Fixed Items To Monitor Further

- Server randomly picks same map from maphistory
- Texture unloading/reloading, etc during resetgl
- Get map from person who suggested a vote

## Other Ideas

- Make docs more comprehensive and include as submodule in base?
- Refactor codebase to C++17 [refactor-plan.md].
- Proper struct method system for CubeScript?
- Implement libcURL instead of home grown HTTP?
- Minimum number of players threshold for entities?
- Use steam auth instead for accounts?
- Can we fix models with transparent diffuse textures?

### Multi-channel Signed Distance Fields / Functions

- https://github.com/Chlumsky/msdfgen
- https://github.com/Chlumsky/msdf-atlas-gen
- https://alekongame.com/morph/
- https://iquilezles.org/articles/distfunctions/
