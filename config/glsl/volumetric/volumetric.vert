// Volumetric light: the light's bounding volume; see volumetric.frag. Only
// the parent shader has a vertex stage, the variant rows reuse it.
attribute vec4 vvertex;
uniform mat4 lightmatrix;
void main(void)
{
    gl_Position = lightmatrix * vvertex;
}
