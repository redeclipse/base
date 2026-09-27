// SMAA pass 3, neighborhood blending with the weights of pass 2.
// SMAA 1.0 by Jimenez et al., MIT license: see smaa_defs.glsl, included in front
// of this file by smaashaders (config/glsl/aa.cfg), for the options it reads.

varying vec2 texcoord0;
uniform sampler2DRect tex0, tex1;
#ifdef SMAA_SPLIT
uniform sampler2DRect tex2, tex3;
#endif
fragdata(0) vec4 fragcolor;

// Neighborhood Blending Pixel Shader (Third Pass)

void main(void)
{
    // Fetch the blending weights for current pixel:
    vec4 a;
    a.xz = texture2DRect(tex1, texcoord0).rb;
    a.y = texture2DRectOffset(tex1, texcoord0, ivec2(0, 1)).g;
    a.w = texture2DRectOffset(tex1, texcoord0, ivec2(1, 0)).a;

    // Up to 4 lines can be crossing a pixel (one through each edge). We
    // favor blending by choosing the line with the maximum weight for each
    // direction:
    vec2 offset;
    offset.x = a.w > a.z ? a.w : -a.z; // left vs. right
    offset.y = a.y > a.x ? a.y : -a.x; // top vs. bottom

    // Then we go in the direction that has the maximum weight:
    if (abs(offset.x) > abs(offset.y)) // horizontal vs. vertical
        offset.y = 0.0;
    else
        offset.x = 0.0;

    // We exploit bilinear filtering to mix current pixel with the chosen
    // neighbor:
    fragcolor = texture2DRect(tex0, texcoord0 + offset);

#ifdef SMAA_SPLIT
    a.xz = texture2DRect(tex3, texcoord0).rb;
    a.y = texture2DRectOffset(tex3, texcoord0, ivec2(0, 1)).g;
    a.w = texture2DRectOffset(tex3, texcoord0, ivec2(1, 0)).a;
    offset.x = a.w > a.z ? a.w : -a.z;
    offset.y = a.y > a.x ? a.y : -a.x;
    if (abs(offset.x) > abs(offset.y))
        offset.y = 0.0;
    else
        offset.x = 0.0;
    fragcolor = 0.5*(fragcolor + texture2DRect(tex2, texcoord0 + offset));
#endif
}
