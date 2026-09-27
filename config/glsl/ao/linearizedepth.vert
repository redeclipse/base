// Linearizes the g-buffer depth into the reduced AO depth buffer (renderao).
attribute vec4 vvertex;
uniform vec4 screentexcoord0;
#define vtexcoord0 (vvertex.xy * screentexcoord0.xy + screentexcoord0.zw)
varying vec2 texcoord0;
void main(void)
{
    gl_Position = vvertex;
    texcoord0 = vtexcoord0;
}
