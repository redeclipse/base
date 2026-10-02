// Screen quad with one texcoord: fxaa, the SMAA edge detection passes and SMAA
// neighborhood blending.
// Uses vtexcoord<n> from config/glsl/shared/screentexcoord.glsl.
in vec4 vvertex;
uniform vec4 screentexcoord0;
out vec2 texcoord0;

void main(void)
{
    gl_Position = vvertex;
    texcoord0 = vtexcoord0;
}
