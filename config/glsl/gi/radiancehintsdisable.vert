// Radiance hints: the no-GI material in the nearest split (rendernogi,
// renderlights.cpp), from world units to the split's slice.
attribute vec4 vvertex;
uniform vec3 rhcenter;
uniform float rhbounds;
void main(void)
{
    gl_Position = vec4((vvertex.xy - rhcenter.xy)/rhbounds, vvertex.zw);
}
