// World geometry into the reflective shadow map (rsmworld); see rsm.frag.
in vec4 vvertex;
in vec3 vnormal;
#ifdef RSM_BLEND
in vec3 vtexcoord0;
#else
in vec2 vtexcoord0;
#endif
uniform mat4 rsmmatrix;
uniform vec2 texgenscroll;
uniform vec4 colorparams;
uniform vec3 rsmdir;
out vec4 normal;
out vec2 texcoord0;
#ifdef RSM_BLEND
out vec4 blendpos;
#endif
void main(void)
{
    vec4 wpos = INSTANCE_POS(vvertex);
    gl_Position = rsmmatrix * wpos;
    texcoord0 = vtexcoord0.xy + texgenscroll;
#ifdef RSM_BLEND
    blendpos = vec4(wpos.xyz, vtexcoord0.z);
#endif
    vec3 wnormal = INSTANCE_DIR(vnormal);
    normal = vec4(wnormal, dot(wnormal, rsmdir));
}
