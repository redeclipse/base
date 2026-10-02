// Radiance hints: a split's border cells, read from the next, coarser split
// (tex3..tex6), fading to RH_ZERO past the depth that split covers.
// Assembled after rh_out.glsl.
uniform sampler3D tex3, tex4, tex5, tex6;
uniform vec3 bordercenter, borderrange, borderscale;
in vec3 texcoord0;

void main(void)
{
    float outside = clamp(borderscale.z*(abs(texcoord0.z - bordercenter.z) - borderrange.z), 0.0, 1.0);
    vec3 tc = vec3(texcoord0.xy, clamp(texcoord0.z, bordercenter.z - borderrange.z, bordercenter.z + borderrange.z));
    rhr = mix(texture(tex3, tc), RH_ZERO, outside);
    rhg = mix(texture(tex4, tc), RH_ZERO, outside);
    rhb = mix(texture(tex5, tc), RH_ZERO, outside);
    rha = mix(texture(tex6, tc), RH_ZERO, outside);
}
