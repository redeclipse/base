// SMAA pass 1, color edge detection: writes left/top edges to rg.
// SMAA 1.0 by Jimenez et al., MIT license: see smaa_defs.glsl, included in front
// of this file by smaashaders (config/glsl/aa.cfg), for the options it reads.

uniform sampler2DRect tex0;
varying vec2 texcoord0;
fragdata(0) vec4 fragcolor;

void main(void)
{
    // Calculate color deltas:
    vec3 C = texture2DRect(tex0, texcoord0).rgb;
    vec3 Cleft = abs(C - texture2DRectOffset(tex0, texcoord0, ivec2(-1, 0)).rgb);
    vec3 Ctop = abs(C - texture2DRectOffset(tex0, texcoord0, ivec2(0, -1)).rgb);
    vec2 delta;
    delta.x = max(max(Cleft.r, Cleft.g), Cleft.b);
    delta.y = max(max(Ctop.r, Ctop.g), Ctop.b);

    // We do the usual threshold:
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
        vec3 Cright = abs(C - texture2DRectOffset(tex0, texcoord0, ivec2(1, 0)).rgb);
        vec3 Cbottom = abs(C - texture2DRectOffset(tex0, texcoord0, ivec2(0, 1)).rgb);
        // Calculate left-left and top-top deltas:
        vec3 Cleftleft = abs(C - texture2DRectOffset(tex0, texcoord0, ivec2(-2, 0)).rgb);
        vec3 Ctoptop = abs(C - texture2DRectOffset(tex0, texcoord0, ivec2(0, -2)).rgb);
        // Calculate the maximum delta in the direct neighborhood:
        vec3 t = max(max(Cright, Cbottom), max(Cleftleft, Ctoptop));
        // Calculate the final maximum delta:
        float maxDelta = max(max(delta.x, delta.y), max(max(t.r, t.g), t.b));

        // Local contrast adaptation in action:
        edges *= step(maxDelta, SMAA_LOCAL_CONTRAST_ADAPTATION_FACTOR * delta);
    }

    fragcolor = vec4(edges, 0.0, 0.0);
}
