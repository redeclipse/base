// Bilateral AO filter; see bilateral.frag.
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
