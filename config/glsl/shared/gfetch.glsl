// Screen-space fetches from the g-buffer and light buffers, the GLSL
// counterpart of gfetchdefs (without a prefix) in config/glsl/shared.cfg.
// Include with shader_include_fs after defining GFETCH_MS: non-zero when the
// buffers are multisampled ($msaalight for the light buffer). Declare the
// buffers as uniform GFETCH_SAMPLER <names>; and, as gfetchdefs did, the
// depth unpacking uniforms with GDEPTH_UNPACK_DECLS
// (config/glsl/shared/gdepth.glsl).
#if GFETCH_MS
#define GFETCH_SAMPLER sampler2DMS
#define gfetchsample 0
#define gfetch(sampler, coords) texelFetch(sampler, ivec2(coords), gfetchsample)
#define gfetchoffset(sampler, coords, offset) texelFetch(sampler, ivec2(coords) + offset, gfetchsample)
#define gfetchproj(sampler, coords) texelFetch(sampler, ivec2(coords.xy / coords.z), gfetchsample)
#else
#define GFETCH_SAMPLER sampler2DRect
#define gfetch(sampler, coords) texture2DRect(sampler, coords)
#define gfetchoffset(sampler, coords, offset) texture2DRectOffset(sampler, coords, offset)
#define gfetchproj(sampler, coords) texture2DRectProj(sampler, coords)
#endif
