// msaaedgedetect: a full-screen pass that discards every pixel that is not an
// MSAA edge (msaaedge.glsl).
in vec4 vvertex;
void main(void)
{
    gl_Position = vvertex;
}
