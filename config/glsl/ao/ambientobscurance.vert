// Ambient obscurance; see ambientobscurance.frag.
attribute vec4 vvertex;
uniform vec4 screentexcoord0;
#define vtexcoord0 (vvertex.xy * screentexcoord0.xy + screentexcoord0.zw)
uniform vec4 screentexcoord1;
#define vtexcoord1 (vvertex.xy * screentexcoord1.xy + screentexcoord1.zw)
varying vec2 texcoord0, texcoord1;
void main(void)
{
    gl_Position = vvertex;
    texcoord0 = vtexcoord0;
    texcoord1 = vtexcoord1;
}
