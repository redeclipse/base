// Ambient obscurance: AO_TAPS depth taps around each pixel, taken from a fixed
// offset table reflected by a noise texture.
// Options, from ambientobscuranceshader in config/glsl/ao.cfg:
//   AO_LINEAR       read the reduced linear depth from linearizedepth ("l")
//   AO_DERIVNORMAL  derive normals from depth derivatives ("d")
//   AO_PACKED       write depth beside the result for the bilateral filter ("p")
//   AO_TAPS         1..12
// Engine state, from aoshaderdefines: MSAA_SAMPLES, GDEPTH_FORMAT, AO_DEPTH_FORMAT
// (see linearizedepth.frag). Uses GDEPTH_UNPACK and GDEPTH_PACK from
// config/glsl/shared/gdepth.glsl.
//
// The taps are unrolled on purpose: every tap is its own AO_TAP line below,
// so the code matches the unrolled CubeScript generator it replaced. Keep the
// macros on one line each; line continuation needs GLSL 4.20.
#if MSAA_SAMPLES && !defined(AO_LINEAR)
uniform sampler2DMS tex0;
#define gdepthfetch(sampler, coords) texelFetch(sampler, ivec2(coords), 0)
#else
uniform sampler2DRect tex0;
#define gdepthfetch(sampler, coords) texture(sampler, coords)
#endif
#if MSAA_SAMPLES
uniform sampler2DMS tex1;
#define gnormfetch(sampler, coords) texelFetch(sampler, ivec2(coords), 0)
#else
uniform sampler2DRect tex1;
#define gnormfetch(sampler, coords) texture(sampler, coords)
#endif
uniform vec3 gdepthscale;
uniform vec3 gdepthunpackparams;
uniform sampler2D tex2;
uniform vec3 tapparams;
uniform vec2 contrastparams;
uniform vec4 offsetscale;
uniform float prefilterdepth;
#ifndef AO_DERIVNORMAL
uniform mat3 normalmatrix;
#endif
#ifdef AO_LINEAR
#define depthtc gl_FragCoord.xy
#else
#define depthtc texcoord0
#endif
uniform vec3 gdepthpackparams;
in vec2 texcoord0, texcoord1;
fragdata(0) vec4 fragcolor;

// Depth at one tap: the reduced linear depth as linearizedepth wrote it, or the g-buffer's.
#if defined(AO_LINEAR) && AO_DEPTH_FORMAT == 0
#define AO_TAPDEPTH(coords) dot(gdepthfetch(tex0, coords).rgb, gdepthunpackparams)
#elif defined(AO_LINEAR)
#define AO_TAPDEPTH(coords) gdepthfetch(tex0, coords).r
#else
#define AO_TAPDEPTH(coords) GDEPTH_UNPACK(gdepthfetch(tex0, coords))
#endif

// One tap at table offset (ox, oy), added to obscure.
#define AO_TAP(ox, oy) { vec2 tapoffset = reflect(vec2(ox, oy), noise); tapoffset = depthtc + tapscale * tapoffset; float tapdepth = AO_TAPDEPTH(tapoffset); vec3 v = vec3(tapdepth*(tapoffset*offsetscale.xy + offsetscale.zw) - pos, tapdepth - depth); float dist2 = dot(v, v); obscure += step(dist2, tapparams.z) * max(0.0, dot(v, normal) + depth*1.0e-2) / (dist2 + 1.0e-5); }

void main(void)
{
#if defined(AO_DERIVNORMAL) && AO_DEPTH_FORMAT == 1
    // tex1 holds the full-size g-buffer depth here, not normals.
  #if GDEPTH_FORMAT
    float depth = GDEPTH_UNPACK(gnormfetch(tex1, texcoord0));
    vec2 tapscale = tapparams.xy/depth;
  #else
    float depth = gnormfetch(tex1, texcoord0).r;
    float w = depth*gdepthscale.y + gdepthscale.z;
    depth = gdepthscale.x/w;
    vec2 tapscale = tapparams.xy*w;
  #endif
#elif defined(AO_LINEAR) && AO_DEPTH_FORMAT == 0 || !defined(AO_LINEAR) && GDEPTH_FORMAT == 1
    vec3 packdepth = gdepthfetch(tex0, depthtc).rgb;
    float depth = dot(packdepth, gdepthunpackparams);
    vec2 tapscale = tapparams.xy/depth;
#elif defined(AO_LINEAR) || GDEPTH_FORMAT > 1
    float depth = gdepthfetch(tex0, depthtc).r;
    vec2 tapscale = tapparams.xy/depth;
#else
    float depth = gdepthfetch(tex0, depthtc).r;
    float w = depth*gdepthscale.y + gdepthscale.z;
    depth = gdepthscale.x/w;
    vec2 tapscale = tapparams.xy*w;
#endif
    vec2 dpos = depthtc*offsetscale.xy + offsetscale.zw, pos = depth*dpos;
#ifdef AO_DERIVNORMAL
    vec2 ddepth = vec2(dFdx(depth), dFdy(depth));
    ddepth *= step(abs(ddepth), vec2(4.0));
    vec3 normal;
    normal.xy = (depth+ddepth.yx)*offsetscale.yx;
    normal.z = normal.x*normal.y;
    normal.xy *= -ddepth;
    normal.z -= dot(dpos, normal.xy);
    normal = normalize(normal);
#else
    vec3 normal = gnormfetch(tex1, texcoord0).rgb*2.0 - 1.0;
    float normscale = inversesqrt(dot(normal, normal));
    normal *= normscale > 0.75 ? normscale : 0.0;
    normal = normalmatrix * normal;
#endif
    vec2 noise = texture(tex2, texcoord1).rg*2.0-1.0;
    float obscure = 0.0;
#if AO_TAPS > 0
    AO_TAP(-0.933103, 0.025116)
#endif
#if AO_TAPS > 1
    AO_TAP(-0.432784, -0.989868)
#endif
#if AO_TAPS > 2
    AO_TAP(0.432416, -0.413800)
#endif
#if AO_TAPS > 3
    AO_TAP(-0.117770, 0.970336)
#endif
#if AO_TAPS > 4
    AO_TAP(0.837276, 0.531114)
#endif
#if AO_TAPS > 5
    AO_TAP(-0.184912, 0.200232)
#endif
#if AO_TAPS > 6
    AO_TAP(-0.955748, 0.815118)
#endif
#if AO_TAPS > 7
    AO_TAP(0.946166, -0.998596)
#endif
#if AO_TAPS > 8
    AO_TAP(-0.897519, -0.581102)
#endif
#if AO_TAPS > 9
    AO_TAP(0.979248, -0.046602)
#endif
#if AO_TAPS > 10
    AO_TAP(-0.155736, -0.488204)
#endif
#if AO_TAPS > 11
    AO_TAP(0.460310, 0.982178)
#endif
    obscure = pow(clamp(1.0 - contrastparams.x*obscure, 0.0, 1.0), contrastparams.y);
#ifdef AO_DERIVNORMAL
    vec2 weights = step(abs(ddepth), vec2(prefilterdepth)) * (2.0*fract((gl_FragCoord.xy - 0.5)*0.5) - 0.5);
#else
    vec2 weights = step(fwidth(depth), prefilterdepth) * (2.0*fract((gl_FragCoord.xy - 0.5)*0.5) - 0.5);
#endif
    obscure -= dFdx(obscure) * weights.x;
    obscure -= dFdy(obscure) * weights.y;
#if defined(AO_PACKED) && AO_DEPTH_FORMAT != 0
    fragcolor.rg = vec2(obscure, depth);
#elif defined(AO_PACKED)
  #if !defined(AO_LINEAR) && GDEPTH_FORMAT != 1
    GDEPTH_PACK(packdepth, depth)
  #endif
    fragcolor = vec4(packdepth, obscure);
#else
    fragcolor = vec4(obscure, 0.0, 0.0, 1.0);
#endif
}
