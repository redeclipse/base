// Model effects (MODEL_EFFECT0/1, model_defs.glsl), for the g-buffer and halo
// fragment shaders: the effect uniforms, the hash-based noise and its
// per-fragment value, which flickers with millis.

#define MODEL_EFFECT_DECLS uniform vec4 effectparams, effectcolor; uniform float millis;
#define MODEL_EFFECT_RAND float rand(vec2 co) { return fract(sin(dot(co.xy, vec2(12.9898, 78.233)))*43758.5453); }
#define MODEL_EFFECT_NOISE rand(texcoord0 + rand(vec2(millis, millis * 3.33)))
