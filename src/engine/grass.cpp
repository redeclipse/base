#include "engine.h"

VAR(IDF_PERSIST, grass, 0, 1, 1);
VAR(IDF_PERSIST, grassdist, 0, 1024, 10000);
VAR(IDF_MAP, grassheight, 1, 4, 64);
VAR(IDF_MAP, grassanimmillis, 0, 3000, 60000); // the period of the wind's gusts
FVAR(IDF_MAP, grassanimscale, 0, 0.03f, 1); // how far the wind bends the grass
FVAR(IDF_MAP, grassdensity, 0, GRASSDENSITY, 16);
FVAR(IDF_MAP, grasswidth, 0, GRASSWIDTH, 8);
FVAR(IDF_MAP, grassspec, 0, 0, 1);
FVAR(IDF_MAP, grassnormalblend, 0, 0.75f, 1); // how far blade normals lean towards the ground's, so blades catch light from above
FVAR(IDF_MAP, grassblend, 0, 1, 1);

// imports the old texture repeat, where a higher scale drew more and thinner blades
void importgrassscale(float scale)
{
    if(scale <= 0) return;
    setfvar("grassdensity", clamp(GRASSDENSITY*scale/2, 0.f, 16.f), true);
    setfvar("grasswidth", GRASSWIDTH*2/scale, true);
}

static Shader *bladeshader = NULL, *bladeshadowshader = NULL;

void loadgrassshaders()
{
    bladeshader = generateshader("blade", "bladeshader");
    bladeshadowshader = generateshader("bladeshadow", "bladeshadowshader");
}

void cleargrassshaders()
{
    bladeshader = bladeshadowshader = NULL;
}

static void cleanupblades();

void cleanupgrass()
{
    cleanupblades();
    cleargrassshaders();
}

#define GRASSCHUNKPATCHES 256 // patches per uniform block, four vec4s each fill the guaranteed 16KB
#define GRASSCHUNKSIZE (GRASSCHUNKPATCHES*4*sizeof(vec4))
#define GRASSCHUNKAREA 128 // chunks gather the patches of a cell this wide, so each can be culled and thinned with distance on its own
#define GRASSBLADEVERTS 7
#define GRASSBLADEINDICES 15
#define GRASSBLADELODINDICES 9 // a far blade drops its lowest bend
#define GRASSUBOBINDING 0

#define GRASSCLUMPSIZE 12 // blades in a clump share their height, facing, lean and tint

FVAR(IDF_PERSIST, grassthin, 0, 0.15f, 1); // share of grassdist where blades start to thin out
FVAR(IDF_PERSIST, grassthinmin, 0.0625f, 0.0625f, 1); // share of blades left at grassdist, widened to cover the same ground
FVAR(IDF_PERSIST, grasslodmesh, 0, 0.4f, 1); // share of grassdist beyond which blades use fewer triangles
FVAR(IDF_PERSIST, grassdensityscale, 0.25f, 1, 1); // share of each texture's blades drawn, widened to cover the same ground
VAR(IDF_PERSIST, grassshadow, 0, 0, 2); // blades cast shadows from the sun: 0 = none, 1 = static, as thin swaying blades shimmer in the shadow map, 2 = animated with the wind
VAR(IDF_PERSIST, grassshadowdist, 0, 256, 10000); // how far from the camera blades cast them, fading out over the last quarter
VAR(IDF_MAP, grasscastshadow, 0, 1, 2); // the most of grassshadow the map's grass casts, where the texture doesn't choose

static GLuint bladevbo = 0, bladeebo = 0;
static int grassuboalign = 0;

static void setupblades()
{
    if(bladevbo) return;
    // three pairs of points up the blade, then the tip: x is the distance up the blade, y the side
    static const float verts[GRASSBLADEVERTS][2] = { { 0, -1 }, { 0, 1 }, { 1/3.0f, -1 }, { 1/3.0f, 1 }, { 2/3.0f, -1 }, { 2/3.0f, 1 }, { 1, 0 } };
    static const ushort indices[GRASSBLADEINDICES + GRASSBLADELODINDICES] = { 0, 1, 3, 0, 3, 2, 2, 3, 5, 2, 5, 4, 4, 5, 6, 0, 1, 5, 0, 5, 4, 4, 5, 6 };
    glGenBuffers_(1, &bladevbo);
    gle::bindvbo(bladevbo);
    glBufferData_(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    gle::clearvbo();
    glGenBuffers_(1, &bladeebo);
    gle::bindebo(bladeebo);
    glBufferData_(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    gle::clearebo();
    GLint align = 0;
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &align);
    grassuboalign = max(int(align), int(sizeof(vec4)));
}

VARR(grasstramplesize, 256); // texels across the trample map, which follows the camera, a power of two
VARR(grasstramplescale, 2); // world units per texel
#define GRASSTRAMPLEMAXAGE 60000 // older stamps are cleared, before their 16 bit time wraps
#define GRASSTRAMPLESWEEP 16 // rows checked for old stamps each frame, covering the map within the 5.5 seconds before a time wraps down to 3 fps

VAR(IDF_PERSIST, grasstrample, 0, 1, 1);
VAR(IDF_MAP, grasstramplerecover, 0, 1500, 60000); // how long trampled grass takes to stand up, if the texture doesn't say
FVAR(IDF_MAP, grasstramplehold, 0, 0.05f, 1); // the share of that time it stays flat before it starts to rise

// where players walk through grass: each texel holds the direction blades are pushed and when, in milliseconds that wrap every 65.5 seconds
struct grasstrampler
{
    GLuint tex;
    uchar *texels;
    bool *dirtyrows;
    ivec2 origin; // the world texel at the window's lowest corner
    int sweep;
    bool valid;

    grasstrampler() : tex(0), texels(NULL), dirtyrows(NULL), origin(0, 0), sweep(0), valid(false) {}

    static int now() { return lastmillis&0xFFFF; }

    uchar *texel(int x, int y) { return &texels[((y&(grasstramplesize-1))*grasstramplesize + (x&(grasstramplesize-1)))*4]; }

    static void clear(uchar *t) { t[0] = t[1] = 128; t[2] = t[3] = 0; }

    void cleanup()
    {
        if(tex) { glDeleteTextures(1, &tex); tex = 0; }
        valid = false;
    }

    void upload(int x, int y, int w, int h)
    {
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, grasstramplesize);
        glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, &texels[(y*grasstramplesize + x)*4]);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    }

    // follows the camera, clearing the rows and columns that wrap around to the far side
    void move(const vec &pos)
    {
        if(!texels)
        {
            texels = new uchar[grasstramplesize*grasstramplesize*4];
            dirtyrows = new bool[grasstramplesize];
            memset(dirtyrows, 0, grasstramplesize);
        }
        ivec2 neworigin(int(floorf(pos.x/grasstramplescale)) - grasstramplesize/2, int(floorf(pos.y/grasstramplescale)) - grasstramplesize/2);
        if(!tex)
        {
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            setuptexparameters(tex, NULL, 0, 0, GL_RGBA8, GL_TEXTURE_2D);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            valid = false;
        }
        else glBindTexture(GL_TEXTURE_2D, tex);
        if(!valid || abs(neworigin.x - origin.x) >= grasstramplesize || abs(neworigin.y - origin.y) >= grasstramplesize)
        {
            loopi(grasstramplesize*grasstramplesize) clear(&texels[i*4]);
            if(valid) upload(0, 0, grasstramplesize, grasstramplesize);
            else glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, grasstramplesize, grasstramplesize, 0, GL_RGBA, GL_UNSIGNED_BYTE, texels);
            origin = neworigin;
            valid = true;
            return;
        }
        for(int x = min(origin.x, neworigin.x); x < max(origin.x, neworigin.x); x++)
        {
            int col = x&(grasstramplesize-1);
            loopi(grasstramplesize) clear(&texels[(i*grasstramplesize + col)*4]);
            upload(col, 0, 1, grasstramplesize);
        }
        for(int y = min(origin.y, neworigin.y); y < max(origin.y, neworigin.y); y++)
        {
            int row = y&(grasstramplesize-1);
            loopi(grasstramplesize) clear(&texels[(row*grasstramplesize + i)*4]);
            dirtyrows[row] = true;
        }
        origin = neworigin;
    }

    // pushes the blades around a player's feet away from them
    void stamp(const vec &feet, float radius)
    {
        int tick = now(), x1 = max(int(floorf((feet.x - radius)/grasstramplescale)), origin.x), y1 = max(int(floorf((feet.y - radius)/grasstramplescale)), origin.y),
            x2 = min(int(floorf((feet.x + radius)/grasstramplescale)), origin.x + grasstramplesize - 1), y2 = min(int(floorf((feet.y + radius)/grasstramplescale)), origin.y + grasstramplesize - 1);
        for(int y = y1; y <= y2; y++)
        {
            for(int x = x1; x <= x2; x++)
            {
                vec2 off((x + 0.5f)*grasstramplescale - feet.x, (y + 0.5f)*grasstramplescale - feet.y);
                float dist = off.magnitude();
                if(dist > radius) continue;
                float edge = dist/radius, push = 1 - edge*edge;
                if(dist > 0.01f) off.mul(push/dist);
                else off = vec2(0, 0);
                uchar *t = texel(x, y);
                t[0] = uchar(clamp(int(128 + off.x*127), 1, 255));
                t[1] = uchar(clamp(int(128 + off.y*127), 1, 255));
                t[2] = tick>>8;
                t[3] = tick&0xFF;
            }
            dirtyrows[y&(grasstramplesize-1)] = true;
        }
    }

    // clears stamps old enough that their time could wrap around
    void sweepold()
    {
        int tick = now();
        loopk(GRASSTRAMPLESWEEP)
        {
            int row = sweep;
            sweep = (sweep + 1)%grasstramplesize;
            loopi(grasstramplesize)
            {
                uchar *t = &texels[(row*grasstramplesize + i)*4];
                if(t[0] == 128 && t[1] == 128) continue;
                int age = (tick - ((t[2]<<8) | t[3]))&0xFFFF;
                if(age < GRASSTRAMPLEMAXAGE) continue;
                clear(t);
                dirtyrows[row] = true;
            }
        }
    }

    void update()
    {
        move(camera1->o);
        loopi(game::numdynents())
        {
            dynent *d = game::iterdynents(i);
            if(!d || d->state != CS_ALIVE || d->physstate < PHYS_SLOPE) continue;
            vec feet = d->feetpos();
            if(feet.dist(camera1->o) > grasstramplesize*grasstramplescale/2 + d->radius) continue;
            stamp(feet, d->radius*1.5f + 1);
        }
        sweepold();
        loopi(grasstramplesize) if(dirtyrows[i])
        {
            int first = i;
            while(i + 1 < grasstramplesize && dirtyrows[i + 1]) i++;
            upload(0, first, grasstramplesize, i + 1 - first);
            memset(&dirtyrows[first], 0, i + 1 - first);
        }
    }
};
static grasstrampler trampler;

// a batch of patches with the same grass in one cell, drawn in one instanced draw when any vertex array they came from is in view
struct grasschunk
{
    int blend, texture, cell, offset, numpatches, numblades, firstva, numvas, shadow;
    float density, height, radius, dist;
    vec center, bbmin, bbmax;
};

// the patches of every vertex array, packed into one uniform buffer, rebuilt when the world or a texture's density changes
struct grassbatches
{
    vector<grasschunk> chunks;
    vector<vtxarray *> chunkvas;
    GLuint buf;
    bool dirty;

    grassbatches() : buf(0), dirty(true) {}

    void cleanup()
    {
        if(buf) { glDeleteBuffers_(1, &buf); buf = 0; }
        chunks.setsize(0);
        chunkvas.setsize(0);
        dirty = true;
    }
};
static grassbatches batches;

void changedgrass()
{
    batches.dirty = true;
}

static void cleanupblades()
{
    trampler.cleanup();
    if(bladevbo) { glDeleteBuffers_(1, &bladevbo); bladevbo = 0; }
    if(bladeebo) { glDeleteBuffers_(1, &bladeebo); bladeebo = 0; }
    batches.cleanup();
}

// editors see the grass as the mapper set it, at full density
static float grassdensityshare()
{
    return editmode ? 1.0f : grassdensityscale;
}

static float grasspatchdensity(const VSlot &s)
{
    float density = s.grassdensity > 0 ? s.grassdensity : grassdensity, blend = s.grassblend > 0 ? s.grassblend : grassblend;
    return density*blend*grassdensityshare();
}

static float grasspatcharea(const grasstri &g, int first)
{
    return vec().cross(vec(g.v[first+1]).sub(g.v[0]), vec(g.v[first+2]).sub(g.v[0])).magnitude()/2;
}

static float grasspatchheight(const VSlot &s)
{
    return s.grassheight > 0 ? s.grassheight : grassheight;
}

static bool grassbatchesstale()
{
    if(batches.dirty) return true;
    loopv(batches.chunks)
    {
        grasschunk &c = batches.chunks[i];
        const VSlot &s = lookupvslot(c.texture, false);
        if(c.density != grasspatchdensity(s) || c.height != grasspatchheight(s)) return true;
    }
    return false;
}

struct grasspatchref
{
    const grasstri *tri;
    vtxarray *va;
    int variant, cell;
};

// the first variant of a texture with the same grass, which the patches of every such variant share chunks with
static int grassvariant(hashtable<int, int> &variants, int texture)
{
    int *cached = variants.access(texture);
    if(cached) return *cached;
    const VSlot &vs = lookupvslot(texture, false);
    int variant = vs.index;
    for(const VSlot *v = vs.slot->variants; v && v != &vs; v = v->next)
    {
        if(v->samegrass(vs)) { variant = v->index; break; }
    }
    variants[texture] = variant;
    return variant;
}

static bool sortgrasspatches(const grasspatchref &a, const grasspatchref &b)
{
    if(a.tri->blend != b.tri->blend) return a.tri->blend < b.tri->blend;
    if(a.variant != b.variant) return a.variant < b.variant;
    if(a.cell != b.cell) return a.cell < b.cell;
    return a.va < b.va;
}

// packs each patch as four vec4s: the corners, with the first blade, the share of the first triangle, the blend orient and the corner count
static void genbatches()
{
    batches.cleanup();
    batches.dirty = false;
    // variants of a texture with the same grass share chunks
    static vector<grasspatchref> patches;
    static hashtable<int, int> variants;
    patches.setsize(0);
    variants.clear();
    loopv(valist)
    {
        vtxarray *va = valist[i];
        loopvj(va->grasstris)
        {
            const grasstri &g = va->grasstris[j];
            grasspatchref &p = patches.add();
            p.tri = &g;
            p.va = va;
            p.variant = grassvariant(variants, g.texture);
            p.cell = (int(floorf(g.center.y/GRASSCHUNKAREA))<<16) + int(floorf(g.center.x/GRASSCHUNKAREA));
        }
    }
    patches.sort(sortgrasspatches);

    static vector<vec4> data;
    data.setsize(0);
    grasschunk *c = NULL;
    vec cmin(0, 0, 0), cmax(0, 0, 0);
    // blades reach up to their height from their roots, leaning, swaying or trampled to any side but never down
    #define ENDCHUNK if(c) { cmin.x -= c->height; cmin.y -= c->height; cmax.add(c->height); c->bbmin = cmin; c->bbmax = cmax; c->center = vec(cmin).add(cmax).mul(0.5f); c->radius = cmax.dist(cmin)/2; }
    loopv(patches)
    {
        const grasspatchref &p = patches[i];
        const grasstri &g = *p.tri;
        const VSlot &s = lookupvslot(p.variant, false);
        if(!s.hasgrass()) continue;
        float density = grasspatchdensity(s), area1 = grasspatcharea(g, 0), area2 = g.numv > 3 ? grasspatcharea(g, 1) : 0;
        int blades = int((area1 + area2)*density + 0.5f);
        if(blades <= 0) continue;
        if(!c || c->texture != p.variant || c->blend != g.blend || c->numpatches >= GRASSCHUNKPATCHES || c->cell != p.cell)
        {
            ENDCHUNK;
            cmin = vec(1e16f, 1e16f, 1e16f);
            cmax = vec(-1e16f, -1e16f, -1e16f);
            while((data.length()*sizeof(vec4))%grassuboalign) data.add(vec4(0, 0, 0, 0));
            c = &batches.chunks.add();
            c->texture = p.variant;
            c->cell = p.cell;
            c->blend = g.blend;
            c->offset = data.length()*sizeof(vec4);
            c->numpatches = c->numblades = 0;
            c->firstva = batches.chunkvas.length();
            c->numvas = 0;
            c->density = density;
            c->height = grasspatchheight(s);
        }
        if(!c->numvas || batches.chunkvas.last() != p.va) { batches.chunkvas.add(p.va); c->numvas++; }
        data.add(vec4(g.v[0], c->numblades));
        data.add(vec4(g.v[1], area2 > 0 ? area1/(area1 + area2) : 1));
        data.add(vec4(g.v[2], g.orient));
        data.add(vec4(g.v[3], g.numv));
        c->numpatches++;
        c->numblades += blades;
        loopk(g.numv) { cmin.min(g.v[k]); cmax.max(g.v[k]); }
    }
    ENDCHUNK;
    #undef ENDCHUNK
    // every chunk binds a full block, so the buffer must reach past the last one
    if(c) data.pad(c->offset/sizeof(vec4) + GRASSCHUNKPATCHES*4 - data.length());
    if(data.empty()) return;

    glGenBuffers_(1, &batches.buf);
    glBindBuffer_(GL_UNIFORM_BUFFER, batches.buf);
    glBufferData_(GL_UNIFORM_BUFFER, data.length()*sizeof(vec4), data.getbuf(), GL_STATIC_DRAW);
    glBindBuffer_(GL_UNIFORM_BUFFER, 0);
}

#define GRASSPIXELSFULL 16 // blades this many pixels tall are drawn at full density
#define GRASSPIXELSTHIN 2 // and thin out to grassthinmin by this height, vanishing below a pixel

static inline float grasssmoothstep(float lo, float hi, float x)
{
    float t = clamp((x - lo)/(hi - lo), 0.0f, 1.0f);
    return t*t*(3 - 2*t);
}

// the share of blades kept at a distance, by how far away and how tall on screen they are, matching the shader
static float grasskeep(float dist, float height, float pixelsize)
{
    float pixels = height/max(dist*pixelsize, 1e-6f);
    if(pixels < 1) return 0;
    float keep = 1 + (grassthinmin - 1)*grasssmoothstep(grassdist*grassthin, grassdist, dist);
    return min(keep, grassthinmin + (1 - grassthinmin)*grasssmoothstep(GRASSPIXELSTHIN, GRASSPIXELSFULL, pixels));
}

static inline float grasspixelsize() { return 2*tanf(fovy/2*RAD)/max(viewh, 1); }

static void setbladeshader(Shader *shader, bool blend)
{
    if(blend) shader->setvariant(0, 0);
    else shader->set();
    GLuint block = glGetUniformBlockIndex_(Shader::lastshader->program, "grasspatches");
    if(block != GL_INVALID_INDEX) glUniformBlockBinding_(Shader::lastshader->program, block, GRASSUBOBINDING);
    LOCALPARAMF(grassdist, grassdist);
    LOCALPARAMF(grassclump, 1.0f/GRASSCLUMPSIZE);
    // the world size of a pixel at a distance of one unit, for the narrowest blade and the thinning by height on screen
    LOCALPARAMF(bladethin, grassdist*grassthin, grassthinmin, grasspixelsize());
    LOCALPARAMF(bladepixels, GRASSPIXELSTHIN, GRASSPIXELSFULL);
    LOCALPARAMF(bladeshadowdist, min(grassdist, grassshadowdist));
}


// draws the blades of each chunk in range, into the g-buffer or, in a shadow pass, into the current sun shadow cascade, whose texels are shadowtexel wide
static void drawblades(bool shadow, float shadowtexel = 0)
{
    float winddist = getwindanimdist();
    Shader *shader = shadow ? bladeshadowshader : bladeshader;

    gle::bindvbo(bladevbo);
    gle::bindebo(bladeebo);
    gle::vertexpointer(2*sizeof(float), NULL, GL_FLOAT, 2);
    gle::enablevertex();

    if(grassbatchesstale()) genbatches();

    // mark the vertex arrays in view, then draw the chunks with any of them, front to back
    static int mark = 0;
    mark++;
    for(vtxarray *va = shadow ? shadowva : visibleva; va; va = shadow ? va->rnext : va->next)
    {
        if(va->grasstris.empty()) continue;
        if(shadow ? !(va->shadowmask&(1<<shadowside)) : va->occluded >= OCCLUDE_GEOM || va->distance > grassdist) continue;
        va->grassmark = mark;
    }
    static vector<grasschunk *> drawn;
    drawn.setsize(0);
    loopv(batches.chunks)
    {
        grasschunk &c = batches.chunks[i];
        bool inview = false;
        loopj(c.numvas) if(batches.chunkvas[c.firstva + j]->grassmark == mark) { inview = true; break; }
        if(!inview) continue;
        if(shadow)
        {
            const VSlot &s = lookupvslot(c.texture, false);
            c.shadow = min(grassshadow, s.grasscastshadow >= 0 ? s.grasscastshadow : grasscastshadow);
            if(!c.shadow) continue;
        }
        // shadows come from the blades the camera keeps, so thinning still follows the camera
        c.dist = camera1->o.dist_to_bb(c.bbmin, c.bbmax);
        if(c.dist > (shadow ? min(grassdist, grassshadowdist) : grassdist) || (!shadow && isvisiblesphere(c.radius, c.center) == VFC_NOT_VISIBLE)) continue;
        drawn.add(&c);
    }
    drawn.sort([](const grasschunk *a, const grasschunk *b) { return a->blend != b->blend ? a->blend < b->blend : a->dist < b->dist; });

    int blend = -1;
    loopv(drawn)
    {
        grasschunk &c = *drawn[i];
        float dist = c.dist;
        const VSlot &s = lookupvslot(c.texture, false);
        // a far chunk skips the blades that are thinned out everywhere in it, keeping every second, fourth, eighth or sixteenth
        float keep = grasskeep(dist, grasspatchheight(s), grasspixelsize());
        if(keep <= 0) continue;
        int stride = 1;
        while(stride < 16 && keep <= 0.5f/stride) stride *= 2;
        bool lodmesh = dist > grassdist*grasslodmesh;
        if(blend != c.blend)
        {
            if(c.blend)
            {
                glActiveTexture_(GL_TEXTURE1);
                bindblendtexture();
                glActiveTexture_(GL_TEXTURE0);
            }
            setbladeshader(shader, c.blend != 0);
            // blades no narrower than a texel and a half of this cascade, so they don't flicker in and out of it
            if(shadow) LOCALPARAMF(bladeshadowtexel, 1.5f*shadowtexel);
            blend = c.blend;
        }
        if(!shadow)
        {
            vec colours[3];
            vec tint = s.grasstint ? vec::fromcolor(getpulsehexcol(s.grasstint)) : vec(1, 1, 1);
            loopk(3) colours[k] = vec::fromcolor(getpulsehexcol(s.grasscolour(k))).mul(tint);
            LOCALPARAMV(bladecolours, colours, 3);
            LOCALPARAMF(bladeshading, s.grassspec >= 0 ? s.grassspec : grassspec, s.grassnormalblend >= 0 ? s.grassnormalblend : grassnormalblend);
        }
        LOCALPARAMF(bladeparams, grasspatchheight(s), (s.grasswidth > 0 ? s.grasswidth : grasswidth)/sqrtf(grassdensityshare()), c.numpatches, stride);
        // the wind at the chunk, fading out beyond where models stop swaying
        float windfade = windanimfalloff ? 1 - clamp((dist - winddist)/windanimfalloff, 0.0f, 1.0f) : (dist <= winddist ? 1.0f : 0.0f);
        vec wind = windfade > 0 && (!shadow || c.shadow >= 2) ? getwind(c.center).mul(windfade*grassanimscale) : vec(0, 0, 0);
        LOCALPARAMF(bladewind, wind.x, wind.y, wind.z, 2*M_PI/max(grassanimmillis, 1));
        LOCALPARAMF(bladeflutter, shadow && c.shadow < 2 ? 0.0f : 1.0f);
        int recover = s.grassrecover > 0 ? s.grassrecover : grasstramplerecover;
        LOCALPARAMF(trampleparams, trampler.origin.x*grasstramplescale, trampler.origin.y*grasstramplescale, grasstrampler::now(), grasstrample && trampler.valid ? recover : 0);
        LOCALPARAMF(tramplehold, min(s.grasstramplehold >= 0 ? s.grasstramplehold : grasstramplehold, 0.99f));
        glBindBufferRange_(GL_UNIFORM_BUFFER, GRASSUBOBINDING, batches.buf, c.offset, GRASSCHUNKSIZE);
        int instances = (c.numblades + stride - 1)/stride;
        if(lodmesh) glDrawElementsInstanced_(GL_TRIANGLES, GRASSBLADELODINDICES, GL_UNSIGNED_SHORT, (const void *)(GRASSBLADEINDICES*sizeof(ushort)), instances);
        else glDrawElementsInstanced_(GL_TRIANGLES, GRASSBLADEINDICES, GL_UNSIGNED_SHORT, NULL, instances);
        xtravertsva += (lodmesh ? 5 : GRASSBLADEVERTS)*instances;
    }

    glBindBufferBase_(GL_UNIFORM_BUFFER, GRASSUBOBINDING, 0);
    gle::disablevertex();
    gle::clearvbo();
    gle::clearebo();
}

void rendergrass()
{
    if(!grass || !grassdist || !bladeshader) return;

    setupblades();

    glDisable(GL_CULL_FACE);

    if(grasstrample)
    {
        glActiveTexture_(GL_TEXTURE2);
        trampler.update();
        glActiveTexture_(GL_TEXTURE0);
    }
    else trampler.cleanup();

    drawblades(false);

    glEnable(GL_CULL_FACE);
}

void rendershadowgrass(float texelsize)
{
    if(!grass || !grassshadow || !grassdist || !bladeshadowshader || !bladevbo) return;

    glDisable(GL_CULL_FACE);
    if(trampler.valid)
    {
        glActiveTexture_(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, trampler.tex);
        glActiveTexture_(GL_TEXTURE0);
    }
    drawblades(true, texelsize);
    glEnable(GL_CULL_FACE);
}
