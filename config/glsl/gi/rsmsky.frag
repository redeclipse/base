// The sky into the reflective shadow map: no light, and a zero normal, so it
// bounces nothing. Assembled after shared/rsm_out.glsl.
void main(void)
{
    gcolor = vec4(0.0, 0.0, 0.0, 1.0);
    gnormal = vec4(0.5, 0.5, 0.5, 0.0);
}
