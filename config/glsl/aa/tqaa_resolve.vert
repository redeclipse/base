// TQAA resolve: blends the current frame with the reprojected previous one.
// Engine state, from tqaaresolvedefines in config/glsl/aa.cfg:
//   TQAA_RESOLVE_GATHER  $tqaaresolvegather: nonzero bounds the history with textureGather
// Uses vtexcoord<n> from config/glsl/shared/screentexcoord.glsl.
in vec4 vvertex;
uniform vec4 screentexcoord0;
out vec2 texcoord0;
#if TQAA_RESOLVE_GATHER
out vec2 texcoord1;
#endif
void main(void)
{
    gl_Position = vvertex;
    texcoord0 = vtexcoord0;
#if TQAA_RESOLVE_GATHER
    texcoord1 = vtexcoord0 - 0.5;
#endif
}
