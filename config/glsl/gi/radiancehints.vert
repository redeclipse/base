// Radiance hints: a quad over one slice. rhcenter is the cell's world
// position, rsmcenter where it falls in the reflective shadow map.
in vec4 vvertex;
in vec3 vtexcoord0;
uniform mat4 rsmtcmatrix;
out vec3 rhcenter;
out vec2 rsmcenter;
void main(void)
{
    gl_Position = vvertex;
    rhcenter = vtexcoord0;
    rsmcenter = (rsmtcmatrix * vec4(vtexcoord0, 1.0)).xy;
}
