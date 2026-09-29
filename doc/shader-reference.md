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
(`config/glsl/blur.cfg`, `config/glsl/blur/`) the third and decals
(`config/glsl/decal.cfg`, `config/glsl/decal/`) the fourth. They are the
pattern for the rest:

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
- Don't turn a loop the generator unrolled into a GLSL loop. Write one macro
  line per tap, each under an `#if` on the tap count. `shaders.ps1 check` proves an unrolled port at the SPIR-V tier.
  A loop compiles differently (`spirv-opt -O` doesn't unroll), so it could only be
  proved by pixels, and offset fetches (`texture2DRectOffset`) need a constant
  offset, which a loop index isn't.
- Keep every macro on one line. Line continuation needs GLSL 4.20, and the engine
  emits lower versions.
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
  - `gdepth.glsl`: `GDEPTH_UNPACK(val)` (the default `gdepthunpack`) and
    `GDEPTH_PACK(name, val)` (`gpackdepth`). Pull it in with
    `shader_include_fs` and define `GDEPTH_FORMAT` first. A shader-specific
    depth variant stays in its own file (e.g. AO's linear reads).
  - `screentexcoord.glsl`: `vtexcoord0`/`vtexcoord1` (`screentexcoord`), for
    `shader_include_vs`. Declare `vvertex` and `uniform vec4 screentexcoord<n>`.
  - `luma.glsl`: `LUMWEIGHTS`, the `vec3` of `lumweights`. Keep the two in
    step until the last `@lumweights` generator is ported.
  - `gnormal.glsl`: `GNORMAL_PACK(n)` and `GNORMAL_PACK_BLEND(n, k)`
    (`gnormpack` without and with its weight). Define `USEPACKNORM`
    (`$usepacknorm`) first; the shader declares `gnormal`.
  - `gcolor.glsl`: `GSPEC_PACK(gloss)`, `GSPEC_PACK_SPEC(gloss, spec)`
    (`gspecpack` with one and two arguments) and `GGLOW_PACK(glow)`
    (`gglowpack glow`). `GGLOW_PACK` declares `glowk`, and `GGLOW_PACKNORM`
    is the weight it leaves for `GNORMAL_PACK_BLEND`, where `gglowpack`
    used to `#define packnorm`. The blend-layer forms of `gspecpack` and the
    glow-less `gglowpack` (world, model) aren't there yet; add them with
    those ports.

  Shared helpers are one-line macros, not GLSL functions: a helper function
  compiles to different SPIR-V than the inline code it replaces. Moving a
  `#define` into an include keeps the preprocessed tokens, so the rows stay
  `PASS-TEXT`.
- Keep array sizes and other literals literal. `weights[BLUR_RADIUS + 1]`
  preprocesses to `weights[3 + 1]`, not `weights[4]`, which costs the TEXT
  tier; `blur_defs.glsl` maps the radius to `BLUR_SIZE` with an `#if` chain.
- A family-private include (`smaa_defs.glsl`, `blur_defs.glsl`) holds the
  macros derived from the defines when more than one stage needs them.
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
