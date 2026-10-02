// SMAA pass 1, luma edge detection: writes left/top edges to rg.
// SMAA 1.0 by Jimenez et al., MIT license: see smaa_defs.glsl, included in front
// of this file by smaashaders (config/glsl/aa.cfg), for the options it reads.

uniform sampler2DRect tex0;
in vec2 texcoord0;
fragdata(0) vec4 fragcolor;

void main(void)
{
    // Calculate lumas:
    float L = SMAA_LUMA(texture(tex0, texcoord0));
    float Lleft = SMAA_LUMA(texture2DRectOffset(tex0, texcoord0, ivec2(-1, 0)));
    float Ltop  = SMAA_LUMA(texture2DRectOffset(tex0, texcoord0, ivec2(0, -1)));

    // We do the usual threshold:
    vec2 delta = abs(L - vec2(Lleft, Ltop));
    vec2 edges = step(SMAA_THRESHOLD, delta);

    // Then discard if there is no edge:
#ifdef SMAA_DISCARD
    if (edges.x + edges.y == 0.0) discard;
    else
#else
    if (edges.x + edges.y > 0.0)
#endif
    {
        // Calculate right and bottom deltas:
        float Lright = SMAA_LUMA(texture2DRectOffset(tex0, texcoord0, ivec2(1, 0)));
        float Lbottom  = SMAA_LUMA(texture2DRectOffset(tex0, texcoord0, ivec2(0, 1)));
        // Calculate the maximum delta in the direct neighborhood:
        vec2 maxDelta = max(delta, abs(L - vec2(Lright, Lbottom)));

        // Calculate left-left and top-top deltas:
        float Lleftleft = SMAA_LUMA(texture2DRectOffset(tex0, texcoord0, ivec2(-2, 0)));
        float Ltoptop = SMAA_LUMA(texture2DRectOffset(tex0, texcoord0, ivec2(0, -2)));
        // Calculate the final maximum delta:
        maxDelta = max(maxDelta, abs(vec2(Lleft, Ltop) - vec2(Lleftleft, Ltoptop)));

        /**
         * Each edge with a delta in luma of less than 50% of the maximum luma
         * surrounding this pixel is discarded. This allows to eliminate spurious
         * crossing edges, and is based on the fact that, if there is too much
         * contrast in a direction, that will hide contrast in the other
         * neighbors.
         * This is done after the discard intentionally as this situation doesn't
         * happen too frequently (but it's important to do as it prevents some
         * edges from going undetected).
         */
        edges *= step(max(maxDelta.x, maxDelta.y), SMAA_LOCAL_CONTRAST_ADAPTATION_FACTOR * delta);
    }

    fragcolor = vec4(edges, 0.0, 0.0);
}
