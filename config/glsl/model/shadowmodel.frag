// Models into the shadow maps: depth only, alpha tested with
// MODEL_ALPHATEST. See shadowmodel.vert.
#ifdef MODEL_ALPHATEST
uniform sampler2D tex0;
uniform float alphatest;
in vec2 texcoord0;
#endif
void main(void)
{
#ifdef MODEL_ALPHATEST
    vec4 color = texture(tex0, texcoord0);
    if(color.a <= alphatest)
        discard;
#endif
}
