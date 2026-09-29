// Opaque world geometry into the shadow map: depth only.
attribute vec4 vvertex;
uniform mat4 shadowmatrix;
void main(void)
{
    gl_Position = shadowmatrix * vvertex;
}
