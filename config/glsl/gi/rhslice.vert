// A quad over one radiance hint slice, passing the 3D texture coordinates
// through: radiancehintsborder and radiancehintscached.
attribute vec4 vvertex;
attribute vec3 vtexcoord0;
varying vec3 texcoord0;
void main(void)
{
    gl_Position = vvertex;
    texcoord0 = vtexcoord0;
}
