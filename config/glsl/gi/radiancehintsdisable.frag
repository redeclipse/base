// Radiance hints: clears the hints inside no-GI material to RH_ZERO.
// Assembled after rh_out.glsl.
void main(void)
{
    rhr = RH_ZERO;
    rhg = RH_ZERO;
    rhb = RH_ZERO;
    rha = RH_ZERO;
}
