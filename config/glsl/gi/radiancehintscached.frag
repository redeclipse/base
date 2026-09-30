// Radiance hints: copies still-valid hints from the cache (tex7..tex10).
// Assembled after rh_out.glsl.
uniform sampler3D tex7, tex8, tex9, tex10;
varying vec3 texcoord0;

void main(void)
{
    rhr = texture3D(tex7, texcoord0);
    rhg = texture3D(tex8, texcoord0);
    rhb = texture3D(tex9, texcoord0);
    rha = texture3D(tex10, texcoord0);
}
