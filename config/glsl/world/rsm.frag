// World geometry into the reflective shadow map (rsmworld), for global
// illumination: the sunlit colour and the normal. RSM_BLEND (variant row 0)
// weights both by the blend-map layer.
uniform vec4 colorparams;
uniform sampler2D diffusemap;
in vec4 normal;
in vec2 texcoord0;
#ifdef RSM_BLEND
uniform float blendlayer;
uniform sampler2D blendmap;
in vec2 texcoord1;
#endif
fragdata(0) vec4 gcolor;
fragdata(1) vec4 gnormal;
void main(void)
{
    vec4 diffuse = texture(diffusemap, texcoord0);

    gcolor.rgb = normal.w*diffuse.rgb*colorparams.rgb;
    gnormal = vec4(normal.xyz*0.5+0.5, 0.0);

#ifdef RSM_BLEND
    float blend = abs(texture(blendmap, texcoord1).r - blendlayer);
    gcolor.rgb *= blend;
    gcolor.a = blendlayer;
    gnormal *= blend;
#else
    gcolor.a = colorparams.a;
#endif
}
