// Deferred lighting: what deferredlight_decls.glsl and deferredlight.frag
// derive from the defines deferredlightvariantshader passes (listed in
// deferredlight.frag). Only #defines, so it can precede the #extension lines.

// What the variant row (deferredlightshader, config/glsl/deferred.cfg)
// selects. Row -1 is the parent, a base-light shader.
#if DL_ROW < 0 || DL_ROW % 4 < 2
#define DL_BASELIGHT 1
#else
#define DL_BASELIGHT 0
#endif
#if DL_ROW >= 0 && DL_ROW % 8 >= 4
#define DL_SPOTLIGHT 1
#else
#define DL_SPOTLIGHT 0
#endif
#if DL_ROW >= 8 && DL_ROW <= 15
#define DL_TRANSPARENT 1
#else
#define DL_TRANSPARENT 0
#endif
#if DL_ROW >= 24 && DL_ROW <= 31
#define DL_AVATAR 1
#else
#define DL_AVATAR 0
#endif
// Coloured shadows: 2 for the lights (rows 16-23) and the sun, 1 for the sun.
#if DL_ROW >= 16 && DL_ROW <= 23
#define DL_COLORSHADOW 2
#elif defined(DL_CSMCOLOR) && !DL_TRANSPARENT
#define DL_COLORSHADOW 1
#else
#define DL_COLORSHADOW 0
#endif

// Light counts.
#ifdef DL_CSM
#define DL_SUN 1
#else
#define DL_SUN 0
#endif
#ifdef DL_LIGHTSHADOW
#define DL_SHADOWLIGHTS DL_NUMLIGHTS
#else
#define DL_SHADOWLIGHTS 0
#endif
// Lights that need the world position and the spec: the batch and the sun.
#define DL_POSLIGHTS (DL_NUMLIGHTS + DL_SUN)
// Lights that read a shadow map with a distance bias.
#define DL_SHADOWS (DL_SHADOWLIGHTS + DL_SUN)
// The base light, or several lights, read the g-buffer once up front; a lone
// light reads it inside its own block.
#if DL_BASELIGHT || DL_NUMLIGHTS > 1
#define DL_EARLYFETCH 1
#else
#define DL_EARLYFETCH 0
#endif
#if DL_NUMLIGHTS + DL_BASELIGHT == 1
#define DL_SINGLELIGHT 1
#else
#define DL_SINGLELIGHT 0
#endif

#if defined(DL_AO) && !DL_AVATAR && !DL_TRANSPARENT
#define DL_USEAO 1
#else
#define DL_USEAO 0
#endif
#if defined(DL_AOSUN) && !DL_AVATAR && !DL_TRANSPARENT
#define DL_USEAOSUN 1
#else
#define DL_USEAOSUN 0
#endif

// shared/smfilter.glsl: only shadowed shaders filter.
#if defined(DL_LIGHTSHADOW) || defined(DL_CSM)
#define SMFILTER
#endif
#if DL_COLORSHADOW
#define SMFILTER_COLOR
#endif

// The sample a non-resolving multisample shader reads.
#if defined(DL_SAMPLESHADING)
#define DL_SAMPLE gl_SampleID
#elif defined(DL_SAMPLE1)
#define DL_SAMPLE 1
#else
#define DL_SAMPLE 0
#endif
// 1/MSAA_SAMPLES, spelt as the generator printed it.
#if MSAA_SAMPLES == 2
#define DL_RESOLVE_SCALE 0.5
#elif MSAA_SAMPLES == 4
#define DL_RESOLVE_SCALE 0.25
#elif MSAA_SAMPLES == 8
#define DL_RESOLVE_SCALE 0.125
#elif MSAA_SAMPLES == 16
#define DL_RESOLVE_SCALE 0.0625
#else
#define DL_RESOLVE_SCALE (1.0/float(MSAA_SAMPLES))
#endif

// The avatar stencil bit the g-buffer keeps in normal.a.
#if MSAA_SAMPLES
#define DL_AVATARBIT step(0.75, normal.a)
#else
#define DL_AVATARBIT normal.a
#endif

// The shadow distance bias: a constant (deferredlight.frag #defines distbias
// to it), or DL_DISTBIAS declares it per pixel.
#if DL_AVATAR
#define DL_DISTBIAS_CONST -AVATAR_SHADOW_BIAS
#elif DL_TRANSPARENT || defined(DL_AVATARVARIANTS) || defined(DL_NODISTBIAS) || defined(DL_MINIMAP)
#define DL_DISTBIAS_CONST 0.0
#endif
#ifdef DL_DISTBIAS_CONST
#define DL_DISTBIAS
#elif defined(DL_AO)
#define DL_DISTBIAS float distbias = -AVATAR_SHADOW_BIAS * avatarmask;
#else
#define DL_DISTBIAS float distbias = -AVATAR_SHADOW_BIAS * step(fogcoord, AVATAR_SHADOW_DIST) * DL_AVATARBIT;
#endif

// getcsmtc: split j's test, and the braces closing DL_NUMSPLITS of them.
#define DL_CSM_SPLIT(j) tc.xy = csmtc[j].xy + pos.xy*csmtc[j].z; offset = csmoffset[j]; if(max(abs(tc.x), abs(tc.y)) >= csmtc[j].w) {
#if DL_NUMSPLITS == 1
#define DL_CSM_CLOSE }
#elif DL_NUMSPLITS == 2
#define DL_CSM_CLOSE } }
#elif DL_NUMSPLITS == 3
#define DL_CSM_CLOSE } } }
#elif DL_NUMSPLITS == 4
#define DL_CSM_CLOSE } } } }
#elif DL_NUMSPLITS == 5
#define DL_CSM_CLOSE } } } } }
#elif DL_NUMSPLITS == 6
#define DL_CSM_CLOSE } } } } } }
#elif DL_NUMSPLITS == 7
#define DL_CSM_CLOSE } } } } } } }
#elif DL_NUMSPLITS == 8
#define DL_CSM_CLOSE } } } } } } } }
#else
#define DL_CSM_CLOSE
#endif

// getrhlight: split j's test with its layer offset, the closing braces, and
// the layer scale 1/DL_NUMRH, spelt as the generator printed them.
#define DL_RH_SPLIT(j, offs) tc = rhtc[j].xyz + pos*rhtc[j].w; offset = offs; if(max(max(abs(tc.x), abs(tc.y)), abs(tc.z)) >= rhbounds) {
#if DL_NUMRH == 1
#define DL_RH_SCALE 1.0
#define DL_RH_OFFSET0 0.5
#define DL_RH_CLOSE }
#elif DL_NUMRH == 2
#define DL_RH_SCALE 0.5
#define DL_RH_OFFSET0 0.25
#define DL_RH_OFFSET1 0.75
#define DL_RH_CLOSE } }
#elif DL_NUMRH == 3
#define DL_RH_SCALE 0.333333
#define DL_RH_OFFSET0 0.166667
#define DL_RH_OFFSET1 0.5
#define DL_RH_OFFSET2 0.833333
#define DL_RH_CLOSE } } }
#elif DL_NUMRH == 4
#define DL_RH_SCALE 0.25
#define DL_RH_OFFSET0 0.125
#define DL_RH_OFFSET1 0.375
#define DL_RH_OFFSET2 0.625
#define DL_RH_OFFSET3 0.875
#define DL_RH_CLOSE } } } }
#endif

// getrhlight with split blending (DL_RHBLEND). DL_RH_BLEND(j, offs) adds split
// j's fine weight w (1 inside, 0 one cell inside its faces) times rest, the
// weight no finer split took. The last split fades the same way, to nothing.
#ifdef DL_RHBLEND
#if DL_NUMRH == 2
#define DL_RH_LAST 1
#define DL_RH_OFFSETLAST DL_RH_OFFSET1
#elif DL_NUMRH == 3
#define DL_RH_LAST 2
#define DL_RH_OFFSETLAST DL_RH_OFFSET2
#elif DL_NUMRH == 4
#define DL_RH_LAST 3
#define DL_RH_OFFSETLAST DL_RH_OFFSET3
#endif
#define DL_RH_BLEND(j, offs) if(rest > 0.0) { tc = rhblendtc[j].xyz + pos*rhblendtc[j].w; w = clamp(rhblendedge - max(max(abs(tc.x), abs(tc.y)), abs(tc.z)), 0.0, 1.0); if(w > 0.0) { addrhsplit(rhtc[j].xyz + pos*rhtc[j].w, offs, w*rest, shr, shg, shb, sha); rest -= w*rest; } }
#endif

// DL_LIGHT(j): light j of the batch, in its own scope. The pieces below are
// empty when they don't apply.
//
// A lone light reads the g-buffer normal itself.
#if DL_TRANSPARENT && !GHASSTENCIL
#define DL_EMPTYDISCARD if(normal.x + normal.y == 0.0) discard;
#else
#define DL_EMPTYDISCARD
#endif
#if DL_SINGLELIGHT && USEPACKNORM
#define DL_LIGHT_NORMAL vec4 normal = gfetch(tex1, gl_FragCoord.xy); DL_EMPTYDISCARD normal.xyz = normal.xyz*2.0 - 1.0; float glowscale = dot(normal.xyz, normal.xyz); normal.xyz *= inversesqrt(glowscale);
#elif DL_SINGLELIGHT
#define DL_LIGHT_NORMAL vec4 normal = gfetch(tex1, gl_FragCoord.xy); DL_EMPTYDISCARD normal.xyz = normal.xyz*2.0 - 1.0;
#else
#define DL_LIGHT_NORMAL
#endif
#if DL_SPOTLIGHT
#define DL_SPOT_BEGIN(j) float spotdist = dot(lightdir, spotparams[j].xyz); float spotatten = 1.0 - (1.0 - lightinvdist * spotdist) * spotparams[j].w; if(spotatten > 0.0) {
#define DL_SPOT_END }
#else
#define DL_SPOT_BEGIN(j)
#define DL_SPOT_END
#endif
#if DL_SINGLELIGHT && !defined(DL_MINIMAP)
#define DL_LIGHT_FOGCOORD float fogcoord = length(camera - pos.xyz);
#else
#define DL_LIGHT_FOGCOORD
#endif
#if DL_SHADOWS == 1
#define DL_LIGHT_DISTBIAS DL_DISTBIAS
#else
#define DL_LIGHT_DISTBIAS
#endif
// Declares lightshadow, the attenuated shadow term; without spot or shadow it
// is lightatten itself (deferredlight.frag #defines it).
#if DL_SPOTLIGHT && defined(DL_LIGHTSHADOW)
#define DL_LIGHT_SHADOW(j) vec3 lighttc = getspottc(lightdir, spotdist, spotparams[j], shadowparams[j], shadowoffset[j], distbias * lightpos[j].w); lightshadowtype lightshadow = lightatten * spotatten * filterlightshadow(lighttc);
#elif DL_SPOTLIGHT
#define DL_LIGHT_SHADOW(j) float lightshadow = lightatten * spotatten;
#elif defined(DL_LIGHTSHADOW)
#define DL_LIGHT_SHADOW(j) vec3 lighttc = getshadowtc(lightdir, shadowparams[j], shadowoffset[j], distbias * lightpos[j].w); lightshadowtype lightshadow = lightatten * filterlightshadow(lighttc);
#else
#define DL_LIGHT_SHADOW(j)
#endif
#if DL_SINGLELIGHT && !DL_TRANSPARENT && USEPACKNORM
#define DL_LIGHT_DIFFUSE vec4 diffuse = gfetch(tex0, gl_FragCoord.xy); GNORMAL_UNPACK_SCALE(glowscale) diffuse.rgb *= glowscale;
#elif DL_SINGLELIGHT && !DL_TRANSPARENT
#define DL_LIGHT_DIFFUSE vec4 diffuse = gfetch(tex0, gl_FragCoord.xy); diffuse.rgb *= glowscale;
#elif DL_SINGLELIGHT
#define DL_LIGHT_DIFFUSE vec4 diffuse = gfetch(tex0, gl_FragCoord.xy);
#else
#define DL_LIGHT_DIFFUSE
#endif
#if DL_POSLIGHTS == 1
#define DL_LIGHT_UNPACKSPEC GSPEC_UNPACK(camera, pos, normal, diffuse)
#else
#define DL_LIGHT_UNPACKSPEC
#endif
#ifdef DL_SPECTOGGLE
#define DL_LIGHT_SPECTOGGLE(j) lightspec *= lightcolor[j].a;
#else
#define DL_LIGHT_SPECTOGGLE(j)
#endif
#if DL_SINGLELIGHT
#define DL_LIGHT_FOG float foglerp = clamp(exp2(fogcoord*fogdensity.x)*fogdensity.y, 0.0, 1.0); light *= foglerp;
#else
#define DL_LIGHT_FOG
#endif
#ifdef DL_MINIMAP
#define DL_LIGHT_SHADE(j) light += diffuse.rgb*lightfacing * lightcolor[j].rgb * lightshadow;
#else
#define DL_LIGHT_SHADE(j) DL_LIGHT_UNPACKSPEC float lightspec = pow(clamp(lightfacing*facing - lightinvdist*dot(camdir, lightdir), 0.0, 1.0), gloss) * specscale; DL_LIGHT_SPECTOGGLE(j) light += (diffuse.rgb*lightfacing + lightspec) * lightcolor[j].rgb * lightshadow; DL_LIGHT_FOG
#endif

#define DL_LIGHT(j) { vec3 lightdir = lightpos[j].xyz - pos.xyz * lightpos[j].w; float lightdist2 = dot(lightdir, lightdir); if(lightdist2 < 1.0) { DL_LIGHT_NORMAL float lightfacing = dot(lightdir, normal.xyz); if(lightfacing > 0.0) { float lightinvdist = inversesqrt(lightdist2); DL_SPOT_BEGIN(j) float lightatten = 1.0 - lightdist2 * lightinvdist; DL_LIGHT_FOGCOORD DL_LIGHT_DISTBIAS DL_LIGHT_SHADOW(j) DL_LIGHT_DIFFUSE lightfacing *= lightinvdist; DL_LIGHT_SHADE(j) DL_SPOT_END } } }
