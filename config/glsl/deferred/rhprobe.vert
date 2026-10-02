// rhprobe (DEBUG_UTILS, renderlights.cpp): one point per probe, drawn at its
// own pixel of the readback target, carrying its world position and normal
// to the DL_RHPROBE main in deferredlight.frag.
in vec4 vvertex;
in vec3 vtexcoord0, vnormal;
out vec3 probepos, probenorm;
void main(void)
{
    gl_Position = vvertex;
    probepos = vtexcoord0;
    probenorm = vnormal;
}
