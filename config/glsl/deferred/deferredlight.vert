// Deferred lighting vertex stage, for the parent shader (DL_ROW -1); the
// variant rows reuse it. lightmatrix is the identity for screen quads and
// the camera projection for a light's bounding volume.
in vec4 vvertex;
uniform mat4 lightmatrix;
void main(void)
{
    gl_Position = lightmatrix * vvertex;
}
