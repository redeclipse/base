// Decals; see decal.frag for the defines.
attribute vec4 vvertex;
#ifdef DECAL_NORMALMAP
attribute vec4 vtangent;
varying mat3 world;
#else
varying vec3 nvec;
#endif
attribute vec4 vnormal;
attribute vec3 vtexcoord0;
uniform mat4 camprojmatrix;
varying vec4 texcoord0;
#if defined(DECAL_PARALLAX) || defined(DECAL_REFLECT)
uniform vec3 camera;
varying vec3 camvec;
#endif
#ifdef DECAL_PULSEGLOW
flat varying float pulse;
#endif
#ifdef DECAL_DISPLACE
varying vec2 dispcoord0, dispcoord1;
#endif
#if defined(DECAL_PULSEGLOW) || defined(DECAL_DISPLACE)
uniform float millis;
#endif

void main(void)
{
    gl_Position = camprojmatrix * vvertex;
    texcoord0.xyz = vtexcoord0;
    texcoord0.w = 3.0*vnormal.w;
#ifdef DECAL_DISPLACE
    dispcoord0 = (texcoord0.xy + millis*dispscroll.xy) * dispscale.xy;
    dispcoord1 = (texcoord0.xy + millis*dispscroll.zw) * dispscale.zw;
#endif

#ifdef DECAL_NORMALMAP
    vec3 bitangent = cross(vnormal.xyz, vtangent.xyz) * vtangent.w;
    // calculate tangent -> world transform
    world = mat3(vtangent.xyz, bitangent, vnormal.xyz);
#else
    nvec = vnormal.xyz;
#endif

#if defined(DECAL_PARALLAX) || defined(DECAL_REFLECT)
    camvec = camera - vvertex.xyz;
#endif

#ifdef DECAL_PULSEGLOW
    pulse = abs(fract(millis*pulseglowspeed.x)*2.0 - 1.0);
#endif
}
