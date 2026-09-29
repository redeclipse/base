// msaaedgedetect; see msaaedge.glsl for the defines.
#if GLEXT_SAMPLES_IDENTICAL
#extension GL_EXT_shader_samples_identical : enable
#endif
uniform sampler2DMS tex1;

void main(void)
{
    MSAA_EDGE_DETECT(discard;)
}
