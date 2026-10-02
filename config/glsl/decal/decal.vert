// Decals; see decal.frag for the defines.
in vec4 vvertex;
#ifdef DECAL_NORMALMAP
in vec4 vtangent;
out mat3 world;
#else
out vec3 nvec;
#endif
in vec4 vnormal;
in vec3 vtexcoord0;
uniform mat4 camprojmatrix;
out vec4 texcoord0;
#if defined(DECAL_PARALLAX) || defined(DECAL_REFLECT)
uniform vec3 camera;
out vec3 camvec;
#endif
#ifdef DECAL_PULSEGLOW
flat out float pulse;
#endif
#ifdef DECAL_DISPLACE
out vec2 dispcoord0, dispcoord1;
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
