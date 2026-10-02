// Normal-mapped world geometry into the g-buffer (bumpshader); see world.frag
// for the defines.
#ifdef WORLD_REFRACT
uniform GFETCH_SAMPLER currentlight, earlydepth;
GDEPTH_UNPACK_DECLS
uniform vec4 refractparams;
uniform float currentdepthscale;
#endif
uniform vec4 colorparams;
uniform sampler2D diffusemap, normalmap;
uniform vec3 rotate;
#if WORLD_MSAADEPTH
uniform float hashid;
#endif
in mat3 world;
#if GDEPTH_FORMAT || BUMP_LINEARDEPTH
GBUFFER_DEPTH_DECLS
#endif
#ifdef WORLD_TRIPLANAR
in vec2 texcoordx, texcoordy, texcoordz;
#ifdef WORLD_DISPLACE
in vec2 dispcoordx0, dispcoordy0, dispcoordz0, dispcoordx1, dispcoordy1, dispcoordz1;
#endif
in vec3 normal, tangentx, tangenty, tangentz;
#ifdef WORLD_DETAIL
uniform sampler2D diffusedetail, normaldetail;
#endif
#else
in vec2 texcoord0;
#ifdef WORLD_DISPLACE
in vec2 dispcoord0, dispcoord1;
#endif
#endif
#if defined(WORLD_PARALLAX) || defined(WORLD_REFLECT) || defined(WORLD_TRIPLANAR)
in vec3 camvec;
#endif
#ifdef WORLD_GLOW
uniform sampler2D glowmap;
#endif
#ifdef WORLD_PULSEGLOW
flat in float pulse;
#endif
#ifdef WORLD_REFLECT
uniform samplerCube envmap;
#endif
#ifdef WORLD_BLEND
uniform float blendlayer;
uniform sampler2D blendmap;
in vec2 texcoord1;
#endif
#ifdef WORLD_DISPLACE
uniform sampler2D dispmap;
#endif

void main(void)
{
#if defined(WORLD_PARALLAX) || defined(WORLD_REFLECT) || defined(WORLD_TRIPLANAR)
    vec3 camvecn = normalize(camvec);
#endif

#define scaledbump(map, tc) mix(vec3(0.5, 0.5, 1.0), texture(map, tc).rgb, normalscale.x)

#ifdef WORLD_TRIPLANAR
    vec3 triblend = max(abs(normal) - triplanarbias.xyz, 0.001);
    triblend *= triblend;
    triblend /= triblend.x + triblend.y + triblend.z;

#define worldx mat3(tangenty, tangentz, normal)
#define worldy mat3(tangentx, tangentz, normal)
#define worldz mat3(tangentx, tangenty, normal)

#ifdef WORLD_DISPLACE
    vec3 dispx = WORLD_DISP(dispcoordx0, dispcoordx1);
    vec3 dispy = WORLD_DISP(dispcoordy0, dispcoordy1);
    vec3 dispz = WORLD_DISP(dispcoordz0, dispcoordz1);
#endif

#ifdef WORLD_PARALLAX
    float heightx = texture(normalmap, WORLD_TC(texcoordx, dispx)).a;
    float heighty = texture(normalmap, WORLD_TC(texcoordy, dispy)).a;
    float heightz = texture(WORLD_NORMALZ, WORLD_TC(texcoordz, dispz)).a;
    vec3 camvect = camvecn * mat3(tangentx, tangenty, tangentz);

    vec2 dtcx = texcoordx + camvect.yz*(heightx*parallaxscale.x + parallaxscale.y);
    vec2 dtcy = texcoordy + camvect.xz*(heighty*parallaxscale.x + parallaxscale.y);
    vec2 dtcz = texcoordz + camvect.xy*(heightz*parallaxscale.x + parallaxscale.y);
#else
#define dtcx texcoordx
#define dtcy texcoordy
#define dtcz texcoordz
#endif

    vec4 diffusex = texture(diffusemap, WORLD_TC(dtcx, dispx));
    vec4 diffusey = texture(diffusemap, WORLD_TC(dtcy, dispy));
    vec4 diffusez = texture(WORLD_DIFFUSEZ, WORLD_TC(dtcz, dispz));
    vec3 bumpx = (scaledbump(normalmap, WORLD_TC(dtcx, dispx))*2.0 - 1.0)*triblend.x;
    vec3 bumpy = (scaledbump(normalmap, WORLD_TC(dtcy, dispy))*2.0 - 1.0)*triblend.y;
    vec3 bumpz = (scaledbump(WORLD_NORMALZ, WORLD_TC(dtcz, dispz))*2.0 - 1.0)*triblend.z;

    vec4 diffuse = diffusex*triblend.x + diffusey*triblend.y + diffusez*triblend.z;
    vec3 bumpw = normalize(worldx*bumpx + worldy*bumpy + worldz*bumpz);

#ifdef WORLD_REFRACT
    vec2 bump = bumpx.xy + bumpy.xy + bumpz.xy;
#endif
#else
#ifdef WORLD_DISPLACE
    vec3 disp = WORLD_DISP(dispcoord0, dispcoord1);
#endif

#ifdef WORLD_PARALLAX
    float height = texture(normalmap, WORLD_TC(texcoord0, disp)).a;
    vec2 pcoord = (camvecn * world).xy;
    WORLD_ROTTEXCOORD(pcoord, rotate)
    vec2 dtc = texcoord0 + pcoord*(height*parallaxscale.x + parallaxscale.y);
#else
#define dtc texcoord0
#endif

    vec4 diffuse = texture(diffusemap, WORLD_TC(dtc, disp));
#if defined(WORLD_ALPHA) && defined(WORLD_ALPHAMASK)
    vec4 normal = texture(normalmap, WORLD_TC(dtc, disp));
    normal.rgb = mix(vec3(0.5, 0.5, 1.0), normal.rgb, normalscale.x);
#define bump normal.rgb
#else
    vec3 bump = scaledbump(normalmap, WORLD_TC(dtc, disp));
#endif

    bump = bump*2.0 - 1.0;
    vec3 bumpw = normalize(world * bump);
#endif

    gcolor.rgb = diffuse.rgb*colorparams.rgb;

#ifdef WORLD_REFLECT
    float invfresnel = dot(camvecn, bumpw);
    vec3 rvec = 2.0*bumpw*invfresnel - camvecn;
    vec3 reflect = texture(envmap, rvec).rgb;
#ifdef WORLD_REFLECT_SPECMAP
    vec3 rmod = envscale.xyz*diffuse.a;
#else
#define rmod envscale.xyz
#endif
    gcolor.rgb = mix(gcolor.rgb, reflect, envmin.xyz + rmod * clamp(1.0 - invfresnel, 0.0, 1.0));
#endif

#ifdef WORLD_GLOW
#ifdef WORLD_TRIPLANAR
    vec3 glowx = texture(glowmap, WORLD_TC(dtcx, dispx)).rgb;
    vec3 glowy = texture(glowmap, WORLD_TC(dtcy, dispy)).rgb;
    vec3 glowz = texture(glowmap, WORLD_TC(dtcz, dispz)).rgb;
    vec3 glow = glowx*triblend.x + glowy*triblend.y + glowz*triblend.z;
#else
    vec3 glow = texture(glowmap, WORLD_TC(dtc, disp)).rgb;
#endif
#ifdef WORLD_PULSEGLOW
    glow *= mix(glowcolor.xyz, pulseglowcolor.xyz, pulse);
#else
    glow *= glowcolor.xyz;
#endif
#ifdef WORLD_ALPHA
    gglow.rgb = glow;
#else
    GGLOW_PACK(glow)
#define packnorm GGLOW_PACKNORM
#endif
#elif defined(WORLD_ALPHA)
    gglow.rgb = vec3(0.0);
#endif

#ifdef WORLD_ALPHA
#ifdef WORLD_REFRACT
    vec2 rtc = bump.xy*refractparams.w;
    float rmask = clamp(currentdepthscale*(lineardepth - dot(gfetch(earlydepth, gl_FragCoord.xy + rtc).rgb, gdepthunpackparams)), 0.0, 1.0);
    vec3 rlight = gfetch(currentlight, gl_FragCoord.xy + rtc*rmask).rgb;
#ifdef WORLD_ALPHAMASK
    gcolor.rgb *= normal.a;
    gglow.rgb += rlight * refractparams.xyz * (1.0 - colorparams.a * normal.a);
#else
    gglow.rgb += rlight * refractparams.xyz * (1.0 - colorparams.a);
#endif
#elif defined(WORLD_ALPHAMASK)
    gcolor.rgb *= normal.a;
#define packnorm normal.a * colorparams.a
#else
#define packnorm colorparams.a
#endif
#endif

#ifdef packnorm
    GNORMAL_PACK_BLEND(bumpw, packnorm)
#else
    GNORMAL_PACK(bumpw)
#endif

#if WORLD_MSAADEPTH
    GBUFFER_PACK_DEPTH_HASH(hashid)
#else
    GBUFFER_PACK_DEPTH
#endif

#ifdef WORLD_BLEND
    float blend = abs(texture(blendmap, texcoord1).r - blendlayer);
    gcolor.rgb *= blend;
    gnormal.rgb *= blend;
    gnormal.a *= blendlayer;
#endif

#if defined(WORLD_SPEC) && defined(WORLD_BLEND)
    GSPEC_PACK_SPEC_BLEND(gloss.x, WORLD_SPECSCALE, blendlayer, blend)
#elif defined(WORLD_SPEC)
    GSPEC_PACK_SPEC(gloss.x, WORLD_SPECSCALE)
#elif defined(WORLD_BLEND)
    GSPEC_PACK_BLEND(gloss.x, blendlayer)
#else
    GSPEC_PACK(gloss.x)
#endif
}
