#include "engine.h"

VAR(IDF_PERSIST, grass, 0, 1, 1);
VAR(IDF_PERSIST, grassdist, 0, 1024, 10000);
VAR(IDF_MAP, grassanimmillis, 0, 3000, 60000); // the period of the wind's gusts
FVAR(IDF_MAP, grassanimscale, 0, 0.03f, 1); // how far the wind bends the grass

// imports the old texture repeat, where a higher scale drew more and thinner blades
void importgrassscale(float scale)
{
    if(scale <= 0) return;
    importgrassmapvar("grassdensity", clamp(GRASSDENSITY*scale/2, 0.f, 16.f));
    importgrassmapvar("grasswidth", GRASSWIDTH*2/scale);
}

static Shader *bladeshader = NULL, *bladefrondshader = NULL, *bladeshadowshader = NULL, *bladeheadshader = NULL, *bladeheadshadowshader = NULL;

void loadgrassshaders()
{
    bladeshader = generateshader("blade", "bladeshader");
    bladefrondshader = generateshader("bladefrond", "bladefrondshader");
    bladeshadowshader = generateshader("bladeshadow", "bladeshadowshader");
    bladeheadshader = generateshader("bladehead", "bladeheadshader");
    bladeheadshadowshader = generateshader("bladeheadshadow", "bladeheadshadowshader");
}

void cleargrassshaders()
{
    bladeshader = bladefrondshader = bladeshadowshader = bladeheadshader = bladeheadshadowshader = NULL;
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
#define GRASSBLADEPOINTS 16 // the most points around a blade shape
#define GRASSBLADEPARTS 4 // the most lines or rings a blade shape is made of
#define GRASSBLADEROWS 4 // rows of points up a blade, the last at its tip
#define GRASSBLADELODSIDES 8 // smooth shapes with at least this many points, an even number, use every second one when far, as they also drop their lowest bend
#define GRASSBLADELODS 3 // a shape uses all its points, every second one, or every fourth on smooth rings of half as many again
#define GRASSHEADRING 20 // points around each ring of a head, between a pole below and one above
#define GRASSHEADRINGS 4
#define GRASSHEADVERTS (GRASSHEADRING*GRASSHEADRINGS + 2)
#define GRASSHEADINDICES (GRASSHEADRING*6*GRASSHEADRINGS)
#define GRASSHEADLEAVES 4 // the most leaves on a head's stalk
#define GRASSHEADLEAFVERTS 6 // each leaf's outline from its base out to its tip
#define GRASSHEADLEAFINDICES 12
#define GRASSHEADSTALKSHAPE "Round" // the blade shape around a head's stalk, unless it picks another
static_assert(GRASSHEADS == 3, "the blade shaders take the heads as vec3s");
#define GRASSHEADSHARE 0.0625f // the share of the blades that grow a head, where the texture doesn't say
#define GRASSHEADLEVELS 6 // a head's outline goes from a pole below through four rings to a pole above
#define GRASSHEADLEVELVEC4S 3 // each level of a head shape takes three vec4s in the shader
#define GRASSUBOBINDING 0
#define GRASSHEADUBOBINDING 1
#define GRASSHEADLISTMAX (GRASSCHUNKSIZE/sizeof(uint)) // blades in each draw from a head list, which binds a block as big as a chunk's
#define GRASSHEADTIERS 5 // the blades every sixteenth, eighth, fourth or second, or any other

#define GRASSCLUMPSIZE 12 // blades in a clump share their height, facing, lean and tint

FVAR(IDF_PERSIST, grassthin, 0, 0.15f, 1); // share of grassdist where blades start to thin out
FVAR(IDF_PERSIST, grassthinmin, 0.0625f, 0.0625f, 1); // share of blades left at grassdist, widened to cover the same ground
FVAR(IDF_PERSIST, grasslodmesh, 0, 0.4f, 1); // share of grassdist beyond which blades use fewer triangles
FVAR(IDF_PERSIST, grasspixelsfull, 8, 16, 64); // blades this many pixels tall on screen are drawn at full density
FVAR(IDF_PERSIST, grasspixelsthin, 1, 2, 8); // and thin out to grassthinmin by this height, vanishing below a pixel
FVAR(IDF_PERSIST, grassstalkpixels, 1, 8, 64); // stalks this many pixels wide on screen use every point of their shape
FVAR(IDF_PERSIST, grassstalkpixelshalf, 1, 3, 64); // and every second one down to this width, then every fourth
FVAR(IDF_PERSIST, grassheadlodpixels, 1, 24, 256); // heads narrower than this on screen use every second point, and a third of it every fourth
FVAR(IDF_PERSIST, grassheadpixels, 0, 0.75f, 16); // heads narrower than this on screen aren't drawn
FVAR(IDF_PERSIST, grassfrondpixels, 0, 1.5f, 16); // fronds whose ribbons are narrower than this on screen are drawn as plain blades, which cost less
FVAR(IDF_PERSIST, grassleafpixels, 0, 3, 32); // leaves shorter than this on screen aren't drawn
FVAR(IDF_PERSIST, grassdensityscale, 0.25f, 1, 1); // share of each texture's blades drawn, widened to cover the same ground
VAR(IDF_PERSIST, grassshadow, 0, 0, 1); // blades cast shadows from the sun, static or animated as the texture's texgrasscastshadow chooses
VAR(IDF_PERSIST, grassshadowdist, 0, 256, 10000); // how far from the camera blades cast them, fading out over the last quarter
VAR(0, oqgrass, 0, 1, 1);

// entries in a blade shape's outline, which the shader indexes: a pair of points at each corner for hard edges, then each cap's ring and centre
VARR(grassbladeoutline, GRASSBLADEPOINTS*3 + GRASSBLADEPARTS);

static GLuint bladevbo = 0, bladeebo = 0, headvbo = 0, headebo = 0;
static int grassuboalign = 0;

// the shapes around blades, defined by grassbladeshapedef in config/grass.cfg, which maps refer to by their order
struct grassbladeshape
{
    char *name;
    bool smooth, open, straight; // straight lines from one point to another only, such as crossed blades
    vector<vec2> points;
    vector<int> parts; // where each line or ring of points starts
    vector<vec4> outline; // each entry's place across the blade and its normal there, a zero normal for a cap, which faces along the blade
    vector<ivec2> segments; // the pairs of entries each side of the blade joins
    vector<ivec> caps; // those of the caps' rings, with the centre they meet at
    int firstindex[GRASSBLADELODS][2], numindices[GRASSBLADELODS][2], numverts[GRASSBLADELODS][2]; // for each detail, with every row or without the lowest bend

    grassbladeshape() : name(NULL), smooth(false), open(false), straight(false)
    {
        memset(firstindex, 0, sizeof(firstindex));
        memset(numindices, 0, sizeof(numindices));
        memset(numverts, 0, sizeof(numverts));
    }
    ~grassbladeshape() { DELETEA(name); }

    int partlength(int part) const { return (parts.inrange(part + 1) ? parts[part + 1] : points.length()) - parts[part]; }

    // the outline of one part, a line or a closed ring
    void buildpart(int part)
    {
        int start = parts[part], n = partlength(part);
        if(n < 2) return;
        const vec2 *p = &points[start];
        int edges = open ? n - 1 : n;
        // the normal of each edge, facing the front of an open shape or out of a closed one, whose points go anticlockwise
        vec2 normals[GRASSBLADEPOINTS];
        loopi(edges)
        {
            vec2 d = vec2(p[(i + 1)%n]).sub(p[i]);
            vec2 e = vec2(d.y, -d.x);
            if(open && e.y < 0) e.neg();
            float len = e.magnitude();
            normals[i] = len > 0 ? e.div(len) : vec2(0, 1);
        }
        int first = outline.length();
        if(smooth || open)
        {
            loopi(n)
            {
                int prev = open ? i - 1 : (i + edges - 1)%edges, next = open && i >= edges ? -1 : i;
                vec2 normal(0, 0);
                if(prev >= 0) normal.add(normals[prev]);
                if(next >= 0) normal.add(normals[next]);
                if(!smooth && prev >= 0 && next >= 0)
                { // a crease in an open shape keeps a point for each side of it
                    outline.add(vec4(p[i].x, p[i].y, normals[prev].x, normals[prev].y));
                    normal = normals[next];
                }
                float len = normal.magnitude();
                if(len > 0) normal.div(len);
                outline.add(vec4(p[i].x, p[i].y, normal.x, normal.y));
            }
            // past each crease, an edge runs from its second point to the next one's first
            loopi(edges) segments.add(smooth ? ivec2(first + i, first + (i + 1)%n) : ivec2(first + (i ? 2*i : 0), first + 2*i + 1));
        }
        else loopi(edges)
        { // each hard edge has its own pair of points with its normal
            int a = outline.length();
            outline.add(vec4(p[i].x, p[i].y, normals[i].x, normals[i].y));
            outline.add(vec4(p[(i + 1)%n].x, p[(i + 1)%n].y, normals[i].x, normals[i].y));
            segments.add(ivec2(a, a + 1));
        }
    }

    // a closed part's cap at the tip, a ring of points around its middle
    void buildcap(int part)
    {
        int start = parts[part], n = partlength(part);
        if(open || n < 3) return;
        vec2 centre(0, 0);
        int first = outline.length();
        loopi(n)
        {
            const vec2 &p = points[start + i];
            outline.add(vec4(p.x, p.y, 0, 0));
            centre.add(p);
            caps.add(ivec(first + i, first + (i + 1)%n, first + n));
        }
        centre.div(n);
        outline.add(vec4(centre.x, centre.y, 0, 0));
    }

    void build()
    {
        outline.setsize(0);
        segments.setsize(0);
        caps.setsize(0);
        straight = open;
        loopv(parts) if(partlength(i) != 2) straight = false;
        loopv(parts) buildpart(i);
        loopv(parts) buildcap(i);
    }

    // the points a detail steps over: smooth rings of many points use every second one, or every fourth
    int lodstep(int lod) const
    {
        if(!smooth || open || parts.length() != 1) return 1;
        int n = points.length();
        if(lod >= 2 && n >= GRASSBLADELODSIDES*3/2 && !(n%4)) return 4;
        return lod >= 1 && n >= GRASSBLADELODSIDES && !(n%2) ? 2 : 1;
    }
};

struct grassbladeshaper
{
    vector<grassbladeshape *> shapes;
    grassbladeshape *cur; // the shape grassbladeshapedef is filling in
    bool warned; // a map asked for a shape there isn't, since it last loaded

    grassbladeshaper() : cur(NULL), warned(false) {}

    const grassbladeshape *get(int n) const { return shapes.inrange(n) ? shapes[n] : (shapes.empty() ? NULL : shapes[0]); }

    grassbladeshape *find(const char *name)
    {
        loopv(shapes) if(!strcmp(shapes[i]->name, name)) return shapes[i];
        return NULL;
    }
};
static grassbladeshaper bladeshapes;

// defines a shape by its name, or replaces the one of that name keeping its place, filled in by the commands in body
ICOMMAND(0, grassbladeshapedef, "se", (char *name, uint *body),
{
    if(!name[0]) { conoutf(colourred, "Error: a grass blade shape needs a name"); return; }
    if(bladeshapes.cur) { conoutf(colourred, "Error: grass blade shape %s is defined inside another", name); return; }
    grassbladeshape *shape = bladeshapes.find(name);
    if(!shape)
    {
        if(bladeshapes.shapes.length() >= 256) { conoutf(colourred, "Error: too many grass blade shapes"); return; }
        shape = bladeshapes.shapes.add(new grassbladeshape);
        shape->name = newstring(name);
    }
    shape->smooth = shape->open = false;
    shape->points.setsize(0);
    shape->parts.setsize(0);
    bladeshapes.cur = shape;
    execute(body);
    bladeshapes.cur = NULL;
    loopv(shape->parts) if(shape->partlength(i) < (shape->open ? 2 : 3))
    {
        conoutf(colourred, "Error: a part of grass blade shape %s needs at least %d points", name, shape->open ? 2 : 3);
        break;
    }
    cleanupblades();
});

static grassbladeshape *curbladeshape(const char *cmd)
{
    if(!bladeshapes.cur) conoutf(colourred, "Error: %s is only used in a grassbladeshapedef", cmd);
    return bladeshapes.cur;
}

// normals blend around the points rather than breaking at each one
ICOMMAND(0, grassbladeshapesmooth, "i", (int *smooth), { grassbladeshape *shape = curbladeshape("grassbladeshapesmooth"); if(shape) shape->smooth = *smooth != 0; });
// the points are a line across the blade, seen from both sides, rather than going around it
ICOMMAND(0, grassbladeshapeopen, "i", (int *open), { grassbladeshape *shape = curbladeshape("grassbladeshapeopen"); if(shape) shape->open = *open != 0; });
// a part of the shape, a line or a ring of points as x and y pairs, from -1 to 1 across the blade's width, y towards its front,
// anticlockwise around a closed shape
ICOMMAND(0, grassbladeshapepoints, "s", (char *list),
{
    grassbladeshape *shape = curbladeshape("grassbladeshapepoints");
    if(!shape) return;
    if(shape->parts.length() >= GRASSBLADEPARTS) { conoutf(colourred, "Error: grass blade shape %s has more than %d parts", shape->name, GRASSBLADEPARTS); return; }
    shape->parts.add(shape->points.length());
    vector<char *> vals;
    explodelist(list, vals);
    if(vals.length()%2) conoutf(colourred, "Error: grass blade shape %s has an x without a y", shape->name);
    for(int i = 0; i + 1 < vals.length(); i += 2)
    {
        if(shape->points.length() >= GRASSBLADEPOINTS) { conoutf(colourred, "Error: grass blade shape %s has more than %d points", shape->name, GRASSBLADEPOINTS); break; }
        shape->points.add(vec2(clamp(parsefloat(vals[i]), -1.0f, 1.0f), clamp(parsefloat(vals[i + 1]), -1.0f, 1.0f)));
    }
    vals.deletearrays();
});
ICOMMAND(0, grassbladeshapenum, "", (), intret(bladeshapes.shapes.length()));
ICOMMAND(0, grassbladeshapename, "i", (int *n), result(bladeshapes.shapes.inrange(*n) ? bladeshapes.shapes[*n]->name : ""));

// the lines and rings of a shape, for drawing it
int getgrassbladeshapeparts(int n) { return bladeshapes.shapes.inrange(n) ? bladeshapes.shapes[n]->parts.length() : 0; }

bool getgrassbladeshapepart(int n, int part, const vec2 *&points, int &num, bool &open)
{
    if(!bladeshapes.shapes.inrange(n) || !bladeshapes.shapes[n]->parts.inrange(part)) return false;
    const grassbladeshape &shape = *bladeshapes.shapes[n];
    points = &shape.points[shape.parts[part]];
    num = shape.partlength(part);
    open = shape.open;
    return num > 0;
}

// a map's shape is kept as it is, so a newer list keeps it, and drawn as the first while there's no such shape
void checkgrassbladeshape(int shape)
{
    if(bladeshapes.shapes.inrange(shape) || bladeshapes.warned) return;
    scriptconoutf(colouryellow, "Warning: there is no grass blade shape %d, so the first is drawn instead", shape);
    bladeshapes.warned = true;
}


// the vec4s of the shapes of a texture's heads, which the shader indexes by kind and level
VARR(grassheadlevelvec4s, GRASSHEADS*GRASSHEADLEVELS*GRASSHEADLEVELVEC4S);

// the shapes of the heads blades grow, defined by grassheadshapedef in config/grass.cfg, which maps refer to by their order
enum { GRASSHEADPLACE_TIP = 0, GRASSHEADPLACE_SPIKE, GRASSHEADPLACE_ABOVE, GRASSHEADPLACE_HANG, GRASSHEADPLACE_GROUND, GRASSHEADPLACE_SUN, GRASSHEADPLACE_MAX };
static const char * const grassheadplacenames[GRASSHEADPLACE_MAX] = { "tip", "spike", "above", "hang", "ground", "sun" };
enum { GRASSHEADCOLOUR_HEAD = 0, GRASSHEADCOLOUR_STALK, GRASSHEADCOLOUR_ACCENT, GRASSHEADCOLOUR_MAX };
static const char * const grassheadcolournames[GRASSHEADCOLOUR_MAX] = { "head", "stalk", "accent" };
enum { GRASSHEADLEVEL_CUP = 1<<0 }; // the colour's source follows, from bit 1
enum { GRASSHEADSTALKCOLOUR_GRASS = 0, GRASSHEADSTALKCOLOUR_HEAD, GRASSHEADSTALKCOLOUR_ACCENT, GRASSHEADSTALKCOLOUR_MAX };
static const char * const grassheadstalkcolournames[GRASSHEADSTALKCOLOUR_MAX] = { "grass", "head", "accent" };

struct grassheadshape
{
    struct level
    {
        float height, radius; // along the head's axis and out from it in its size
        int flags, colour;
        float lobebase, lobedepth, lobecount, lobetwist, lobepower, lobealternate; // how far each point of the ring reaches out, for petals, grains, knobs and spikes
        float colourscale, colourblend; // the colour from its source, blended to the accent

        level() : height(0), radius(0), flags(0), colour(GRASSHEADCOLOUR_HEAD), lobebase(1), lobedepth(0), lobecount(0), lobetwist(0), lobepower(0), lobealternate(0), colourscale(1), colourblend(0) {}
    };

    char *name;
    vec4 stalk; // the stalk's height and width against the grass's, its usual stiffness and how much more it leans, 0 height for none
    float taper, size, placeparam, glow; // how far the stalk narrows, the head's size against the grass's height, and how much it glows
    int place, accent; // where the head sits on its stalk, and its usual accent
    int stalkshape, stalkcolour; // the blade shape around its stalk, below 0 for the grass's, and whose colours it takes
    vec4 leaves; // how many leaves its stalk has, their length against the grass's height, their width against their length, and how far they rise
    float leaflow, leafhigh; // where along the stalk its lowest and highest leaves grow
    level levels[GRASSHEADLEVELS];

    grassheadshape() : name(NULL) { reset(); }
    ~grassheadshape() { DELETEA(name); }

    void reset()
    {
        stalk = vec4(1.2f, 0.25f, 0.5f, 0);
        taper = 0.2f;
        size = 0.09f;
        placeparam = glow = 0;
        place = GRASSHEADPLACE_TIP;
        accent = 0xFFFFFF;
        grassbladeshape *round = bladeshapes.find(GRASSHEADSTALKSHAPE);
        stalkshape = round ? bladeshapes.shapes.find(round) : 0;
        stalkcolour = GRASSHEADSTALKCOLOUR_GRASS;
        leaves = vec4(0, 0, 0, 0);
        leaflow = leafhigh = 0;
        loopi(GRASSHEADLEVELS) levels[i] = level();
    }

    // the shape as the shader takes it, three vec4s for each level
    void pack(vec4 *out) const
    {
        loopi(GRASSHEADLEVELS)
        {
            const level &l = levels[i];
            out[i*GRASSHEADLEVELVEC4S] = vec4(l.height, l.radius, 0, l.flags | (l.colour<<1));
            out[i*GRASSHEADLEVELVEC4S + 1] = vec4(l.lobebase, l.lobedepth, l.lobecount, l.lobetwist);
            out[i*GRASSHEADLEVELVEC4S + 2] = vec4(l.lobepower, l.lobealternate, l.colourscale, l.colourblend);
        }
    }
};

struct grassheadshaper
{
    vector<grassheadshape *> shapes;
    grassheadshape *cur; // the shape grassheadshapedef is filling in
    grassheadshape::level *curlevel; // the level grassheadshapelevel is filling in
    bool warned; // a map asked for a shape there isn't, since it last loaded

    grassheadshaper() : cur(NULL), curlevel(NULL), warned(false) {}

    const grassheadshape *get(int n) const { return shapes.inrange(n) ? shapes[n] : (shapes.empty() ? NULL : shapes[0]); }

    grassheadshape *find(const char *name)
    {
        loopv(shapes) if(!strcmp(shapes[i]->name, name)) return shapes[i];
        return NULL;
    }
};
static grassheadshaper headshapes;

// defines a head shape by its name, or replaces the one of that name keeping its place, filled in by the commands in body
ICOMMAND(0, grassheadshapedef, "se", (char *name, uint *body),
{
    if(!name[0]) { conoutf(colourred, "Error: a grass head shape needs a name"); return; }
    if(headshapes.cur) { conoutf(colourred, "Error: grass head shape %s is defined inside another", name); return; }
    grassheadshape *shape = headshapes.find(name);
    if(!shape)
    {
        if(headshapes.shapes.length() >= 256) { conoutf(colourred, "Error: too many grass head shapes"); return; }
        shape = headshapes.shapes.add(new grassheadshape);
        shape->name = newstring(name);
    }
    shape->reset();
    headshapes.cur = shape;
    execute(body);
    headshapes.cur = NULL;
    changedgrass();
});

static grassheadshape *curheadshape(const char *cmd)
{
    if(!headshapes.cur || headshapes.curlevel) conoutf(colourred, "Error: %s is only used in a grassheadshapedef", cmd);
    return headshapes.cur && !headshapes.curlevel ? headshapes.cur : NULL;
}

static grassheadshape::level *curheadlevel(const char *cmd)
{
    if(!headshapes.curlevel) conoutf(colourred, "Error: %s is only used in a grassheadshapelevel", cmd);
    return headshapes.curlevel;
}

static int findgrassheadname(const char *name, const char * const *names, int num, const char *cmd)
{
    loopi(num) if(!strcmp(name, names[i])) return i;
    conoutf(colourred, "Error: %s has no %s", cmd, name);
    return -1;
}

// the stalk's height and width against the grass's, its usual stiffness, how much more it leans and how far it narrows to its top
ICOMMAND(0, grassheadshapestalk, "fffff", (float *height, float *width, float *stiffness, float *droop, float *taper),
{
    grassheadshape *shape = curheadshape("grassheadshapestalk");
    if(!shape) return;
    shape->stalk = vec4(clamp(*height, 0.0f, 4.0f), clamp(*width, 0.0f, 4.0f), clamp(*stiffness, 0.0f, 1.0f), clamp(*droop, 0.0f, 2.0f));
    shape->taper = clamp(*taper, 0.0f, 1.0f);
});
// the blade shape around the stalk by its name, or the grass's for none
ICOMMAND(0, grassheadshapestalkshape, "s", (char *name),
{
    grassheadshape *shape = curheadshape("grassheadshapestalkshape");
    if(!shape) return;
    if(!*name) { shape->stalkshape = -1; return; }
    grassbladeshape *blade = bladeshapes.find(name);
    if(!blade) { conoutf(colourred, "Error: grassheadshapestalkshape has no %s", name); return; }
    shape->stalkshape = bladeshapes.shapes.find(blade);
});
// whose colours the stalk takes: the grass's, from its root to its tip, the head's or the accent's
ICOMMAND(0, grassheadshapestalkcolour, "s", (char *source),
{
    grassheadshape *shape = curheadshape("grassheadshapestalkcolour");
    if(!shape) return;
    int n = findgrassheadname(source, grassheadstalkcolournames, GRASSHEADSTALKCOLOUR_MAX, "grassheadshapestalkcolour");
    if(n >= 0) shape->stalkcolour = n;
});
// the leaves along the stalk: how many, their length against the grass's height, their width against their length,
// where along the stalk the lowest and highest grow, and how far they rise from it before they arch over
ICOMMAND(0, grassheadshapeleaves, "iffffff", (int *count, float *length, float *width, float *low, float *high, float *rise),
{
    grassheadshape *shape = curheadshape("grassheadshapeleaves");
    if(!shape) return;
    shape->leaves = vec4(clamp(*count, 0, GRASSHEADLEAVES), clamp(*length, 0.0f, 4.0f), clamp(*width, 0.0f, 2.0f), clamp(*rise, 0.0f, 2.0f));
    shape->leaflow = clamp(*low, 0.0f, 1.0f);
    shape->leafhigh = clamp(*high, shape->leaflow, 1.0f);
});
// the head's size against the grass's height
ICOMMAND(0, grassheadshapesize, "f", (float *size), { grassheadshape *shape = curheadshape("grassheadshapesize"); if(shape) shape->size = clamp(*size, 0.01f, 1.0f); });
// where the head sits: on the tip of its stalk, around it as a spike at a share of its length, above the tip at a share of its size facing up,
// hanging from the tip, leaning down by a share, growing from the ground, or on the tip turning to the sun by a share
ICOMMAND(0, grassheadshapeplace, "sf", (char *place, float *param),
{
    grassheadshape *shape = curheadshape("grassheadshapeplace");
    if(!shape) return;
    int n = findgrassheadname(place, grassheadplacenames, GRASSHEADPLACE_MAX, "grassheadshapeplace");
    if(n < 0) return;
    shape->place = n;
    shape->placeparam = clamp(*param, 0.0f, 2.0f);
});
// the accent the head's secondary part takes where the texture doesn't set one
ICOMMAND(0, grassheadshapeaccent, "i", (int *colour), { grassheadshape *shape = curheadshape("grassheadshapeaccent"); if(shape) shape->accent = *colour&0xFFFFFF; });
ICOMMAND(0, grassheadshapeglow, "f", (float *glow), { grassheadshape *shape = curheadshape("grassheadshapeglow"); if(shape) shape->glow = clamp(*glow, 0.0f, 4.0f); });
// a level of the head's outline, 0 for the pole below to 5 for the pole above, its height along the head's axis and its radius, in the head's size
ICOMMAND(0, grassheadshapelevel, "iffe", (int *n, float *height, float *radius, uint *body),
{
    grassheadshape *shape = curheadshape("grassheadshapelevel");
    if(!shape) return;
    if(*n < 0 || *n >= GRASSHEADLEVELS) { conoutf(colourred, "Error: grass head shape %s has no level %d", shape->name, *n); return; }
    grassheadshape::level &l = shape->levels[*n];
    l = grassheadshape::level();
    l.height = clamp(*height, -4.0f, 4.0f);
    l.radius = clamp(*radius, 0.0f, 4.0f);
    headshapes.curlevel = &l;
    execute(body);
    headshapes.curlevel = NULL;
});
// the points of the ring reach out by base plus depth times the cosine of count times the way around, turned by twist for each level,
// unsigned and raised to power where it is above 0, and every second point reaches alternate instead where it is above 0
ICOMMAND(0, grassheadshapelevellobe, "ffffff", (float *base, float *depth, float *count, float *twist, float *power, float *alternate),
{
    grassheadshape::level *l = curheadlevel("grassheadshapelevellobe");
    if(!l) return;
    l->lobebase = clamp(*base, 0.0f, 2.0f);
    l->lobedepth = clamp(*depth, -2.0f, 2.0f);
    l->lobecount = clamp(*count, 0.0f, 16.0f);
    l->lobetwist = *twist;
    l->lobepower = clamp(*power, 0.0f, 4.0f);
    l->lobealternate = clamp(*alternate, 0.0f, 2.0f);
});
// the petals of this level cup up to their tips, as far as they reach out
ICOMMAND(0, grassheadshapelevelcup, "i", (int *cup),
{
    grassheadshape::level *l = curheadlevel("grassheadshapelevelcup");
    if(l) l->flags = (l->flags&~GRASSHEADLEVEL_CUP) | (*cup ? GRASSHEADLEVEL_CUP : 0);
});
// the level's colour, from the head, the stalk or the accent, times scale and blended to the accent by blend
ICOMMAND(0, grassheadshapelevelcolour, "sff", (char *source, float *scale, float *blend),
{
    grassheadshape::level *l = curheadlevel("grassheadshapelevelcolour");
    if(!l) return;
    int n = findgrassheadname(source, grassheadcolournames, GRASSHEADCOLOUR_MAX, "grassheadshapelevelcolour");
    if(n < 0) return;
    l->colour = n;
    l->colourscale = clamp(*scale, 0.0f, 4.0f);
    l->colourblend = clamp(*blend, 0.0f, 1.0f);
});
ICOMMAND(0, grassheadshapenum, "", (), intret(headshapes.shapes.length()));
ICOMMAND(0, grassheadshapename, "i", (int *n), result(headshapes.shapes.inrange(*n) ? headshapes.shapes[*n]->name : ""));

// a map's head shape is kept as it is, so a newer list keeps it, and drawn as the first while there's no such shape
void checkgrassheadshape(int shape)
{
    if(headshapes.shapes.inrange(shape) || headshapes.warned) return;
    scriptconoutf(colouryellow, "Warning: there is no grass head shape %d, so the first is drawn instead", shape);
    headshapes.warned = true;
}

void resetgrassshapewarnings() { bladeshapes.warned = headshapes.warned = false; }

// every shape's rows of points share one vertex buffer, each entry its distance up the blade and the outline entry it takes, with index lists for each
static void setupbladeshapes()
{
    if(bladeshapes.shapes.empty())
    { // without config/grass.cfg, grass is flat
        grassbladeshape *shape = bladeshapes.shapes.add(new grassbladeshape);
        shape->name = newstring("Flat");
        shape->open = true;
        shape->parts.add(0);
        shape->points.add(vec2(-1, 0));
        shape->points.add(vec2(1, 0));
    }
    static const float rows[GRASSBLADEROWS] = { 0, 1/3.0f, 2/3.0f, 1 };
    static const int lodrows[] = { 0, 2, 3 }; // far blades drop their lowest bend
    vector<vec2> verts;
    loopi(GRASSBLADEROWS) loopj(grassbladeoutline) verts.add(vec2(rows[i], j));
    vector<ushort> indices;
    loopv(bladeshapes.shapes)
    {
        grassbladeshape &shape = *bladeshapes.shapes[i];
        shape.build();
        loopl(GRASSBLADELODS) loopk(2)
        {
            int first = indices.length(), step = shape.lodstep(l);
            int numrows = k ? int(sizeof(lodrows)/sizeof(lodrows[0])) : GRASSBLADEROWS;
            for(int r = 0; r + 1 < numrows; r++)
            {
                int lo = (k ? lodrows[r] : r)*grassbladeoutline, hi = (k ? lodrows[r + 1] : r + 1)*grassbladeoutline;
                for(int j = 0; j < shape.segments.length(); j += step)
                {
                    int a = shape.segments[j].x, b = shape.segments[j + step - 1].y;
                    indices.add(lo + a); indices.add(lo + b); indices.add(hi + b);
                    indices.add(lo + a); indices.add(hi + b); indices.add(hi + a);
                }
            }
            int top = (GRASSBLADEROWS - 1)*grassbladeoutline;
            for(int j = 0; j < shape.caps.length(); j += step)
            {
                indices.add(top + shape.caps[j].z);
                indices.add(top + shape.caps[j].x);
                indices.add(top + shape.caps[j + step - 1].y);
            }
            int verts = numrows*shape.segments.length()*2/step + shape.caps.length()/step + shape.parts.length();
            shape.firstindex[l][k] = first;
            shape.numindices[l][k] = indices.length() - first;
            shape.numverts[l][k] = verts;
        }
    }
    glGenBuffers_(1, &bladevbo);
    gle::bindvbo(bladevbo);
    glBufferData_(GL_ARRAY_BUFFER, verts.length()*sizeof(vec2), verts.getbuf(), GL_STATIC_DRAW);
    gle::clearvbo();
    glGenBuffers_(1, &bladeebo);
    gle::bindebo(bladeebo);
    glBufferData_(GL_ELEMENT_ARRAY_BUFFER, indices.length()*sizeof(ushort), indices.getbuf(), GL_STATIC_DRAW);
    gle::clearebo();
}

static void setupblades()
{
    if(bladevbo) return;
    setupbladeshapes();
    // heads: x is the way around the rings, y the level, 0 for the pole below, 1 to GRASSHEADRINGS for the rings up the head, then the pole above,
    // then the leaves on the stalk, with x the point of each leaf's outline and y past the pole above for each leaf
    float headverts[GRASSHEADVERTS + GRASSHEADLEAVES*GRASSHEADLEAFVERTS][2];
    ushort headindices[(GRASSHEADINDICES + GRASSHEADINDICES/2 + GRASSHEADINDICES/4) + 3*GRASSHEADLEAVES*GRASSHEADLEAFINDICES], *idx = headindices;
    headverts[0][0] = 0; headverts[0][1] = 0;
    headverts[GRASSHEADVERTS-1][0] = 0; headverts[GRASSHEADVERTS-1][1] = GRASSHEADRINGS + 1;
    loopi(GRASSHEADRING) loopj(GRASSHEADRINGS)
    {
        headverts[1 + j*GRASSHEADRING + i][0] = i/float(GRASSHEADRING);
        headverts[1 + j*GRASSHEADRING + i][1] = j + 1;
    }
    for(int lod = 1; lod <= 4; lod *= 2) for(int i = 0; i < GRASSHEADRING; i += lod)
    {
        int a = 1 + i, b = 1 + (i + lod)%GRASSHEADRING, top = (GRASSHEADRINGS - 1)*GRASSHEADRING;
        *idx++ = 0; *idx++ = b; *idx++ = a;
        loopj(GRASSHEADRINGS - 1)
        {
            int c = a + j*GRASSHEADRING, d = b + j*GRASSHEADRING;
            *idx++ = c; *idx++ = d; *idx++ = d + GRASSHEADRING;
            *idx++ = c; *idx++ = d + GRASSHEADRING; *idx++ = c + GRASSHEADRING;
        }
        *idx++ = a + top; *idx++ = b + top; *idx++ = GRASSHEADVERTS-1;
        // the leaves follow each detail's head, so a draw takes as many as it needs with it
        if(i + lod >= GRASSHEADRING) loopj(GRASSHEADLEAVES)
        {
            static const ushort leaftris[GRASSHEADLEAFINDICES] = { 0, 1, 2, 1, 4, 2, 1, 3, 4, 3, 5, 4 };
            loopk(GRASSHEADLEAFINDICES) *idx++ = GRASSHEADVERTS + j*GRASSHEADLEAFVERTS + leaftris[k];
        }
    }
    loopi(GRASSHEADLEAVES) loopj(GRASSHEADLEAFVERTS)
    {
        headverts[GRASSHEADVERTS + i*GRASSHEADLEAFVERTS + j][0] = j;
        headverts[GRASSHEADVERTS + i*GRASSHEADLEAFVERTS + j][1] = GRASSHEADRINGS + 2 + i;
    }
    glGenBuffers_(1, &headvbo);
    gle::bindvbo(headvbo);
    glBufferData_(GL_ARRAY_BUFFER, sizeof(headverts), headverts, GL_STATIC_DRAW);
    gle::clearvbo();
    glGenBuffers_(1, &headebo);
    gle::bindebo(headebo);
    glBufferData_(GL_ELEMENT_ARRAY_BUFFER, sizeof(headindices), headindices, GL_STATIC_DRAW);
    gle::clearebo();
    GLint align = 0;
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &align);
    grassuboalign = max(int(align), int(sizeof(vec4)));
}

VARR(grasstramplesize, 256); // texels across the trample map, which follows the camera, a power of two
VARR(grasstramplescale, 2); // world units per texel
VARR(grassscorchsize, 512); // texels across the scorch map, reaching as far as grass usually does
VARR(grassscorchscale, 4); // world units per texel
#define GRASSMAPMAXAGE 60000 // older stamps are cleared, before their 16 bit time wraps
#define GRASSMAPREACH 256 // the furthest a shot marks the maps, as each cell it reaches looks for the ground and weapon radii have no limit
#define GRASSMAPSWEEP 16 // a sixteenth of the rows is checked for old stamps each frame, covering the map within the 5.5 seconds before a time wraps down to 3 fps

VAR(IDF_PERSIST, grasstrample, 0, 1, 1);

// a window of stamps on the ground which follows the camera: each texel holds two byte values of the stamp's own, when it was made,
// in milliseconds that wrap every 65.5 seconds, and the lowest and highest ground it applies to, so grass above or below it is left alone
struct grassmap
{
    GLuint tex;
    ushort *texels;
    bool *dirtyrows;
    ivec2 origin; // the world texel at the window's lowest corner
    int size, scale, sweep; // texels across, a power of two, and world units per texel
    uchar empty; // what both values hold where there's no stamp
    bool valid;

    grassmap(int size, int scale, uchar empty) : tex(0), texels(NULL), dirtyrows(NULL), origin(0, 0), size(size), scale(scale), sweep(0), empty(empty), valid(false) {}

    static int now() { return lastmillis&0xFFFF; }

    // the cells of the window within reach of a position, if any
    bool cells(const vec &o, float radius, ivec2 &low, ivec2 &high) const
    {
        float reach = min(radius, float(GRASSMAPREACH));
        low = ivec2(max(int(floorf((o.x - reach)/scale)), origin.x), max(int(floorf((o.y - reach)/scale)), origin.y));
        high = ivec2(min(int(floorf((o.x + reach)/scale)), origin.x + size - 1), min(int(floorf((o.y + reach)/scale)), origin.y + size - 1));
        return low.x <= high.x && low.y <= high.y;
    }

    ushort *texel(int x, int y) { return &texels[((y&(size-1))*size + (x&(size-1)))*4]; }

    void clear(ushort *t) { t[0] = (empty<<8) | empty; t[1] = t[2] = t[3] = 0; }

    void cleanup()
    {
        if(tex) { glDeleteTextures(1, &tex); tex = 0; }
        valid = false;
    }

    void upload(int x, int y, int w, int h)
    {
        glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, size);
        glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RGBA, GL_UNSIGNED_SHORT, &texels[(y*size + x)*4]);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    }

    // follows the camera, clearing the rows and columns that wrap around to the far side
    void move(const vec &pos)
    {
        if(!texels)
        {
            texels = new ushort[size*size*4];
            dirtyrows = new bool[size];
            memset(dirtyrows, 0, size);
        }
        ivec2 neworigin(int(floorf(pos.x/scale)) - size/2, int(floorf(pos.y/scale)) - size/2);
        if(!tex)
        {
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            setuptexparameters(tex, NULL, 0, 0, GL_RGBA16, GL_TEXTURE_2D);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            valid = false;
        }
        else glBindTexture(GL_TEXTURE_2D, tex);
        if(!valid || abs(neworigin.x - origin.x) >= size || abs(neworigin.y - origin.y) >= size)
        {
            loopi(size*size) clear(&texels[i*4]);
            if(valid) upload(0, 0, size, size);
            else glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16, size, size, 0, GL_RGBA, GL_UNSIGNED_SHORT, texels);
            origin = neworigin;
            valid = true;
            return;
        }
        for(int x = min(origin.x, neworigin.x); x < max(origin.x, neworigin.x); x++)
        {
            int col = x&(size-1);
            loopi(size) clear(&texels[(i*size + col)*4]);
            upload(col, 0, 1, size);
        }
        for(int y = min(origin.y, neworigin.y); y < max(origin.y, neworigin.y); y++)
        {
            int row = y&(size-1);
            loopi(size) clear(&texels[(row*size + i)*4]);
            dirtyrows[row] = true;
        }
        origin = neworigin;
    }

    // clears stamps old enough that their time could wrap around
    void sweepold()
    {
        int tick = now();
        loopk(size/GRASSMAPSWEEP)
        {
            int row = sweep;
            sweep = (sweep + 1)%size;
            loopi(size)
            {
                ushort *t = &texels[(row*size + i)*4];
                if(t[0] == ((empty<<8) | empty)) continue;
                int age = (tick - t[1])&0xFFFF;
                if(age < GRASSMAPMAXAGE) continue;
                clear(t);
                dirtyrows[row] = true;
            }
        }
    }

    void flush()
    {
        loopi(size) if(dirtyrows[i])
        {
            int first = i;
            while(i + 1 < size && dirtyrows[i + 1]) i++;
            upload(0, first, size, i + 1 - first);
            memset(&dirtyrows[first], 0, i + 1 - first);
        }
    }
};

// where players walk through grass: each stamp holds the direction blades are pushed
struct grasstrampler : grassmap
{
    grasstrampler() : grassmap(grasstramplesize, grasstramplescale, 128) {}

    // pushes the blades of one texel away from a point, or towards it, where their ground is within these heights
    void push(int x, int y, const vec &o, float radius, bool pull, float zlow, float zhigh)
    {
        if(x < origin.x || y < origin.y || x >= origin.x + size || y >= origin.y + size) return;
        vec2 off((x + 0.5f)*scale - o.x, (y + 0.5f)*scale - o.y);
        float dist = off.magnitude();
        if(dist > radius) return;
        float edge = dist/radius, amt = 1 - edge*edge;
        if(dist > 0.01f) off.mul((pull ? -amt : amt)/dist);
        else off = vec2(0, 0);
        ushort *t = texel(x, y);
        t[0] = (clamp(int(128 + off.x*127), 1, 255)<<8) | clamp(int(128 + off.y*127), 1, 255);
        t[1] = now();
        t[2] = clamp(int(floorf(zlow)), 0, 0xFFFF);
        t[3] = clamp(int(ceilf(zhigh)), 0, 0xFFFF);
        dirtyrows[y&(size-1)] = true;
    }

    // pushes the blades around a player's feet away from them, on ground as steep as the stamp is wide
    void stamp(const vec &feet, float radius)
    {
        int x1 = int(floorf((feet.x - radius)/scale)), y1 = int(floorf((feet.y - radius)/scale)),
            x2 = int(floorf((feet.x + radius)/scale)), y2 = int(floorf((feet.y + radius)/scale));
        for(int y = y1; y <= y2; y++) for(int x = x1; x <= x2; x++) push(x, y, feet, radius, false, feet.z - radius, feet.z + radius);
    }

    void update()
    {
        move(camera1->o);
        loopi(game::numdynents())
        {
            dynent *d = game::iterdynents(i);
            if(!d || d->state != CS_ALIVE || d->physstate < PHYS_SLOPE) continue;
            vec feet = d->feetpos();
            if(feet.dist(camera1->o) > size*scale/2 + d->radius) continue;
            stamp(feet, d->radius*1.5f + 1);
        }
        sweepold();
        flush();
    }
};
static grasstrampler trampler;

// each pulse's colours, for blades and heads to go through in their own order: a row of a texture for each pulse
static GLuint grasspulsetex = 0;

static void bindgrasspulses()
{
    if(!grasspulsetex)
    {
        static uchar colours[PULSE_MAX][PULSE_COLS][4];
        loopi(PULSE_MAX) loopj(PULSE_COLS)
        {
            int c = pulsecols[i][j];
            colours[i][j][0] = c>>16; colours[i][j][1] = (c>>8)&0xFF; colours[i][j][2] = c&0xFF; colours[i][j][3] = 0xFF;
        }
        glGenTextures(1, &grasspulsetex);
        glBindTexture(GL_TEXTURE_2D, grasspulsetex);
        setuptexparameters(grasspulsetex, NULL, 0, 0, GL_RGBA8, GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, PULSE_COLS, PULSE_MAX, 0, GL_RGBA, GL_UNSIGNED_BYTE, colours);
    }
    else glBindTexture(GL_TEXTURE_2D, grasspulsetex);
}

// a colour for the shader, where a pulse stays negative for each piece to read in its own order
static vec grasspiececolour(int colour)
{
    return colour < 0 ? vec(float(colour), 0, 0) : vec::fromcolor(colour);
}

// where shots have marked the grass: each stamp holds the element and whether it has already flashed
struct grassscorcher : grassmap
{
    grassscorcher() : grassmap(grassscorchsize, grassscorchscale, 0) {}

    // whether a texel has only just caught from this element, which a repeat leaves alone
    bool catching(int x, int y, int element)
    {
        ushort *t = texel(x, y);
        return t[0]>>8 == element && !(t[0]&0xFF) && ((now() - t[1])&0xFFFF) < GRASSSCORCHFLASH;
    }

    // marks one texel, where a repeat of the same element only extends the time, and a different one replaces it
    void mark(int x, int y, int element, int lasts, float zlow, float zhigh)
    {
        if(x < origin.x || y < origin.y || x >= origin.x + size || y >= origin.y + size || catching(x, y, element)) return;
        ushort *t = texel(x, y);
        int flashed = t[0]>>8 == element && ((now() - t[1])&0xFFFF) < lasts ? 1 : 0;
        t[0] = (element<<8) | flashed;
        t[1] = now();
        t[2] = clamp(int(floorf(zlow)), 0, 0xFFFF);
        t[3] = clamp(int(ceilf(zhigh)), 0, 0xFFFF);
        dirtyrows[y&(size-1)] = true;
    }

    void update()
    {
        move(camera1->o);
        sweepold();
        flush();
    }
};
static grassscorcher scorcher;

// a batch of patches with the same grass in one cell, drawn in one instanced draw when its box is in view
struct grasschunk
{
    int blend, texture, cell, offset, numpatches, numblades, shadow;
    int headoffset, headcounts[GRASSHEADTIERS]; // the blades with heads, those every sixteenth blade first, and how many there are up to each tier
    float density, height, dist, water, headshares[GRASSHEADS]; // water is the lowest over its patches
    vec center, bbmin, bbmax;
    ivec boxmin, boxsize; // around bbmin and bbmax in whole units, for culling
    occludequery *query; // whether last frame's view could see the box
};

// the patches of every vertex array, packed into one uniform buffer, rebuilt when the world or a texture's density changes
struct grassbatches
{
    vector<grasschunk> chunks;
    GLuint buf;
    bool dirty;

    grassbatches() : buf(0), dirty(true) {}

    void cleanup()
    {
        if(buf) { glDeleteBuffers_(1, &buf); buf = 0; }
        chunks.setsize(0);
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
    scorcher.cleanup();
    if(grasspulsetex) { glDeleteTextures(1, &grasspulsetex); grasspulsetex = 0; }
    if(bladevbo) { glDeleteBuffers_(1, &bladevbo); bladevbo = 0; }
    if(bladeebo) { glDeleteBuffers_(1, &bladeebo); bladeebo = 0; }
    if(headvbo) { glDeleteBuffers_(1, &headvbo); headvbo = 0; }
    if(headebo) { glDeleteBuffers_(1, &headebo); headebo = 0; }
    batches.cleanup();
}

// editors see the grass as the mapper set it, at full density
static float grassdensityshare()
{
    return editmode ? 1.0f : grassdensityscale;
}

static float grasspatchdensity(const VSlot &s)
{
    float density = s.grassdensity > 0 ? s.grassdensity : GRASSDENSITY, blend = s.grassblend > 0 ? s.grassblend : 1.0f;
    return density*blend*grassdensityshare();
}

static float grasspatcharea(const grasstri &g, int first)
{
    return vec().cross(vec(g.v[first+1]).sub(g.v[0]), vec(g.v[first+2]).sub(g.v[0])).magnitude()/2;
}

static int grassscorchhold(const VSlot &s) { return s.grassscorchhold > 0 ? s.grassscorchhold : GRASSSCORCHHOLD; }
static int grassscorchregrow(const VSlot &s) { return s.grassscorchregrow > 0 ? s.grassscorchregrow : GRASSSCORCHREGROW; }

static float grasspatchheight(const VSlot &s)
{
    return s.grassheight > 0 ? s.grassheight : (s.grassfrondribbons > 0 ? GRASSFRONDHEIGHT : GRASSHEIGHT);
}

#define GRASSMOTTLEHEIGHT 1.25f // the lushest patches raise blades by a quarter

static inline bool grassheads(const VSlot &s)
{
    if(headshapes.shapes.empty()) return false;
    loopi(GRASSHEADS) if(s.grassheadcolours[i]) return true;
    return false;
}

// the biggest a texture's heads grow against their usual size
static float grassheadbiggest(const VSlot &s)
{
    float biggest = 0;
    loopi(GRASSHEADS) if(s.grassheadcolours[i]) biggest = max(biggest, s.grassheadsizes[i]*(1 + s.grassheadvariances[i]));
    return biggest;
}

// the widest a texture's heads grow against the grass's height, from their shapes' sizes
static float grassheadwidest(const VSlot &s)
{
    float widest = 0;
    loopi(GRASSHEADS) if(s.grassheadcolours[i])
    {
        const grassheadshape *shape = headshapes.get(s.grassheadshapes[i]);
        if(shape) widest = max(widest, 2*shape->size*s.grassheadsizes[i]*(1 + s.grassheadvariances[i]));
    }
    return widest;
}

// the tallest a texture's head stalks grow against the grass's height, and no less than the grass
static float grassheadtallest(const VSlot &s)
{
    float tallest = 1;
    loopi(GRASSHEADS) if(s.grassheadcolours[i])
    {
        const grassheadshape *shape = headshapes.get(s.grassheadshapes[i]);
        if(shape) tallest = max(tallest, shape->stalk.x*(s.grassheadstalkheights[i] > 0 ? s.grassheadstalkheights[i] : 1.0f));
    }
    return tallest;
}

static inline float grassheadshare(const VSlot &s, int i)
{
    return s.grassheadcolours[i] ? (s.grassheadshares[i] > 0 ? s.grassheadshares[i] : GRASSHEADSHARE) : 0.0f;
}

#define GRASSHEADSTEPMAX 16 // the sparsest heads grow on every sixteenth blade, the most thinning ever keeps

// heads grow on as few of the blades as hold their shares, even where clumps double them, so fewer are looked at for them: every sixteenth blade,
// eighth, fourth, second or every blade
// the most of the blades a texture's heads may grow on, of those that can, as clumps double the share in places, matching the shader
static float grassheadreach(const VSlot &s, int step)
{
    float reach = 0;
    loopi(GRASSHEADS)
    {
        float share = grassheadshare(s, i)*step;
        if(share > 1) return 1e16f; // more than the blades that can
        reach += share*(2 - share);
    }
    return reach;
}

static int grassheadstep(const VSlot &s)
{
    if(!grassheads(s)) return 0;
    int step = GRASSHEADSTEPMAX;
    while(step > 1 && grassheadreach(s, step) > 1) step /= 2;
    return step;
}

// each of a frond's ribbons is as wide as a blade would be, or narrower the more it has, so they cover about as much
static float grassfrondwidth(const VSlot &s)
{
    return s.grasswidth > 0 ? s.grasswidth : GRASSFRONDWIDTH/sqrtf(s.grassfrondribbons);
}

static inline float grassfrondswing(const VSlot &s) { return s.grassfrondswing >= 0 ? s.grassfrondswing : GRASSFRONDSWING; }
static inline float grassfrondruffle(const VSlot &s) { return s.grassfrondruffle >= 0 ? s.grassfrondruffle : GRASSFRONDRUFFLE; }

// the width of the blade a frond is cut from, in ribbon widths: its ribbons, as wide as they ruffle, a ribbon and a half apart at their tips, and their swing to each side
static inline float grassfrondspan(const VSlot &s)
{
    return 2*grassfrondswing(s) + 1 + grassfrondruffle(s) + 1.5f*(s.grassfrondribbons - 1);
}

// a blade's width, which holds all of a frond's ribbons
static float grassbladewidth(const VSlot &s)
{
    return s.grassfrondribbons > 0 ? grassfrondwidth(s)*grassfrondspan(s) : (s.grasswidth > 0 ? s.grasswidth : GRASSWIDTH);
}

// how far the tallest blades reach from their roots
static float grasspatchreach(const VSlot &s)
{
    return grasspatchheight(s)*GRASSMOTTLEHEIGHT*(grassheads(s) ? grassheadtallest(s) + 0.2f*grassheadbiggest(s) : 1.0f) + (s.grassfrondribbons > 0 ? grassbladewidth(s)/2 : 0.0f);
}

static bool grassbatchesstale()
{
    if(batches.dirty) return true;
    loopv(batches.chunks)
    {
        grasschunk &c = batches.chunks[i];
        const VSlot &s = lookupvslot(c.texture, false);
        if(c.density != grasspatchdensity(s) || c.height != grasspatchreach(s)) return true;
        loopk(GRASSHEADS) if(c.headshares[k] != grassheadshare(s, k)) return true;
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
    // by where the vertex arrays are rather than where they were allocated, so the blades are laid out the same each time the map loads,
    // then by their place in the array's patches
    if(a.va != b.va)
    {
        if(a.va->o.z != b.va->o.z) return a.va->o.z < b.va->o.z;
        if(a.va->o.y != b.va->o.y) return a.va->o.y < b.va->o.y;
        if(a.va->o.x != b.va->o.x) return a.va->o.x < b.va->o.x;
        return a.va->size < b.va->size;
    }
    return a.tri < b.tri;
}

// the shader's hashes, which the head lists match
static inline uint grasshash(uint x)
{
    x ^= x >> 16; x *= 0x7feb352du;
    x ^= x >> 15; x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}
static inline float grassrnd(uint x) { return float(grasshash(x)&0xFFFFFFu)*(1.0f/16777216.0f); }
static inline uint grasscell(const vec4 &p) { return grasshash(uint(int(floorf(p.x*8))) ^ grasshash(uint(int(floorf(p.y*8))) ^ grasshash(uint(int(floorf(p.z*8)))))); }

// which kind of head a blade of a patch grows, below 0 for none, as the shader finds it from the hashes of the patch's first and third corners' cells
static int grassheadslot(const vec4 *p, uint cell0, uint cell2, int blade, const VSlot &s, int headstep, float headreach)
{
    uint h = grasshash(cell0 ^ grasshash(cell2 + uint(blade - int(p[0].w))*0x9e3779b9u));
    float r = grassrnd(h + 17);
    if(r >= headreach) return -1;
    vec a(p[0]), b(p[1]), c(p[2]);
    if(grassrnd(h + 1) >= p[1].w) { b = vec(p[2]); c = vec(p[3]); }
    float r1 = grassrnd(h + 2), r2 = grassrnd(h + 3);
    if(r1 + r2 > 1) { r1 = 1 - r1; r2 = 1 - r2; }
    vec root = vec(a).add(vec(b).sub(a).mul(r1)).add(vec(c).sub(a).mul(r2));
    float scale = 1.0f/GRASSCLUMPSIZE, cpx = root.x*scale, cpy = root.y*scale, cellx = floorf(cpx), celly = floorf(cpy), best = 1e9f;
    uint clump = 0;
    for(int y = -1; y <= 1; y++) for(int x = -1; x <= 1; x++)
    {
        float ccx = cellx + x, ccy = celly + y;
        uint ch = grasshash((uint(int(ccx))*73856093u) ^ (uint(int(ccy))*19349663u));
        float dx = ccx + grassrnd(ch) - cpx, dy = ccy + grassrnd(ch + 1) - cpy, dist = dx*dx + dy*dy;
        if(dist < best) { best = dist; clump = ch; }
    }
    float reach = 0;
    loopi(GRASSHEADS)
    {
        float share = grassheadshare(s, i)*headstep, t = min(share, 1.0f);
        reach += share*(2*grassrnd(clump + 18 + i)*(1 - t) + t);
        if(r < reach) return i;
    }
    return -1;
}

static inline int grassheadtier(int blade) { return blade%16 ? (blade%8 ? (blade%4 ? (blade%2 ? 4 : 3) : 2) : 1) : 0; }

// a chunk's head list, by a hash of its patches and heads, so a rebuild only works out those of the chunks that changed
struct grassheadmemo
{
    ullong key;
    int first, counts[GRASSHEADTIERS];
};

struct grassheadlists
{
    vector<uint> list, oldlist;
    vector<grassheadmemo> memos, oldmemos;
    hashtable<uint, int> oldkeys;
    vector<uint> tiers[GRASSHEADTIERS];

    // the last build's lists, for this one to take those of the chunks that are the same
    void begin()
    {
        oldlist.setsize(0);
        oldlist.move(list);
        oldmemos.setsize(0);
        oldmemos.move(memos);
        oldkeys.clear();
        loopv(oldmemos) oldkeys[uint(oldmemos[i].key)] = i;
    }

    // the blades of a chunk's patches that grow heads, those every sixteenth blade first, then every eighth, fourth and second, then the rest
    void build(grasschunk &c, const vec4 *patches)
    {
        while((list.length()*sizeof(uint))%grassuboalign) list.add(0);
        c.headoffset = list.length()*sizeof(uint);
        memset(c.headcounts, 0, sizeof(c.headcounts));
        const VSlot &s = lookupvslot(c.texture, false);
        int headstep = grassheadstep(s);
        if(!headstep) return;
        float headreach = min(grassheadreach(s, headstep), 1.0f) + 1e-3f; // a little over, so rounding never loses a head
        ullong key = 14695981039346656037ULL;
        #define HEADKEY(v) { uint k; memcpy(&k, &(v), sizeof(k)); key = (key^k)*1099511628211ULL; }
        loopi(c.numpatches*4) { HEADKEY(patches[i].x); HEADKEY(patches[i].y); HEADKEY(patches[i].z); HEADKEY(patches[i].w); }
        HEADKEY(c.numblades); HEADKEY(headstep); HEADKEY(headreach);
        loopi(GRASSHEADS) HEADKEY(c.headshares[i]);
        #undef HEADKEY
        grassheadmemo &memo = memos.add();
        memo.key = key;
        memo.first = list.length();
        int *old = oldkeys.access(uint(key));
        if(old && oldmemos[*old].key == key)
        {
            const grassheadmemo &o = oldmemos[*old];
            list.put(&oldlist[o.first], o.counts[GRASSHEADTIERS - 1]);
            memcpy(c.headcounts, o.counts, sizeof(c.headcounts));
        }
        else
        {
            loopi(c.numpatches)
            {
                // heads grow on every headstep-th blade of the chunk
                const vec4 *p = &patches[i*4];
                int first = int(p[0].w), last = i + 1 < c.numpatches ? int(p[4].w) : c.numblades;
                uint cell0 = grasscell(p[0]), cell2 = grasscell(p[2]);
                for(int blade = (first + headstep - 1)/headstep*headstep; blade < last; blade += headstep)
                    if(grassheadslot(p, cell0, cell2, blade, s, headstep, headreach) >= 0) tiers[grassheadtier(blade)].add(blade);
            }
            loopk(GRASSHEADTIERS)
            {
                list.put(tiers[k].getbuf(), tiers[k].length());
                c.headcounts[k] = list.length() - memo.first;
                tiers[k].setsize(0);
            }
        }
        memcpy(memo.counts, c.headcounts, sizeof(memo.counts));
    }
};
static grassheadlists headlists;

// the height of the water over a patch, from just inside its lowest point, or far below the world where it's dry
static float grasspatchwater(const grasstri &g)
{
    vec low = g.v[0];
    for(int i = 1; i < g.numv; i++) if(g.v[i].z < low.z) low = g.v[i];
    low.add(vec(g.center).sub(low).mul(0.1f)).addz(1);
    int mat = lookupmaterial(low), abovemat;
    return (mat&MATF_VOLUME) == MAT_WATER ? findsurface(mat, low, abovemat) - VOLUME_OFFSET : -1e6f;
}

// packs each patch as four vec4s: the corners, with the first blade, the share of the first triangle, the blend orient and the height of the water
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
    headlists.begin();
    grasschunk *c = NULL;
    vec cmin(0, 0, 0), cmax(0, 0, 0);
    // blades reach up to their height from their roots, leaning, swaying or trampled to any side but never down, and the chunk's head list follows the others'
    #define ENDCHUNK if(c) { \
        headlists.build(*c, &data[c->offset/sizeof(vec4)]); \
        cmin.x -= c->height; cmin.y -= c->height; cmax.add(c->height); c->bbmin = cmin; c->bbmax = cmax; c->center = vec(cmin).add(cmax).mul(0.5f); \
        c->boxmin = ivec(int(floorf(cmin.x)), int(floorf(cmin.y)), int(floorf(cmin.z))); \
            c->boxsize = ivec(int(ceilf(cmax.x)), int(ceilf(cmax.y)), int(ceilf(cmax.z))).sub(c->boxmin); }
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
            c->query = NULL;
            c->density = density;
            c->water = 1e16f;
            c->height = grasspatchreach(s);
            loopk(GRASSHEADS) c->headshares[k] = grassheadshare(s, k);
        }
        data.add(vec4(g.v[0], c->numblades));
        data.add(vec4(g.v[1], area2 > 0 ? area1/(area1 + area2) : 1));
        data.add(vec4(g.v[2], g.orient));
        float water = grasspatchwater(g);
        c->water = min(c->water, water);
        data.add(vec4(g.v[3], water));
        c->numpatches++;
        c->numblades += blades;
        loopk(g.numv) { cmin.min(g.v[k]); cmax.max(g.v[k]); }
    }
    ENDCHUNK;
    #undef ENDCHUNK
    // every chunk binds a full block, so the buffer must reach past the last one, and so must each draw from a head list
    if(c) data.pad(c->offset/sizeof(vec4) + GRASSCHUNKPATCHES*4 - data.length());
    if(data.empty()) return;
    while((data.length()*sizeof(vec4))%grassuboalign) data.add(vec4(0, 0, 0, 0));
    int headbase = data.length()*sizeof(vec4);
    loopv(batches.chunks) batches.chunks[i].headoffset += headbase;
    vector<uint> &list = headlists.list;
    int numheads = list.length();
    list.pad(GRASSHEADLISTMAX + 3);
    memcpy((void *)data.pad((numheads + GRASSHEADLISTMAX + 3)/4), list.getbuf(), (numheads + GRASSHEADLISTMAX + 3)/4*sizeof(vec4));
    list.setsize(numheads);

    glGenBuffers_(1, &batches.buf);
    glBindBuffer_(GL_UNIFORM_BUFFER, batches.buf);
    glBufferData_(GL_UNIFORM_BUFFER, data.length()*sizeof(vec4), data.getbuf(), GL_STATIC_DRAW);
    glBindBuffer_(GL_UNIFORM_BUFFER, 0);
}

// the height on screen where blades are full, above where they are thinnest
static inline float grasspixelsfullat() { return max(grasspixelsfull, grasspixelsthin + 1); }

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
    return min(keep, grassthinmin + (1 - grassthinmin)*grasssmoothstep(grasspixelsthin, grasspixelsfullat(), pixels));
}

static inline float grasspixelsize() { return 2*tanf(fovy/2*RAD)/max(viewh, 1); }

// the share of a blade's width a frond's ribbons cover, which a plain blade of that width stands in for
static inline float grassfrondcover(const VSlot &s) { return s.grassfrondribbons/grassfrondspan(s); }

// whether a chunk's fronds are near enough for their curves to show
static inline bool grassfrondnear(const VSlot &s, float width, float dist)
{
    return s.grassfrondribbons > 0 && width/grassfrondspan(s)/max(dist*grasspixelsize(), 1e-6f) >= grassfrondpixels;
}

static void setbladeshader(Shader *shader, bool blend)
{
    if(blend) shader->setvariant(0, 0);
    else shader->set();
    GLuint block = glGetUniformBlockIndex_(Shader::lastshader->program, "grasspatches");
    if(block != GL_INVALID_INDEX) glUniformBlockBinding_(Shader::lastshader->program, block, GRASSUBOBINDING);
    block = glGetUniformBlockIndex_(Shader::lastshader->program, "grassheadlist");
    if(block != GL_INVALID_INDEX) glUniformBlockBinding_(Shader::lastshader->program, block, GRASSHEADUBOBINDING);
    LOCALPARAMF(grassdist, grassdist);
    LOCALPARAMF(grassclump, 1.0f/GRASSCLUMPSIZE);
    // the world size of a pixel at a distance of one unit, for the narrowest blade and the thinning by height on screen
    LOCALPARAMF(bladethin, grassdist*grassthin, grassthinmin, grasspixelsize());
    LOCALPARAMF(bladepixels, grasspixelsthin, grasspixelsfullat());
    LOCALPARAMF(bladeshadowdist, min(grassdist, grassshadowdist));
    // the way to the sun, never below the horizon, or straight up where there's none
    vec sun = getworldcol(WORLDCOL_S_SUNLIGHT).iszero() ? vec(0, 0, 1) : vec(getpielightdir());
    sun.z = max(sun.z, 0.0f);
    if(sun.iszero()) sun = vec(0, 0, 1);
    LOCALPARAM(bladesun, sun.normalize());
}


// draws a chunk's blades with heads in the shape, those of every stride-th blade, a block of its list at a time
static void drawheadlist(const grasschunk &c, int stride, GLsizei count, const void *indices, int verts)
{
    int tier = grassheadtier(stride), heads = c.headcounts[tier];
    for(int first = 0; first < heads; first += GRASSHEADLISTMAX)
    {
        int instances = min(heads - first, int(GRASSHEADLISTMAX));
        glBindBufferRange_(GL_UNIFORM_BUFFER, GRASSHEADUBOBINDING, batches.buf, c.headoffset + first*sizeof(uint), GRASSCHUNKSIZE);
        glDrawElementsInstanced_(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, indices, instances);
        xtravertsva += verts*instances;
    }
}

// draws the instances of a chunk's blades in the shape, with the stalks of that shape, or only those stalks from its head list, every stride-th,
// in a detail and with or without the lowest bend
static void drawbladeshape(const grasschunk &c, const grassbladeshape *shape, const grassbladeshape * const *stalkshapes, bool onlystalks, int stride, int lod, bool lodrows,
    const grassbladeshape *&lastshape)
{
    if(shape != lastshape)
    {
        LOCALPARAMV(bladeoutline, shape->outline.getbuf(), shape->outline.length());
        LOCALPARAMF(bladeopen, shape->open ? 1.0f : 0.0f);
        LOCALPARAMF(bladefrondstraight, shape->straight ? 1.0f : 0.0f);
        lastshape = shape;
    }
    LOCALPARAMF(bladestalks, stalkshapes[0] == shape ? 1.0f : 0.0f, stalkshapes[1] == shape ? 1.0f : 0.0f, stalkshapes[2] == shape ? 1.0f : 0.0f, onlystalks ? 1.0f : 0.0f);
    const void *indices = (const void *)(shape->firstindex[lod][lodrows]*sizeof(ushort));
    if(onlystalks) { drawheadlist(c, stride, shape->numindices[lod][lodrows], indices, shape->numverts[lod][lodrows]); return; }
    int instances = (c.numblades + stride - 1)/stride;
    glDrawElementsInstanced_(GL_TRIANGLES, shape->numindices[lod][lodrows], GL_UNSIGNED_SHORT, indices, instances);
    xtravertsva += shape->numverts[lod][lodrows]*instances;
}

// draws the blades of the chunks, or the heads grown on them, setting each chunk's grass
static void drawchunks(const vector<grasschunk *> &drawn, bool shadow, float shadowtexel, bool heads, bool fronds = false)
{
    Shader *shader = heads ? (shadow ? bladeheadshadowshader : bladeheadshader) : (shadow ? bladeshadowshader : (fronds ? bladefrondshader : bladeshader));
    if(!shader) return;
    float winddist = getwindanimdist();
    gle::bindvbo(heads ? headvbo : bladevbo);
    gle::bindebo(heads ? headebo : bladeebo);
    gle::vertexpointer(2*sizeof(float), NULL, GL_FLOAT, 2);

    int blend = -1;
    const grassbladeshape *lastshape = NULL;
    const grassheadshape *lastheads[GRASSHEADS] = { NULL, NULL, NULL };
    loopv(drawn)
    {
        grasschunk &c = *drawn[i];
        float dist = c.dist;
        const VSlot &s = lookupvslot(c.texture, false);
        if(heads && !grassheads(s)) continue;
        float headpixels = 0;
        // a far chunk skips the blades that are thinned out everywhere in it, keeping every second, fourth, eighth or sixteenth
        float keep = grasskeep(dist, grasspatchheight(s), grasspixelsize());
        if(keep <= 0) continue;
        int stride = 1;
        while(stride < 16 && keep <= 0.5f/stride) stride *= 2;
        if(heads)
        {
            headpixels = grasspatchheight(s)*grassheadwidest(s)/max(dist*grasspixelsize(), 1e-6f);
            if(headpixels < grassheadpixels) continue;
            stride = max(stride, grassheadstep(s));
        }
        float width = grassbladewidth(s)/sqrtf(grassdensityshare());
        if(!heads)
        {
            if(fronds != (!shadow && grassfrondnear(s, width, dist))) continue;
            if(s.grassfrondribbons > 0 && !fronds) width *= grassfrondcover(s);
        }
        if(blend != c.blend)
        {
            if(c.blend)
            {
                glActiveTexture_(GL_TEXTURE1);
                bindblendtexture();
                glActiveTexture_(GL_TEXTURE0);
            }
            setbladeshader(shader, c.blend != 0);
            lastshape = NULL;
            loopk(GRASSHEADS) lastheads[k] = NULL;
            // blades no narrower than a texel and a half of this cascade, so they don't flicker in and out of it
            if(shadow) LOCALPARAMF(bladeshadowtexel, 1.5f*shadowtexel);
            blend = c.blend;
        }
        if(!shadow)
        {
            LOCALPARAMF(bladepulse, lastmillis/PULSE_CYCLE, (lastmillis%PULSE_CYCLE)/float(PULSE_CYCLE));
            vec colours[3];
            loopk(3) colours[k] = grasspiececolour(s.getgrasscolour(k));
            LOCALPARAMV(bladecolours, colours, 3);
            if(heads)
            {
                vec headcolours[GRASSHEADS];
                loopk(GRASSHEADS) headcolours[k] = grasspiececolour(s.grassheadcolours[k]);
                LOCALPARAMV(bladeheadcolours, headcolours, GRASSHEADS);
            }
            LOCALPARAMF(bladeshading, s.grassspec >= 0 ? s.grassspec : 0.0f, s.grassnormalblend >= 0 ? s.grassnormalblend : GRASSNORMALBLEND);
        }
        if(fronds)
        {
            float share = 1/grassfrondspan(s);
            LOCALPARAMF(bladefrond, s.grassfrondribbons, s.grassfrondwaves >= 0 ? s.grassfrondwaves : grasspatchheight(s)/GRASSFRONDWAVELENGTH, 2*grassfrondswing(s)*share, share);
            float rootshare = share*(1 + grassfrondruffle(s));
            LOCALPARAMF(bladefrondcard, rootshare, 1 - rootshare);
            LOCALPARAMF(bladefrondmotion, grassfrondruffle(s), s.grassfrondripple >= 0 ? s.grassfrondripple : GRASSFRONDRIPPLE);
        }
        LOCALPARAMF(bladeparams, grasspatchheight(s), width, c.numpatches, stride);
        LOCALPARAMF(bladeheads, grassheadshare(s, 0), grassheadshare(s, 1), grassheadshare(s, 2));
        const grassheadshape *hs[GRASSHEADS] = { NULL, NULL, NULL };
        if(grassheads(s))
        {
            bool changed = false;
            loopk(GRASSHEADS)
            {
                hs[k] = headshapes.get(s.grassheadshapes[k]);
                if(hs[k] != lastheads[k]) changed = true;
            }
            if(changed)
            { // each kind of head's shape, set when the texture's differ from the last
                vec4 stalks[GRASSHEADS], places[GRASSHEADS], leaves[GRASSHEADS], levels[GRASSHEADS*GRASSHEADLEVELS*GRASSHEADLEVELVEC4S];
                loopk(GRASSHEADS)
                {
                    const grassheadshape &h = *hs[k];
                    stalks[k] = h.stalk;
                    places[k] = vec4(h.size, h.taper, h.place, h.placeparam);
                    leaves[k] = h.leaves;
                    h.pack(&levels[k*GRASSHEADLEVELS*GRASSHEADLEVELVEC4S]);
                    lastheads[k] = hs[k];
                }
                LOCALPARAMV(bladeheadstalks, stalks, GRASSHEADS);
                LOCALPARAMV(bladeheadplaces, places, GRASSHEADS);
                if(heads)
                {
                    LOCALPARAMV(bladeheadlevels, levels, GRASSHEADS*GRASSHEADLEVELS*GRASSHEADLEVELVEC4S);
                    LOCALPARAMV(bladeheadleaves, leaves, GRASSHEADS);
                    LOCALPARAMF(bladeheadleaflows, hs[0]->leaflow, hs[1]->leaflow, hs[2]->leaflow);
                    LOCALPARAMF(bladeheadleafhighs, hs[0]->leafhigh, hs[1]->leafhigh, hs[2]->leafhigh);
                }
            }
            if(!shadow)
            { // the texture's accents, or its shapes', and the stalks that take the head's colour (1) or the accent (2)
                vec4 accents[GRASSHEADS], stalkcolours[GRASSHEADS];
                loopk(GRASSHEADS)
                {
                    vec accent = grasspiececolour(s.grassheadaccents[k] ? s.grassheadaccents[k] : hs[k]->accent);
                    accents[k] = vec4(accent, hs[k]->glow);
                    switch(hs[k]->stalkcolour)
                    {
                        case GRASSHEADSTALKCOLOUR_HEAD: stalkcolours[k] = vec4(grasspiececolour(s.grassheadcolours[k]), 1); break;
                        case GRASSHEADSTALKCOLOUR_ACCENT: stalkcolours[k] = vec4(accent, 2); break;
                        default: stalkcolours[k] = vec4(0, 0, 0, 0); break;
                    }
                }
                if(heads) LOCALPARAMV(bladeheadaccents, accents, GRASSHEADS);
                LOCALPARAMV(bladeheadstalkcolours, stalkcolours, GRASSHEADS);
            }
        }
        int headstep = grassheadstep(s);
        LOCALPARAMF(bladeheadstep, headstep);
        LOCALPARAMF(bladeheadsizes, s.grassheadsizes[0], s.grassheadsizes[1], s.grassheadsizes[2]);
        LOCALPARAMF(bladeheadvariances, s.grassheadvariances[0], s.grassheadvariances[1], s.grassheadvariances[2]);
        LOCALPARAMF(bladeheadstiffs, s.grassheadstiffnesses[0], s.grassheadstiffnesses[1], s.grassheadstiffnesses[2]);
        LOCALPARAMF(bladeheadstalkheights, s.grassheadstalkheights[0], s.grassheadstalkheights[1], s.grassheadstalkheights[2]);
        // stalks are sized from a frond's ribbons, not the wider blade they are cut from
        float stalkscale = fronds ? grassfrondcover(s) : 1;
        vec stalkwidths(s.grassheadstalkwidths[0], s.grassheadstalkwidths[1], s.grassheadstalkwidths[2]);
        if(fronds) loopk(GRASSHEADS) stalkwidths[k] = (stalkwidths[k] > 0 ? stalkwidths[k] : 1.0f)*stalkscale;
        LOCALPARAM(bladeheadstalkwidths, stalkwidths);
        LOCALPARAMF(bladeheadstalktapers, s.grassheadstalktapers[0], s.grassheadstalktapers[1], s.grassheadstalktapers[2]);
        // the wind at the chunk, fading out beyond where models stop swaying, and none where it's all under water
        float windfade = windanimfalloff ? 1 - clamp((dist - winddist)/windanimfalloff, 0.0f, 1.0f) : (dist <= winddist ? 1.0f : 0.0f);
        float animscale = s.grassanimscale >= 0 ? s.grassanimscale : grassanimscale;
        vec wind = windfade > 0 && (!shadow || c.shadow >= 2) && c.water < c.bbmax.z ? getwind(c.center).mul(windfade*animscale) : vec(0, 0, 0);
        LOCALPARAMF(bladewind, wind.x, wind.y, wind.z, 2*M_PI/max(s.grassanimmillis > 0 ? s.grassanimmillis : grassanimmillis, 1));
        LOCALPARAMF(bladevary, s.grassvariation >= 0 ? s.grassvariation : GRASSVARIATION, s.grassmottle >= 0 ? s.grassmottle : GRASSMOTTLE,
            1.0f/(s.grassmottlesize > 0 ? s.grassmottlesize : GRASSMOTTLESIZE));
        LOCALPARAMF(bladeflutter, shadow && c.shadow < 2 ? 0.0f : 1.0f);
        LOCALPARAMF(bladestiff, s.grassstiffness >= 0 ? s.grassstiffness : 0.0f);
        LOCALPARAMF(bladetaper, s.grassbladetaper >= 0 ? s.grassbladetaper : 1.0f);
        int recover = s.grassrecover > 0 ? s.grassrecover : GRASSRECOVER;
        LOCALPARAMF(trampleparams, trampler.origin.x*grasstramplescale, trampler.origin.y*grasstramplescale, grasstrampler::now(), grasstrample && trampler.valid ? recover : 0);
        LOCALPARAMF(tramplehold, min(s.grasstramplehold >= 0 ? s.grasstramplehold : GRASSTRAMPLEHOLD, 0.99f));
        LOCALPARAMF(scorchorigin, scorcher.origin.x*grassscorchscale, scorcher.origin.y*grassscorchscale, grassmap::now(),
            scorcher.valid ? (s.grasselements >= 0 ? s.grasselements : GRASSELEMENTS) : 0);
        LOCALPARAMF(scorchtimes, grassscorchhold(s), grassscorchregrow(s), GRASSSCORCHFLASH, GRASSSCORCHBRIGHT);
        glBindBufferRange_(GL_UNIFORM_BUFFER, GRASSUBOBINDING, batches.buf, c.offset, GRASSCHUNKSIZE);
        if(heads)
        {
            // the index lists for every point, every second and every fourth follow each other, each with the leaves after it,
            // as many as the kinds of head have whose leaves are long enough on screen
            int lod = headpixels < grassheadlodpixels/3 ? 2 : (headpixels < grassheadlodpixels ? 1 : 0), first = 0, leaves = 0;
            loopk(lod) first += (GRASSHEADINDICES>>k) + GRASSHEADLEAVES*GRASSHEADLEAFINDICES;
            loopk(GRASSHEADS) if(hs[k] && grassheadshare(s, k) > 0 && grasspatchheight(s)*hs[k]->leaves.y >= grassleafpixels*max(dist*grasspixelsize(), 1e-6f))
                leaves = max(leaves, int(hs[k]->leaves.x));
            drawheadlist(c, stride, (GRASSHEADINDICES>>lod) + leaves*GRASSHEADLEAFINDICES, (const void *)(first*sizeof(ushort)),
                (GRASSHEADVERTS - 2)/(1<<lod) + 2 + leaves*GRASSHEADLEAFVERTS);
            continue;
        }
        const grassbladeshape *shape = bladeshapes.get(s.grassbladeshape);
        if(!shape || shape->outline.empty()) continue;
        // the shape around each kind of head's stalk, which another draw holds when it isn't the grass's
        const grassbladeshape *stalkshapes[GRASSHEADS];
        loopk(GRASSHEADS)
        {
            stalkshapes[k] = shape;
            int n = s.grassheadstalkshapes[k] >= 0 ? s.grassheadstalkshapes[k] : (hs[k] ? hs[k]->stalkshape : -1);
            if(!hs[k] || n < 0 || grassheadshare(s, k) <= 0) continue;
            const grassbladeshape *stalkshape = bladeshapes.get(n);
            if(stalkshape && !stalkshape->outline.empty()) stalkshapes[k] = stalkshape;
        }
        // shadows only need the outline, a texel or two across, so they drop the lowest bend wherever they are
        bool lodmesh = shadow || dist > grassdist*grasslodmesh;
        // a frond's waves are in its fragment shader, so its blade only follows the lean, which doesn't need the lowest bend
        drawbladeshape(c, shape, stalkshapes, false, stride, lodmesh ? 1 : 0, lodmesh || fronds, lastshape);
        loopk(GRASSHEADS)
        {
            if(stalkshapes[k] == shape || (k > 0 && stalkshapes[k] == stalkshapes[k - 1]) || (k > 1 && stalkshapes[k] == stalkshapes[k - 2])) continue;
            // thin stalks use fewer of their shape's points, by how wide the widest in this shape is on screen at the nearest of the chunk,
            // and the fewest in shadows, where they are a texel or two across
            int stalklod = GRASSBLADELODS - 1;
            if(!shadow)
            {
                float stalkwidth = 0;
                for(int j = k; j < GRASSHEADS; j++) if(stalkshapes[j] == stalkshapes[k] && hs[j])
                    stalkwidth = max(stalkwidth, hs[j]->stalk.y*(s.grassheadstalkwidths[j] > 0 ? s.grassheadstalkwidths[j] : 1.0f));
                float stalkpixels = 1.3f*width*stalkscale*stalkwidth/(sqrtf(max(keep, 1e-6f))*max(dist*grasspixelsize(), 1e-6f));
                stalklod = max(lodmesh ? 1 : 0, stalkpixels >= grassstalkpixels ? 0 : (stalkpixels >= grassstalkpixelshalf ? 1 : 2));
            }
            drawbladeshape(c, stalkshapes[k], stalkshapes, true, max(stride, headstep), stalklod, lodmesh, lastshape);
        }
    }
}

// whether a chunk could be seen, by its own box since its blades can show where the ground they grow from can't, adding it to those to query for the next frame
static bool grasschunkvisible(grasschunk &c, bool doquery, vector<grasschunk *> &queried)
{
    ivec boxmax = ivec(c.boxmin).add(c.boxsize);
    if(isvisiblebb(c.boxmin, c.boxsize) >= VFC_FOGGED || pvsoccluded(c.boxmin, boxmax)) { if(doquery) c.query = NULL; return false; }
    bool inside = camera1->o.insidebb(c.boxmin, boxmax, 2), hidden = doquery && !inside && c.query && c.query->owner == &c && checkquery(c.query);
    if(doquery) // a view without queries of its own leaves those of the main view
    {
        c.query = !inside ? newquery(&c) : NULL;
        if(c.query) queried.add(&c);
    }
    return !hidden;
}

// draws the blades of each chunk in range, into the g-buffer or, in a shadow pass, into the current sun shadow cascade, whose texels are shadowtexel wide
static void drawblades(bool shadow, float shadowtexel = 0)
{
    if(grassbatchesstale()) genbatches();

    bool doquery = !shadow && (!drawtex || isoqstate()) && oqfrags && oqgrass;
    static vector<grasschunk *> drawn, queried;
    drawn.setsize(0);
    queried.setsize(0);
    bool heads = false, fronds = false;
    loopv(batches.chunks)
    {
        grasschunk &c = batches.chunks[i];
        const VSlot &s = lookupvslot(c.texture, false);
        if(shadow)
        {
            c.shadow = grassshadow ? s.grasscastshadow : 0;
            if(!c.shadow || !(calcbbcsmsplits(c.boxmin, ivec(c.boxmin).add(c.boxsize))&(1<<shadowside))) continue;
        }
        // shadows come from the blades the camera keeps, so thinning still follows the camera
        c.dist = camera1->o.dist_to_bb(c.bbmin, c.bbmax);
        if(c.dist > (shadow ? min(grassdist, grassshadowdist) : grassdist)) { if(doquery) c.query = NULL; continue; }
        if(!shadow && !grasschunkvisible(c, doquery, queried)) continue;
        drawn.add(&c);
        if(grassheads(s)) heads = true;
        if(s.grassfrondribbons > 0) fronds = true;
    }
    // against the world alone, so blades don't hide the grass behind them
    if(queried.length())
    {
        startbb();
        loopv(queried)
        {
            startquery(queried[i]->query);
            drawbb(queried[i]->boxmin, queried[i]->boxsize);
            endquery(queried[i]->query);
        }
        endbb();
    }
    drawn.sort([](const grasschunk *a, const grasschunk *b) { return a->blend != b->blend ? a->blend < b->blend : a->dist < b->dist; });

    gle::enablevertex();
    drawchunks(drawn, shadow, shadowtexel, false);
    if(!shadow && fronds) drawchunks(drawn, shadow, shadowtexel, false, true); // after the plain blades, whose depth saves them work
    if(heads) drawchunks(drawn, shadow, shadowtexel, true);
    glBindBufferBase_(GL_UNIFORM_BUFFER, GRASSUBOBINDING, 0);
    glBindBufferBase_(GL_UNIFORM_BUFFER, GRASSHEADUBOBINDING, 0);
    gle::disablevertex();
    gle::clearvbo();
    gle::clearebo();
}

#define GRASSTALLEST 80 // the most a blade can stand above the ground, as slots reach 64 and the lushest patches raise them by a quarter
#define GRASSGROUNDSTEP 4 // world units between the points where a shot looks for the ground under it
#define GRASSEYERISE 8 // how far above a shot it looks for what it can reach
#define GRASSGROUNDSLACK 6 // how far the ground can vary either side of where it was found, within that step
#define GRASSSCORCHSTEP 8 // the same for what scorches, which leaves longer and has less to show between them
#define GRASSSCORCHSLACK 12

// whether a shot could reach the grass of any chunk, since looking for the ground around it is far more work
static bool grasswithin(const vec &o, float radius)
{
    loopv(batches.chunks) if(o.dist_to_bb(batches.chunks[i].bbmin, batches.chunks[i].bbmax) <= radius) return true;
    return false;
}

// finds the ground under a point near a shot which the shot can reach: above it or uphill from it, with nothing solid in the way
static bool grassground(const vec &o, float x, float y, float radius, float &ground, int *lasts = NULL)
{
    float across = vec2(x - o.x, y - o.y).magnitude();
    if(across > radius) return false;
    vec from(x, y, o.z), down(0, 0, -1), hit;
    float limit = radius + GRASSTALLEST, dist = raycubepos(from, down, hit, limit, RAY_CLIPMAT);
    if(dist < 0.01f)
    {
        from.z += radius;
        limit += radius;
        dist = raycubepos(from, down, hit, limit, RAY_CLIPMAT);
        if(dist < 0.01f) return false;
    }
    if(dist < 0 || dist >= limit) return false;
    const VSlot &s = lookupvslot(lookupcube(ivec(int(hit.x), int(hit.y), int(hit.z) - 1)).texture[O_TOP], false);
    if(!s.hasgrass()) return false;
    float top = hit.z + grasspatchheight(s)*GRASSMOTTLEHEIGHT, gap = o.z > top ? o.z - top : (o.z < hit.z ? hit.z - o.z : 0);
    if(across*across + gap*gap > radius*radius) return false;
    // from above the shot to the blades' tips, so bumps in the ground don't shelter what a wall would
    vec eye(o.x, o.y, o.z + GRASSEYERISE), to(x, y, top);
    to.sub(eye);
    float toward = to.magnitude();
    if(toward > 1 && raycube(eye, to.div(toward), toward, RAY_CLIPMAT) < toward - 1) return false;
    ground = hit.z;
    if(lasts) *lasts = grassscorchhold(s) + grassscorchregrow(s);
    return true;
}

void grassblast(const vec &o, float radius, bool pull)
{
    if(!grass || !grasstrample || !trampler.valid || radius <= 0 || !grasswithin(o, radius)) return;
    ivec2 low, high;
    if(!trampler.cells(o, radius, low, high)) return;
    int step = GRASSGROUNDSTEP/trampler.scale;
    for(int y = low.y&~(step - 1); y <= high.y; y += step) for(int x = low.x&~(step - 1); x <= high.x; x += step)
    {
        float ground;
        if(!grassground(o, (x + step*0.5f)*trampler.scale, (y + step*0.5f)*trampler.scale, radius, ground)) continue;
        loopj(step) loopi(step) trampler.push(x + i, y + j, o, radius, pull, ground - GRASSGROUNDSLACK, ground + GRASSGROUNDSLACK);
    }
}

void grassscorch(const vec &o, float radius, int element)
{
    if(!grass || !scorcher.valid || radius <= 0 || element <= ELEMENT_NONE || element >= ELEMENT_MAX || !grasswithin(o, radius)) return;
    ivec2 low, high;
    if(!scorcher.cells(o, radius, low, high)) return;
    int step = GRASSSCORCHSTEP/scorcher.scale;
    for(int y = low.y&~(step - 1); y <= high.y; y += step) for(int x = low.x&~(step - 1); x <= high.x; x += step)
    {
        bool caught = true;
        loopj(step) loopi(step) if(!scorcher.catching(x + i, y + j, element)) caught = false;
        if(caught) continue; // a stream of fire has nothing to add yet
        float ground;
        int lasts;
        if(!grassground(o, (x + step*0.5f)*scorcher.scale, (y + step*0.5f)*scorcher.scale, radius, ground, &lasts)) continue;
        loopj(step) loopi(step) scorcher.mark(x + i, y + j, element, lasts, ground - GRASSSCORCHSLACK, ground + GRASSSCORCHSLACK);
    }
}

static void bindgrassmaps()
{
    if(trampler.valid)
    {
        glActiveTexture_(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, trampler.tex);
        glActiveTexture_(GL_TEXTURE0);
    }
    if(scorcher.valid)
    {
        glActiveTexture_(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, scorcher.tex);
        glActiveTexture_(GL_TEXTURE0);
    }
}

void rendergrass()
{
    if(!grass || !grassdist || !bladeshader) return;

    setupblades();

    glDisable(GL_CULL_FACE);

    if(drawtex) bindgrassmaps(); // a view from elsewhere leaves the maps around the player, which would lose their stamps following it
    else
    {
        if(grasstrample)
        {
            glActiveTexture_(GL_TEXTURE2);
            trampler.update();
            glActiveTexture_(GL_TEXTURE0);
        }
        else trampler.cleanup();
        glActiveTexture_(GL_TEXTURE3);
        scorcher.update();
    }
    glActiveTexture_(GL_TEXTURE4);
    bindgrasspulses();
    glActiveTexture_(GL_TEXTURE0);

    drawblades(false);

    glEnable(GL_CULL_FACE);
}

void rendershadowgrass(float texelsize)
{
    if(!grass || !grassshadow || !grassdist || !bladeshadowshader || !bladevbo) return;

    glDisable(GL_CULL_FACE);
    bindgrassmaps();
    drawblades(true, texelsize);
    glEnable(GL_CULL_FACE);
}
