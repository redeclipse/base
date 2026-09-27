// SMAA pass 2 vertex stage: the search texcoords.
attribute vec4 vvertex;
uniform vec4 screentexcoord0;
#define vtexcoord0 (vvertex.xy * screentexcoord0.xy + screentexcoord0.zw)
varying vec2 texcoord0, texcoord1, texcoord2, texcoord3, texcoord4, texcoord5;

void main(void)
{
    gl_Position = vvertex;
    texcoord0 = vtexcoord0;

    // We will use these offsets for the searches later on (see PSEUDO_GATHER4):
    texcoord1 = vtexcoord0 + vec2( -0.25, -0.125);
    texcoord2 = vtexcoord0 + vec2(  1.25, -0.125);
    texcoord3 = vtexcoord0 + vec2(-0.125, -0.25);
    texcoord4 = vtexcoord0 + vec2(-0.125,  1.25);
    texcoord5 = vtexcoord0 + vec2(  0.25,  0.0);
}
