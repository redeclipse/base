// G-buffer depth helpers, the GLSL counterparts of gdepthunpack (default
// arguments), gdepthunpackortho and gpackdepth in config/glsl/shared.cfg.
// Include with shader_include_fs after defining GDEPTH_FORMAT ($gdepthformat:
// 0 hyperbolic, 1 packed RGB8, >1 linear float). The shader declares the
// uniforms the macros read: gdepthscale and gdepthunpackparams, and
// gdepthpackparams.

// Linear depth from a g-buffer depth sample.
#if GDEPTH_FORMAT > 1
#define GDEPTH_UNPACK(val) val.r
#elif GDEPTH_FORMAT == 1
#define GDEPTH_UNPACK(val) dot(val.rgb, gdepthunpackparams)
#else
#define GDEPTH_UNPACK(val) gdepthscale.x / (val.r*gdepthscale.y + gdepthscale.z)
#endif

// Declares float depth, the depth of the g-buffer sample val at screen
// position coord, and pos, the surface's world position through the
// uniform mat4 worldmatrix (a vec4 whose .xyz is the position for hyperbolic
// depth, a vec3 otherwise); gdepthunpack with both position arguments.
#if GDEPTH_FORMAT
#define GDEPTH_UNPACK_POS(depth, pos, val, coord) float depth = GDEPTH_UNPACK(val); vec3 pos = (worldmatrix * vec4(depth*coord, depth, 1.0)).xyz;
#else
#define GDEPTH_UNPACK_POS(depth, pos, val, coord) float depth = val.r; vec4 pos = worldmatrix * vec4(coord, depth, 1.0); pos.xyz /= pos.w;
#endif

// Declares vec3 name holding the linear depth val packed into RGB8.
#define GDEPTH_PACK(name, val) vec3 name = val * gdepthpackparams; name = vec3(name.x, fract(name.yz)); name.xy -= name.yz * (1.0/255.0);

// Linear depth from an orthographic (minimap) g-buffer sample, which is never
// hyperbolic; gdepthunpackortho.
#if GDEPTH_FORMAT == 1
#define GDEPTH_UNPACK_ORTHO(val) dot(val.rgb, gdepthunpackparams)
#else
#define GDEPTH_UNPACK_ORTHO(val) val.r
#endif
