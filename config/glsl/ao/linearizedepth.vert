// Linearizes the g-buffer depth into the reduced AO depth buffer (renderao).
// Uses vtexcoord<n> from config/glsl/shared/screentexcoord.glsl.
attribute vec4 vvertex;
uniform vec4 screentexcoord0;
varying vec2 texcoord0;
void main(void)
{
    gl_Position = vvertex;
    texcoord0 = vtexcoord0;
}
