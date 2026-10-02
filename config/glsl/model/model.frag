// Models into the g-buffer (modelshader): colour, normal, glow and depth.
// See model_defs.glsl for the type defines; MODEL_TRANSPARENT (row 1) writes
// the glow to gglow for the transparent pass instead of folding it into
// gcolor. Also needs GDEPTH_FORMAT, MSAA_SAMPLES, USEPACKNORM and
// DEBUG_VERTCOLORS. The outputs come from the include the gbufferoutputs
// alias picks.

// Tints diffuse between material3 and material1/material2 by the blend value
// v and the matsplit thresholds, or between material1 and material2 when
// there is no split.
#define MODEL_MATSPLIT(v) if(matsplit.x > 0.0) { if(v < matsplit.x) diffuse.rgb = mix(diffuse.rgb * material3, diffuse.rgb * material1, (matsplit.x - v) * matsplit.z); else if(v > matsplit.y) diffuse.rgb = mix(diffuse.rgb * material3, diffuse.rgb * material2, (v - matsplit.y) * matsplit.z); else diffuse.rgb *= material3; } else diffuse.rgb *= mix(material1, material2, smoothstep(0.0, 1.0, v));
// Tints diffuse with the four materials, weighted by the channels of mask;
// declares vec3 buf.
#define MODEL_MATMASK(buf, mask) vec3 buf = diffuse.rgb; buf = mix(odiffuse.rgb * material1, buf, mask.r); buf = mix(odiffuse.rgb * material2, buf, mask.g); buf = mix(odiffuse.rgb * material3, buf, mask.b); buf = mix(odiffuse.rgb * material4, buf, mask.a); diffuse.rgb = buf;

#ifdef MODEL_NORMALMAP
in mat3 world;
#else
in vec3 nvec;
#endif
#ifdef MODEL_ENVMAP
uniform vec2 envmapscale;
in vec3 camvec;
#endif
uniform vec4 colorscale;
uniform vec3 material1, material2, material3;
uniform vec4 matsplit;
uniform vec2 fullbright;
uniform vec3 maskscale;
#ifdef MODEL_ALPHATEST
uniform float alphatest;
#endif
uniform sampler2D tex0;
#ifdef MODEL_MASKS
uniform sampler2D tex1;
#endif
#ifdef MODEL_ENVMAP
uniform samplerCube tex2;
#endif
#ifdef MODEL_NORMALMAP
uniform sampler2D tex3;
#endif
#ifdef MODEL_DECALED
uniform sampler2D tex4;
#endif
#ifdef MODEL_EFFECT
MODEL_EFFECT_DECLS
#endif
#ifdef MODEL_PATTERNED
in vec2 texcoord1;
uniform sampler2D tex5;
#endif
#ifdef MODEL_MIXED
in vec2 texcoord2;
uniform sampler2D tex6;
#endif
#ifdef MODEL_MATERIAL4
uniform vec3 material4;
#endif
#if GDEPTH_FORMAT || MSAA_SAMPLES
GBUFFER_DEPTH_DECLS
#endif
in vec2 texcoord0;
uniform float aamask;
#if DEBUG_VERTCOLORS
in vec4 vcolordbg;
#endif

#ifdef MODEL_EFFECT
MODEL_EFFECT_RAND
#endif

void main(void)
{
#ifdef MODEL_EFFECT1
    float effectnoise = MODEL_EFFECT_NOISE;
    if(effectnoise > effectparams.x) discard;
#endif

    vec4 diffuse = texture(tex0, texcoord0), odiffuse = diffuse;

#ifdef MODEL_ALPHATEST
#ifdef MODEL_DITHER
    vec2 coords = step(0.5, fract(gl_FragCoord.xy*0.5));
    float dither = 0.5*coords.x + 0.75*coords.y - coords.x*coords.y + 0.25;
    if(diffuse.a <= alphatest * dither)
        discard;
#else
    if(diffuse.a <= alphatest)
        discard;
#endif
#endif

#ifdef MODEL_MASKS
    vec4 masks = texture(tex1, texcoord0);
#endif

#ifdef MODEL_MIXERMASK
    vec4 mixer = texture(tex6, texcoord2);
    MODEL_MATMASK(mixerbuf, mixer)
#elif defined(MODEL_MIXER)
    float mixblend = texture(tex6, texcoord2).r;
    MODEL_MATSPLIT(mixblend)
#endif

#ifdef MODEL_PATTERNMASK
    vec4 pattern = texture(tex5, texcoord1);
    MODEL_MATMASK(patternbuf, pattern)
#else
    float matblend = 0.0;
#ifdef MODEL_MASKS
    matblend = 1.0 - masks.a;
#endif
#ifdef MODEL_PATTERN
    matblend = texture(tex5, texcoord1).r;
#endif

    if(matblend >= 0.0)
    {
        MODEL_MATSPLIT(matblend)
    }
#endif

#ifdef MODEL_NORMALMAP
    vec3 normal = texture(tex3, texcoord0).rgb - 0.5;
#ifdef MODEL_DOUBLESIDED
    if(!gl_FrontFacing) normal.z = -normal.z;
#endif
    normal = normalize(world * normal);
#else
    vec3 normal = normalize(nvec);
#ifdef MODEL_DOUBLESIDED
    if(!gl_FrontFacing) normal = -normal;
#endif
#endif

#ifdef MODEL_EFFECT
    float effectdist = distance(-normal.z, effectparams.x * 2.0 - 1.0), effectbright = 0.0;
    if(effectdist < effectparams.y)
    {
        effectbright = smoothstep(0.0, 1.0, 1.0 - (effectdist * effectparams.z)) * effectcolor.a;
        diffuse.rgb = mix(diffuse.rgb, effectcolor.rgb * effectparams.w, effectbright);
    }
#endif

    gcolor.rgb = diffuse.rgb * colorscale.rgb;

    float spec = maskscale.x;
#ifdef MODEL_MASKS
    float glowk = max(maskscale.z * masks.g, fullbright.y); // glow mask in green channel

    spec *= masks.r; // specmap in red channel
#ifdef MODEL_EFFECT
    spec = max(spec, effectbright * 0.5);
#endif

#ifdef MODEL_ENVMAP
    vec3 camn = normalize(camvec);
    float invfresnel = dot(camn, normal);
    vec3 rvec = 2.0 * invfresnel * normal - camn;
    float emod = envmapscale.x * clamp(invfresnel, 0.0, 1.0) + envmapscale.y;
    vec3 eref = texture(tex2, rvec).rgb;
    gcolor.rgb = mix(gcolor.rgb, eref, emod*masks.b); // envmap mask in blue channel
#endif
#else
    float glowk = fullbright.y;
#endif

#ifdef MODEL_EFFECT
    glowk += effectbright * effectparams.w;
#endif

    GSPEC_PACK_SPEC(maskscale.y, spec)

#ifdef MODEL_DECALED
    vec4 decal = texture(tex4, texcoord0);
#ifdef MODEL_ALPHADECAL
    gcolor.rgb = mix(gcolor.rgb, decal.rgb, decal.a);
#else
    gcolor.rgb += decal.rgb;
#endif
#endif

    float colork = clamp(fullbright.x - glowk, 0.0, 1.0);

#if defined(MODEL_TRANSPARENT) || defined(MODEL_ALPHABLEND)
#ifdef MODEL_ALPHABLEND
#define alpha colorscale.a*diffuse.a
#else
#define alpha colorscale.a
#endif
    gcolor *= alpha;
    gglow.rgb = gcolor.rgb*glowk;
    gcolor.rgb *= colork;
#define packnorm alpha
#else
    GGLOW_PACK_WEIGHT
#define packnorm GGLOW_PACKNORM
#endif

#if DEBUG_VERTCOLORS
    gcolor = vcolordbg;
#endif

    GNORMAL_PACK_BLEND(normal, packnorm)

#if MSAA_SAMPLES
    GBUFFER_PACK_DEPTH_HASH_ALPHA(aamask)
#else
    GBUFFER_PACK_DEPTH_ALPHA(aamask)
#endif
}
