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
