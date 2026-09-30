// Model auras: material3 where the model is the visible surface, against the
// early depth sampled at auramask.xy of the screen position with auramask.z
// of slack; where an on-top model (colorscale.a > 0) is hidden behind
// something it only adds colour (alpha 0); elsewhere nothing. Needs
// GFETCH_MS ($msaalight) and GDEPTH_FORMAT for the shared includes. See
// auramodel.vert.
uniform vec4 colorscale;
uniform vec3 material3;
uniform sampler2D tex0;
uniform vec3 auramask;
varying vec2 texcoord0;
varying float lineardepth;

#ifdef MODEL_ALPHATEST
uniform float alphatest;
#endif

uniform GFETCH_SAMPLER earlydepth;
GDEPTH_UNPACK_DECLS

fragdata(0) vec4 fragcolor;

void main(void)
{
#ifdef MODEL_ALPHATEST
    vec4 color = texture2D(tex0, texcoord0);
    if(color.a <= alphatest)
        discard;
#endif

    float depth = -dot(gfetch(earlydepth, gl_FragCoord.xy * auramask.xy).rgb, gdepthunpackparams);
    if(lineardepth <= depth + auramask.z) fragcolor = vec4(material3, 1.0); // model is the visible surface
    else if(colorscale.a > 0.0) fragcolor = vec4(material3, 0.0); // on-top model hidden behind something, it only adds colour
    else discard;
}
