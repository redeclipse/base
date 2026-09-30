// Radiance hints: a quad over one slice. rhcenter is the cell's world
// position, rsmcenter where it falls in the reflective shadow map.
attribute vec4 vvertex;
attribute vec3 vtexcoord0;
uniform mat4 rsmtcmatrix;
varying vec3 rhcenter;
varying vec2 rsmcenter;
void main(void)
{
    gl_Position = vvertex;
    rhcenter = vtexcoord0;
    rsmcenter = (rsmtcmatrix * vec4(vtexcoord0, 1.0)).xy;
}
