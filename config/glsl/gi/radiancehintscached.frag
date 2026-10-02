// Radiance hints: copies still-valid hints from the cache (tex7..tex10).
// Assembled after rh_out.glsl.
uniform sampler3D tex7, tex8, tex9, tex10;
in vec3 texcoord0;

void main(void)
{
    rhr = texture(tex7, texcoord0);
    rhg = texture(tex8, texcoord0);
    rhb = texture(tex9, texcoord0);
    rha = texture(tex10, texcoord0);
}
