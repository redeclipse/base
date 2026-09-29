// TQAA resolve: blends the current frame with the reprojected previous one,
// clamped to the current frame's local luma range.
// Engine state, from tqaaresolvedefines in config/glsl/aa.cfg:
//   MSAA_LIGHT           $msaalight: nonzero reads a multisampled g-buffer depth
//   GDEPTH_FORMAT        $gdepthformat: 0 hyperbolic, 1 packed RGB8, >1 linear float
//   TQAA_RESOLVE_GATHER  $tqaaresolvegather: nonzero bounds the history with textureGather
// Uses GDEPTH_UNPACK from config/glsl/shared/gdepth.glsl.
#if MSAA_LIGHT
uniform sampler2DMS tex2;
#define gfetch(sampler, coords) texelFetch(sampler, ivec2(coords), 0)
#else
uniform sampler2DRect tex2;
#define gfetch(sampler, coords) texture2DRect(sampler, coords)
#endif
uniform vec3 gdepthscale;
uniform vec3 gdepthunpackparams;
uniform vec4 quincunx;
uniform sampler2DRect tex0, tex1;
uniform mat4 reprojectmatrix;
uniform vec2 maxvelocity;
uniform vec2 colorweight;
varying vec2 texcoord0;
#if TQAA_RESOLVE_GATHER
varying vec2 texcoord1;
#endif
fragdata(0) vec4 fragcolor;
void main(void)
{
#if GDEPTH_FORMAT
    float depth = GDEPTH_UNPACK(gfetch(tex2, texcoord0));
    vec4 prevtc = reprojectmatrix * vec4(depth*texcoord0, depth, 1.0);
#else
    // Hyperbolic depth reprojects as sampled, not linearized.
    float depth = gfetch(tex2, texcoord0).r;
    vec4 prevtc = reprojectmatrix * vec4(texcoord0, depth, 1.0);
#endif

    vec2 vel = prevtc.xy/prevtc.w - texcoord0;
    float scale = clamp(maxvelocity.x*inversesqrt(dot(vel, vel) + 1e-6), 0.0, 1.0);

    float mask = 1.0 - texture2DRect(tex0, texcoord0 + quincunx.xy).a;
    vec4 color = texture2DRect(tex0, texcoord0 + mask*quincunx.xy);
    vec4 prevcolor = texture2DRect(tex1, texcoord0 + mask*(quincunx.zw + vel*scale));

#if TQAA_RESOLVE_GATHER
    vec4 l0 = textureGather(tex0, texcoord1, 1);
    vec4 l1 = textureGatherOffset(tex0, texcoord1, ivec2(1, 1), 1);
    float l2 = texture2DRectOffset(tex0, texcoord0, ivec2(1, -1)).g;
    float l3 = texture2DRectOffset(tex0, texcoord0, ivec2(-1, 1)).g;
    vec4 l01min = min(l0, l1), l01max = max(l0, l1);
    l01min.xy = min(l01min.xy, l01min.zw);
    l01max.xy = max(l01max.xy, l01max.zw);
    float lmin = min(min(l01min.x, l01min.y), min(l2, l3));
    float lmax = max(max(l01max.x, l01max.y), max(l2, l3));
#else
    float l0 = texture2DRect(tex0, texcoord0 + vec2(-1.0, -0.5)).g;
    float l1 = texture2DRect(tex0, texcoord0 + vec2( 0.5, -1.0)).g;
    float l2 = texture2DRect(tex0, texcoord0 + vec2( 1.0,  0.5)).g;
    float l3 = texture2DRect(tex0, texcoord0 + vec2(-0.5,  1.0)).g;
    float lmin = min(color.g, min(min(l0, l1), min(l2, l3)));
    float lmax = max(color.g, max(max(l0, l1), max(l2, l3)));
#endif

    float weight = 0.5 - 0.5*clamp((colorweight.x*max(prevcolor.g - lmax, lmin - prevcolor.g) + colorweight.y) / (lmax - lmin + 1e-4), 0.0, 1.0);
    weight *= clamp(1.0 - 2.0*(prevcolor.a - color.a), 0.0, 1.0);
    fragcolor.rgb = mix(color.rgb, prevcolor.rgb, weight);
    fragcolor.a = color.a;
}
