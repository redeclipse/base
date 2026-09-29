// World geometry into the reflective shadow map (rsmworld); see rsm.frag.
attribute vec4 vvertex;
attribute vec3 vnormal;
#ifdef RSM_BLEND
attribute vec3 vtexcoord0;
#else
attribute vec2 vtexcoord0;
#endif
uniform mat4 rsmmatrix;
uniform vec2 texgenscroll;
uniform vec4 colorparams;
uniform vec3 rsmdir;
varying vec4 normal;
varying vec2 texcoord0;
#ifdef RSM_BLEND
varying vec4 blendpos;
#endif
void main(void)
{
    gl_Position = rsmmatrix * vvertex;
    texcoord0 = vtexcoord0.xy + texgenscroll;
#ifdef RSM_BLEND
    blendpos = vec4(vvertex.xyz, vtexcoord0.z);
#endif
    normal = vec4(vnormal, dot(vnormal, rsmdir));
}
