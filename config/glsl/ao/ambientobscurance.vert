// Ambient obscurance; see ambientobscurance.frag.
// Uses vtexcoord<n> from config/glsl/shared/screentexcoord.glsl.
attribute vec4 vvertex;
uniform vec4 screentexcoord0;
uniform vec4 screentexcoord1;
varying vec2 texcoord0, texcoord1;
void main(void)
{
    gl_Position = vvertex;
    texcoord0 = vtexcoord0;
    texcoord1 = vtexcoord1;
}
