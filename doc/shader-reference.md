# Red Eclipse Shader System Reference

This document provides comprehensive information about Red Eclipse's shader system, including shader types, CubeScript integration, and C++ usage patterns.

## Shader Type System

Red Eclipse uses a bitfield system for shader types defined in `SHADER_ENUM`:

### Shader Type Definitions
```cpp
enum
{
    SHADER_DEFAULT   = 0,      // Basic shader without special features
    SHADER_WORLD     = 1<<0,   // World geometry rendering shaders  
    SHADER_ENVMAP    = 1<<1,   // Environment mapping/reflections
    SHADER_REFRACT   = 1<<2,   // Refractive materials (glass, water)
    SHADER_OPTION    = 1<<3,   // Optional shader features
    SHADER_DYNAMIC   = 1<<4,   // Dynamic/animated shaders (pulse glow)
    SHADER_TRIPLANAR = 1<<5,   // Triplanar texture mapping
    SHADER_INVALID   = 1<<6,   // Shader compilation failed
    SHADER_DEFERRED  = 1<<7    // Deferred loading shader
};
```

### Shader Type Combinations
Types combine with bitwise OR for multi-feature shaders:

```cubescript
// Basic world shader with environment mapping
stype = (| $SHADER_WORLD $SHADER_ENVMAP)

// Dynamic world shader with triplanar mapping
stype = (| $SHADER_WORLD $SHADER_DYNAMIC $SHADER_TRIPLANAR)

// Conditional shader type building
stype = $SHADER_WORLD
if (wtopt "e") [ stype = (| $stype $SHADER_ENVMAP) ]      // Add environment mapping
if (wtopt "G") [ stype = (| $stype $SHADER_DYNAMIC) ]     // Add dynamic features  
if (wtopt "T") [ stype = (| $stype $SHADER_TRIPLANAR) ]   // Add triplanar mapping
if (wtopt "A") [ stype = (| $stype $SHADER_REFRACT) ]     // Add refraction
```

## CubeScript Shader Definition

### Basic Shader Definition Patterns
```cubescript
// Basic shader definition
shader $SHADER_DEFAULT "basicshader" [
    // Vertex shader code
    attribute vec4 vvertex;
    uniform mat4 camprojmatrix;
    void main(void) {
        gl_Position = camprojmatrix * vvertex;
    }
] [
    // Fragment shader code
    uniform vec3 color;
    void main(void) {
        gl_FragColor = vec4(color, 1.0);
    }
]

// Deferred loading shader
defershader $SHADER_WORLD "worldshader" [
    // Vertex shader will be loaded when needed
    @(ginterpvert)
] [
    // Fragment shader will be loaded when needed
    @(ginterpfrag)
]

// Lazy shader (loaded on first use)
lazyshader $SHADER_ENVMAP "envmapshader" [
    @(ginterpvert)
    varying vec3 reflect;
    void main(void) {
        gl_Position = camprojmatrix * vvertex;
        reflect = reflect(normalize(vvertex.xyz), vnormal);
    }
] [
    @(ginterpfrag)
    uniform samplerCube envmap;
    varying vec3 reflect;
    void main(void) {
        gl_FragColor = textureCube(envmap, reflect);
    }
]

// Variant shader with multiple configurations
variantshader $stype "materialshader" $srow [
    @(ginterpvert)
    // Vertex code for material shader
] [
    @(ginterpfrag)
    // Fragment code varies based on srow configuration
]
```

### Shader Utilities and Includes
```cubescript
// Common shader includes
@(ginterpvert)     // Standard vertex interpolation setup
@(ginterpfrag)     // Standard fragment interpolation setup
@(gdepthpackvert)  // Depth packing vertex shader
@(gdepthpackfrag)  // Depth packing fragment shader

// World shader option checking
wtopt = [ >= (strstr $worldtype $arg1) 0 ]  // Check if option exists in worldtype

// Usage examples
if (wtopt "e") [ echo "Environment mapping enabled" ]
if (wtopt "G") [ echo "Glow/dynamic effects enabled" ]
if (wtopt "T") [ echo "Triplanar mapping enabled" ]
```

### Shader Source Files

`shader_new` builds a shader from GLSL files instead of CubeScript-generated text.
The body describes the shader; the engine then reads the files, assembles each stage
and passes the result to `shader`, so fog (`//:fog`), generic variants (`//:variant`)
and texture-slot uniforms behave exactly as for inline shaders.

```cubescript
ambientobscuranceshader = [
    shader_new $SHADER_DEFAULT (format "ambientobscurance%1%2" $arg1 $arg2) [
        aoshaderdefines                                                 // engine state, one shader_define each
        if (>= (strstr $arg1 "l") 0) [shader_define AO_LINEAR ""]       // "#define AO_LINEAR"
        if (>= (strstr $arg1 "d") 0) [shader_define AO_DERIVNORMAL ""]
        if (>= (strstr $arg1 "p") 0) [shader_define AO_PACKED ""]
        shader_define AO_TAPS $arg2                                     // "#define AO_TAPS 5"
        shader_source "config/glsl/ao/ambientobscurance.vert" "config/glsl/ao/ambientobscurance.frag"
    ]
]
```

- Each stage is: its defines in call order, its includes in call order (`shader_include_vs`,
  `shader_include_fs`), then its `shader_source` file. Defines go to every stage that has a file.
- Carriage returns are dropped and every piece ends with a newline, so CRLF and LF checkouts
  assemble to the same text.
- Paths must be under `config/glsl/`, with forward slashes and no `.` or `..` components. They
  are found like `exec` finds files: mounted archives, then the home directory, then the packages.
- `shader_new` does nothing if the shader is already loaded; its body does not run.
- Any failure (unreadable file, refused path, invalid define, a missing stage) is logged and
  no shader is created. `shader_define`, `shader_include_*` and `shader_source` outside a body
  are logged and ignored.
- A failed `shader_new`/`variantshader_new` does not consume texture-slot params staged with
  `setshaderparam`/`defuniformparam` beforehand; they stay pending for the next `shader`/
  `shader_new` that actually gets created.
- A `shader_new` nested directly inside another body's script runs first, so it takes any
  texture-slot params that were pending for the outer one; defines are isolated per body, but
  texture-slot params are not, so don't rely on nesting order for them.
- The `#version` header and compatibility macros are added in front of the assembled text
  when it is compiled, so defines may use them.
- Caveat: uniform and fog declarations are inserted at the line containing the first
  occurrence of the text `main`. Keep that word out of includes (identifiers such as `domain`
  and comments included), or the declarations can land inside an include's function.

`variantshader_new <type> <name> <row> <maxvariants> [body]` is the file-based `variantshader`.
A stage with no file (`shader_source "" "config/glsl/..."`) reuses the parent's stage. A
negative row behaves like `shader_new`. The body does not run when the parent is not loaded
or the row is out of range.

```cubescript
variantshader_new $SHADER_DEFAULT "bumpworld" 1 2 [
    shader_define BUMP_TRIPLANAR ""
    shader_source "" "config/glsl/world/bump.frag"
]
```

`lazyshader_new <type> <name> [body]` (`config/glsl/shared.cfg`) is the file-based
`lazyshader`: the shader is registered with `defershader` and built on first use. The body
runs then, not when the `.cfg` is executed, so it reads engine vars at that point.
`tqaaresolve` (`config/glsl/aa.cfg`) uses it.

#### Porting a generator

The AO family (`config/glsl/ao.cfg`, `config/glsl/ao/`) was the first port,
AA (`config/glsl/aa.cfg`, `config/glsl/aa/`) the second, blur
(`config/glsl/blur.cfg`, `config/glsl/blur/`) the third, decals
(`config/glsl/decal.cfg`, `config/glsl/decal/`) the fourth, deferred
lighting (`config/glsl/deferred.cfg`, `config/glsl/deferred/`) the fifth,
world geometry (`config/glsl/world.cfg`, `config/glsl/world/`) the sixth,
volumetric lights (`config/glsl/volumetric.cfg`, `config/glsl/volumetric/`)
the seventh and radiance hints (`config/glsl/gi.cfg`, `config/glsl/gi/`) the
eighth. They are the pattern for the rest:

- The alias passes raw values only (engine vars such as `$gdepthformat` and
  the `generateshader` arguments) as defines. All branching is `#if` in the GLSL.
- The one exception is output declarations. The engine reads
  `fragdata(n)`/`fragblend(n)` declarations from the fragment text
  (`findfragdatalocs`, `src/engine/shader.cpp`) without running the
  preprocessor, and so does the harness contract (`fragdata` lines in
  `meta.txt`). Below GLSL 1.30 without `EXT_gpu_shader4`, each name it finds
  becomes `#define <name> gl_FragData[<n>]`, so a name declared in two `#if`
  branches is defined twice. When the outputs differ between configurations,
  put each set in its own include and have the alias pick it
  (`decal/out_*.glsl`). Keep the text `fragdata(`/`fragblend(` out of comments too.
  The g-buffer outputs of `ginterpfrag` are such a case (`gglow` moves from
  location 2 to 3 when `$gdepthformat` adds `gdepth`): include
  `(gbufferoutputs)`, which names `shared/gbuffer_out.glsl` or
  `shared/gbuffer_out_depth.glsl`. Includes precede the source, so the
  outputs move ahead of the shader's other declarations. That costs the TEXT
  tier but not SPIR-V (glslang creates a variable where it is first used, and
  `reflect.txt` lists uniforms by name), so a world row is `PASS-SPIRV`.
- Don't turn a loop the generator unrolled into a GLSL loop. Write one macro
  line per tap, each under an `#if` on the tap count. `shaders.ps1 check` proves an unrolled port at the SPIR-V tier.
  A loop compiles differently (`spirv-opt -O` doesn't unroll), so it could only be
  proved by pixels, and offset fetches (`texture2DRectOffset`) need a constant
  offset, which a loop index isn't.
- Keep every macro on one line. Line continuation needs GLSL 4.20, and the engine
  emits lower versions.
- Don't use token pasting (`##`): glslang rejects it below `#version 130`
  ("token pasting (##): not supported for this version") and the engine can
  emit 120. Where a generator suffixed names with a loop index, pass the name
  to the tap macro (`MSAA_EDGE_TAP(e1, 1)` in `deferred/msaaedge.glsl`) and
  keep the tokens, or, when there are too many names, give each unrolled copy
  its own `{ }` scope and plain names (`DL_LIGHT(j)` in
  `deferred/deferredlight_defs.glsl`). Scoping costs the TEXT tier but not
  SPIR-V: `spirv-remap --strip all` drops the names.
- A `#define` the generator emitted inside an unrolled block can't go in a
  macro, but it emits no tokens, so define it once before the block
  (`distbias`, `glowscale`, `lightshadow` at the top of `deferredlight.frag`).
  Mind a function parameter of the same name: the define must come after
  that function.
- Include order is token order. When a shared include needs declarations the
  family makes (`shared/smfilter.glsl` reads `tex4`), split the family's text
  around it: `deferredlight_defs.glsl` (macros only),
  `deferredlight_decls.glsl` (extensions, uniforms, output), the shared
  include, then the `.frag`. `#extension` lines must precede every
  non-preprocessor token, so only directive-only includes may come before
  them.
- Don't end any line with a backslash, not even a comment. A comment in a
  generator's `.cfg` is CubeScript, but in a `.frag` it is GLSL, where a
  trailing `\` splices the next line in (4.20+) or draws a warning. The SMAA
  ASCII-art banner was dropped from `smaa_defs.glsl` for this reason.
- A macro used inside a block must not declare names the file `#define`s at
  function scope (`bilateral.frag` defines `color` and `depth`).
- Keep integer-only operators (`<<`, `>>`, `&`, `|`, `%`, `uint`) out of GLSL
  code — they need GLSL 1.30 or `EXT_gpu_shader4`, and the engine can emit
  1.20. They're fine inside an `#if`, which the preprocessor evaluates as
  integers regardless of the shader's `#version`. The harness compiles at the
  driver's `#version` (400 here), so it won't catch a `#version 120` failure;
  check with `glslangValidator` directly when in doubt.
- For a fragment shader below GLSL 1.50, the engine inserts
  `precision highp float;` before the first declaration line that isn't a `#`
  directive (`finddecls`, `src/engine/shader.cpp`). If that first declaration
  sits inside an `#if`, the insertion is compiled out along with it — a no-op
  on desktop GLSL, which is why the AO files leave their first declaration
  inside `#if MSAA_SAMPLES`. A port that cares about the precision statement
  should put an unconditional declaration first.
- Expect `PASS-TEXT` when the tokens are unchanged and `PASS-SPIRV` otherwise.
  `PASS-PIXEL` means either the compiled code changed, or glslang rejected the
  shader so tier 2 is n/a and the check fell to tier 3.
- Make sure the sweep reaches every `#if` branch. When the golden baseline
  can't, record the missing points from the unported build into a separate run
  (`shaders.ps1 record -Run <name> -Sids ... -NoMaps`) and check against it with
  `-Run <name>`.
- Reuse the shared helpers in `config/glsl/shared/` instead of re-spelling
  them. Each is the GLSL counterpart of a `shared.cfg` alias; the shader still
  declares the uniforms and inputs the macros read.
  - `gdepth.glsl`: `GDEPTH_UNPACK(val)` (the default `gdepthunpack`),
    `GDEPTH_UNPACK_ORTHO(val)` (`gdepthunpackortho`),
    `GDEPTH_UNPACK_POS(depth, pos, val, coord)` (`gdepthunpack` with both
    position blocks: declares `depth` and the world position `pos` through
    `worldmatrix`), `GDEPTH_PACK(name, val)` (`gpackdepth`),
    `GDEPTH_UNPACK_DECLS` (the `gdepthunpackparams` uniforms) and
    `GDEPTH_HASH(depth, hashid)` (`ghashdepth` without an alpha). Pull it in
    with `shader_include_fs` and define `GDEPTH_FORMAT` first. A
    shader-specific depth variant stays in its own file (e.g. AO's linear
    reads).
  - `gbuffer.glsl`: linear depth for the g-buffer shaders.
    `GBUFFER_DEPTH_DECLS` (`ginterpdepth`) and `GBUFFER_DEPTH_VERT`
    (`gdepthpackvert`), for a shader that interpolates `lineardepth`
    (`#if GDEPTH_FORMAT || <per-sample depth>`, the argument of
    `ginterpvert`); `GBUFFER_PACK_DEPTH` and `GBUFFER_PACK_DEPTH_HASH(hashid)`
    (`gdepthpackfrag` without an alpha, without and with the MSAA hash).
    Include it in both stages after defining `GDEPTH_FORMAT` and
    `USEPACKNORM`; the packing macros also need `gdepth.glsl`. The outputs
    are `gbuffer_out.glsl`/`gbuffer_out_depth.glsl`, picked by the
    `gbufferoutputs` alias (see the output declarations above).
  - `gfetch.glsl`: `gfetch`, `gfetchoffset`, `gfetchproj` and
    `GFETCH_SAMPLER` (`gfetchdefs` without a prefix): define `GFETCH_MS`
    (non-zero for multisampled buffers) first, declare the buffers as
    `uniform GFETCH_SAMPLER <names>;` and write `GDEPTH_UNPACK_DECLS` where
    `gfetchdefs` put them.
  - `screentexcoord.glsl`: `vtexcoord0`/`vtexcoord1` (`screentexcoord`), for
    `shader_include_vs`. Declare `vvertex` and `uniform vec4 screentexcoord<n>`.
  - `luma.glsl`: `LUMWEIGHTS`, the `vec3` of `lumweights`. Keep the two in
    step until the last `@lumweights` generator is ported.
  - `gnormal.glsl`: `GNORMAL_PACK(n)` and `GNORMAL_PACK_BLEND(n, k)`
    (`gnormpack` without and with its weight). Define `USEPACKNORM`
    (`$usepacknorm`) first; the shader declares `gnormal`.
    `GNORMAL_UNPACK_SCALE(k)` (`unpacknorm`) turns a packed normal's squared
    length back into the weight.
  - `gcolor.glsl`: `GSPEC_PACK(gloss)`, `GSPEC_PACK_SPEC(gloss, spec)`
    (`gspecpack` with one and two arguments) and `GGLOW_PACK(glow)`
    (`gglowpack glow`). `GGLOW_PACK` declares `glowk`, and `GGLOW_PACKNORM`
    is the weight it leaves for `GNORMAL_PACK_BLEND`, where `gglowpack`
    used to `#define packnorm`. `GSPEC_PACK_BLEND(gloss, layer)` and
    `GSPEC_PACK_SPEC_BLEND(gloss, spec, layer, blend)` are the blend-layer
    `gspecpack` (world). The glow-less `gglowpack` (model) isn't there yet;
    add it with that port. `GSPEC_UNPACK(camera, pos, normal, diffuse)` (`unpackspec`)
    declares `camdir`, `facing`, `specscale` and `gloss`. `unpacknorm` and
    `unpackspec` stay in `shared.cfg` for `ui.cfg` until it is ported.
  - `smfilter.glsl`: the shadow-map filters (formerly the `smfilter*`
    aliases). Define `SMFILTER` to emit `filtershadow`, one of
    `SMFILTER_GATHER5`/`_GATHER3`/`_BILINEAR5`/`_BILINEAR3`/`_ROTATED` (none =
    a single compare), `USETEXGATHER`, and `SMFILTER_COLOR` for
    `filtercolorshadow`; declare `tex4` and `shadowatlasscale` before it.
    `SMFILTER_SINGLE` (volumetric) makes `filtershadow` a macro doing one
    unfiltered compare from the atlas the filter letter implies instead;
    `SMFILTER_COLOR` works with either or neither.
  - `bilateral.glsl` and `bilateral.vert`: the separable bilateral filters
    (AO, volumetric). `tapvec`, `texval`/`texvaloffset` (the filtered buffer
    `tex0` at `tc`), `depthval`/`depthvaloffset` (`BILATERAL_DEPTHTEX` at
    `depthtc`), `BILATERAL_FITS(o)` (the offset fits an offset fetch) and
    `BILATERAL_DEPTHSCALE` (2^`BILATERAL_REDUCE`). Define `BILATERAL_REDUCE`,
    `TEXRECT_MINOFFSET`/`TEXRECT_MAXOFFSET` and optionally `BILATERAL_X`; the
    shader `#define`s `tc`, `depthtc` and `BILATERAL_DEPTHTEX` and supplies
    `gfetch`/`gfetchoffset`. The tap chain itself stays in the family: the
    taps' colour and weight differ. `bilateral.vert` is the screen quad,
    with the depth coordinates in `texcoord0` under `BILATERAL_REDUCE`
    (include `screentexcoord.glsl` with it).
  - `rsm_out.glsl`: the reflective shadow map outputs `gcolor` and `gnormal`
    (`rsmsky`). `world/rsm.frag` and the model RSM shaders declare the same
    pair; `rsm.frag` keeps its own so the `rsmworld` rows stay `PASS-TEXT`
    (an include would move the outputs ahead of its uniforms).

  A family's own shared text stays in the family: `gi/rh_out.glsl` holds the
  four radiance hint outputs and `RH_ZERO` (an empty hint) for every
  `radiancehints*` shader, and `gi/rhslice.vert` is the vertex stage of both
  `radiancehintsborder` and `radiancehintscached`.

  Shared helpers are one-line macros, not GLSL functions: a helper function
  compiles to different SPIR-V than the inline code it replaces. Moving a
  `#define` into an include keeps the preprocessed tokens, so the rows stay
  `PASS-TEXT`.
- Keep array sizes and other literals literal. `weights[BLUR_RADIUS + 1]`
  preprocesses to `weights[3 + 1]`, not `weights[4]`, which costs the TEXT
  tier; `blur_defs.glsl` maps the radius to `BLUR_SIZE` with an `#if` chain.
  The same goes for a constant the generator computed with `divf` and printed
  (`%.6g`, integers as `1.0`): `1.0/float(n)` would round differently from the
  printed `0.333333`, so spell the printed values out in an `#if` chain
  (`volumetric/volumetric_steps.glsl`, generated for all 64 step counts).
- Where a generator suffixed names with a tap index but the taps are few,
  pass the names to a per-tap macro and write one line per tap count
  (`VOLBILATERAL_TAPM1(color0, depth0, weight0)` in
  `volumetric/bilateral.frag`): the tokens stay the same, so the rows stay
  `PASS-TEXT`.
- A family-private include (`smaa_defs.glsl`, `blur_defs.glsl`) holds the
  macros derived from the defines when more than one stage needs them.
  `world/world_defs.glsl` serves both `world.*` and `bump.*`, and it sets
  `GFETCH_MS` for `shared/gfetch.glsl`, so it goes first among the includes.
- A generator whose shaders are all registered when `glsl.cfg` runs
  (`blurshader`) is covered by the golden baseline at every sweep point, so
  `check -Sids s00 -NoMaps` proves it. Deferred ones count too:
  `shaderdumpall` forces every registered shader, so the baseline holds all
  173 decal rows at every point. When the generator reads engine state, run
  the check over every sweep point, or at least those that change that state
  (`$usepacknorm` is 1 under MSAA).
- A generator that reads nothing but its arguments (`fxaashaders`,
  `smaashaders`) can be proved for every argument combination, including ones
  the engine can't reach on this GPU. Before porting, call it for each
  combination and dump with `shaderdumpall <run> s00 0`. After porting, do the
  same from a fresh client (shaders created by a direct call survive
  `resetshaders`) and pass the same-name pairs to `shadercheck.py --pairs`.
  The same works for a generator whose engine inputs can be set
  (`decalvariantshader`, with `forcepacknorm` standing in for
  `$usepacknorm`), and for paths this GPU never takes (single-pass decals).
- An engine input that can't be set (`$usetexgather` is read-only, 1 on
  NVIDIA) can still be proved: exec a copy of the old generator with the var
  renamed to an alias (`$gen_usetexgather`), set the alias, call, and dump;
  then the same with the ported alias renamed the same way. A path that
  doesn't compile on this GPU (`GL_EXT_shader_samples_identical`) has no
  blob; compare the old helper's text (`writetofile` of its result, input
  renamed the same way) with the new macro after `glslangValidator -E`.
- Wrap such direct calls in `defershader` and run `shaderforceall`, as a
  real registration would. A variant (row ≥ 0) takes its uniform defaults
  from its parent, so the `defuniformparam`s staged before it are never
  consumed; called bare, they leak into whatever shader is created next.
  `Shader::force` isolates each body, so forced definitions don't leak.

### Shader Parameter Binding
```cubescript
// Shader parameter definitions in CubeScript
setshader "materialshader"
setuniform "diffuse" 1.0 1.0 1.0       // RGB diffuse color
setuniform "specular" 0.5 0.5 0.5 32.0  // RGB specular + shininess
setuniform "ambient" 0.2 0.2 0.2         // RGB ambient

// Texture binding
texture 0 "textures/diffuse.png"    // Bind to texture unit 0
texture 1 "textures/normal.png"     // Bind to texture unit 1
texture 2 "textures/specular.png"   // Bind to texture unit 2
```

## C++ Shader Usage

### Basic Shader Operations
```cpp
// Set active shader with parameters
SETSHADER(materialshader, diffuse, normal, specular);

// Set shader variant
SETVARIANT(materialshader, variant_index, slot, vslot);

// Direct shader binding
Shader *s = lookupshaderbyname("materialshader");
if(s) s->set(slot, vslot);

// Variant binding with row/column selection
slot.shader->setvariant(col, row, slot, vslot);
```

### Shader Parameter Definitions
```cpp
// Global parameters (shared across all shader instances)
GLOBALPARAM(name, vals);              // Vector parameter
GLOBALPARAMF(name, x, y, z, w);      // Float parameter
GLOBALPARAMI(name, x, y, z, w);      // Integer parameter

// Local parameters (per-shader instance)
LOCALPARAM(name, vals);               // Vector parameter
LOCALPARAMF(name, x, y, z, w);       // Float parameter
LOCALPARAMI(name, x, y, z, w);       // Integer parameter

// Example usage
GLOBALPARAMF(lightdir, lightdir.x, lightdir.y, lightdir.z, 0);
LOCALPARAM(diffuse, slot.color);
```

### Shader Type Checking and Conditional Logic
```cpp
// Check shader capabilities
if(shader->type & SHADER_ENVMAP)
{
    // Handle environment mapping
    bindcubemap(envmap);
}

if(shader->type & SHADER_DYNAMIC)
{
    // Handle dynamic features like pulse glow
    GLOBALPARAMF(pulse, pulsetime, pulseamp, 0, 0);
}

if(shader->type & SHADER_TRIPLANAR)
{
    // Setup triplanar mapping parameters
    GLOBALPARAM(triplanarscale, vec3(triplanarscale));
}

if(shader->type & SHADER_REFRACT)
{
    // Setup refraction parameters
    GLOBALPARAMF(refractindex, 1.33f, 0, 0, 0);  // Water refraction index
}
```

### Advanced Shader Management
```cpp
// Shader compilation and error handling
bool compileshader(const char *name, const char *vs, const char *fs, int type)
{
    Shader *s = newshader(type, name, vs, fs);
    if(!s || s->type & SHADER_INVALID)
    {
        conoutf(CON_ERROR, "Failed to compile shader: %s", name);
        return false;
    }
    return true;
}

// Dynamic shader generation
void generateshadervariant(int features)
{
    string vs, fs;
    formatstring(vs, "%s%s%s",
        basevertex,
        (features & SHADER_ENVMAP) ? envmapvertex : "",
        (features & SHADER_DYNAMIC) ? dynamicvertex : "");
    
    formatstring(fs, "%s%s%s",
        basefragment,
        (features & SHADER_ENVMAP) ? envmapfragment : "",
        (features & SHADER_DYNAMIC) ? dynamicfragment : "");
    
    compileshader("generated", vs, fs, features);
}
```

## Deferred Rendering Integration

### Deferred Shader Patterns
```cubescript
// G-buffer generation shader
defershader $SHADER_DEFERRED "gbuffer" [
    @(ginterpvert)
    varying vec3 normal;
    varying vec2 texcoord;
    void main(void) {
        gl_Position = camprojmatrix * vvertex;
        normal = vnormal;
        texcoord = vtexcoord0.xy;
    }
] [
    @(ginterpfrag)
    uniform sampler2D diffusemap;
    uniform sampler2D normalmap;
    varying vec3 normal;
    varying vec2 texcoord;
    void main(void) {
        vec3 albedo = texture2D(diffusemap, texcoord).rgb;
        vec3 n = normalize(normal);
        gl_FragData[0] = vec4(albedo, 1.0);      // Albedo buffer
        gl_FragData[1] = vec4(n * 0.5 + 0.5, 1.0); // Normal buffer
    }
]
```

### Lighting Pass Shaders
```cpp
// Setup lighting pass
void setuplightingpass()
{
    SETSHADER(deferredlight);
    GLOBALPARAM(lightpos, lightpos);
    GLOBALPARAM(lightcolor, lightcolor);
    GLOBALPARAMF(lightradius, lightradius, 1.0f/lightradius, 0, 0);
    
    // Bind G-buffers
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, galbedo);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gnormal);
}
```

### Radiance hint split blending (`h`)

`deferredlight` gets `h` after `r<N>` when `rhblend > 0` and `rhsplits > 1`
(`loaddeferredlightshader`, `src/engine/renderlights.cpp`). `deferred.cfg` turns it
into `DL_RHBLEND`, which switches `getrhlight` (`deferred/deferredlight.frag`) to the
blended path: each split fades into the next coarser one over `rhblend` cells inside
its faces, and the last one fades out to an empty hint (no GI), with weights from
`rhblendtc[]` and `rhblendedge` (`radiancehints::bindparams`). Without `h`, the text is the hard lookup, token for
token.

## Performance Optimization

### Shader Optimization Guidelines
```cpp
// Prefer uniform buffer objects for large parameter sets
struct materialuniforms
{
    vec3 diffuse;
    vec3 specular;
    float shininess;
    float alpha;
};

// Batch shader state changes
void rendermaterials(const vector<Material*> &materials)
{
    Shader *lastshader = NULL;
    loopv(materials)
    {
        Material *m = materials[i];
        if(m->shader != lastshader)
        {
            m->shader->set();
            lastshader = m->shader;
        }
        m->bind();
        m->render();
    }
}

// Use shader variants efficiently
void selectshadervariant(const Material &mat)
{
    int variant = 0;
    if(mat.hasnormalmap()) variant |= 1;
    if(mat.hasspecularmap()) variant |= 2;
    if(mat.hasglowmap()) variant |= 4;
    
    SETVARIANT(materialshader, variant, mat.slot, mat.vslot);
}
```
