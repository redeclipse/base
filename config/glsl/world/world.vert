// World geometry without a normal map; see world.frag for the defines.
attribute vec4 vvertex;
attribute vec3 vnormal;
attribute vec2 vtexcoord0;
uniform mat4 camprojmatrix;
uniform vec2 texgenscroll;
uniform vec3 rotate;
varying vec3 nvec;
#if GDEPTH_FORMAT || WORLD_MSAADEPTH
GBUFFER_DEPTH_DECLS
#endif
#ifdef WORLD_TRIPLANAR
uniform vec2 texgenscale;
varying vec2 texcoordx, texcoordy, texcoordz;
#ifdef WORLD_DISPLACE
varying vec2 dispcoordx0, dispcoordy0, dispcoordz0, dispcoordx1, dispcoordy1, dispcoordz1;
#endif
#ifdef WORLD_DETAIL
uniform vec2 detailscale;
#endif
#else
varying vec2 texcoord0;
#ifdef WORLD_DISPLACE
varying vec2 dispcoord0, dispcoord1;
#endif
#endif
#ifdef WORLD_REFLECT
uniform vec3 camera; varying vec3 camvec;
#endif
#ifdef WORLD_PULSEGLOW
flat varying float pulse;
#endif
#ifdef WORLD_BLEND
uniform vec4 blendmapparams;
varying vec2 texcoord1;
#endif
#if defined(WORLD_PULSEGLOW) || defined(WORLD_DISPLACE)
uniform float millis;
#endif

void main(void)
{
    gl_Position = camprojmatrix * vvertex;
#ifdef WORLD_TRIPLANAR
    texcoordx = vec2(vvertex.y, -vvertex.z) * texgenscale;
    texcoordy = vec2(vvertex.x, -vvertex.z) * texgenscale;
#ifdef WORLD_DETAIL
    texcoordz = vvertex.xy * detailscale;
#else
    texcoordz = vvertex.xy * texgenscale;
#endif
#ifdef WORLD_DISPLACE
    dispcoordx0 = (texcoordx + millis*dispscroll.xy) * dispscale.xy;
    dispcoordy0 = (texcoordy + millis*dispscroll.xy) * dispscale.xy;
    dispcoordz0 = (texcoordz + millis*dispscroll.xy) * dispscale.xy;
    dispcoordx1 = (texcoordx + millis*dispscroll.zw) * dispscale.zw;
    dispcoordy1 = (texcoordy + millis*dispscroll.zw) * dispscale.zw;
    dispcoordz1 = (texcoordz + millis*dispscroll.zw) * dispscale.zw;
#endif
#else
    texcoord0 = vtexcoord0 + texgenscroll;
    WORLD_ROTTEXCOORD(texcoord0, rotate)
#ifdef WORLD_DISPLACE
    dispcoord0 = (texcoord0 + millis*dispscroll.xy) * dispscale.xy;
    dispcoord1 = (texcoord0 + millis*dispscroll.zw) * dispscale.zw;
#endif
#endif
#ifdef WORLD_BLEND
    texcoord1 = (vvertex.xy - blendmapparams.xy)*blendmapparams.zw;
#endif
    nvec = vnormal;

#if GDEPTH_FORMAT || WORLD_MSAADEPTH
    GBUFFER_DEPTH_VERT
#endif

#ifdef WORLD_REFLECT
    camvec = camera - vvertex.xyz;
#endif

#ifdef WORLD_PULSEGLOW
    pulse = abs(fract((millis+pulseglowoffset.x)*pulseglowspeed.x)*2.0 - 1.0);
#endif
}
