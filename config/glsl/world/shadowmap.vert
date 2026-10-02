// Transparent world geometry into the shadow map (shadowmapshader); see
// shadowmap.frag for the defines.
in vec4 vvertex;

in vec2 vtexcoord0;
uniform vec2 texgenscroll;
uniform vec4 colorparams;
out vec2 texcoord0;

uniform mat4 shadowmatrix;

void main(void)
{
    gl_Position = shadowmatrix * vvertex;

    texcoord0 = vtexcoord0 + texgenscroll;
}
