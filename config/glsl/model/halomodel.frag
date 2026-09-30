// Model halos: material3 tinted by colorscale, alpha tested with
// MODEL_ALPHATEST, with the effect noise mixed in for MODEL_EFFECT (and
// scaled for MODEL_EFFECT1). See halomodel.vert.
uniform vec4 colorscale;
uniform vec3 material3;
uniform sampler2D tex0;
varying vec2 texcoord0;

#ifdef MODEL_ALPHATEST
uniform float alphatest;
#endif

#ifdef MODEL_EFFECT
MODEL_EFFECT_DECLS
#endif

fragdata(0) vec4 fragcolor;

#ifdef MODEL_EFFECT
MODEL_EFFECT_RAND
#endif

void main(void)
{
    if(colorscale.a <= 0) discard;

    vec4 color = texture2D(tex0, texcoord0);

#ifdef MODEL_ALPHATEST
    if(color.a <= alphatest)
        discard;
#endif

    vec4 outcolor = vec4(material3 * colorscale.rgb, colorscale.a);

#ifdef MODEL_EFFECT
    float effectlevel = (0.5 + (effectparams.x * 2.0 - 1.0) * 0.5) * MODEL_EFFECT_NOISE;
    outcolor.rgb = mix(outcolor.rgb, effectcolor.rgb * effectparams.w, effectlevel);
#endif

#ifdef MODEL_EFFECT1
    outcolor.rgb *= effectparams.x;
#endif

    fragcolor = outcolor;
}
