// World geometry into the reflective shadow map (rsmworld); see rsm.frag.
in vec4 vvertex;
in vec3 vnormal;
in vec2 vtexcoord0;
uniform mat4 rsmmatrix;
uniform vec2 texgenscroll;
uniform vec4 colorparams;
uniform vec3 rsmdir;
out vec4 normal;
out vec2 texcoord0;
#ifdef RSM_BLEND
uniform vec4 blendmapparams;
out vec2 texcoord1;
#endif
void main(void)
{
    vec4 wpos = INSTANCE_POS(vvertex);
    gl_Position = rsmmatrix * wpos;
    texcoord0 = vtexcoord0 + texgenscroll;
#ifdef RSM_BLEND
    texcoord1 = (wpos.xy - blendmapparams.xy)*blendmapparams.zw;
#endif
    vec3 wnormal = INSTANCE_DIR(vnormal);
    normal = vec4(wnormal, dot(wnormal, rsmdir));
}
