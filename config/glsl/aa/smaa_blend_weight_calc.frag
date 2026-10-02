// SMAA pass 2, blending weight calculation from the edges of pass 1.
// SMAA 1.0 by Jimenez et al., MIT license: see smaa_defs.glsl, included in front
// of this file by smaashaders (config/glsl/aa.cfg), for the options it reads.

in vec2 texcoord0, texcoord1, texcoord2, texcoord3, texcoord4, texcoord5;
uniform sampler2DRect tex0, tex1, tex2;
uniform vec4 subsamples;
fragdata(0) vec4 fragcolor;

//-----------------------------------------------------------------------------
// Diagonal Search Functions

#if SMAA_MAX_SEARCH_STEPS_DIAG > 0

/**
* Allows to decode two binary values from a bilinear-filtered access.
*/
#define SMAADecodeDiagBilinearAccess(e) (e * abs(6.0 * e - 5.0))

/**
 * These functions allows to perform diagonal pattern searches.
 */
float SMAASearchDiagRightUp(void) {
    vec2 e = texture2DRectOffset(tex0, texcoord0, ivec2(1, -1)).rg;
    vec2 texcoord = texcoord0 + vec2(1.0, -1.0);
    for (int i = 1; i < SMAA_MAX_SEARCH_STEPS_DIAG; i++) {
        if (e.x + e.y < 1.5) break;
        texcoord += vec2(1.0, -1.0);
        e = texture(tex0, texcoord).rg;
    }
    return (texcoord.x - texcoord0.x) - 1.0;
}

float SMAASearchDiagLeftDown(void) {
    vec2 e = texture2DRectOffset(tex0, texcoord0, ivec2(-1, 1)).rg;
    vec2 texcoord = texcoord0 + vec2(-1.0, 1.0);
    for (int i = 1; i < SMAA_MAX_SEARCH_STEPS_DIAG; i++) {
        if (e.x + e.y < 1.5) break;
        texcoord += vec2(-1.0, 1.0);
        e = texture(tex0, texcoord).rg;
    }
    return (texcoord0.x - texcoord.x) - 1.0 + round(e.y);
}

float SMAASearchDiagLeftUp(void) {
    vec2 e = texture2DRectOffset(tex0, texcoord5, ivec2(-1, -1)).rg;
    vec2 texcoord = texcoord5 + vec2(-1.0, -1.0);
    for (int i = 1; i < SMAA_MAX_SEARCH_STEPS_DIAG; i++) {
        if (SMAADecodeDiagBilinearAccess(e.x) + e.y < 1.5) break;
        texcoord += vec2(-1.0, -1.0);
        e = texture(tex0, texcoord).rg;
    }
    return (texcoord5.x - texcoord.x) - 1.0;
}

float SMAASearchDiagRightDown(void) {
    vec2 e = texture2DRectOffset(tex0, texcoord5, ivec2(1, 1)).rg;
    vec2 texcoord = texcoord5 + vec2(1.0, 1.0);
    for (int i = 1; i < SMAA_MAX_SEARCH_STEPS_DIAG; i++) {
        if (SMAADecodeDiagBilinearAccess(e.x) + e.y < 1.5) break;
        texcoord += vec2(1.0, 1.0);
        e = texture(tex0, texcoord).rg;
    }
    return (texcoord.x - texcoord5.x) - 1.0 + round(e.y);
}

/**
 * Similar to SMAAArea, this calculates the area corresponding to a certain
 * diagonal distance and crossing edges 'e'.
 */
vec2 SMAAAreaDiag(vec2 dist, vec2 e, float offset) {
    vec2 texcoord = float(SMAA_AREATEX_MAX_DISTANCE_DIAG) * e + dist;

    // We do a scale and bias for mapping to texel space:
    texcoord = texcoord + 0.5;

    // Diagonal areas are on the second half of the texture:
    texcoord.x += 0.5*float(SMAA_AREATEX_WIDTH);

    // Move to proper place, according to the subpixel offset:
    SMAA_AREA_OFFSET(texcoord, offset);

    return SMAA_AREA(texture(tex1, texcoord));
}

/**
 * This searches for diagonal patterns and returns the corresponding weights.
 */
vec2 SMAACalculateDiagWeights(vec2 e) {
    vec2 weights = vec2(0.0);

    vec2 d;
    d.x = e.r > 0.5 ? SMAASearchDiagLeftDown() : 0.0;
    d.y = SMAASearchDiagRightUp();

    if (d.x + d.y > 2.0) { // d.x + d.y + 1 > 3
        vec4 coords = vec4(0.25 - d.x, d.x, d.y, -0.25 - d.y) + texcoord0.xyxy;
        vec4 c;
        c.xy = texture2DRectOffset(tex0, coords.xy, ivec2(-1, 0)).rg;
        c.zw = texture2DRectOffset(tex0, coords.zw, ivec2( 1, 0)).rg;
        c.xz = SMAADecodeDiagBilinearAccess(c.xz);
        c = round(c);

        vec2 e = 2.0 * c.yw + c.xz;
        e *= step(d, vec2(float(SMAA_MAX_SEARCH_STEPS_DIAG) - 0.5));

        weights += SMAAAreaDiag(d, e, subsamples.z);
    }

    d.x = SMAASearchDiagLeftUp();
    d.y = SMAADecodeDiagBilinearAccess(e.r) > 0.5 ? SMAASearchDiagRightDown() : 0.0;

    if (d.x + d.y > 2.0) { // d.x + d.y + 1 > 3
        vec4 coords = vec4(-d.xx, d.yy) + texcoord0.xyxy;
        vec4 c;
        c.x  = texture2DRectOffset(tex0, coords.xy, ivec2(-1,  0)).g;
        c.y  = texture2DRectOffset(tex0, coords.xy, ivec2( 0, -1)).r;
        c.zw = texture2DRectOffset(tex0, coords.zw, ivec2( 1,  0)).gr;

        vec2 e = 2.0 * c.xz + c.yw;
        e *= step(d, vec2(float(SMAA_MAX_SEARCH_STEPS_DIAG) - 0.5));

        weights += SMAAAreaDiag(d, e, subsamples.w).gr;
    }

    return weights;
}
#endif

//-----------------------------------------------------------------------------
// Horizontal/Vertical Search Functions

/**
 * This allows to determine how much length should we add in the last step
 * of the searches. It takes the bilinearly interpolated edge (see
 * PSEUDO_GATHER4), and adds 0, 1 or 2, depending on which edges and
 * crossing edges are active.
 */
float SMAASearchLength(vec2 e, float bias, float scale) {
    e.r = bias + e.r * scale;
    return 255.0 * texture(tex2, e*vec2(float(SMAA_SEARCHTEX_WIDTH), float(SMAA_SEARCHTEX_HEIGHT))).r;
}

/**
* Horizontal/vertical search functions for the 2nd pass.
*/
float SMAASearchXLeft(void) {
    /**
     * PSEUDO_GATHER4
     * This texcoord has been offset by (-0.25, -0.125) in the vertex shader to
     * sample between edge, thus fetching four edges in a row.
     * Sampling with different offsets in each direction allows to disambiguate
     * which edges are active from the four fetched ones.
     */
    vec2 e = texture(tex0, texcoord1).rg;
    vec2 texcoord = texcoord1;
    for(int i = 1; i < SMAA_MAX_SEARCH_STEPS; i++) {
        if(e.g <= 0.8281 || e.r > 0.0) break; // Is there some edge not activated or a crossing edge that breaks the line?
        texcoord.x -= 2.0;
        e = texture(tex0, texcoord).rg;
    }
    // We correct the previous (-0.25, -0.125) offset we applied:
    // The searches are bias by 1, so adjust the coords accordingly:
    // Disambiguate the length added by the last step:
    return texcoord.x + (0.25 + 1.0) - SMAASearchLength(e, 0.0, 0.5);
}

float SMAASearchXRight(void) {
    vec2 e = texture(tex0, texcoord2).rg;
    vec2 texcoord = texcoord2;
    for(int i = 1; i < SMAA_MAX_SEARCH_STEPS; i++) {
        if(e.g <= 0.8281 || e.r > 0.0) break; // Is there some edge not activated or a crossing edge that breaks the line?
        texcoord.x += 2.0;
        e = texture(tex0, texcoord).rg;
    }
    return texcoord.x - (0.25 + 1.0) + SMAASearchLength(e, 0.5, 0.5);
}

float SMAASearchYUp(void) {
    vec2 e = texture(tex0, texcoord3).rg;
    vec2 texcoord = texcoord3;
    for(int i = 1; i < SMAA_MAX_SEARCH_STEPS; i++) {
        if(e.r <= 0.8281 || e.g > 0.0) break; // Is there some edge not activated or a crossing edge that breaks the line?
        texcoord.y -= 2.0;
        e = texture(tex0, texcoord).rg;
    }
    return texcoord.y + (0.25 + 1.0) - SMAASearchLength(e.gr, 0.0, 0.5);
}

float SMAASearchYDown(void) {
    vec2 e = texture(tex0, texcoord4).rg;
    vec2 texcoord = texcoord4;
    for(int i = 1; i < SMAA_MAX_SEARCH_STEPS; i++) {
        if(e.r <= 0.8281 || e.g > 0.0) break; // Is there some edge not activated or a crossing edge that breaks the line?
        texcoord.y += 2.0;
        e = texture(tex0, texcoord).rg;
    }
    return texcoord.y - (0.25 + 1.0) + SMAASearchLength(e.gr, 0.5, 0.5);
}

/**
 * Ok, we have the distance and both crossing edges. So, what are the areas
 * at each side of current edge?
 */
vec2 SMAAArea(vec2 dist, vec2 e, float offset) {
    // SMAAArea below needs a sqrt, as the areas texture is compressed
    // quadratically:
    // Rounding prevents precision errors of bilinear filtering:
    vec2 texcoord = float(SMAA_AREATEX_MAX_DISTANCE) * round(4.0 * e) + sqrt(dist);

    // We do a scale and bias for mapping to texel space:
    texcoord = texcoord + 0.5;

    // Move to proper place, according to the subpixel offset:
    SMAA_AREA_OFFSET(texcoord, offset);

    return SMAA_AREA(texture(tex1, texcoord));
}

//-----------------------------------------------------------------------------
// Corner Detection Functions

#if SMAA_CORNER_ROUNDING < 100

vec2 SMAADetectHorizontalCornerPattern(vec3 coords, vec2 d) {
    vec2 e;
    if(d.x >= d.y) coords.x = coords.z + 1.0 - 0.5*step(d.x, d.y);
    e.r = texture2DRectOffset(tex0, coords.xy, ivec2(0,  1)).r;
    e.g = texture2DRectOffset(tex0, coords.xy, ivec2(0, -2)).r;
    return clamp(1.0 - (1.0 - SMAA_CORNER_ROUNDING_NORM) * e, 0.0, 1.0);
}

vec2 SMAADetectVerticalCornerPattern(vec3 coords, vec2 d) {
    vec2 e;
    if(d.x >= d.y) coords.y = coords.z + 1.0 - 0.5*step(d.x, d.y);
    e.r = texture2DRectOffset(tex0, coords.xy, ivec2( 1, 0)).g;
    e.g = texture2DRectOffset(tex0, coords.xy, ivec2(-2, 0)).g;
    return clamp(1.0 - (1.0 - SMAA_CORNER_ROUNDING_NORM) * e, 0.0, 1.0);
}

#endif

//-----------------------------------------------------------------------------
// Blending Weight Calculation Pixel Shader (Second Pass)

void main(void)
{
    vec4 weights = vec4(0.0);

    vec2 e = texture(tex0, texcoord5).rg;

    if (e.g > 0.5) { // Edge at north
        #if SMAA_MAX_SEARCH_STEPS_DIAG > 0
        // Diagonals have both north and west edges, so searching for them in
        // one of the boundaries is enough.
        weights.rg = SMAACalculateDiagWeights(e);

        // We give priority to diagonals, so if we find a diagonal we skip
        // horizontal/vertical processing.
        if (weights.r + weights.g == 0.0) {
        #endif

        // Find the distance to the left:
        vec3 coords;
        coords.x = SMAASearchXLeft();
        coords.y = texcoord3.y; // texcoord3.y = texcoord0.y - 0.25 (CROSSING_OFFSET)
        // Find the distance to the right:
        coords.z = SMAASearchXRight();

        // We want the distances to be in pixel units (doing this here allow to
        // better interleave arithmetic and memory accesses):
        vec2 d = round(abs(coords.xz - texcoord0.x));

        // Now fetch the left crossing edges, two at a time using bilinear
        // filtering. Sampling at -0.25 (see CROSSING_OFFSET) enables to
        // discern what value each edge has:
        vec2 e;
        e.x = texture(tex0, coords.xy).r;
        // Fetch the right crossing edges:
        e.y = texture2DRectOffset(tex0, coords.zy, ivec2(1, 0)).r;

        // Ok, we know how this pattern looks like, now it is time for getting
        // the actual area:
        weights.rg = SMAAArea(d, e, subsamples.y);

        #if SMAA_CORNER_ROUNDING < 100
        // Fix corners:
        coords.y = texcoord0.y;
        weights.rg *= SMAADetectHorizontalCornerPattern(coords, d);
        #endif

        #if SMAA_MAX_SEARCH_STEPS_DIAG > 0
        } else
            e.r = 0.0; // Skip vertical processing.
        #endif
    }

    if (e.r > 0.5) { // Edge at west
        // Find the distance to the top:
        vec3 coords;
        coords.y = SMAASearchYUp();
        coords.x = texcoord1.x; // texcoord1.x = texcoord0.x - 0.25;
        // Find the distance to the bottom:
        coords.z = SMAASearchYDown();

        // We want the distances to be in pixel units:
        vec2 d = round(abs(coords.yz - texcoord0.y));

        // Fetch the top crossing edges:
        vec2 e;
        e.x = texture(tex0, coords.xy).g;
        // Fetch the bottom crossing edges:
        e.y = texture2DRectOffset(tex0, coords.xz, ivec2(0, 1)).g;

        // Get the area for this direction:
        weights.ba = SMAAArea(d, e, subsamples.x);

        #if SMAA_CORNER_ROUNDING < 100
        // Fix corners:
        coords.x = texcoord0.x;
        weights.ba *= SMAADetectVerticalCornerPattern(coords, d);
        #endif
    }

    fragcolor = weights;
}
