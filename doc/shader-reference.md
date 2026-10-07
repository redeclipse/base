# Red Eclipse Shader System Reference

This document provides comprehensive information about Red Eclipse's shader system, including shader types, CubeScript integration, and C++ usage patterns.

## Shader Type System

Red Eclipse uses a bitfield system for shader types, defined with `SHADER_ENUM` and `ENUM_ALN(SHADER)` in `src/engine/texture.h` (so the `$SHADER_*` values are also available to CubeScript):

### Shader Type Definitions
```cpp
#define SHADER_ENUM(en, um) \
    en(um, Default, DEFAULT, 0) en(um, World, WORLD, 1<<0) en(um, Environment Map, ENVMAP, 1<<1) en(um, Refract, REFRACT, 1<<2) \
    en(um, Option, OPTION, 1<<3) en(um, Dynamic, DYNAMIC, 1<<4) en(um, Triplanar, TRIPLANAR, 1<<5) \
    en(um, Invlaid, INVALID, 1<<6) en(um, Deferred, DEFERRED, 1<<7)
ENUM_ALN(SHADER);

// SHADER_DEFAULT    basic shader without special features
// SHADER_WORLD      world geometry rendering shaders
// SHADER_ENVMAP     environment mapping/reflections
// SHADER_REFRACT    refractive materials (glass, water)
// SHADER_OPTION     optional shader features
// SHADER_DYNAMIC    dynamic/animated shaders (pulse glow)
// SHADER_TRIPLANAR  triplanar texture mapping
// SHADER_INVALID    shader compilation failed
// SHADER_DEFERRED   deferred loading shader
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

// Lazy shader (loaded on first use); lazyshader is a CubeScript alias in config/glsl/shared.cfg
// that wraps defershader and shader
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

### Shader Parameter Binding
```cubescript
// Select the shader for the following texture slots and set its parameters
setshader "materialshader"
setshaderparam specscale 0.5 0.5 0.5     // name x y z w
setshaderparam glowcolor 1.0 0.5 0.0
setshaderparam envscale 0.2 0.2 0.2
// setuniformparam and defuniformparam take the same arguments for uniform parameters

// Texture slots: texture <type> <file> [rot xoffset yoffset scale]
// Type 0 or c (diffuse) starts a new slot, other types add layers to it:
// n normal, s specular, g glow, z depth, a alpha, e environment, v displacement, decal
texture 0 "textures/diffuse.png"
texture n "textures/normal.png"
texture s "textures/specular.png"
```

## C++ Shader Usage

### Basic Shader Operations
```cpp
// Look up (once, cached in a static) and set a shader; extra arguments go to Shader::set()
SETSHADER(materialshader);
SETSHADER(materialshader, slot, vslot);

// Set a shader variant by column and row; extra arguments go to Shader::setvariant()
SETVARIANT(materialshader, col, row);
SETVARIANT(materialshader, col, row, slot, vslot);

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
// Shaders are normally created from CubeScript (shader, defershader, variantshader), which call
// newshader() in src/engine/shader.cpp:
Shader *newshader(int type, const char *name, const char *vs, const char *ps, bool mapdef = false, Shader *variant = NULL, int row = 0);

// Look up and check shaders by name
Shader *s = lookupshaderbyname("materialshader"); // NULL if not defined
Shader *u = useshaderbyname("materialshader");    // also forces a deferred shader to load
if(s && s->invalid()) conoutf(colourred, "Shader failed to compile: %s", s->name);
if(s && s->loaded()) s->set();                   // not deferred and not invalid
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
    
    SETVARIANT(materialshader, variant, 0, mat.slot, mat.vslot);
}
```
