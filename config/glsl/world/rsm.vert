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
    gl_Position = rsmmatrix * vvertex;
    texcoord0 = vtexcoord0 + texgenscroll;
#ifdef RSM_BLEND
    texcoord1 = (vvertex.xy - blendmapparams.xy)*blendmapparams.zw;
#endif
    normal = vec4(vnormal, dot(vnormal, rsmdir));
}
