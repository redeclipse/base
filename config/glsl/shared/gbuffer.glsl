// G-buffer linear depth: the GLSL counterparts of ginterpdepth,
// gdepthpackvert and gdepthpackfrag in config/glsl/shared.cfg. Include with
// shader_include_vs and shader_include_fs after defining GDEPTH_FORMAT
// ($gdepthformat) and USEPACKNORM ($usepacknorm); the packing macros also
// need config/glsl/shared/gdepth.glsl. The outputs come from
// gbuffer_out.glsl or gbuffer_out_depth.glsl, which the gbufferoutputs
// alias picks.
//
// A shader interpolates linear depth when GDEPTH_FORMAT is set, or when it
// keeps per-sample depth for MSAA even without a depth target (the argument
// of ginterpvert and ginterpfrag). It then writes GBUFFER_DEPTH_DECLS in both
// stages and GBUFFER_DEPTH_VERT after setting gl_Position.

#define GBUFFER_DEPTH_DECLS uniform vec2 lineardepthscale; uniform vec3 gdepthpackparams; varying float lineardepth;
#define GBUFFER_DEPTH_VERT lineardepth = dot(lineardepthscale, gl_Position.zw);

// Writes lineardepth to the depth target, if there is one.
#if GDEPTH_FORMAT == 1
#define GBUFFER_PACK_GDEPTH GDEPTH_PACK(packdepth, lineardepth) gdepth.rgb = packdepth; gdepth.a = 0.0;
#elif GDEPTH_FORMAT > 1
#define GBUFFER_PACK_GDEPTH gdepth.r = lineardepth;
#else
#define GBUFFER_PACK_GDEPTH
#endif

// GBUFFER_PACK_GDEPTH, and with USEPACKNORM gnormal.a: 0.0, or for a shader
// that keeps per-sample depth the depth hashed with the hashid uniform
// (gdepthpackfrag without an alpha).
#if USEPACKNORM
#define GBUFFER_PACK_DEPTH GBUFFER_PACK_GDEPTH gnormal.a = 0.0;
#define GBUFFER_PACK_DEPTH_HASH(hashid) GBUFFER_PACK_GDEPTH gnormal.a = GDEPTH_HASH(lineardepth, hashid);
#else
#define GBUFFER_PACK_DEPTH GBUFFER_PACK_GDEPTH
#define GBUFFER_PACK_DEPTH_HASH(hashid) GBUFFER_PACK_GDEPTH
#endif
