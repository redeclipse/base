// G-buffer colour packing, the GLSL counterparts of gspecpack and gglowpack
// in config/glsl/shared.cfg. Include with shader_include_fs. The shader
// declares gcolor (the output or a local vec3/vec4).

// Packs glossiness into gcolor.a (gspecpack with no spec).
#define GSPEC_PACK(gloss) gcolor.a = (gloss * 85.0/255.0 + 0.5/255.0);
// Packs glossiness and specular intensity into gcolor.a.
#define GSPEC_PACK_SPEC(gloss, spec) gcolor.a = (gloss * 85.0/255.0 + 0.5/255.0) + 84.0/255.0*clamp(1.5 - 1.5/(1.0 + spec), 0.0, 1.0);

// Folds the glow colour into gcolor.rgb, leaving float glowk declared.
#define GGLOW_PACK(glow) float colork = max(gcolor.r, max(gcolor.g, gcolor.b)), glowk = max(glow.r, max(glow.g, glow.b)); glowk /= glowk + colork + 1.0e-3; gcolor.rgb = mix(gcolor.rgb, glow, glowk) * (1.0 - 2.0*glowk*(glowk - 1.0));
// The normal blend weight GGLOW_PACK leaves, for GNORMAL_PACK_BLEND
// (config/glsl/shared/gnormal.glsl); gglowpack's packnorm.
#define GGLOW_PACKNORM 1.0-glowk
