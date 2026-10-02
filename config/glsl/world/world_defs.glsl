// Switches and macros shared by world.* and bump.*, derived from the defines
// listed in world.frag. Directives only, so it can go before any include.

// The surface keeps per-sample depth for MSAA (interpolates lineardepth and
// hashes it into gnormal.a): every surface when the lighting is multisampled,
// only opaque ones when just the g-buffer is.
#if MSAA_LIGHT || (MSAA_SAMPLES && !defined(WORLD_ALPHA))
#define WORLD_MSAADEPTH 1
#else
#define WORLD_MSAADEPTH 0
#endif

// A bump-mapped refraction masks the offset by linear depth, so it
// interpolates that too.
#if WORLD_MSAADEPTH || defined(WORLD_REFRACT)
#define BUMP_LINEARDEPTH 1
#else
#define BUMP_LINEARDEPTH 0
#endif

// Refraction reads the light buffer, multisampled with the lighting.
#define GFETCH_MS MSAA_LIGHT

// The textures of the triplanar z axis, the detail ones with WORLD_DETAIL.
#ifdef WORLD_DETAIL
#define WORLD_DIFFUSEZ diffusedetail
#define WORLD_NORMALZ normaldetail
#else
#define WORLD_DIFFUSEZ diffusemap
#define WORLD_NORMALZ normalmap
#endif

// The texture coordinate tc offset by the displacement d, when there is one.
#ifdef WORLD_DISPLACE
#define WORLD_TC(tc, d) tc + d.xy
#else
#define WORLD_TC(tc, d) tc
#endif

// The displacement from the two scrolled dispmap samples at c0 and c1.
#define WORLD_DISP(c0, c1) (texture(dispmap, c0).rgb*dispcontrib.x + texture(dispmap, c1).rgb*dispcontrib.y - (dispcontrib.x+dispcontrib.y)*0.5) * dispcontrib.z

// Rotates the texture coordinate tc by the texture slot rotation rot
// (rottexcoord).
#define WORLD_ROTTEXCOORD(tc, rot) tc = rot.x * tc.xy + rot.yz * tc.yx;

// The spec intensity: the spec map (diffuse alpha) scaled, or the scale.
#ifdef WORLD_SPECMAP
#define WORLD_SPECSCALE diffuse.a*specscale.x
#else
#define WORLD_SPECSCALE specscale.x
#endif
