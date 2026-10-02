// Radiance hints: gathers the light the reflective shadow map (tex0 depth,
// tex1 colour, tex2 normal) bounces into one cell, as spherical harmonics per
// colour channel plus the sky occlusion, from a fixed set of taps.
// Assembled after rh_out.glsl.
//
// radiancehintsshader (config/glsl/gi.cfg) defines:
//   RH_TAPS  $rhtaps, 0..32; up to 12 takes 12 taps, up to 20 takes 20,
//            more takes 32
#if RH_TAPS > 20
#define RH_NUMTAPS 32.0
#elif RH_TAPS > 12
#define RH_NUMTAPS 20.0
#else
#define RH_NUMTAPS 12.0
#endif
// Averages the summed samples and biases them like RH_ZERO.
#define RH_ENCODE(sh) sh * (vec4(0.5, 0.5, 0.5, 1.0)/RH_NUMTAPS) + RH_ZERO

uniform sampler2DRect tex0, tex1, tex2;
uniform mat4 rsmworldmatrix;
uniform vec2 rsmspread;
uniform float rhatten, rhspread, rhaothreshold, rhaoatten, rhaoheight;
uniform vec3 rsmdir;
in vec3 rhcenter;
in vec2 rsmcenter;

void calcrhsample(vec3 rhtap, vec2 rsmtap, inout vec4 shr, inout vec4 shg, inout vec4 shb, inout vec4 sha)
{
    vec3 rhpos = rhcenter + rhtap*rhspread;
    vec2 rsmtc = rsmcenter + rsmtap*rsmspread;
    float rsmdepth = texture(tex0, rsmtc).x;
    vec3 rsmcolor = texture(tex1, rsmtc).rgb;
    vec3 rsmnormal = texture(tex2, rsmtc).xyz*2.0 - 1.0;
    vec3 rsmpos = (rsmworldmatrix * vec4(rsmtc, rsmdepth, 1.0)).xyz;

    vec3 dir = rhpos - rsmpos;

    sha += step(rhaothreshold, dir.z) * vec4(normalize(vec3(dir.xy, min(dot(dir.xy, dir.xy) * rhaoatten - rhaoheight, 0.0))), 1.0);

    float dist = dot(dir, dir);
    if(dist > 0.000049) dir = normalize(dir);
    float atten = clamp(dot(dir, rsmnormal), 0.0, 1.0) / (0.1 + dist*rhatten);
    rsmcolor *= atten;

    shr += vec4(rsmcolor.r*dir, rsmcolor.r);
    shg += vec4(rsmcolor.g*dir, rsmcolor.g);
    shb += vec4(rsmcolor.b*dir, rsmcolor.b);
}

void main(void)
{
    vec4 shr = vec4(0.0), shg = vec4(0.0), shb = vec4(0.0), sha = vec4(0.0);

    // One tap per line: the offsets in the cell (vec3) and in the RSM (vec2).
#if RH_TAPS > 20
    calcrhsample(vec3(0.0553911, 0.675924, 0.22129)*2.0 - 1.0, vec2(0.0262032, 0.215221)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.562975, 0.508286, 0.549883)*2.0 - 1.0, vec2(0.0359769, 0.0467256)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.574816, 0.703452, 0.0513016)*2.0 - 1.0, vec2(0.0760799, 0.713481)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.981017, 0.930479, 0.243873)*2.0 - 1.0, vec2(0.115087, 0.461431)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.889309, 0.133091, 0.319071)*2.0 - 1.0, vec2(0.119488, 0.927444)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.329112, 0.00759911, 0.472213)*2.0 - 1.0, vec2(0.22346, 0.319747)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.314463, 0.985839, 0.54442)*2.0 - 1.0, vec2(0.225964, 0.679227)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.407697, 0.202643, 0.985748)*2.0 - 1.0, vec2(0.238626, 0.0618425)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.998169, 0.760369, 0.792932)*2.0 - 1.0, vec2(0.243326, 0.535066)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.0917692, 0.0666829, 0.0169683)*2.0 - 1.0, vec2(0.29832, 0.90826)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.0157781, 0.632954, 0.740806)*2.0 - 1.0, vec2(0.335208, 0.212103)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.938139, 0.235878, 0.87936)*2.0 - 1.0, vec2(0.356438, 0.751969)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.442305, 0.184942, 0.0901212)*2.0 - 1.0, vec2(0.401021, 0.478664)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.578051, 0.863948, 0.799554)*2.0 - 1.0, vec2(0.412027, 0.0245297)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.0698569, 0.259194, 0.667592)*2.0 - 1.0, vec2(0.48477, 0.320659)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.872494, 0.576312, 0.344157)*2.0 - 1.0, vec2(0.494311, 0.834621)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.10123, 0.930082, 0.959929)*2.0 - 1.0, vec2(0.515007, 0.165552)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.178594, 0.991302, 0.046205)*2.0 - 1.0, vec2(0.534574, 0.675536)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.690176, 0.527543, 0.930509)*2.0 - 1.0, vec2(0.585357, 0.432483)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.982025, 0.389447, 0.0344554)*2.0 - 1.0, vec2(0.600102, 0.94139)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.033845, 0.0156865, 0.963866)*2.0 - 1.0, vec2(0.650182, 0.563571)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.655293, 0.154271, 0.640553)*2.0 - 1.0, vec2(0.672336, 0.771816)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.317881, 0.598621, 0.97998)*2.0 - 1.0, vec2(0.701811, 0.187078)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.247261, 0.398206, 0.121586)*2.0 - 1.0, vec2(0.734207, 0.359024)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.822626, 0.985076, 0.655232)*2.0 - 1.0, vec2(0.744775, 0.924466)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.00201422, 0.434278, 0.388348)*2.0 - 1.0, vec2(0.763628, 0.659075)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.511399, 0.977416, 0.278695)*2.0 - 1.0, vec2(0.80735, 0.521281)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.32371, 0.540147, 0.361187)*2.0 - 1.0, vec2(0.880585, 0.107684)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.365856, 0.41493, 0.758232)*2.0 - 1.0, vec2(0.898505, 0.904047)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.792871, 0.979217, 0.0309763)*2.0 - 1.0, vec2(0.902536, 0.718989)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.0509049, 0.459151, 0.996277)*2.0 - 1.0, vec2(0.928022, 0.347802)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.0305185, 0.13422, 0.306009)*2.0 - 1.0, vec2(0.971243, 0.504885)*2.0 - 1.0, shr, shg, shb, sha);
#elif RH_TAPS > 12
    calcrhsample(vec3(0.0540788, 0.411725, 0.134068)*2.0 - 1.0, vec2(0.00240055, 0.643992)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.0163579, 0.416211, 0.992035)*2.0 - 1.0, vec2(0.0356464, 0.851616)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.692068, 0.549272, 0.886502)*2.0 - 1.0, vec2(0.101733, 0.21876)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.305795, 0.781854, 0.571337)*2.0 - 1.0, vec2(0.166119, 0.0278085)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.791681, 0.139042, 0.247047)*2.0 - 1.0, vec2(0.166438, 0.474999)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.83929, 0.973663, 0.460982)*2.0 - 1.0, vec2(0.24991, 0.766405)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.0336314, 0.0867641, 0.582324)*2.0 - 1.0, vec2(0.333714, 0.130407)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.148198, 0.961974, 0.0378124)*2.0 - 1.0, vec2(0.400681, 0.374781)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.948729, 0.0713828, 0.916379)*2.0 - 1.0, vec2(0.424067, 0.888211)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.586413, 0.591845, 0.031251)*2.0 - 1.0, vec2(0.448511, 0.678962)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.00189215, 0.973968, 0.932981)*2.0 - 1.0, vec2(0.529383, 0.213568)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.435865, 0.0853603, 0.995148)*2.0 - 1.0, vec2(0.608569, 0.47715)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.36848, 0.820612, 0.942717)*2.0 - 1.0, vec2(0.617996, 0.862528)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.500107, 0.0658284, 0.623005)*2.0 - 1.0, vec2(0.631784, 0.0515881)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.580187, 0.4485, 0.379223)*2.0 - 1.0, vec2(0.740969, 0.20753)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.258614, 0.0201422, 0.241005)*2.0 - 1.0, vec2(0.788203, 0.41923)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.987152, 0.441664, 0.43318)*2.0 - 1.0, vec2(0.794066, 0.615141)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.925108, 0.917203, 0.921506)*2.0 - 1.0, vec2(0.834504, 0.836612)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.988372, 0.822047, 0.12479)*2.0 - 1.0, vec2(0.89446, 0.0677863)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.330393, 0.43611, 0.762566)*2.0 - 1.0, vec2(0.975609, 0.446056)*2.0 - 1.0, shr, shg, shb, sha);
#else
    calcrhsample(vec3(0.0565813, 0.61211, 0.763359)*2.0 - 1.0, vec2(0.031084, 0.572114)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.375225, 0.285592, 0.987915)*2.0 - 1.0, vec2(0.040671, 0.95653)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.615192, 0.668996, 0.604938)*2.0 - 1.0, vec2(0.160921, 0.367819)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.963195, 0.355937, 0.175787)*2.0 - 1.0, vec2(0.230518, 0.134321)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.0295724, 0.484268, 0.265694)*2.0 - 1.0, vec2(0.247078, 0.819415)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.917783, 0.88702, 0.201972)*2.0 - 1.0, vec2(0.428665, 0.440522)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.408948, 0.0675985, 0.427564)*2.0 - 1.0, vec2(0.49846, 0.80717)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.19071, 0.923612, 0.0553606)*2.0 - 1.0, vec2(0.604285, 0.0307766)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.968078, 0.403943, 0.847224)*2.0 - 1.0, vec2(0.684075, 0.283001)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.384503, 0.922269, 0.990844)*2.0 - 1.0, vec2(0.688304, 0.624171)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.480605, 0.342418, 0.00195318)*2.0 - 1.0, vec2(0.833995, 0.832414)*2.0 - 1.0, shr, shg, shb, sha);
    calcrhsample(vec3(0.956664, 0.923643, 0.915799)*2.0 - 1.0, vec2(0.975397, 0.189911)*2.0 - 1.0, shr, shg, shb, sha);
#endif

    rhr = RH_ENCODE(shr);
    rhg = RH_ENCODE(shg);
    rhb = RH_ENCODE(shb);
    rha = RH_ENCODE(sha);
}
