// The sky into the reflective shadow map: geometry in RSM space.
attribute vec4 vvertex;
uniform mat4 rsmmatrix;
void main(void)
{
    gl_Position = rsmmatrix * vvertex;
}
