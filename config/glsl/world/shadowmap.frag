// Transparent world geometry into the shadow map (shadowmapshader): the
// colour it tints the light with. Defines, from shadowmapworldvariantshader
// in config/glsl/world.cfg (the type letter in brackets):
//   SM_ALPHA      transparent (a)
//   SM_ALPHAMASK  alpha mask (m), variant row 0
//   SM_NORMALMAP  the mask is in the normal map's alpha (n), variant row 1
uniform vec4 colorparams;
uniform sampler2D diffusemap;
in vec2 texcoord0;

#ifdef SM_NORMALMAP
uniform sampler2D normalmap;
#endif

#ifdef SM_ALPHA
uniform float shadowopacity;
#endif

fragdata(0) vec4 gcolor;

void main(void)
{
    vec4 diffuse = texture(diffusemap, texcoord0);

#ifdef SM_ALPHA
    float alpha = colorparams.a;
#ifdef SM_ALPHAMASK
#ifdef SM_NORMALMAP
    alpha *= texture(normalmap, texcoord0).a;
#else
    alpha *= diffuse.a;
#endif
#endif
#define mask alpha * shadowopacity
#else
#define alpha 1.0
#define mask 1.0
#endif

    gcolor.rgb = mix(vec3(1.0), diffuse.rgb*colorparams.rgb, alpha) * (1.0 - mask);
    gcolor.a = alpha;
}
