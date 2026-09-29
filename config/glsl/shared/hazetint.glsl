// World haze tint, the GLSL counterpart of hazetintdefs in
// config/glsl/shared.cfg; keep the two in step. hazeamount is how much of the
// world haze colour a surface takes at a distance; hazetint.a stays 0 until
// the opaque scene has been tinted, so only what is drawn after it tints
// itself. Declares its own uniforms.
uniform vec4 hazetint;
uniform vec3 hazetintparams;
float hazeamount(float dist)
{
    float offset = dist - hazetintparams.x;
    return clamp(offset*hazetintparams.y, 0.0, 1.0) * clamp(offset*hazetintparams.z, 0.0, 1.0) * hazetint.a;
}
