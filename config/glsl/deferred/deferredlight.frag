// Deferred lighting: shades the g-buffer with the base light (ambient, glow,
// AO, sunlight, fog) and/or a batch of point or spot lights. Assembled after
// deferredlight_defs.glsl, deferredlight_decls.glsl and shared/smfilter.glsl.
//
// deferredlightvariantshader (config/glsl/deferred.cfg) defines:
//   DL_ROW              variant row, -1 for the parent (see deferredlight_defs.glsl)
//   DL_NUMLIGHTS        lights in the batch, 0-8
//   DL_NUMSPLITS        CSM splits, 0-8
//   DL_NUMRH            radiance hint splits, 0-4
//   and one define per type letter (deferredlighttype in deferred.cfg):
//   DL_LIGHTSHADOW p, DL_CSM c, DL_CSMCOLOR C, DL_AO a, DL_AOSUN A, DL_RH r,
//   DL_RHBLEND h, DL_MINIMAP m, DL_MSAA M, DL_SAMPLE1 O, DL_RESOLVE R,
//   DL_SAMPLESHADING S, DL_EDGEDETECT T, DL_AVATARVARIANTS d, DL_NODISTBIAS D,
//   DL_SPECTOGGLE z, SMFILTER_GATHER5 G, SMFILTER_GATHER3 g, SMFILTER_BILINEAR5 E,
//   SMFILTER_BILINEAR3 F, SMFILTER_ROTATED f
//   and the engine state: MSAA_SAMPLES, USEPACKNORM, GHASSTENCIL,
//   GDEPTH_FORMAT, USETEXGATHER, GLEXT_SAMPLES_IDENTICAL, AVATAR_SHADOW_BIAS,
//   AVATAR_SHADOW_DIST.
//   DL_RHPROBE replaces main with the rhprobe test output (rhprobeshader).

#ifdef DL_CSM
vec3 getcsmtc(vec3 pos, float distbias)
{
    vec3 tc, offset;
    pos = csmmatrix * pos;
    pos.z -= distbias;
    tc.z = csmz.x + pos.z*csmz.y;
#if DL_NUMSPLITS > 0
    DL_CSM_SPLIT(0)
#endif
#if DL_NUMSPLITS > 1
    DL_CSM_SPLIT(1)
#endif
#if DL_NUMSPLITS > 2
    DL_CSM_SPLIT(2)
#endif
#if DL_NUMSPLITS > 3
    DL_CSM_SPLIT(3)
#endif
#if DL_NUMSPLITS > 4
    DL_CSM_SPLIT(4)
#endif
#if DL_NUMSPLITS > 5
    DL_CSM_SPLIT(5)
#endif
#if DL_NUMSPLITS > 6
    DL_CSM_SPLIT(6)
#endif
#if DL_NUMSPLITS > 7
    DL_CSM_SPLIT(7)
#endif
            offset = vec3(-16384.0);
    DL_CSM_CLOSE
    return tc + offset;
}

#ifdef DL_RH
#ifdef DL_RHBLEND
// Adds w times one split's four hint texels at tc (that split's box
// coordinates, as DL_RH_SPLIT computes them) to the running sums. The RH
// textures have no mipmaps, so fetching under a branch is safe.
void addrhsplit(vec3 tc, float layer, float w, inout vec4 shr, inout vec4 shg, inout vec4 shb, inout vec4 sha)
{
    tc.xy += 0.5;
    tc.z = tc.z * DL_RH_SCALE + layer;
    shr += w*texture(tex6, tc);
    shg += w*texture(tex7, tc);
    shb += w*texture(tex8, tc);
    sha += w*texture(tex9, tc);
}
#endif

vec4 getrhlight(vec3 pos, vec3 norm)
{
#ifdef DL_RHBLEND
    // Each split fades into the next coarser one over rhblend cells inside
    // its faces, and the last one into an empty hint (renderlights.cpp,
    // radiancehints::bindparams). The weights sum to 1 and the hints are
    // linear, so blending the raw texels and decoding once is exact.
    vec3 tc;
    float w, rest = 1.0;
    vec4 shr = vec4(0.0), shg = vec4(0.0), shb = vec4(0.0), sha = vec4(0.0);
    pos += norm*rhnudge;
    DL_RH_BLEND(0, DL_RH_OFFSET0)
#if DL_NUMRH > 2
    DL_RH_BLEND(1, DL_RH_OFFSET1)
#endif
#if DL_NUMRH > 3
    DL_RH_BLEND(2, DL_RH_OFFSET2)
#endif
    // The last split fades to an empty hint (RH_ZERO in gi/rh_out.glsl: 0.5
    // in the biased rgb), so the GI thins out towards the edge of the volume
    // instead of stopping at it.
    DL_RH_BLEND(DL_RH_LAST, DL_RH_OFFSETLAST)
    vec3 empty = vec3(0.5*rest);
    shr.rgb += empty;
    shg.rgb += empty;
    shb.rgb += empty;
    sha.rgb += empty;
#else
    vec3 tc;
    float offset;
    pos += norm*rhnudge;
#if DL_NUMRH > 0
    DL_RH_SPLIT(0, DL_RH_OFFSET0)
#endif
#if DL_NUMRH > 1
    DL_RH_SPLIT(1, DL_RH_OFFSET1)
#endif
#if DL_NUMRH > 2
    DL_RH_SPLIT(2, DL_RH_OFFSET2)
#endif
#if DL_NUMRH > 3
    DL_RH_SPLIT(3, DL_RH_OFFSET3)
#endif
            tc = vec3(4.0);
    DL_RH_CLOSE
    tc.xy += 0.5;
    tc.z = tc.z * DL_RH_SCALE + offset;
    vec4 shr = texture(tex6, tc), shg = texture(tex7, tc), shb = texture(tex8, tc), sha = texture(tex9, tc);
#endif
    shr.rgb -= 0.5;
    shg.rgb -= 0.5;
    shb.rgb -= 0.5;
    sha.rgb -= 0.5;
    vec4 basis = vec4(norm*-(1.023326*0.488603/3.14159*2.0), (0.886226*0.282095/3.14159));
    return clamp(vec4(dot(basis, shr), dot(basis, shg), dot(basis, shb), min(dot(basis, sha), norm.z + 1.0)), 0.0, 1.0);
}
#endif
#endif

#ifdef DL_RHPROBE
// rhprobe (DEBUG_UTILS, renderlights.cpp): the radiance hint light at the
// point and normal rhprobe.vert passes in, one point per pixel.
in vec3 probepos, probenorm;
void main(void)
{
    fragcolor = getrhlight(probepos, probenorm);
}
#else
void main(void)
{
    // Names that are aliases rather than variables in some configurations.
    // A #define emits no code, so these are set once, up front.
#ifdef DL_DISTBIAS_CONST
    #define distbias DL_DISTBIAS_CONST
#endif
#if !USEPACKNORM
    #define glowscale normal.a
#endif

#ifdef DL_MSAA
#ifdef DL_RESOLVE
    // Shade every sample and average them.
#ifdef DL_EDGEDETECT
    bool shouldresolve = true;
    MSAA_EDGE_DETECT(shouldresolve = false;)
#endif

    #define gfetch(sampler, coords) texelFetch(sampler, ivec2(coords), sampleidx)

    vec4 resolved = vec4(0.0);
    #define accumlight(light) resolved.rgb += light
    #define accumalpha(alpha) resolved.a += alpha

#if DL_USEAO
    float ao = texture(tex5, gl_FragCoord.xy*aoscale).r;
#endif

    for(int sampleidx = 0; sampleidx < MSAA_SAMPLES; sampleidx++)
    {
#else
#ifdef DL_EDGEDETECT
    MSAA_EDGE_DETECT(discard;)
#endif

    #define gfetch(sampler, coords) texelFetch(sampler, ivec2(coords), DL_SAMPLE)

    #define accumlight(light) fragcolor.rgb = light
    #define accumalpha(alpha) fragcolor.a = alpha
#endif
#else
    #define gfetch(sampler, coords) texture(sampler, coords)

    #define accumlight(light) fragcolor.rgb = light
    #define accumalpha(alpha) fragcolor.a = alpha
#endif

#if DL_EARLYFETCH
    vec4 normal = gfetch(tex1, gl_FragCoord.xy);

#if DL_TRANSPARENT
    DL_EMPTYDISCARD

    normal.xyz = normal.xyz*2.0 - 1.0;
#if USEPACKNORM
    float alpha = dot(normal.xyz, normal.xyz);
    normal.xyz *= inversesqrt(alpha);
#if DL_BASELIGHT
    GNORMAL_UNPACK_SCALE(alpha)
#endif
#else
    #define alpha normal.a
#endif

    vec4 diffuse = gfetch(tex0, gl_FragCoord.xy);
#if DL_BASELIGHT
    vec3 glow = gfetch(tex2, gl_FragCoord.xy).rgb * darknessenv.y;
#endif
#else
#if DL_BASELIGHT
    float alpha = float(normal.x + normal.y != 0.0);
#else
    #define alpha 1.0
#endif

    normal.xyz = normal.xyz*2.0 - 1.0;
#if USEPACKNORM
    float glowscale = dot(normal.xyz, normal.xyz);
    normal.xyz *= inversesqrt(glowscale);
    GNORMAL_UNPACK_SCALE(glowscale)
#endif

    vec4 diffuse = gfetch(tex0, gl_FragCoord.xy);
#if DL_BASELIGHT
    vec3 glow = diffuse.rgb * (1.0 - glowscale) * darknessenv.y;
#endif
    diffuse.rgb *= glowscale;
#endif
#endif

    // The surface position; the base light alone only needs fog distance.
#ifdef DL_MINIMAP
#if DL_POSLIGHTS
    float depth = GDEPTH_UNPACK_ORTHO(gfetch(tex3, gl_FragCoord.xy));
    vec3 pos = (worldmatrix * vec4(gl_FragCoord.xy, depth, 1.0)).xyz;
#endif
#elif DL_POSLIGHTS
    GDEPTH_UNPACK_POS(depth, pos, gfetch(tex3, gl_FragCoord.xy), gl_FragCoord.xy)
#if DL_EARLYFETCH
    float fogcoord = length(camera - pos.xyz);
#endif
#if DL_POSLIGHTS > 1
    GSPEC_UNPACK(camera, pos, normal, diffuse)
#endif
#else
    float depth = GDEPTH_UNPACK(gfetch(tex3, gl_FragCoord.xy));
#if DL_BASELIGHT
    float fogcoord = -depth*length(vec3(gl_FragCoord.xy*radialfogscale.xy + radialfogscale.zw, 1.0));
#endif
#endif

#if DL_BASELIGHT
    vec3 light = lightscale.rgb;
#ifdef DL_RH
    vec4 rhlight = getrhlight(pos.xyz, normal.xyz);
    light += rhlight.a * skylightcolor;
#endif
    light *= diffuse.rgb;

#if DL_USEAO
#if !defined(DL_MSAA) || !defined(DL_RESOLVE)
    float ao = texture(tex5, gl_FragCoord.xy*aoscale).r;
#endif
#ifdef DL_AVATARVARIANTS
    #define aomask ao
#else
    float avatarmask = step(fogcoord, AVATAR_SHADOW_DIST) * DL_AVATARBIT;
    float aomask = clamp(ao + avatarmask, 0.0, 1.0);
#endif
    light *= aoparams.x + aomask*aoparams.y;
#endif
    light += glow * lightscale.a;
#else
    vec3 light = vec3(0.0);
#endif

#if DL_SHADOWS > 1
    DL_DISTBIAS
#endif

#ifdef DL_CSM
#ifdef DL_RH
    vec3 sunlight = rhlight.rgb * giscale * diffuse.rgb;
#endif
    float sunfacing = dot(sunlightdir, normal.xyz);
    if(sunfacing > 0.0)
    {
#if DL_SHADOWS == 1
        DL_DISTBIAS
#endif
        vec3 csmtc = getcsmtc(pos.xyz, distbias);
        sunshadowtype sunoccluded = filtersunshadow(csmtc);
#ifdef DL_MINIMAP
        light += diffuse.rgb*sunfacing * sunlightcolor * sunoccluded;
#else
#if DL_POSLIGHTS == 1
        GSPEC_UNPACK(camera, pos, normal, diffuse)
#endif
        float sunspec = pow(clamp(sunfacing*facing - dot(camdir, sunlightdir), 0.0, 1.0), gloss) * specscale;
#ifdef DL_RH
        sunlight += (diffuse.rgb*sunfacing + sunspec) * sunoccluded;
#else
#if DL_USEAOSUN
        sunoccluded *= aoparams.z + aomask*aoparams.w;
#endif
        light += (diffuse.rgb*sunfacing + sunspec) * sunoccluded * sunlightcolor;
#endif
#endif
    }
#ifdef DL_RH
#if DL_USEAOSUN
    sunlight *= aoparams.z + aomask*aoparams.w;
#endif
    light += sunlight * sunlightcolor;
#endif
#endif

    // The batch, unrolled; DL_LIGHT is in deferredlight_defs.glsl.
#if DL_NUMLIGHTS > 0
    DL_LIGHT(0)
#endif
#if DL_NUMLIGHTS > 1
    DL_LIGHT(1)
#endif
#if DL_NUMLIGHTS > 2
    DL_LIGHT(2)
#endif
#if DL_NUMLIGHTS > 3
    DL_LIGHT(3)
#endif
#if DL_NUMLIGHTS > 4
    DL_LIGHT(4)
#endif
#if DL_NUMLIGHTS > 5
    DL_LIGHT(5)
#endif
#if DL_NUMLIGHTS > 6
    DL_LIGHT(6)
#endif
#if DL_NUMLIGHTS > 7
    DL_LIGHT(7)
#endif

#ifdef DL_MINIMAP
    accumlight(light);
#if DL_BASELIGHT
    accumalpha(alpha);
#else
    accumalpha(0.0);
#endif
#elif DL_EARLYFETCH
    float foglerp = clamp(exp2(fogcoord*fogdensity.x)*fogdensity.y, 0.0, 1.0);
#if DL_BASELIGHT
    accumlight(mix(fogcolor*alpha, light, foglerp));
    accumalpha(alpha);
#else
    accumlight(light*foglerp);
    accumalpha(0.0);
#endif
#else
    accumlight(light);
    accumalpha(0.0);
#endif

#ifdef DL_RESOLVE
#ifdef DL_EDGEDETECT
        if(!shouldresolve) break;
#endif
    }

#ifdef DL_EDGEDETECT
    if(shouldresolve)
#endif
    resolved *= DL_RESOLVE_SCALE;
    fragcolor = resolved;
#endif
}
#endif
