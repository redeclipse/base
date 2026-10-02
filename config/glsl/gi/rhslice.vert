// A quad over one radiance hint slice, passing the 3D texture coordinates
// through: radiancehintsborder and radiancehintscached.
in vec4 vvertex;
in vec3 vtexcoord0;
out vec3 texcoord0;
void main(void)
{
    gl_Position = vvertex;
    texcoord0 = vtexcoord0;
}
