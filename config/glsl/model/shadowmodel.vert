// Models into the shadow maps (shadowmodelshader): shadowmodel,
// alphashadowmodel (a) and windshadowmodel (aw), each with skeletal
// variants. See model_defs.glsl for the type defines.
in vec4 vvertex;
#ifdef MODEL_SKELETAL
SKELANIM_DECLS
#endif
uniform mat4 modelmatrix;
#ifdef MODEL_ALPHATEST
in vec2 vtexcoord0;
uniform vec3 texscroll;
out vec2 texcoord0;
#endif
#ifdef MODEL_WIND
WIND_DECLS(shadowmatrix)
WIND_FUNCS
#endif

ROTATEUV_FUNC

void main(void)
{
#ifdef MODEL_SKELETAL
    SKELANIM_BLEND
    SKELANIM_POS
#else
#define mpos vvertex
#endif

    gl_Position = modelmatrix * mpos;

#ifdef MODEL_WIND
    WIND_ANIM(shadowmatrix)
#endif

#ifdef MODEL_ALPHATEST
    MODEL_TEXCOORD
#endif
}
