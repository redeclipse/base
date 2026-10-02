// MSAA edge detection, formerly the msaadetectedges alias in
// config/glsl/deferred.cfg. Include with shader_include_fs after defining
// MSAA_SAMPLES ($msaasamples) and GLEXT_SAMPLES_IDENTICAL (glext
// GL_EXT_shader_samples_identical, 0/1). The shader declares
// uniform sampler2DMS tex1 (the g-buffer normals) and, with
// GLEXT_SAMPLES_IDENTICAL, enables the extension.
//
// MSAA_EDGE_DETECT(action) runs the statement action when the pixel is not an
// edge: all its samples are identical, or every sample's normal and depth
// hash (tex1.w) agree with sample 0's. One nested if per sample, unrolled.

#define MSAA_EDGE_TAP(n) vec4 e##n = texelFetch(tex1, ivec2(gl_FragCoord.xy), n); e##n.xyz -= 0.5; if(abs(e.w-e##n.w) <= 2.0/255.0 && pow(dot(e##n.xyz, e.xyz), 2.0) >= maxdiff*dot(e##n.xyz, e##n.xyz)) {

// The taps and closing braces for n samples.
#define MSAA_EDGE_TAPS2 MSAA_EDGE_TAP(1)
#define MSAA_EDGE_TAPS3 MSAA_EDGE_TAPS2 MSAA_EDGE_TAP(2)
#define MSAA_EDGE_TAPS4 MSAA_EDGE_TAPS3 MSAA_EDGE_TAP(3)
#define MSAA_EDGE_TAPS5 MSAA_EDGE_TAPS4 MSAA_EDGE_TAP(4)
#define MSAA_EDGE_TAPS6 MSAA_EDGE_TAPS5 MSAA_EDGE_TAP(5)
#define MSAA_EDGE_TAPS7 MSAA_EDGE_TAPS6 MSAA_EDGE_TAP(6)
#define MSAA_EDGE_TAPS8 MSAA_EDGE_TAPS7 MSAA_EDGE_TAP(7)
#define MSAA_EDGE_TAPS9 MSAA_EDGE_TAPS8 MSAA_EDGE_TAP(8)
#define MSAA_EDGE_TAPS10 MSAA_EDGE_TAPS9 MSAA_EDGE_TAP(9)
#define MSAA_EDGE_TAPS11 MSAA_EDGE_TAPS10 MSAA_EDGE_TAP(10)
#define MSAA_EDGE_TAPS12 MSAA_EDGE_TAPS11 MSAA_EDGE_TAP(11)
#define MSAA_EDGE_TAPS13 MSAA_EDGE_TAPS12 MSAA_EDGE_TAP(12)
#define MSAA_EDGE_TAPS14 MSAA_EDGE_TAPS13 MSAA_EDGE_TAP(13)
#define MSAA_EDGE_TAPS15 MSAA_EDGE_TAPS14 MSAA_EDGE_TAP(14)
#define MSAA_EDGE_TAPS16 MSAA_EDGE_TAPS15 MSAA_EDGE_TAP(15)
#define MSAA_EDGE_CLOSE2 }
#define MSAA_EDGE_CLOSE3 MSAA_EDGE_CLOSE2 }
#define MSAA_EDGE_CLOSE4 MSAA_EDGE_CLOSE3 }
#define MSAA_EDGE_CLOSE5 MSAA_EDGE_CLOSE4 }
#define MSAA_EDGE_CLOSE6 MSAA_EDGE_CLOSE5 }
#define MSAA_EDGE_CLOSE7 MSAA_EDGE_CLOSE6 }
#define MSAA_EDGE_CLOSE8 MSAA_EDGE_CLOSE7 }
#define MSAA_EDGE_CLOSE9 MSAA_EDGE_CLOSE8 }
#define MSAA_EDGE_CLOSE10 MSAA_EDGE_CLOSE9 }
#define MSAA_EDGE_CLOSE11 MSAA_EDGE_CLOSE10 }
#define MSAA_EDGE_CLOSE12 MSAA_EDGE_CLOSE11 }
#define MSAA_EDGE_CLOSE13 MSAA_EDGE_CLOSE12 }
#define MSAA_EDGE_CLOSE14 MSAA_EDGE_CLOSE13 }
#define MSAA_EDGE_CLOSE15 MSAA_EDGE_CLOSE14 }
#define MSAA_EDGE_CLOSE16 MSAA_EDGE_CLOSE15 }

#if MSAA_SAMPLES > 16
#error MSAA_EDGE_DETECT handles at most 16 samples
#elif MSAA_SAMPLES < 2
#define MSAA_EDGE_TAPS
#define MSAA_EDGE_CLOSE
#else
#define MSAA_EDGE_TAPS_N(n) MSAA_EDGE_TAPS##n
#define MSAA_EDGE_CLOSE_N(n) MSAA_EDGE_CLOSE##n
#define MSAA_EDGE_TAPS_X(n) MSAA_EDGE_TAPS_N(n)
#define MSAA_EDGE_CLOSE_X(n) MSAA_EDGE_CLOSE_N(n)
#define MSAA_EDGE_TAPS MSAA_EDGE_TAPS_X(MSAA_SAMPLES)
#define MSAA_EDGE_CLOSE MSAA_EDGE_CLOSE_X(MSAA_SAMPLES)
#endif

#if GLEXT_SAMPLES_IDENTICAL
#define MSAA_EDGE_IDENTICAL(action) if(textureSamplesIdenticalEXT(tex1, ivec2(gl_FragCoord.xy))) { action } else
#else
#define MSAA_EDGE_IDENTICAL(action)
#endif

#define MSAA_EDGE_DETECT(action) { MSAA_EDGE_IDENTICAL(action) { vec4 e = texelFetch(tex1, ivec2(gl_FragCoord.xy), 0); e.xyz -= 0.5; float maxdiff = 0.98*0.98*dot(e.xyz, e.xyz); MSAA_EDGE_TAPS action MSAA_EDGE_CLOSE } }
