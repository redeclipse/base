// Models into the reflective shadow map (rsmmodelshader): the sunlit colour
// and the normal, into the outputs of config/glsl/shared/rsm_out.glsl. Type
// defines (model_defs.glsl): MODEL_ALPHATEST (a), MODEL_DOUBLESIDED (c).
varying vec2 texcoord0;
varying vec3 nvec;
uniform vec4 colorscale;
#ifdef MODEL_ALPHATEST
uniform float alphatest;
#endif
uniform vec3 rsmdir;
uniform sampler2D tex0;
void main(void)
{
    vec4 diffuse = texture2D(tex0, texcoord0);
#ifdef MODEL_ALPHATEST
    if(diffuse.a <= alphatest)
        discard;
#endif
    vec3 normal = normalize(nvec);
#ifdef MODEL_DOUBLESIDED
    if(!gl_FrontFacing) normal = -normal;
#endif
    gcolor = vec4(dot(normal, rsmdir)*diffuse.rgb*colorscale.rgb, 1.0);
    gnormal = vec4(normal*0.5+0.5, 0.0);
}
