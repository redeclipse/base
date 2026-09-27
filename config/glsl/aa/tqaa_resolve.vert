// TQAA resolve: blends the current frame with the reprojected previous one.
// Engine state, from tqaaresolvedefines in config/glsl/aa.cfg:
//   TQAA_RESOLVE_GATHER  $tqaaresolvegather: nonzero bounds the history with textureGather
attribute vec4 vvertex;
uniform vec4 screentexcoord0;
#define vtexcoord0 (vvertex.xy * screentexcoord0.xy + screentexcoord0.zw)
varying vec2 texcoord0;
#if TQAA_RESOLVE_GATHER
varying vec2 texcoord1;
#endif
void main(void)
{
    gl_Position = vvertex;
    texcoord0 = vtexcoord0;
#if TQAA_RESOLVE_GATHER
    texcoord1 = vtexcoord0 - 0.5;
#endif
}
