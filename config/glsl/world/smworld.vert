// Opaque world geometry into the shadow map: depth only.
in vec4 vvertex;
uniform mat4 shadowmatrix;
void main(void)
{
    gl_Position = shadowmatrix * vvertex;
}
