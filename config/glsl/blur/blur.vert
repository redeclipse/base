// Separable blur; see blur.frag. Taps 1..3 get interpolated texcoords, the
// rest are built in the fragment stage.
// Uses vtexcoord<n> from config/glsl/shared/screentexcoord.glsl.
in vec4 vvertex;
uniform vec4 screentexcoord0;
uniform float offsets[BLUR_SIZE];
out vec2 texcoord0, texcoordp1, texcoordn1;
#if BLUR_RADIUS >= 2
out vec2 texcoordp2, texcoordn2;
#endif
#if BLUR_RADIUS >= 3
out vec2 texcoordp3, texcoordn3;
#endif

void main(void)
{
    gl_Position = vvertex;
    texcoord0 = vtexcoord0;

    vec2 tcp = vtexcoord0, tcn = vtexcoord0;
    tcp.BLUR_AXIS += offsets[1];
    tcn.BLUR_AXIS -= offsets[1];
    texcoordp1 = tcp;
    texcoordn1 = tcn;
#if BLUR_RADIUS >= 2
    tcp.BLUR_AXIS = vtexcoord0.BLUR_AXIS + offsets[2];
    tcn.BLUR_AXIS = vtexcoord0.BLUR_AXIS - offsets[2];
    texcoordp2 = tcp;
    texcoordn2 = tcn;
#endif
#if BLUR_RADIUS >= 3
    tcp.BLUR_AXIS = vtexcoord0.BLUR_AXIS + offsets[3];
    tcn.BLUR_AXIS = vtexcoord0.BLUR_AXIS - offsets[3];
    texcoordp3 = tcp;
    texcoordn3 = tcn;
#endif
}
