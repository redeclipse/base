// Models into the reflective shadow map (rsmmodelshader), for global
// illumination; see rsmmodel.frag.
attribute vec4 vvertex, vtangent;
attribute vec2 vtexcoord0;
#ifdef MODEL_SKELETAL
SKELANIM_DECLS
#endif
uniform mat4 modelmatrix;
uniform mat3 modelworld;
uniform vec3 texscroll;
varying vec2 texcoord0;
varying vec3 nvec;

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

    MODEL_TEXCOORD

    nvec = modelworld * mnormal;
}
