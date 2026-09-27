// Copyright (C) 2013 Jorge Jimenez (jorge@iryoku.com)
// Copyright (C) 2013 Jose I. Echevarria (joseignacioechevarria@gmail.com)
// Copyright (C) 2013 Belen Masia (bmasia@unizar.es)
// Copyright (C) 2013 Fernando Navarro (fernandn@microsoft.com)
// Copyright (C) 2013 Diego Gutierrez (diegog@unizar.es)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
// of the Software, and to permit persons to whom the Software is furnished to
// do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software. As clarification, there
// is no requirement that the copyright notice and permission be included in
// binary distributions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// SMAA: Enhanced Subpixel Morphological Antialiasing, http://www.iryoku.com/smaa/
//
// Preset and option macros shared by the SMAA passes, included into each
// fragment stage. Options, from smaashaders in config/glsl/aa.cfg:
//   SMAA_PRESET     0..3, $smaaquality: low, medium, high, ultra
//   SMAA_ALPHAAREA  the area texture is luminance-alpha, not RG ("a", no texture_rg)
//   SMAA_DISCARD    edge detection discards edgeless pixels, for the depth or
//                   stencil mask ("d")
//   SMAA_SPLIT      2x MSAA, both samples resolved separately ("s")
//   SMAA_GREENLUMA  luma is the green channel ("g"); otherwise the alpha
//                   channel holds it, written by the HDR resolve
//   SMAA_TEMPORAL   combined with TQAA ("t")

#if SMAA_PRESET == 1
#define SMAA_THRESHOLD 0.1
#define SMAA_MAX_SEARCH_STEPS 8
#define SMAA_MAX_SEARCH_STEPS_DIAG 0
#define SMAA_CORNER_ROUNDING 100
#elif SMAA_PRESET == 2
#define SMAA_THRESHOLD 0.1
#define SMAA_MAX_SEARCH_STEPS 16
#define SMAA_MAX_SEARCH_STEPS_DIAG 8
#define SMAA_CORNER_ROUNDING 25
#elif SMAA_PRESET == 3
#define SMAA_THRESHOLD 0.05
#define SMAA_MAX_SEARCH_STEPS 32
#define SMAA_MAX_SEARCH_STEPS_DIAG 16
#define SMAA_CORNER_ROUNDING 25
#else
#define SMAA_THRESHOLD 0.15
#define SMAA_MAX_SEARCH_STEPS 4
#define SMAA_MAX_SEARCH_STEPS_DIAG 0
#define SMAA_CORNER_ROUNDING 100
#endif

#define SMAA_LOCAL_CONTRAST_ADAPTATION_FACTOR 2.0
#define SMAA_CORNER_ROUNDING_NORM (float(SMAA_CORNER_ROUNDING) / 100.0)

#define SMAA_AREATEX_MAX_DISTANCE 16
#define SMAA_AREATEX_MAX_DISTANCE_DIAG 20
#define SMAA_AREATEX_WIDTH 160
#define SMAA_AREATEX_HEIGHT 560
#define SMAA_SEARCHTEX_WIDTH 66
#define SMAA_SEARCHTEX_HEIGHT 33

#define SMAA_AREATEX_SUBSAMPLES 80


#ifdef SMAA_GREENLUMA
#define SMAA_LUMA(color) (color.g)
#else
#define SMAA_LUMA(color) (color.a)
#endif

#ifdef SMAA_ALPHAAREA
#define SMAA_AREA(vals) (vals.ra)
#else
#define SMAA_AREA(vals) (vals.rg)
#endif

#if defined(SMAA_TEMPORAL) || defined(SMAA_SPLIT)
#define SMAA_AREA_OFFSET(texcoord, offset) texcoord.y += offset*float(SMAA_AREATEX_SUBSAMPLES)
#else
#define SMAA_AREA_OFFSET(texcoord, offset)
#endif
