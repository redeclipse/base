// Wind sway for foliage models (windanimdefs, windanim): vertices move with
// windvec, the phase and sway weights coming from the vertex colour. The
// shader writes WIND_DECLS(proj) and WIND_FUNCS where it declares things,
// and WIND_ANIM(proj) after setting gl_Position from vec4 mpos; proj is the
// matrix that takes the sway to clip space (camprojmatrix, or shadowmatrix
// for the shadow map).

#define WIND_SWAY_SCALE 2
#define WIND_DETAIL1_ZSWAY 1
#define WIND_DETAIL2_ZSWAY 0.75
#define WIND_DETAIL1_SWAY_SCALE 2
#define WIND_DETAIL2_SWAY_SCALE 1.5
#define WIND_DETAIL1_SWAY_FREQ 2.0
#define WIND_DETAIL2_SWAY_FREQ 9.0
#define WIND_PHASE_SHIFT_SCALE 123.0

#define WIND_DECLS(proj) attribute vec4 vcolor; uniform float millis; uniform mat4 proj; uniform vec3 windparams; uniform vec3 windvec;

// curve, triangle and curvefunc shape the sway; windsway is one sway layer.
#define WIND_FUNCS float curve(float x) { return x * x * (3.0 - 2.0 * x); } float triangle(float x) { return abs(fract(x + 0.5) * 2.0 - 1.0); } float curvefunc(float x) { return curve(triangle(x)) * 2.0 - 1.0; } vec3 windsway(vec3 wind, vec3 crosswind, float phase, float factor1, float factor2, float zsway) { float basesway = curvefunc(phase); vec3 result = vec3(0, 0, 0); result += (vec3(basesway, basesway, basesway) * wind * factor1) + (wind * 10); result += vec3(basesway, curvefunc(phase + 0.25), curvefunc(phase + 0.75)) * ( (crosswind + vec3(0, 0, zsway)) * factor1 * 2); return result * factor2; }

// A base sway plus two detail layers, added to gl_Position.
#define WIND_ANIM(proj) if (windparams.x > 0.0) { float theta = (millis + windparams.y) * 0.4f; float detailphase1 = vcolor.g * WIND_PHASE_SHIFT_SCALE; float detailphase2 = dot(mpos.xyz, vec3(detailphase1)) + 0.1 + (vcolor.g * WIND_PHASE_SHIFT_SCALE); float force = length(windvec); vec3 windhorizontal = vec3(windvec.x, windvec.y, 0); vec3 crosswind = cross(windhorizontal, vec3(0, 0, 1)); vec3 wind = vec3(0, 0, 0); wind += windsway(windvec, crosswind, theta, WIND_SWAY_SCALE, mpos.z * 0.01, 0); wind += windsway(windvec, crosswind, theta * WIND_DETAIL1_SWAY_FREQ + detailphase1, 1, vcolor.b * WIND_DETAIL1_SWAY_SCALE, WIND_DETAIL1_ZSWAY * force); wind += windsway(windvec, crosswind, theta * WIND_DETAIL2_SWAY_FREQ + detailphase2, 1, vcolor.r * WIND_DETAIL2_SWAY_SCALE, WIND_DETAIL2_ZSWAY * force); gl_Position += proj * vec4(wind, 0.0); }
