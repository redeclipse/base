// World geometry into the reflective shadow map (rsmworld); see rsm.frag.
attribute vec4 vvertex;
attribute vec3 vnormal;
attribute vec2 vtexcoord0;
uniform mat4 rsmmatrix;
uniform vec2 texgenscroll;
uniform vec4 colorparams;
uniform vec3 rsmdir;
varying vec4 normal;
varying vec2 texcoord0;
#ifdef RSM_BLEND
uniform vec4 blendmapparams;
varying vec2 texcoord1;
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
