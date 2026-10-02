// Models into the g-buffer (modelshader); see model_defs.glsl for the type
// defines. Also needs GDEPTH_FORMAT, MSAA_SAMPLES and DEBUG_VERTCOLORS
// ($debugvertcolors: the vertex colour replaces the diffuse colour).
in vec4 vvertex, vtangent;
in vec2 vtexcoord0;
#ifdef MODEL_SKELETAL
SKELANIM_DECLS
#endif
uniform mat4 modelmatrix;
uniform mat3 modelworld;
uniform vec3 modelcamera;
uniform vec3 texscroll;
#ifdef MODEL_NORMALMAP
out mat3 world;
#else
out vec3 nvec;
#endif
out vec3 camvec;
#if GDEPTH_FORMAT || MSAA_SAMPLES
GBUFFER_DEPTH_DECLS
#endif
out vec2 texcoord0;
#ifdef MODEL_WIND
WIND_DECLS(camprojmatrix)
WIND_FUNCS
#endif
#if DEBUG_VERTCOLORS
out vec4 vcolordbg;
#endif

ROTATEUV_FUNC

void main(void)
{
#ifdef MODEL_SKELETAL
    SKELANIM_BLEND
    SKELANIM_POS
    SKELANIM_QUAT
#else
#define mpos vvertex
#define mquat vtangent
#endif
    MODEL_QDECODE

    gl_Position = modelmatrix * mpos;
#if DEBUG_VERTCOLORS
    vcolordbg = vec4(0, 0, 0, 0);
#endif
#ifdef MODEL_WIND
    WIND_ANIM(camprojmatrix)
#if DEBUG_VERTCOLORS
    vcolordbg = vcolor;
#endif
#endif

    MODEL_TEXCOORD

#if GDEPTH_FORMAT || MSAA_SAMPLES
    GBUFFER_DEPTH_VERT
#endif

    // left unnormalised so it interpolates exactly, as large triangles close to the camera bend a normalised one
    camvec = modelworld * (modelcamera - mpos.xyz);

#ifdef MODEL_NORMALMAP
    // composition of tangent -> object and object -> world transforms
    //   becomes tangent -> world
    vec3 wnormal = modelworld * mnormal;
    vec3 wtangent = modelworld * mtangent;
    vec3 wbitangent = cross(wnormal, wtangent) * (vtangent.w < 0.0 ? -1.0 : 1.0);
    world = mat3(wtangent, wbitangent, wnormal);
#else
    nvec = modelworld * mnormal;
#endif
}
