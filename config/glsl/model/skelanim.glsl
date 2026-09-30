// Dual-quaternion skeletal animation (skelanimdefs, skelanim), for the
// skeletal vertex variants only: modeldefines (config/glsl/model.cfg)
// includes this file when the type has b, because the engine reads the
// uniform pragma below from the raw text (genuniformlocs, shader.cpp)
// whatever #if surrounds it. Needs MODEL_BONES (1-4) and the raw
// $maxvsuniforms and $maxskelanimdata as MAXVSUNIFORMS and MAXSKELANIMDATA.
//:uniform animdata

// The bone array holds min($maxvsuniforms, $maxskelanimdata) vec4s.
#if MAXVSUNIFORMS < MAXSKELANIMDATA
#define SKELANIM_SIZE MAXVSUNIFORMS
#else
#define SKELANIM_SIZE MAXSKELANIMDATA
#endif
#define SKELANIM_DECLS attribute vec4 vboneweight, vboneindex; uniform vec4 animdata[SKELANIM_SIZE];

// Blends the bones' dual quaternions into vec4 dqreal and dqdual.
#define SKELANIM_BONE(c) index = int(vboneindex.c); dqreal += animdata[index] * vboneweight.c; dqdual += animdata[index+1] * vboneweight.c;
#define SKELANIM_FIRST int index = int(vboneindex.x); vec4 dqreal = animdata[index] * vboneweight.x; vec4 dqdual = animdata[index+1] * vboneweight.x;
#define SKELANIM_NORMALIZE float len = length(dqreal); dqreal /= len; dqdual /= len;
#if MODEL_BONES == 1
#define SKELANIM_BLEND int index = int(vboneindex.x); vec4 dqreal = animdata[index]; vec4 dqdual = animdata[index+1];
#elif MODEL_BONES == 2
#define SKELANIM_BLEND SKELANIM_FIRST SKELANIM_BONE(y) SKELANIM_NORMALIZE
#elif MODEL_BONES == 3
#define SKELANIM_BLEND SKELANIM_FIRST SKELANIM_BONE(y) SKELANIM_BONE(z) SKELANIM_NORMALIZE
#else
#define SKELANIM_BLEND SKELANIM_FIRST SKELANIM_BONE(y) SKELANIM_BONE(z) SKELANIM_BONE(w) SKELANIM_NORMALIZE
#endif

// Declares vec4 mpos, vvertex moved by the blended bones.
#define SKELANIM_POS vec4 mpos = vec4((cross(dqreal.xyz, cross(dqreal.xyz, vvertex.xyz) + vvertex.xyz*dqreal.w + dqdual.xyz) + dqdual.xyz*dqreal.w - dqreal.xyz*dqdual.w)*2.0 + vvertex.xyz, vvertex.w);
// Declares vec4 mquat, the tangent-frame quaternion vtangent rotated by the
// blended bones, for MODEL_QDECODE (model_defs.glsl).
#define SKELANIM_QUAT vec4 mquat = vec4(cross(dqreal.xyz, vtangent.xyz) + dqreal.xyz*vtangent.w + vtangent.xyz*dqreal.w, dqreal.w*vtangent.w - dot(dqreal.xyz, vtangent.xyz));
