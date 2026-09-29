// G-buffer colour packing and unpacking, the GLSL counterparts of gspecpack,
// gglowpack and unpackspec in config/glsl/shared.cfg. Include with
// shader_include_fs. The shader declares gcolor (the output or a local
// vec3/vec4) for the packing macros.

// Packs glossiness into gcolor.a (gspecpack with no spec).
#define GSPEC_PACK(gloss) gcolor.a = (gloss * 85.0/255.0 + 0.5/255.0);
// Packs glossiness and specular intensity into gcolor.a.
#define GSPEC_PACK_SPEC(gloss, spec) gcolor.a = (gloss * 85.0/255.0 + 0.5/255.0) + 84.0/255.0*clamp(1.5 - 1.5/(1.0 + spec), 0.0, 1.0);

// Folds the glow colour into gcolor.rgb, leaving float glowk declared.
#define GGLOW_PACK(glow) float colork = max(gcolor.r, max(gcolor.g, gcolor.b)), glowk = max(glow.r, max(glow.g, glow.b)); glowk /= glowk + colork + 1.0e-3; gcolor.rgb = mix(gcolor.rgb, glow, glowk) * (1.0 - 2.0*glowk*(glowk - 1.0));
// The normal blend weight GGLOW_PACK leaves, for GNORMAL_PACK_BLEND
// (config/glsl/shared/gnormal.glsl); gglowpack's packnorm.
#define GGLOW_PACKNORM 1.0-glowk

// Unpacks what GSPEC_PACK_SPEC wrote into diffuse.a, for a surface at pos with
// unit normal normal seen from camera; unpackspec. Declares vec3 camdir and
// floats facing, specscale and gloss.
#define GSPEC_UNPACK(camera, pos, normal, diffuse) vec3 camdir = normalize(camera - pos.xyz); float facing = 2.0*dot(normal.xyz, camdir); float specscale = min(3.0*(diffuse.a + 0.5/255.0), 2.999), gloss = floor(specscale); specscale -= gloss; specscale = (0.35 + 0.15*gloss) * specscale / (1.5 - specscale); gloss = 5.0 + 17.0*gloss;

// GSPEC_PACK and GSPEC_PACK_SPEC for a blend-map layer: the glossiness is
// scaled by the layer (blendlayer), the spec by the blend weight.
#define GSPEC_PACK_BLEND(gloss, layer) gcolor.a = layer * (gloss * 85.0/255.0 + 0.5/255.0);
#define GSPEC_PACK_SPEC_BLEND(gloss, spec, layer, blend) gcolor.a = layer * (gloss * 85.0/255.0 + 0.5/255.0) + blend * 84.0/255.0*clamp(1.5 - 1.5/(1.0 + spec), 0.0, 1.0);
