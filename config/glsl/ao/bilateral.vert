// Bilateral AO filter; see bilateral.frag.
attribute vec4 vvertex;
#if BILATERAL_REDUCE
uniform vec4 screentexcoord0;
#define vtexcoord0 (vvertex.xy * screentexcoord0.xy + screentexcoord0.zw)
varying vec2 texcoord0;
#endif
#ifdef BILATERAL_UPSCALED
uniform vec4 screentexcoord1;
#define vtexcoord1 (vvertex.xy * screentexcoord1.xy + screentexcoord1.zw)
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
