// Separable bilateral filters (ao/bilateral.frag, volumetric/bilateral.frag):
// a screen quad, with the depth buffer's coordinates in texcoord0 when it is
// larger than the filtered buffer (BILATERAL_REDUCE), and the reduced AO
// buffer's in texcoord1 when upscaling it (BILATERAL_UPSCALED).
// Uses vtexcoord<n> from config/glsl/shared/screentexcoord.glsl.
attribute vec4 vvertex;
#if BILATERAL_REDUCE
uniform vec4 screentexcoord0;
varying vec2 texcoord0;
#endif
#ifdef BILATERAL_UPSCALED
uniform vec4 screentexcoord1;
varying vec2 texcoord1;
#endif
void main(void)
{
    gl_Position = vvertex;
#if BILATERAL_REDUCE
    texcoord0 = vtexcoord0;
#endif
#ifdef BILATERAL_UPSCALED
    texcoord1 = vtexcoord1;
#endif
}
