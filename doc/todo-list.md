# To-do List

## General Stuff

- Pack versions as major*100000 + minor*1000 + patch instead of major*100 + minor*10 + patch (`CUR_VERSION`), before any patch reaches 10, as 2.1.100 would otherwise read as 3.1.0; it is in master registration, `mastermainver`/`masterminver`, the server list's `version` and the website's version column, and new numbers stay above every old one (all under 1000) so either can be told apart
- A sign-in page served by the master itself, so the remembered sign-in's cookie is set while play.redeclipse.net is the page; Safari caps cookies from a server whose address differs from the page's at 7 days, which cuts the website's remembered sign-ins short there
- Ability to stack shots to make them bigger, etc
- Fix triplanar shader activation method
- Proper detail texture support for all textures
- Rail grinding system
- Underwater bubbles
- Graph widget for UI
- Refactor sound mapping system to no longer need unmapped sounds
- Cleanup editmode helpers (partially done)
- Fix dropped items? (hotfix with virtual entities currently)
- Navmesh for AI based on octree?
- Minimap entity with selectable areas that gets switched between?
- Ability for UI to access internal engine textures (FBOs etc) (replace `debug*` stuff)
- Add more texture controls to modelstate
- Password prompt for the server list
- Support for wind on players, replace wavepush with it
- Documentation - start with textures/composites, models, and variables
- Team swapping from spectator does a request rather than choosing properly
- A way to programmatically cull/rehash unused mapmodels/sounds/decals
- Variant of noclip that works only on projectiles?
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

## Rendering

- Particles: send one record per particle and expand it to a quad on the GPU with an instanced draw, instead of building four vertices per particle on the CPU (profile particle generation in a busy match first)
- Texture arrays for world texture slots, so batches with different slots can merge (slots differ in size and format, which reaches map loading and vslots)
- Conditional render in place of the same-frame occlusion query wait in `renderva.cpp` (`oqwait`); it breaks geometry batching across vertex arrays
- Pack model tangents and world normals as `GL_INT_2_10_10_10_REV` vertex attributes
- `glDrawElementsBaseVertex` to pack model meshes into shared vertex buffers, alongside instanced mapmodels
- `GL_TIMESTAMP` queries so GPU timers can nest
- Instanced draws for repeated static mapmodels, with transforms in a uniform buffer (mapmodels were about 4% of the main thread on Bloodlust on both a fast and a weak machine)
- Sort model and light draws to cut shader program switches; `Shader::bindprograms` was about 6% of the main thread on NVIDIA and 2% on Mesa radeonsi

## Fixed Items To Monitor Further

- Server randomly picks same map from maphistory
- Texture unloading/reloading, etc during resetgl
- Get map from person who suggested a vote

## Other Ideas

- Make docs more comprehensive and include as submodule in base?
- Refactor codebase to C++17 [refactor-plan.md].
- Proper struct method system for CubeScript?
- Minimum number of players threshold for entities?
- Can we fix models with transparent diffuse textures?

### Multi-channel Signed Distance Fields / Functions

- https://github.com/Chlumsky/msdfgen
- https://github.com/Chlumsky/msdf-atlas-gen
- https://alekongame.com/morph/
- https://iquilezles.org/articles/distfunctions/
