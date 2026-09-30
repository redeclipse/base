// Switches and macros shared by the model shaders, derived from the MODEL_*
// defines modeldefines (config/glsl/model.cfg) passes, one per type letter:
//   MODEL_ALPHATEST    alpha test (a)
//   MODEL_DITHER       dithered alpha test (u, with a)
//   MODEL_ALPHABLEND   alpha blend (A)
//   MODEL_TRANSPARENT  the transparent fragment variant, row 1 (t)
//   MODEL_ENVMAP       environment map (e, with m)
//   MODEL_NORMALMAP    normal map (n)
//   MODEL_MASKS        spec/glow/env/material masks (m)
//   MODEL_DECAL        additive decal (d)
//   MODEL_ALPHADECAL   alpha-blended decal (D)
//   MODEL_SKELETAL     dual-quaternion skinning, vertex row 0 (b), with
//                      MODEL_BONES (1-4)
//   MODEL_DOUBLESIDED  no face culling (c)
//   MODEL_PATTERN      material pattern (p)
//   MODEL_PATTERNMASK  four-material pattern mask (P)
//   MODEL_MIXER        material mixer (x)
//   MODEL_MIXERMASK    four-material mixer mask (X)
//   MODEL_WIND         wind sway (w)
//   MODEL_EFFECT0      effect 0, MDLFX_SHIMMER (0)
//   MODEL_EFFECT1      effect 1, which no MDLFX value selects (1)
// Directives only, so it can go before any include.

#if defined(MODEL_PATTERN) || defined(MODEL_PATTERNMASK)
#define MODEL_PATTERNED
#endif
#if defined(MODEL_MIXER) || defined(MODEL_MIXERMASK)
#define MODEL_MIXED
#endif
// The four-material masks read a fourth material colour.
#if defined(MODEL_PATTERNMASK) || defined(MODEL_MIXERMASK)
#define MODEL_MATERIAL4
#endif
#if defined(MODEL_DECAL) || defined(MODEL_ALPHADECAL)
#define MODEL_DECALED
#endif
#if defined(MODEL_EFFECT0) || defined(MODEL_EFFECT1)
#define MODEL_EFFECT
#endif

// Decodes the tangent-frame quaternion mquat into the vec3s mnormal and,
// with a normal map, mtangent (qtangentdecode).
#ifdef MODEL_NORMALMAP
#define MODEL_QDECODE vec4 qxyz = mquat.xxyy*mquat.yzyz, qxzw = vec4(mquat.xzw, -mquat.w); vec3 mtangent = (qxzw.yzw*mquat.zzy + qxyz.zxy)*vec3(-2.0, 2.0, 2.0) + vec3(1.0, 0.0, 0.0); vec3 mnormal = (qxzw.zwx*mquat.yxx + qxyz.ywz)*vec3(2.0, 2.0, -2.0) + vec3(0.0, 0.0, 1.0);
#else
#define MODEL_QDECODE vec3 mnormal = cross(mquat.xyz, vec3(mquat.y, -mquat.x, mquat.w))*2.0 + vec3(0.0, 0.0, 1.0);
#endif

// texcoord0: vtexcoord0 scrolled by texscroll.xy and rotated by texscroll.z
// about the texture's centre (rotateuv, config/glsl/shared/rotateuv.glsl).
#define MODEL_TEXCOORD texcoord0 = vtexcoord0 + texscroll.xy; if(texscroll.z != 0.0) texcoord0 = rotateuv(texcoord0, texscroll.z, vec2(0.5, 0.5));
