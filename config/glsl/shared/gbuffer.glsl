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
// stages and GBUFFER_DEPTH_VERT after setting gl_Position. The one
// declaration serves both stages, so it says varying, which the engine
// defines as out for the vertex stage and in for the fragment stage.

#define GBUFFER_DEPTH_DECLS uniform vec2 lineardepthscale; uniform vec3 gdepthpackparams; varying float lineardepth;
#define GBUFFER_DEPTH_VERT lineardepth = dot(lineardepthscale, gl_Position.zw);

// Writes lineardepth to the depth target, if there is one, with alpha in
// gdepth.a for the packed format.
#if GDEPTH_FORMAT == 1
#define GBUFFER_PACK_GDEPTH_ALPHA(alpha) GDEPTH_PACK(packdepth, lineardepth) gdepth.rgb = packdepth; gdepth.a = alpha;
#elif GDEPTH_FORMAT > 1
#define GBUFFER_PACK_GDEPTH_ALPHA(alpha) gdepth.r = lineardepth;
#else
#define GBUFFER_PACK_GDEPTH_ALPHA(alpha)
#endif
#define GBUFFER_PACK_GDEPTH GBUFFER_PACK_GDEPTH_ALPHA(0.0)

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

// The same with a coverage alpha (gdepthpackfrag with an alpha): alpha goes
// to gdepth.a and, with USEPACKNORM, to gnormal.a, hashed with the depth
// when the shader keeps per-sample depth (GDEPTH_HASH_ALPHA).
#if USEPACKNORM
#define GBUFFER_PACK_DEPTH_ALPHA(alpha) GBUFFER_PACK_GDEPTH_ALPHA(alpha) gnormal.a = alpha;
#define GBUFFER_PACK_DEPTH_HASH_ALPHA(alpha) GBUFFER_PACK_GDEPTH_ALPHA(alpha) gnormal.a = GDEPTH_HASH_ALPHA(lineardepth, alpha);
#else
#define GBUFFER_PACK_DEPTH_ALPHA(alpha) GBUFFER_PACK_GDEPTH_ALPHA(alpha)
#define GBUFFER_PACK_DEPTH_HASH_ALPHA(alpha) GBUFFER_PACK_GDEPTH_ALPHA(alpha)
#endif
