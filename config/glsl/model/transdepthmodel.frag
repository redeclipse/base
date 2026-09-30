// The depth of a transparent model for the transparent depth, as material and
// alpha faces write it in transdepth (transdepthmodelshader). The vertex stage
// is auramodel.vert.
uniform vec4 transclip;
uniform sampler2D tex0;
varying vec2 texcoord0;
varying float lineardepth;

#ifdef MODEL_ALPHATEST
uniform float alphatest;
#endif

fragdata(0) vec4 fragcolor;

void main(void)
{
#ifdef MODEL_ALPHATEST
    vec4 color = texture2D(tex0, texcoord0);
    if(color.a <= alphatest)
        discard;
#endif

    fragcolor = vec4(lineardepth*transclip.z, 0.0, 0.0, 0.0);
}
