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
FVAR(IDF_MAP, grassstiffness, 0, 0, 1); // how far blades resist leaning, the wind and trampling, standing straight like reeds
FVAR(IDF_MAP, grassvariation, 0, 0.5f, 1); // how far clumps and blades stray from the grass colours, warmer and drier or cooler and lusher
FVAR(IDF_MAP, grassmottle, 0, 0.5f, 1); // how much the grass changes across the ground, in broad patches of height, colour and density
VAR(IDF_MAP, grassmottlesize, 16, 384, 4096); // how wide those patches are

// imports the old texture repeat, where a higher scale drew more and thinner blades
void importgrassscale(float scale)
{
    if(scale <= 0) return;
    setfvar("grassdensity", clamp(GRASSDENSITY*scale/2, 0.f, 16.f), true);
    setfvar("grasswidth", GRASSWIDTH*2/scale, true);
}

static Shader *bladeshader = NULL, *bladeshadowshader = NULL, *bladeheadshader = NULL, *bladeheadshadowshader = NULL;

void loadgrassshaders()
{
    bladeshader = generateshader("blade", "bladeshader");
    bladeshadowshader = generateshader("bladeshadow", "bladeshadowshader");
    bladeheadshader = generateshader("bladehead", "bladeheadshader");
    bladeheadshadowshader = generateshader("bladeheadshadow", "bladeheadshadowshader");
}

void cleargrassshaders()
{
    bladeshader = bladeshadowshader = bladeheadshader = bladeheadshadowshader = NULL;
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
#define GRASSHEADRING 20 // points around each ring of a head, between a pole below and one above
#define GRASSHEADRINGS 4
#define GRASSHEADVERTS (GRASSHEADRING*GRASSHEADRINGS + 2)
#define GRASSHEADINDICES (GRASSHEADRING*6*GRASSHEADRINGS)
#define GRASSHEADSTALKSHAPE "Round" // the blade shape around a head's stalk, unless it picks another
#define GRASSHEADLODPIXELS 24.0f // heads smaller than this on screen use every second point, and a third of it every fourth
static_assert(GRASSHEADS == 3, "the blade shaders take the heads as vec3s");
#define GRASSHEADPIXELS 0.75f // heads smaller than this on screen aren't drawn
#define GRASSHEADSHARE 0.0625f // the share of the blades that grow a head, where the texture doesn't say
#define GRASSHEADSIZE 0.06f // the smallest heads, glowing bulbs, are this share of the grass's height across
#define GRASSHEADLEVELS 6 // a head's outline goes from a pole below through four rings to a pole above
#define GRASSHEADLEVELVEC4S 3 // each level of a head shape takes three vec4s in the shader
#define GRASSUBOBINDING 0

#define GRASSCLUMPSIZE 12 // blades in a clump share their height, facing, lean and tint

FVAR(IDF_PERSIST, grassthin, 0, 0.15f, 1); // share of grassdist where blades start to thin out
FVAR(IDF_PERSIST, grassthinmin, 0.0625f, 0.0625f, 1); // share of blades left at grassdist, widened to cover the same ground
FVAR(IDF_PERSIST, grasslodmesh, 0, 0.4f, 1); // share of grassdist beyond which blades use fewer triangles
FVAR(IDF_PERSIST, grassdensityscale, 0.25f, 1, 1); // share of each texture's blades drawn, widened to cover the same ground
VAR(IDF_PERSIST, grassshadow, 0, 0, 2); // blades cast shadows from the sun: 0 = none, 1 = static, as thin swaying blades shimmer in the shadow map, 2 = animated with the wind
VAR(IDF_PERSIST, grassshadowdist, 0, 256, 10000); // how far from the camera blades cast them, fading out over the last quarter
VAR(IDF_MAP, grasscastshadow, 0, 0, 2); // the most of grassshadow the map's grass casts, where the texture doesn't choose

// entries in a blade shape's outline, which the shader indexes: a pair of points at each corner for hard edges, then each cap's ring and centre
VARR(grassbladeoutline, GRASSBLADEPOINTS*3 + GRASSBLADEPARTS);

static GLuint bladevbo = 0, bladeebo = 0, headvbo = 0, headebo = 0;
static int grassuboalign = 0;

// the shapes around blades, defined by grassbladeshapedef in config/grass.cfg, which maps refer to by their order
struct grassbladeshape
{
    char *name;
    bool smooth, open;
    vector<vec2> points;
    vector<int> parts; // where each line or ring of points starts
    vector<vec4> outline; // each entry's place across the blade and its normal there, a zero normal for a cap, which faces along the blade
    vector<ivec2> segments; // the pairs of entries each side of the blade joins
    vector<ivec> caps; // those of the caps' rings, with the centre they meet at
    int firstindex, numindices, firstlod, numlod, numverts, numlodverts;

    grassbladeshape() : name(NULL), smooth(false), open(false), firstindex(0), numindices(0), firstlod(0), numlod(0), numverts(0), numlodverts(0) {}
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
        loopv(parts) buildpart(i);
        loopv(parts) buildcap(i);
    }

    // far, smooth rings of many points use every second one
    bool lodhalves() const { return smooth && !open && parts.length() == 1 && points.length() >= GRASSBLADELODSIDES && !(points.length()%2); }
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
    conoutf(colouryellow, "Warning: there is no grass blade shape %d, so the first is drawn instead", shape);
    bladeshapes.warned = true;
}


// the vec4s of the shapes of a texture's heads, which the shader indexes by kind and level
VARR(grassheadlevelvec4s, GRASSHEADS*GRASSHEADLEVELS*GRASSHEADLEVELVEC4S);

// the shapes of the heads blades grow, defined by grassheadshapedef in config/grass.cfg, which maps refer to by their order
enum { GRASSHEADPLACE_TIP = 0, GRASSHEADPLACE_SPIKE, GRASSHEADPLACE_ABOVE, GRASSHEADPLACE_HANG, GRASSHEADPLACE_GROUND, GRASSHEADPLACE_MAX };
static const char * const grassheadplacenames[GRASSHEADPLACE_MAX] = { "tip", "spike", "above", "hang", "ground" };
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
// the head's size against the grass's height
ICOMMAND(0, grassheadshapesize, "f", (float *size), { grassheadshape *shape = curheadshape("grassheadshapesize"); if(shape) shape->size = clamp(*size, 0.01f, 1.0f); });
// where the head sits: on the tip of its stalk, around it as a spike at a share of its length, above the tip at a share of its size facing up,
// hanging from the tip, leaning down by a share, or growing from the ground
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
    conoutf(colouryellow, "Warning: there is no grass head shape %d, so the first is drawn instead", shape);
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
        loopk(2)
        {
            int first = indices.length(), step = k && shape.lodhalves() ? 2 : 1;
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
            if(k) { shape.firstlod = first; shape.numlod = indices.length() - first; shape.numlodverts = verts; }
            else { shape.firstindex = first; shape.numindices = indices.length() - first; shape.numverts = verts; }
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
    // heads: x is the way around the rings, y the level, 0 for the pole below, 1 to GRASSHEADRINGS for the rings up the head, then the pole above
    float headverts[GRASSHEADVERTS][2];
    ushort headindices[GRASSHEADINDICES + GRASSHEADINDICES/2 + GRASSHEADINDICES/4], *idx = headindices;
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

// heads grow on as few of the blades as hold their shares, even where clumps double them, so the head pass draws fewer: every sixteenth blade,
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

// how far the tallest blades reach from their roots
static float grasspatchreach(const VSlot &s)
{
    return grasspatchheight(s)*GRASSMOTTLEHEIGHT*(grassheads(s) ? grassheadtallest(s) + 0.2f*grassheadbiggest(s) : 1.0f);
}

static bool grassbatchesstale()
{
    if(batches.dirty) return true;
    loopv(batches.chunks)
    {
        grasschunk &c = batches.chunks[i];
        const VSlot &s = lookupvslot(c.texture, false);
        if(c.density != grasspatchdensity(s) || c.height != grasspatchreach(s)) return true;
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
            c->height = grasspatchreach(s);
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


// draws the instances of a chunk's blades in the shape, with the stalks of that shape, or only those stalks
static void drawbladeshape(const grassbladeshape *shape, const grassbladeshape * const *stalkshapes, bool onlystalks, bool lodmesh, int instances, const grassbladeshape *&lastshape)
{
    if(shape != lastshape)
    {
        LOCALPARAMV(bladeoutline, shape->outline.getbuf(), shape->outline.length());
        LOCALPARAMF(bladeopen, shape->open ? 1.0f : 0.0f);
        lastshape = shape;
    }
    LOCALPARAMF(bladestalks, stalkshapes[0] == shape ? 1.0f : 0.0f, stalkshapes[1] == shape ? 1.0f : 0.0f, stalkshapes[2] == shape ? 1.0f : 0.0f, onlystalks ? 1.0f : 0.0f);
    if(lodmesh) glDrawElementsInstanced_(GL_TRIANGLES, shape->numlod, GL_UNSIGNED_SHORT, (const void *)(shape->firstlod*sizeof(ushort)), instances);
    else glDrawElementsInstanced_(GL_TRIANGLES, shape->numindices, GL_UNSIGNED_SHORT, (const void *)(shape->firstindex*sizeof(ushort)), instances);
    xtravertsva += (lodmesh ? shape->numlodverts : shape->numverts)*instances;
}

// draws the blades of the chunks, or the heads grown on them, setting each chunk's grass
static void drawchunks(const vector<grasschunk *> &drawn, bool shadow, float shadowtexel, bool heads)
{
    Shader *shader = heads ? (shadow ? bladeheadshadowshader : bladeheadshader) : (shadow ? bladeshadowshader : bladeshader);
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
            headpixels = grasspatchheight(s)*GRASSHEADSIZE*grassheadbiggest(s)/max(dist*grasspixelsize(), 1e-6f);
            if(headpixels < GRASSHEADPIXELS) continue;
            stride = max(stride, grassheadstep(s));
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
            vec colours[3];
            loopk(3) colours[k] = vec::fromcolor(getpulsehexcol(s.getgrasscolour(k)));
            LOCALPARAMV(bladecolours, colours, 3);
            if(heads)
            {
                vec headcolours[GRASSHEADS];
                loopk(GRASSHEADS) headcolours[k] = s.grassheadcolours[k] ? vec::fromcolor(getpulsehexcol(s.grassheadcolours[k])) : vec(0, 0, 0);
                LOCALPARAMV(bladeheadcolours, headcolours, GRASSHEADS);
            }
            LOCALPARAMF(bladeshading, s.grassspec >= 0 ? s.grassspec : grassspec, s.grassnormalblend >= 0 ? s.grassnormalblend : grassnormalblend);
        }
        float width = (s.grasswidth > 0 ? s.grasswidth : grasswidth)/sqrtf(grassdensityshare());
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
                vec4 stalks[GRASSHEADS], places[GRASSHEADS], levels[GRASSHEADS*GRASSHEADLEVELS*GRASSHEADLEVELVEC4S];
                loopk(GRASSHEADS)
                {
                    const grassheadshape &h = *hs[k];
                    stalks[k] = h.stalk;
                    places[k] = vec4(h.size, h.taper, h.place, h.placeparam);
                    h.pack(&levels[k*GRASSHEADLEVELS*GRASSHEADLEVELVEC4S]);
                    lastheads[k] = hs[k];
                }
                LOCALPARAMV(bladeheadstalks, stalks, GRASSHEADS);
                LOCALPARAMV(bladeheadplaces, places, GRASSHEADS);
                if(heads) LOCALPARAMV(bladeheadlevels, levels, GRASSHEADS*GRASSHEADLEVELS*GRASSHEADLEVELVEC4S);
            }
            if(!shadow)
            { // the texture's accents, or its shapes', which pulse colours change each frame, and the stalks that take them or the head's
                vec4 accents[GRASSHEADS], stalkcolours[GRASSHEADS];
                loopk(GRASSHEADS)
                {
                    vec accent = vec::fromcolor(s.grassheadaccents[k] ? getpulsehexcol(s.grassheadaccents[k]) : hs[k]->accent);
                    accents[k] = vec4(accent, hs[k]->glow);
                    switch(hs[k]->stalkcolour)
                    {
                        case GRASSHEADSTALKCOLOUR_HEAD: stalkcolours[k] = vec4(vec::fromcolor(getpulsehexcol(s.grassheadcolours[k])), 1); break;
                        case GRASSHEADSTALKCOLOUR_ACCENT: stalkcolours[k] = vec4(accent, 1); break;
                        default: stalkcolours[k] = vec4(0, 0, 0, 0); break;
                    }
                }
                if(heads) LOCALPARAMV(bladeheadaccents, accents, GRASSHEADS);
                LOCALPARAMV(bladeheadstalkcolours, stalkcolours, GRASSHEADS);
            }
        }
        int headstep = grassheadstep(s);
        LOCALPARAMF(bladeheadstep, headstep, min(grassheadreach(s, headstep), 1.0f) + 1e-3f); // a little over, so rounding never loses a head
        LOCALPARAMF(bladeheadsizes, s.grassheadsizes[0], s.grassheadsizes[1], s.grassheadsizes[2]);
        LOCALPARAMF(bladeheadvariances, s.grassheadvariances[0], s.grassheadvariances[1], s.grassheadvariances[2]);
        LOCALPARAMF(bladeheadstiffs, s.grassheadstiffnesses[0], s.grassheadstiffnesses[1], s.grassheadstiffnesses[2]);
        LOCALPARAMF(bladeheadstalkheights, s.grassheadstalkheights[0], s.grassheadstalkheights[1], s.grassheadstalkheights[2]);
        LOCALPARAMF(bladeheadstalkwidths, s.grassheadstalkwidths[0], s.grassheadstalkwidths[1], s.grassheadstalkwidths[2]);
        LOCALPARAMF(bladeheadstalktapers, s.grassheadstalktapers[0], s.grassheadstalktapers[1], s.grassheadstalktapers[2]);
        // the wind at the chunk, fading out beyond where models stop swaying
        float windfade = windanimfalloff ? 1 - clamp((dist - winddist)/windanimfalloff, 0.0f, 1.0f) : (dist <= winddist ? 1.0f : 0.0f);
        float animscale = s.grassanimscale >= 0 ? s.grassanimscale : grassanimscale;
        vec wind = windfade > 0 && (!shadow || c.shadow >= 2) ? getwind(c.center).mul(windfade*animscale) : vec(0, 0, 0);
        LOCALPARAMF(bladewind, wind.x, wind.y, wind.z, 2*M_PI/max(s.grassanimmillis > 0 ? s.grassanimmillis : grassanimmillis, 1));
        LOCALPARAMF(bladevary, s.grassvariation >= 0 ? s.grassvariation : grassvariation, s.grassmottle >= 0 ? s.grassmottle : grassmottle, 1.0f/(s.grassmottlesize > 0 ? s.grassmottlesize : grassmottlesize));
        LOCALPARAMF(bladeflutter, shadow && c.shadow < 2 ? 0.0f : 1.0f);
        LOCALPARAMF(bladestiff, s.grassstiffness >= 0 ? s.grassstiffness : grassstiffness);
        LOCALPARAMF(bladetaper, s.grassbladetaper >= 0 ? s.grassbladetaper : 1.0f);
        int recover = s.grassrecover > 0 ? s.grassrecover : grasstramplerecover;
        LOCALPARAMF(trampleparams, trampler.origin.x*grasstramplescale, trampler.origin.y*grasstramplescale, grasstrampler::now(), grasstrample && trampler.valid ? recover : 0);
        LOCALPARAMF(tramplehold, min(s.grasstramplehold >= 0 ? s.grasstramplehold : grasstramplehold, 0.99f));
        glBindBufferRange_(GL_UNIFORM_BUFFER, GRASSUBOBINDING, batches.buf, c.offset, GRASSCHUNKSIZE);
        int instances = (c.numblades + stride - 1)/stride;
        if(heads)
        {
            // the index lists for every point, every second and every fourth follow each other
            int lod = headpixels < GRASSHEADLODPIXELS/3 ? 2 : (headpixels < GRASSHEADLODPIXELS ? 1 : 0), first = 0;
            loopk(lod) first += GRASSHEADINDICES>>k;
            glDrawElementsInstanced_(GL_TRIANGLES, GRASSHEADINDICES>>lod, GL_UNSIGNED_SHORT, (const void *)(first*sizeof(ushort)), instances);
            xtravertsva += ((GRASSHEADVERTS - 2)/(1<<lod) + 2)*instances;
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
        bool lodmesh = dist > grassdist*grasslodmesh;
        drawbladeshape(shape, stalkshapes, false, lodmesh, instances, lastshape);
        int stalkstride = max(stride, headstep);
        bool stalkparams = false;
        loopk(GRASSHEADS)
        {
            if(stalkshapes[k] == shape || (k > 0 && stalkshapes[k] == stalkshapes[k - 1]) || (k > 1 && stalkshapes[k] == stalkshapes[k - 2])) continue;
            if(!stalkparams)
            {
                LOCALPARAMF(bladeparams, grasspatchheight(s), width, c.numpatches, stalkstride);
                stalkparams = true;
            }
            drawbladeshape(stalkshapes[k], stalkshapes, true, lodmesh, (c.numblades + stalkstride - 1)/stalkstride, lastshape);
        }
    }
}

// draws the blades of each chunk in range, into the g-buffer or, in a shadow pass, into the current sun shadow cascade, whose texels are shadowtexel wide
static void drawblades(bool shadow, float shadowtexel = 0)
{
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
    bool heads = false;
    loopv(batches.chunks)
    {
        grasschunk &c = batches.chunks[i];
        bool inview = false;
        loopj(c.numvas) if(batches.chunkvas[c.firstva + j]->grassmark == mark) { inview = true; break; }
        if(!inview) continue;
        const VSlot &s = lookupvslot(c.texture, false);
        if(shadow)
        {
            c.shadow = min(grassshadow, s.grasscastshadow >= 0 ? s.grasscastshadow : grasscastshadow);
            if(!c.shadow) continue;
        }
        // shadows come from the blades the camera keeps, so thinning still follows the camera
        c.dist = camera1->o.dist_to_bb(c.bbmin, c.bbmax);
        if(c.dist > (shadow ? min(grassdist, grassshadowdist) : grassdist) || (!shadow && isvisiblesphere(c.radius, c.center) == VFC_NOT_VISIBLE)) continue;
        drawn.add(&c);
        if(grassheads(s)) heads = true;
    }
    drawn.sort([](const grasschunk *a, const grasschunk *b) { return a->blend != b->blend ? a->blend < b->blend : a->dist < b->dist; });

    gle::enablevertex();
    drawchunks(drawn, shadow, shadowtexel, false);
    if(heads) drawchunks(drawn, shadow, shadowtexel, true);
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
