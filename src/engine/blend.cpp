#include "engine.h"

enum
{
    BM_BRANCH = 0,
    BM_SOLID,
    BM_IMAGE
};

struct BlendMapBranch;
struct BlendMapSolid;
struct BlendMapImage;

struct BlendMapNode
{
    union
    {
        BlendMapBranch *branch;
        BlendMapSolid *solid;
        BlendMapImage *image;
    };

    void cleanup(int type);
    void splitsolid(uchar &type, uchar val);
};

struct BlendMapBranch
{
    uchar type[4];
    BlendMapNode children[4];

    ~BlendMapBranch()
    {
        loopi(4) children[i].cleanup(type[i]);
    }

    uchar shrink(BlendMapNode &child, int quadrant);
};

struct BlendMapSolid
{
    uchar val;

    BlendMapSolid(uchar val) : val(val) {}
};

#define BM_SCALE 1
#define BM_IMAGE_SIZE 16
#define BM_LEGACY_IMAGE_SIZE 64 // image size in the top-down blendmap of version 1
#define BM_EDGE_FILL 3 // samples filled past a face's edge, covering the GPU's 4-unit texels and their filtering

struct BlendMapImage
{
    uchar data[BM_IMAGE_SIZE*BM_IMAGE_SIZE];
};

void BlendMapNode::cleanup(int type)
{
    switch(type)
    {
        case BM_BRANCH: delete branch; break;
        case BM_IMAGE: delete image; break;
    }
}

#define DEFBMSOLIDS(n) n, n+1, n+2, n+3, n+4, n+5, n+6, n+7, n+8, n+9, n+10, n+11, n+12, n+13, n+14, n+15

static BlendMapSolid bmsolids[256] =
{
    DEFBMSOLIDS(0x00), DEFBMSOLIDS(0x10), DEFBMSOLIDS(0x20), DEFBMSOLIDS(0x30),
    DEFBMSOLIDS(0x40), DEFBMSOLIDS(0x50), DEFBMSOLIDS(0x60), DEFBMSOLIDS(0x70),
    DEFBMSOLIDS(0x80), DEFBMSOLIDS(0x90), DEFBMSOLIDS(0xA0), DEFBMSOLIDS(0xB0),
    DEFBMSOLIDS(0xC0), DEFBMSOLIDS(0xD0), DEFBMSOLIDS(0xE0), DEFBMSOLIDS(0xF0),
};

void BlendMapNode::splitsolid(uchar &type, uchar val)
{
    cleanup(type);
    type = BM_BRANCH;
    branch = new BlendMapBranch;
    loopi(4)
    {
        branch->type[i] = BM_SOLID;
        branch->children[i].solid = &bmsolids[val];
    }
}

uchar BlendMapBranch::shrink(BlendMapNode &child, int quadrant)
{
    uchar childtype = type[quadrant];
    child = children[quadrant];
    type[quadrant] = BM_SOLID;
    children[quadrant].solid = &bmsolids[0];
    return childtype;
}

struct BlendMapRoot : BlendMapNode
{
    uchar type;

    BlendMapRoot() : type(BM_SOLID) { solid = &bmsolids[0xFF]; }
    BlendMapRoot(uchar type, const BlendMapNode &node) : BlendMapNode(node), type(type) {}

    void cleanup() { BlendMapNode::cleanup(type); }

    bool unpainted() const { return type == BM_SOLID && solid == &bmsolids[0xFF]; }

    void shrink(int quadrant)
    {
        if(type == BM_BRANCH)
        {
            BlendMapRoot oldroot = *this;
            type = branch->shrink(*this, quadrant);
            oldroot.cleanup();
        }
    }

    void enlarge()
    {
        if(type == BM_SOLID) return;
        BlendMapBranch *newbranch = new BlendMapBranch;
        newbranch->type[0] = type;
        newbranch->children[0] = *this;
        loopi(3)
        {
            newbranch->type[i+1] = BM_SOLID;
            newbranch->children[i+1].solid = &bmsolids[0xFF];
        }
        type = BM_BRANCH;
        branch = newbranch;
    }
};

// The blendmap is kept per axis, in slices BLENDSLICE_SIZE units deep along that axis. Each slice is a quadtree over
// the two other axes (R[axis] and C[axis]) with one sample every 1<<BM_SCALE units; a face reads the slice in front of
// it on its own axis (see blendslice in octa.h). Slices nothing has painted are NULL, which reads as 0xFF. Faces on
// the world's edge read the slice just outside it, so slices run from -1 to numblendslices(), stored one up.
static struct blendmapstate
{
    vector<BlendMapRoot *> slices[3];
    BlendMapRoot *legacy = NULL; // a version 1 (top-down) blendmap waiting to be converted
} blendmap;

static inline int numblendslices() { return worldsize/BLENDSLICE_SIZE; }
static inline int blendmapsize() { return worldsize>>BM_SCALE; }

static inline bool validblendslice(int k) { return k >= -1 && k <= numblendslices(); }

// the slices along an axis that faces in a world box can read: those crossing it and one more each side
static inline int blendboxslicemin(const ivec &bbmin, int dim) { return bbmin[dim]/BLENDSLICE_SIZE - 1; }
static inline int blendboxslicemax(const ivec &bbmax, int dim) { return (bbmax[dim] - 1)/BLENDSLICE_SIZE + 1; }

static BlendMapRoot *findblendslice(int dim, int k)
{
    vector<BlendMapRoot *> &slices = blendmap.slices[dim];
    return slices.inrange(k+1) ? slices[k+1] : NULL;
}

static BlendMapRoot *makeblendslice(int dim, int k)
{
    if(!validblendslice(k)) return NULL;
    vector<BlendMapRoot *> &slices = blendmap.slices[dim];
    while(slices.length() <= k+1) slices.add(NULL);
    if(!slices[k+1]) slices[k+1] = new BlendMapRoot;
    return slices[k+1];
}

static void freeblendslice(BlendMapRoot *&root)
{
    if(!root) return;
    root->cleanup();
    delete root;
    root = NULL;
}

static void freeblendslices()
{
    loopk(3)
    {
        loopv(blendmap.slices[k]) freeblendslice(blendmap.slices[k][i]);
        blendmap.slices[k].setsize(0);
    }
}

static uchar lookupblendmap(int x, int y, BlendMapBranch *bm, int bmscale)
{
    for(;;)
    {
        bmscale--;
        int n = (((y>>bmscale)&1)<<1) | ((x>>bmscale)&1);
        switch(bm->type[n])
        {
            case BM_SOLID: return bm->children[n].solid->val;
            case BM_IMAGE: return bm->children[n].image->data[(y&((1<<bmscale)-1))*BM_IMAGE_SIZE + (x&((1<<bmscale)-1))];
        }
        bm = bm->children[n].branch;
    }
}

static uchar lookupblendmap(const BlendMapRoot &root, int x, int y)
{
    switch(root.type)
    {
        case BM_SOLID: return root.solid->val;
        case BM_IMAGE: return root.image->data[y*BM_IMAGE_SIZE + x];
    }
    return lookupblendmap(x, y, root.branch, worldscale-BM_SCALE);
}

static uchar lookupblendmap(int dim, int k, int s, int t)
{
    BlendMapRoot *root = findblendslice(dim, k);
    return root ? lookupblendmap(*root, s, t) : 0xFF;
}

static void fillblendmap(uchar &type, BlendMapNode &node, int size, uchar val, int x1, int y1, int x2, int y2)
{
    if(max(x1, y1) <= 0 && min(x2, y2) >= size)
    {
        node.cleanup(type);
        type = BM_SOLID;
        node.solid = &bmsolids[val];
        return;
    }

    if(type==BM_BRANCH)
    {
        size /= 2;
        if(y1 < size)
        {
            if(x1 < size) fillblendmap(node.branch->type[0], node.branch->children[0], size, val,
                                        x1, y1, min(x2, size), min(y2, size));
            if(x2 > size) fillblendmap(node.branch->type[1], node.branch->children[1], size, val,
                                        max(x1-size, 0), y1, x2-size, min(y2, size));
        }
        if(y2 > size)
        {
            if(x1 < size) fillblendmap(node.branch->type[2], node.branch->children[2], size, val,
                                        x1, max(y1-size, 0), min(x2, size), y2-size);
            if(x2 > size) fillblendmap(node.branch->type[3], node.branch->children[3], size, val,
                                        max(x1-size, 0), max(y1-size, 0), x2-size, y2-size);
        }
        loopi(4) if(node.branch->type[i]!=BM_SOLID || node.branch->children[i].solid->val!=val) return;
        node.cleanup(type);
        type = BM_SOLID;
        node.solid = &bmsolids[val];
        return;
    }
    else if(type==BM_SOLID)
    {
        uchar oldval = node.solid->val;
        if(oldval==val) return;

        if(size > BM_IMAGE_SIZE)
        {
            node.splitsolid(type, oldval);
            fillblendmap(type, node, size, val, x1, y1, x2, y2);
            return;
        }

        type = BM_IMAGE;
        node.image = new BlendMapImage;
        memset(node.image->data, oldval, sizeof(node.image->data));
    }

    uchar *dst = &node.image->data[y1*BM_IMAGE_SIZE + x1];
    loopi(y2-y1)
    {
        memset(dst, val, x2-x1);
        dst += BM_IMAGE_SIZE;
    }
}

// an image-sized block of a slice along some axis, by its slice and its first sample across
struct BlendBlockKey
{
    int k, s, t;

    BlendBlockKey() {}
    BlendBlockKey(int k, int s, int t) : k(k), s(s - s%BM_IMAGE_SIZE), t(t - t%BM_IMAGE_SIZE) {}
};

static inline uint hthash(const BlendBlockKey &key) { return uint(key.k)*0x9E3779B1U ^ uint(key.s)*0x85EBCA6BU ^ uint(key.t)*0xC2B2AE35U; }
static inline bool htcmp(const BlendBlockKey &x, const BlendBlockKey &y) { return x.k == y.k && x.s == y.s && x.t == y.t; }

// The samples an edit wrote and their new values, per image-sized block of a slice. A stamp sends these rather than
// the paint around it, so a stamp from another client in flight at the same time keeps the samples it painted.
struct BlendDelta
{
    struct block { uchar bits[BM_IMAGE_SIZE*BM_IMAGE_SIZE/8], vals[BM_IMAGE_SIZE*BM_IMAGE_SIZE]; };

    hashtable<BlendBlockKey, block *> blocks[3];

    ~BlendDelta() { loopk(3) enumerate(blocks[k], block *, b, delete b); }

    void set(int dim, int k, int s, int t, uchar val)
    {
        block *&b = blocks[dim].access(BlendBlockKey(k, s, t), (block *)NULL);
        if(!b) { b = new block; memset(b->bits, 0, sizeof(b->bits)); }
        int n = (t%BM_IMAGE_SIZE)*BM_IMAGE_SIZE + s%BM_IMAGE_SIZE;
        b->bits[n>>3] |= 1<<(n&7);
        b->vals[n] = val;
    }

    // per block its axis plus one, slice, first sample across, a bitmap of its samples and their values; then a zero
    void write(vector<uchar> &buf)
    {
        loopk(3) enumeratekt(blocks[k], BlendBlockKey, key, block *, b,
        {
            buf.add(k+1);
            putint(buf, key.k);
            putint(buf, key.s);
            putint(buf, key.t);
            buf.put(b->bits, sizeof(b->bits));
            loopi(BM_IMAGE_SIZE*BM_IMAGE_SIZE) if(b->bits[i>>3]&(1<<(i&7))) buf.add(b->vals[i]);
        });
        buf.add(0);
    }
};

static BlendDelta *blenddelta = NULL; // records the samples written while set

static void fillblendmap(int dim, int k, int x1, int y1, int x2, int y2, uchar val)
{
    int bmsize = blendmapsize();
    x1 = clamp(x1, 0, bmsize);
    y1 = clamp(y1, 0, bmsize);
    x2 = clamp(x2, 0, bmsize);
    y2 = clamp(y2, 0, bmsize);
    if(x1 >= x2 || y1 >= y2) return;
    // only samples that change, so writes that leave another client's paint as it was don't send it back
    if(blenddelta && validblendslice(k)) for(int y = y1; y < y2; y++) for(int x = x1; x < x2; x++)
        if(lookupblendmap(dim, k, x, y) != val) blenddelta->set(dim, k, x, y, val);
    BlendMapRoot *root = findblendslice(dim, k);
    if(!root)
    {
        if(val == 0xFF) return;
        root = makeblendslice(dim, k);
        if(!root) return;
    }
    fillblendmap(root->type, *root, bmsize, val, x1, y1, x2, y2);
}

static void optimizeblendmap(uchar &type, BlendMapNode &node)
{
    switch(type)
    {
        case BM_IMAGE:
        {
            uint val = node.image->data[0];
            val |= val<<8;
            val |= val<<16;
            for(uint *data = (uint *)node.image->data, *end = &data[sizeof(node.image->data)/sizeof(uint)]; data < end; data++)
                if(*data != val) return;
            node.cleanup(type);
            type = BM_SOLID;
            node.solid = &bmsolids[val&0xFF];
            break;
        }
        case BM_BRANCH:
        {
            loopi(4) optimizeblendmap(node.branch->type[i], node.branch->children[i]);
            if(node.branch->type[3]!=BM_SOLID) return;
            uint val = node.branch->children[3].solid->val;
            loopi(3) if(node.branch->type[i]!=BM_SOLID || node.branch->children[i].solid->val != val) return;
            node.cleanup(type);
            type = BM_SOLID;
            node.solid = &bmsolids[val];
            break;
        }
    }
}

void optimizeblendmap()
{
    loopk(3) loopv(blendmap.slices[k])
    {
        BlendMapRoot *&root = blendmap.slices[k][i];
        if(!root) continue;
        optimizeblendmap(root->type, *root);
        if(root->unpainted()) freeblendslice(root);
    }
}

ICOMMAND(0, optimizeblendmap, "", (), optimizeblendmap());

VARF(0, autoshowblendmap, 0, 1, 1,
{
    extern int blendpaintmode;
    extern void showblendmap();

    if(autoshowblendmap && blendpaintmode)
        showblendmap();
});

VARF(0, blendpaintmode, 0, 0, 5,
{
    static int lastblendpaintmode = 0;
    extern void showblendmap();

    if(!blendpaintmode) stoppaintblendmap();
    else if(autoshowblendmap && !lastblendpaintmode) showblendmap();

    lastblendpaintmode = blendpaintmode;
});
VAR(0, blendselsnap, 0, 0, 1);
VAR(0, blendbrushreach, 2, 16, 1024);
FVAR(IDF_PERSIST, blendbrushscale, 1/16.0f, 0.25f, 4);

static inline uchar blendpaint(uchar dst, uchar src, int smode)
{
    switch(smode)
    {
        case 1: return src;
        case 2: return min(dst, src);
        case 3: return max(dst, src);
        case 4: return min(dst, uchar(0xFF - src));
        case 5: return max(dst, uchar(0xFF - src));
    }
    return dst;
}

// mask, when given, is laid out like src and skips the samples where it is zero
static void blitblendmap(uchar &type, BlendMapNode &node, int bmx, int bmy, int bmsize, const uchar *src, const uchar *mask, int sx, int sy, int sw, int sh, int smode)
{
    if(type==BM_BRANCH)
    {
        bmsize /= 2;
        if(sy < bmy + bmsize)
        {
            if(sx < bmx + bmsize) blitblendmap(node.branch->type[0], node.branch->children[0], bmx, bmy, bmsize, src, mask, sx, sy, sw, sh, smode);
            if(sx + sw > bmx + bmsize) blitblendmap(node.branch->type[1], node.branch->children[1], bmx+bmsize, bmy, bmsize, src, mask, sx, sy, sw, sh, smode);
        }
        if(sy + sh > bmy + bmsize)
        {
            if(sx < bmx + bmsize) blitblendmap(node.branch->type[2], node.branch->children[2], bmx, bmy+bmsize, bmsize, src, mask, sx, sy, sw, sh, smode);
            if(sx + sw > bmx + bmsize) blitblendmap(node.branch->type[3], node.branch->children[3], bmx+bmsize, bmy+bmsize, bmsize, src, mask, sx, sy, sw, sh, smode);
        }
        return;
    }
    int x1 = clamp(sx - bmx, 0, bmsize), y1 = clamp(sy - bmy, 0, bmsize),
        x2 = clamp(sx+sw - bmx, 0, bmsize), y2 = clamp(sy+sh - bmy, 0, bmsize);
    int offset = max(bmy - sy, 0)*sw + max(bmx - sx, 0);
    src += offset;
    if(mask)
    {
        mask += offset;
        const uchar *row = mask;
        bool any = false;
        loopi(y2-y1)
        {
            loopj(x2-x1) if(row[j]) { any = true; break; }
            if(any) break;
            row += sw;
        }
        if(!any) return;
    }
    if(type==BM_SOLID)
    {
        uchar val = node.solid->val;
        if(bmsize > BM_IMAGE_SIZE)
        {
            node.splitsolid(type, val);
            blitblendmap(type, node, bmx, bmy, bmsize, src - offset, mask ? mask - offset : NULL, sx, sy, sw, sh, smode);
            return;
        }

        type = BM_IMAGE;
        node.image = new BlendMapImage;
        memset(node.image->data, val, sizeof(node.image->data));
    }

    uchar *dst = &node.image->data[y1*BM_IMAGE_SIZE + x1];
    loopi(y2-y1)
    {
        if(smode == 1 && !mask) memcpy(dst, src, x2 - x1);
        else loopj(x2 - x1) if(!mask || mask[j]) dst[j] = blendpaint(dst[j], src[j], smode);
        dst += BM_IMAGE_SIZE;
        src += sw;
        if(mask) mask += sw;
    }
}

static void blitblendmap(BlendMapRoot &root, const uchar *src, const uchar *mask, int sx, int sy, int sw, int sh, int smode)
{
    int bmsize = blendmapsize();
    if(max(sx, sy) >= bmsize || min(sx+sw, sy+sh) <= 0 || min(sw, sh) <= 0) return;
    blitblendmap(root.type, root, 0, 0, bmsize, src, mask, sx, sy, sw, sh, smode);
}

void resetblendmap()
{
    clearblendtextures();
    freeblendslices();
    freeblendslice(blendmap.legacy);
}

void enlargeblendmap()
{
    clearblendtextures();
    loopk(3) loopv(blendmap.slices[k]) if(blendmap.slices[k][i]) blendmap.slices[k][i]->enlarge();
}

void shrinkblendmap(int octant)
{
    // worldsize has already halved; keep the octant's slices and shift them to the new origin
    clearblendtextures();
    ivec offset(octant, ivec(0, 0, 0), worldsize);
    loopk(3)
    {
        vector<BlendMapRoot *> old;
        old.move(blendmap.slices[k]);
        int first = offset[k]/BLENDSLICE_SIZE, quadrant = ((octant>>R[k])&1) | (((octant>>C[k])&1)<<1);
        loopv(old) if(old[i])
        {
            int slice = i-1 - first;
            if(!validblendslice(slice) || findblendslice(k, slice)) { freeblendslice(old[i]); continue; }
            old[i]->shrink(quadrant);
            makeblendslice(k, slice);
            freeblendslice(blendmap.slices[k][slice+1]);
            blendmap.slices[k][slice+1] = old[i];
        }
    }
}

static int calcblendlayer(uchar &type, BlendMapNode &node, int bmx, int bmy, int bmsize, int cx, int cy, int cw, int ch)
{
    if(type==BM_BRANCH)
    {
        bmsize /= 2;
        int layer = -1;
        if(cy < bmy + bmsize)
        {
            if(cx < bmx + bmsize)
            {
                int clayer = calcblendlayer(node.branch->type[0], node.branch->children[0], bmx, bmy, bmsize, cx, cy, cw, ch);
                if(layer < 0) layer = clayer; else if(clayer != layer) return LAYER_BLEND;
            }
            if(cx + cw > bmx + bmsize)
            {
                int clayer = calcblendlayer(node.branch->type[1], node.branch->children[1], bmx+bmsize, bmy, bmsize, cx, cy, cw, ch);
                if(layer < 0) layer = clayer; else if(clayer != layer) return LAYER_BLEND;
            }
        }
        if(cy + ch > bmy + bmsize)
        {
            if(cx < bmx + bmsize)
            {
                int clayer = calcblendlayer(node.branch->type[2], node.branch->children[2], bmx, bmy+bmsize, bmsize, cx, cy, cw, ch);
                if(layer < 0) layer = clayer; else if(clayer != layer) return LAYER_BLEND;
            }
            if(cx + cw > bmx + bmsize)
            {
                int clayer = calcblendlayer(node.branch->type[3], node.branch->children[3], bmx+bmsize, bmy+bmsize, bmsize, cx, cy, cw, ch);
                if(layer < 0) layer = clayer; else if(clayer != layer) return LAYER_BLEND;
            }
        }
        return layer >= 0 ? layer : LAYER_TOP;
    }
    uchar val;
    if(type == BM_SOLID) val = node.solid->val;
    else
    {
        int x1 = clamp(cx - bmx, 0, bmsize), y1 = clamp(cy - bmy, 0, bmsize),
            x2 = clamp(cx+cw - bmx, 0, bmsize), y2 = clamp(cy+ch - bmy, 0, bmsize);
        uchar *src = &node.image->data[y1*BM_IMAGE_SIZE + x1];
        val = src[0];
        loopi(y2-y1)
        {
            loopj(x2-x1) if(src[j] != val) return LAYER_BLEND;
            src += BM_IMAGE_SIZE;
        }
    }
    switch(val)
    {
        case 0xFF: return LAYER_TOP;
        case 0: return LAYER_BOTTOM;
        default: return LAYER_BLEND;
    }
}

static inline int blendlayer(uchar val) { return val == 0xFF ? LAYER_TOP : (val ? LAYER_BLEND : LAYER_BOTTOM); }

// A face as the blendmap sees it: its polygon projected onto the two axes across it, with the depth along its own
// axis, so each sample under it can find the slice it reads. A face covers the samples whose centres lie inside its
// polygon, or the one nearest its middle when it is too small to hold any; s1, t1, s2 and t2 bound them.
struct BlendFace
{
    int orient, dim, s1, t1, s2, t2, numverts, flatslice;
    bool rect;
    vec2 tv[MAXFACEVERTS];
    float dv[MAXFACEVERTS];

    bool setup(int o, const vec *pos, int n)
    {
        if(n < 3) return false;
        orient = o;
        dim = dimension(o);
        numverts = n;
        vec2 lo(1e16f, 1e16f), hi(-1e16f, -1e16f);
        bool flat = true;
        loopi(n)
        {
            tv[i] = vec2(pos[i][R[dim]], pos[i][C[dim]]);
            dv[i] = pos[i][dim];
            if(dv[i] != dv[0]) flat = false;
            lo.min(tv[i]);
            hi.max(tv[i]);
        }
        rect = n == 4;
        loopi(n) if((tv[i].x != lo.x && tv[i].x != hi.x) || (tv[i].y != lo.y && tv[i].y != hi.y)) rect = false;
        int bmsize = blendmapsize();
        s1 = int(floorf(lo.x/(1<<BM_SCALE)));
        t1 = int(floorf(lo.y/(1<<BM_SCALE)));
        s2 = max(int(ceilf(hi.x/(1<<BM_SCALE))), s1+1);
        t2 = max(int(ceilf(hi.y/(1<<BM_SCALE))), t1+1);
        s1 = max(s1, 0);
        t1 = max(t1, 0);
        s2 = min(s2, bmsize);
        t2 = min(t2, bmsize);
        flatslice = flat ? blendslice(orient, dv[0]) : INT_MIN;
        return s1 < s2 && t1 < t2;
    }

    bool isflat() const { return flatslice != INT_MIN; }

    // depth of the face at a point across it, from the fan triangle holding it (or nearest to it)
    float depth(float r, float c) const
    {
        float best = -1e16f, d = dv[0];
        for(int i = 1; i+1 < numverts; i++)
        {
            const vec2 &a = tv[0], &b = tv[i], &e = tv[i+1];
            float area = (b.x-a.x)*(e.y-a.y) - (e.x-a.x)*(b.y-a.y);
            if(fabs(area) < 1e-6f) continue;
            float wb = ((r-a.x)*(e.y-a.y) - (e.x-a.x)*(c-a.y))/area,
                  we = ((b.x-a.x)*(c-a.y) - (r-a.x)*(b.y-a.y))/area,
                  wa = 1 - wb - we, inside = min(wa, min(wb, we));
            if(inside > best)
            {
                best = inside;
                d = wa*dv[0] + wb*dv[i] + we*dv[i+1];
            }
        }
        return d;
    }

    bool covers(int s, int t) const
    {
        if(rect) return true;
        float r = float((s<<BM_SCALE) + (1<<(BM_SCALE-1))), c = float((t<<BM_SCALE) + (1<<(BM_SCALE-1)));
        for(int i = 1; i+1 < numverts; i++)
        {
            const vec2 &a = tv[0], &b = tv[i], &e = tv[i+1];
            float area = (b.x-a.x)*(e.y-a.y) - (e.x-a.x)*(b.y-a.y);
            if(fabs(area) < 1e-6f) continue;
            float wb = ((r-a.x)*(e.y-a.y) - (e.x-a.x)*(c-a.y))/area,
                  we = ((b.x-a.x)*(c-a.y) - (r-a.x)*(b.y-a.y))/area;
            if(min(1 - wb - we, min(wb, we)) >= -1e-4f) return true;
        }
        return false;
    }

    // calls f(s, t) for each sample the face covers inside the given sample bounds
    template<class F> void loopsamples(int cs1, int ct1, int cs2, int ct2, F f) const
    {
        int ls1 = max(s1, cs1), lt1 = max(t1, ct1), ls2 = min(s2, cs2), lt2 = min(t2, ct2);
        bool any = rect;
        if(!any) for(int t = t1; t < t2 && !any; t++) for(int s = s1; s < s2 && !any; s++) if(covers(s, t)) any = true;
        if(!any)
        {
            vec2 mid(0, 0);
            loopi(numverts) mid.add(tv[i]);
            mid.mul(1.0f/numverts);
            int s = clamp(int(floorf(mid.x/(1<<BM_SCALE))), s1, s2-1), t = clamp(int(floorf(mid.y/(1<<BM_SCALE))), t1, t2-1);
            if(s >= ls1 && s < ls2 && t >= lt1 && t < lt2) f(s, t);
            return;
        }
        for(int t = lt1; t < lt2; t++) for(int s = ls1; s < ls2; s++) if(covers(s, t)) f(s, t);
    }
    template<class F> void loopsamples(F f) const { loopsamples(s1, t1, s2, t2, f); }

    int slice(int s, int t) const
    {
        if(isflat()) return flatslice;
        return blendslice(orient, depth(float((s<<BM_SCALE) + (1<<(BM_SCALE-1))), float((t<<BM_SCALE) + (1<<(BM_SCALE-1)))));
    }

    // the world position of a sample on the face
    vec samplepos(int s, int t) const
    {
        float r = float((s<<BM_SCALE) + (1<<(BM_SCALE-1))), c = float((t<<BM_SCALE) + (1<<(BM_SCALE-1)));
        vec p;
        p[R[dim]] = r;
        p[C[dim]] = c;
        p[dim] = isflat() ? dv[0] : depth(r, c);
        return p;
    }
};

static inline bool blendlayerface(const cube &c, int orient)
{
    return c.texture[orient] != DEFAULT_SKY && !(c.material&MAT_ALPHA) && lookupvslot(c.texture[orient], false).layer;
}

// the face's polygon, from its built vertices when it has them, otherwise from the cube's corners as calcsurfaces
// triangulates them
static bool getblendface(const cube &c, int orient, const ivec &co, int size, BlendFace &f)
{
    vec pos[MAXFACEVERTS], po(ivec(co).mask(~0xFFF));
    int numverts = c.ext ? c.ext->surfaces[orient].numverts&MAXFACEVERTS : 0;
    if(numverts)
    {
        const vertinfo *verts = c.ext->verts() + c.ext->surfaces[orient].verts;
        loopi(numverts) pos[i] = vec(verts[i].getxyz()).mul(1.0f/8).add(po);
    }
    else
    {
        int usefaces = visibletris(c, orient, co, size);
        if(!usefaces) return false;
        ivec v[4];
        genfaceverts(c, orient, v);
        int convex = flataxisface(c, orient) ? 0 : faceconvexity(v),
            order = usefaces&4 || convex < 0 ? 1 : 0;
        vec vo(co);
        pos[numverts++] = vec(v[order]).mul(size/8.0f).add(vo);
        if(usefaces&1) pos[numverts++] = vec(v[order+1]).mul(size/8.0f).add(vo);
        pos[numverts++] = vec(v[order+2]).mul(size/8.0f).add(vo);
        if(usefaces&2) pos[numverts++] = vec(v[(order+3)&3]).mul(size/8.0f).add(vo);
    }
    return f.setup(orient, pos, numverts);
}

static int calcblendlayer(const BlendFace &f)
{
    if(f.isflat() && f.rect)
    {
        BlendMapRoot *root = findblendslice(f.dim, f.flatslice);
        if(!root) return LAYER_TOP;
        return calcblendlayer(root->type, *root, 0, 0, blendmapsize(), f.s1, f.t1, f.s2-f.s1, f.t2-f.t1);
    }
    int first = -1;
    bool mixed = false;
    f.loopsamples([&](int s, int t)
    {
        if(mixed) return;
        int val = lookupblendmap(f.dim, f.slice(s, t), s, t);
        if(first < 0) first = val;
        else if(val != first) mixed = true;
    });
    return mixed ? LAYER_BLEND : blendlayer(first < 0 ? 0xFF : first);
}

int calcblendlayer(int orient, const vec *pos, int numverts)
{
    BlendFace f;
    if(!f.setup(orient, pos, numverts)) return LAYER_TOP;
    return calcblendlayer(f);
}

int calcfaceblendlayer(const cube &c, int orient, const ivec &co, int size)
{
    BlendFace f;
    if(!getblendface(c, orient, co, size, f)) return LAYER_TOP;
    return calcblendlayer(f);
}

// A set of samples, kept as one bitmap per image-sized block of a slice
struct BlendMarks
{
    struct block { uchar bits[BM_IMAGE_SIZE*BM_IMAGE_SIZE/8]; };

    hashtable<BlendBlockKey, block *> *blocks[3];

    BlendMarks(int size = 1<<12) { loopi(3) blocks[i] = new hashtable<BlendBlockKey, block *>(size); }
    ~BlendMarks()
    {
        clear();
        loopi(3) delete blocks[i];
    }

    void clear()
    {
        loopk(3)
        {
            enumerate(*blocks[k], block *, b, delete b);
            blocks[k]->clear();
        }
    }

    static BlendBlockKey key(int k, int s, int t) { return BlendBlockKey(k, s, t); }
    static int bit(int s, int t) { return (t%BM_IMAGE_SIZE)*BM_IMAGE_SIZE + s%BM_IMAGE_SIZE; }

    bool mark(int dim, int k, int s, int t)
    {
        block *&b = blocks[dim]->access(key(k, s, t), (block *)NULL);
        if(!b) { b = new block; memset(b->bits, 0, sizeof(b->bits)); }
        int n = bit(s, t);
        if(b->bits[n>>3]&(1<<(n&7))) return false;
        b->bits[n>>3] |= 1<<(n&7);
        return true;
    }

    bool marked(int dim, int k, int s, int t)
    {
        block **b = blocks[dim]->access(key(k, s, t));
        if(!b) return false;
        int n = bit(s, t);
        return ((*b)->bits[n>>3]&(1<<(n&7))) != 0;
    }

    block *find(int dim, int k, int s, int t)
    {
        block **b = blocks[dim]->access(key(k, s, t));
        return b ? *b : NULL;
    }

    template<class F> static void loopblock(int dim, const BlendBlockKey &bkey, const block *b, F &f)
    {
        loopj(BM_IMAGE_SIZE) loopi(BM_IMAGE_SIZE)
        {
            int n = j*BM_IMAGE_SIZE + i;
            if(b->bits[n>>3]&(1<<(n&7))) f(dim, bkey.k, bkey.s+i, bkey.t+j);
        }
    }

    // calls f(dim, k, s, t) for every marked sample
    template<class F> void loopmarked(F f)
    {
        loopk(3) enumeratekt(*blocks[k], BlendBlockKey, bkey, block *, b, loopblock(k, bkey, b, f));
    }

    template<class F> static void loopblockwith(int dim, const BlendBlockKey &bkey, const block *b, F &f)
    {
        loopj(BM_IMAGE_SIZE) loopi(BM_IMAGE_SIZE)
        {
            int n = j*BM_IMAGE_SIZE + i;
            if(b->bits[n>>3]&(1<<(n&7))) f(dim, bkey.k, bkey.s+i, bkey.t+j, b);
        }
    }

    // calls f(dim, k, s, t, block) for every marked sample, with the block holding it
    template<class F> void loopmarkedblocks(F f)
    {
        loopk(3) enumeratekt(*blocks[k], BlendBlockKey, bkey, block *, b, loopblockwith(k, bkey, b, f));
    }
};

// calls f(face, s, t, slice) for each sample that blend layer faces read inside the box (in world units), in the slices
// the box holds, so an edit through it changes no more than a blend region of the box records
template<class F> static void loopblendfaces(cube *c, const ivec &co, int size, const ivec &bbmin, const ivec &bbmax, F &f)
{
    loopoctabox(co, size, bbmin, bbmax)
    {
        ivec o(i, co, size);
        if(c[i].children)
        {
            loopblendfaces(c[i].children, o, size>>1, bbmin, bbmax, f);
            continue;
        }
        if(isempty(c[i])) continue;
        loopj(6)
        {
            if(!blendlayerface(c[i], j)) continue;
            BlendFace face;
            if(!getblendface(c[i], j, o, size, face)) continue;
            int r = R[face.dim], cd = C[face.dim], k1 = max(blendboxslicemin(bbmin, face.dim), -1), k2 = min(blendboxslicemax(bbmax, face.dim), numblendslices());
            face.loopsamples(bbmin[r]>>BM_SCALE, bbmin[cd]>>BM_SCALE, (bbmax[r] + (1<<BM_SCALE) - 1)>>BM_SCALE, (bbmax[cd] + (1<<BM_SCALE) - 1)>>BM_SCALE, [&](int s, int t)
            {
                int k = face.slice(s, t);
                if(k >= k1 && k <= k2) f(face, s, t, k);
            });
        }
    }
}

static void markblendfaces(cube *c, const ivec &co, int size, const ivec &bbmin, const ivec &bbmax, BlendMarks &marks)
{
    auto mark = [&](const BlendFace &face, int s, int t, int k) { marks.mark(face.dim, k, s, t); };
    loopblendfaces(c, co, size, bbmin, bbmax, mark);
}

// Filtering reaches past a face's edge into samples no face reads; give those the value of the read samples beside
// them so the face's edge doesn't fade towards whatever they held. Only samples inside the box, and in the slices it
// holds, are filled, so the marks must cover BM_EDGE_FILL samples more than it.
static void fillblendedges(BlendMarks &marks, const ivec &bbmin, const ivec &bbmax)
{
    struct edgefill { int dim, k, s, t; uchar val; };
    vector<edgefill> frontier, next;
    static const int dx[4] = { -1, 1, 0, 0 }, dy[4] = { 0, 0, -1, 1 };
    int bmsize = blendmapsize();
    // queues the unmarked neighbours of a marked sample, marking them as it goes; block, when given, holds the sample
    // and answers for the neighbours inside it without a lookup
    auto visit = [&](int dim, int k, int s, int t, const BlendMarks::block *b)
    {
        if(k < blendboxslicemin(bbmin, dim) || k > blendboxslicemax(bbmax, dim)) return;
        uchar val = 0;
        bool hasval = false;
        loopi(4)
        {
            int ns = s + dx[i], nt = t + dy[i];
            if(ns < 0 || nt < 0 || ns >= bmsize || nt >= bmsize) continue;
            if(b && ns/BM_IMAGE_SIZE == s/BM_IMAGE_SIZE && nt/BM_IMAGE_SIZE == t/BM_IMAGE_SIZE)
            {
                int n = BlendMarks::bit(ns, nt);
                if(b->bits[n>>3]&(1<<(n&7))) continue;
            }
            if((ns<<BM_SCALE) < bbmin[R[dim]] || (ns<<BM_SCALE) >= bbmax[R[dim]] || (nt<<BM_SCALE) < bbmin[C[dim]] || (nt<<BM_SCALE) >= bbmax[C[dim]]) continue;
            if(!marks.mark(dim, k, ns, nt)) continue;
            if(!hasval) { val = lookupblendmap(dim, k, s, t); hasval = true; }
            edgefill &e = next.add();
            e.dim = dim; e.k = k; e.s = ns; e.t = nt; e.val = val;
        }
    };
    // the first pass marks samples as it queues them, so it must not see its own marks: collect, then queue
    vector<edgefill> seeds;
    marks.loopmarkedblocks([&](int dim, int k, int s, int t, const BlendMarks::block *b)
    {
        loopi(4)
        {
            int ns = s + dx[i], nt = t + dy[i];
            if(ns < 0 || nt < 0 || ns >= bmsize || nt >= bmsize) continue;
            if(ns/BM_IMAGE_SIZE == s/BM_IMAGE_SIZE && nt/BM_IMAGE_SIZE == t/BM_IMAGE_SIZE)
            {
                int n = BlendMarks::bit(ns, nt);
                if(b->bits[n>>3]&(1<<(n&7))) continue;
            }
            else if(marks.marked(dim, k, ns, nt)) continue;
            edgefill &e = seeds.add();
            e.dim = dim; e.k = k; e.s = s; e.t = t;
            break;
        }
    });
    loopv(seeds) visit(seeds[i].dim, seeds[i].k, seeds[i].s, seeds[i].t, NULL);
    loopi(BM_EDGE_FILL)
    {
        loopvj(next)
        {
            edgefill &e = next[j];
            fillblendmap(e.dim, e.k, e.s, e.t, e.s+1, e.t+1, e.val);
        }
        if(i+1 >= BM_EDGE_FILL) break;
        frontier.setsize(0);
        frontier.move(next);
        loopvj(frontier) visit(frontier[j].dim, frontier[j].k, frontier[j].s, frontier[j].t, NULL);
    }
}

// sets every unread sample in the slice to a value that lets its block fold, and unread blocks to unpainted
static void pruneblendmap(uchar &type, BlendMapNode &node, int bmx, int bmy, int bmsize, int dim, int k, BlendMarks &marks)
{
    switch(type)
    {
        case BM_BRANCH:
        {
            int half = bmsize/2;
            loopi(4) pruneblendmap(node.branch->type[i], node.branch->children[i], bmx + (i&1 ? half : 0), bmy + (i&2 ? half : 0), half, dim, k, marks);
            break;
        }
        case BM_SOLID:
        {
            if(node.solid->val == 0xFF) break;
            bool used = false;
            for(int t = bmy; t < bmy + bmsize && !used; t += BM_IMAGE_SIZE)
                for(int s = bmx; s < bmx + bmsize && !used; s += BM_IMAGE_SIZE)
                    if(marks.find(dim, k, s, t)) used = true;
            if(!used) node.solid = &bmsolids[0xFF];
            break;
        }
        case BM_IMAGE:
        {
            BlendMarks::block *b = marks.find(dim, k, bmx, bmy);
            if(!b)
            {
                node.cleanup(type);
                type = BM_SOLID;
                node.solid = &bmsolids[0xFF];
                break;
            }
            int counts[256];
            memset(counts, 0, sizeof(counts));
            int size = min(bmsize, BM_IMAGE_SIZE);
            loopj(size) loopi(size)
            {
                int n = j*BM_IMAGE_SIZE + i;
                if(b->bits[n>>3]&(1<<(n&7))) counts[node.image->data[n]]++;
            }
            int common = 0;
            loopi(256) if(counts[i] > counts[common]) common = i;
            loopj(size) loopi(size)
            {
                int n = j*BM_IMAGE_SIZE + i;
                if(!(b->bits[n>>3]&(1<<(n&7)))) node.image->data[n] = common;
            }
            break;
        }
    }
}

static int countblendimages(uchar type, const BlendMapNode &node)
{
    switch(type)
    {
        case BM_IMAGE: return 1;
        case BM_BRANCH:
        {
            int n = 0;
            loopi(4) n += countblendimages(node.branch->type[i], node.branch->children[i]);
            return n;
        }
    }
    return 0;
}

void showblendmap();

// drops paint no face reads, so it doesn't take up memory or keep slices alive
void pruneblendmap(bool msg)
{
    BlendMarks marks(1<<16);
    ivec bbmin(0, 0, 0), bbmax(worldsize, worldsize, worldsize);
    markblendfaces(worldroot, ivec(0, 0, 0), worldsize>>1, bbmin, bbmax, marks);
    fillblendedges(marks, bbmin, bbmax);
    int oldslices = 0, oldimages = 0, newslices = 0, newimages = 0;
    loopk(3) loopv(blendmap.slices[k])
    {
        BlendMapRoot *&root = blendmap.slices[k][i];
        if(!root) continue;
        oldslices++;
        oldimages += countblendimages(root->type, *root);
        pruneblendmap(root->type, *root, 0, 0, blendmapsize(), k, i-1, marks);
        optimizeblendmap(root->type, *root);
        if(root->unpainted())
        {
            freeblendslice(root);
            continue;
        }
        newslices++;
        newimages += countblendimages(root->type, *root);
    }
    clearblendtextures();
    if(msg) conoutf(colourwhite, "Pruned blendmap: %d of %d slices and %d of %d images freed", oldslices-newslices, oldslices, oldimages-newimages, oldimages);
}

ICOMMAND(0, pruneblendmap, "", (),
{
    // other players would keep the paint it drops
    if(noedit(true) || multiplayer()) return;
    pruneblendmap(true);
    showblendmap();
});

// Copies a version 1 blendmap, which was top-down, into the slices the blended faces read: floors keep their paint
// and walls keep the streaks they showed. The first pass writes each blended face's slices out to BM_EDGE_FILL samples
// past its edges, so filtering at the edges finds the old map's own values; the second writes the samples every blend
// layer wall reads exactly, over anything a neighbour's first pass put there. Floors and ceilings need no second pass:
// their value depends only on where they are across the slice, so a neighbour writes them the same value.
static void convertblendmap(cube *c, const ivec &co, int size, const BlendMapRoot &legacy, bool exact)
{
    static vector<uchar> buf;
    struct band { int k, s1, t1, s2, t2; };
    static vector<band> bands;
    int bmsize = blendmapsize();
    loopi(8)
    {
        ivec o(i, co, size);
        if(c[i].children)
        {
            convertblendmap(c[i].children, o, size>>1, legacy, exact);
            continue;
        }
        if(!c[i].ext || isempty(c[i])) continue;
        loopj(6)
        {
            int numverts = c[i].ext->surfaces[j].numverts;
            if(exact ? dimension(j) == 2 || !blendlayerface(c[i], j) : !(numverts&LAYER_BOTTOM) || !(numverts&MAXFACEVERTS)) continue;
            BlendFace f;
            if(!getblendface(c[i], j, o, size, f)) continue;
            auto legacyval = [&](int s, int t)
            {
                vec p = f.samplepos(s, t);
                int x = clamp(int(floorf(p.x))>>BM_SCALE, 0, bmsize-1), y = clamp(int(floorf(p.y))>>BM_SCALE, 0, bmsize-1);
                return lookupblendmap(legacy, x, y);
            };
            auto writerect = [&](int k, int s1, int t1, int s2, int t2)
            {
                BlendMapRoot *root = makeblendslice(f.dim, k);
                if(!root) return;
                buf.setsize(0);
                for(int t = t1; t < t2; t++) for(int s = s1; s < s2; s++) buf.add(legacyval(s, t));
                blitblendmap(*root, buf.getbuf(), NULL, s1, t1, s2-s1, t2-t1, 1);
            };
            if(f.isflat() && (f.rect || !exact))
            {
                int pad = exact ? 0 : BM_EDGE_FILL;
                writerect(f.flatslice, f.s1-pad, f.t1-pad, f.s2+pad, f.t2+pad);
            }
            else if(exact) f.loopsamples([&](int s, int t)
            {
                int k = f.slice(s, t);
                uchar val = legacyval(s, t);
                if(lookupblendmap(f.dim, k, s, t) != val) fillblendmap(f.dim, k, s, t, s+1, t+1, val);
            });
            else
            {
                bands.setsize(0);
                f.loopsamples([&](int s, int t)
                {
                    int k = f.slice(s, t);
                    loopv(bands) if(bands[i].k == k)
                    {
                        band &b = bands[i];
                        b.s1 = min(b.s1, s); b.t1 = min(b.t1, t); b.s2 = max(b.s2, s+1); b.t2 = max(b.t2, t+1);
                        return;
                    }
                    band &b = bands.add();
                    b.k = k; b.s1 = s; b.t1 = t; b.s2 = s+1; b.t2 = t+1;
                });
                loopv(bands) writerect(bands[i].k, bands[i].s1-BM_EDGE_FILL, bands[i].t1-BM_EDGE_FILL, bands[i].s2+BM_EDGE_FILL, bands[i].t2+BM_EDGE_FILL);
            }
        }
    }
}

void convertblendmap()
{
    if(!blendmap.legacy) return;
    convertblendmap(worldroot, ivec(0, 0, 0), worldsize>>1, *blendmap.legacy, false);
    convertblendmap(worldroot, ivec(0, 0, 0), worldsize>>1, *blendmap.legacy, true);
    freeblendslice(blendmap.legacy);
    optimizeblendmap();
}

// applies an operation to every sample that blend layer faces read inside the box
template<class F> static void editblendfaces(const ivec &bbmin, const ivec &bbmax, F op)
{
    BlendMarks marks;
    int pad = BM_EDGE_FILL<<BM_SCALE;
    ivec mmin = ivec(bbmin).sub(pad), mmax = ivec(bbmax).add(pad);
    markblendfaces(worldroot, ivec(0, 0, 0), worldsize>>1, bbmin, bbmax, marks);
    marks.loopmarked([&](int dim, int k, int s, int t)
    {
        uchar val = lookupblendmap(dim, k, s, t), newval = op(val);
        if(newval != val) fillblendmap(dim, k, s, t, s+1, t+1, newval);
    });
    BlendMarks edges;
    markblendfaces(worldroot, ivec(0, 0, 0), worldsize>>1, mmin, mmax, edges);
    fillblendedges(edges, bbmin, bbmax);
}

static void moveblendmap(uchar type, BlendMapNode &node, int size, int x, int y, BlendMapRoot &dst)
{
    if(type == BM_BRANCH)
    {
        size /= 2;
        moveblendmap(node.branch->type[0], node.branch->children[0], size, x, y, dst);
        moveblendmap(node.branch->type[1], node.branch->children[1], size, x + size, y, dst);
        moveblendmap(node.branch->type[2], node.branch->children[2], size, x, y + size, dst);
        moveblendmap(node.branch->type[3], node.branch->children[3], size, x + size, y + size, dst);
        return;
    }
    int bmsize = blendmapsize();
    if(max(x, y) >= bmsize || min(x+size, y+size) <= 0) return;
    if(type == BM_SOLID)
    {
        fillblendmap(dst.type, dst, bmsize, node.solid->val, max(x, 0), max(y, 0), min(x+size, bmsize), min(y+size, bmsize));
    }
    else if(type == BM_IMAGE)
    {
        blitblendmap(dst, node.image->data, NULL, x, y, size, size, 1);
    }
}

// moves every slice by a world offset, in multiples of an image along the axes across a slice
static void moveblendmap(const ivec &d)
{
    loopk(3)
    {
        vector<BlendMapRoot *> old;
        old.move(blendmap.slices[k]);
        int dk = d[k]/BLENDSLICE_SIZE, ds = d[R[k]]>>BM_SCALE, dt = d[C[k]]>>BM_SCALE;
        loopv(old) if(old[i])
        {
            BlendMapRoot *root = makeblendslice(k, i-1 + dk);
            if(root) moveblendmap(old[i]->type, *old[i], blendmapsize(), ds, dt, *root);
            freeblendslice(old[i]);
        }
    }
    optimizeblendmap();
}

struct BlendBrush
{
    char *name;
    int w, h;
    uchar *data;
    GLuint tex;

    BlendBrush(const char *name, int w, int h) :
      name(newstring(name)), w(w), h(h), data(new uchar[w*h]), tex(0)
    {}

    ~BlendBrush()
    {
        cleanup();
        delete[] name;
        if(data) delete[] data;
    }

    void cleanup()
    {
        if(tex) { glDeleteTextures(1, &tex); tex = 0; }
    }

    void gentex()
    {
        if(!tex) glGenTextures(1, &tex);
        uchar *buf = new uchar[w*h];
        uchar *dst = buf, *src = data;
        loopi(h)
        {
            loopj(w) *dst++ = 255 - *src++;
        }
        createtexture(tex, w, h, buf, 3, 1, hasTRG ? GL_R8 : GL_LUMINANCE8);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        GLfloat border[4] = { 0, 0, 0, 0 };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
        delete[] buf;
    }

    void reorient(bool flipx, bool flipy, bool swapxy)
    {
        uchar *rdata = new uchar[w*h];
        int stridex = 1, stridey = 1;
        if(swapxy) stridex *= h; else stridey *= w;
        uchar *src = data, *dst = rdata;
        if(flipx) { dst += (w-1)*stridex; stridex = -stridex; }
        if(flipy) { dst += (h-1)*stridey; stridey = -stridey; }
        loopi(h)
        {
            uchar *curdst = dst;
            loopj(w)
            {
                *curdst = *src++;
                curdst += stridex;
            }
            dst += stridey;
        }
        if(swapxy) swap(w, h);
        delete[] data;
        data = rdata;
        if(tex) gentex();
    }
};

static vector<BlendBrush *> brushes;
static int curbrush = -1;
// The GPU copy, rebuilt from the slices. Each axis has a root table over the world, one entry per BT_REGION_SIZE
// region; an entry is either solid or a table of BT_TABLE_SIZE^3 chunks, each either solid or a table of
// BT_TABLE_SIZE^3 pages, each either solid or a block of one entry per slice the page spans along the axis, each
// either solid or a tile in the atlas: BT_TILE_TEXELS texels across, each the average of the samples under it, with a
// border copied from around it for filtering. Tables are only made where something is painted, so the copy grows with
// the paint rather than the map. All the tables share one index texture of two 16-bit halves per entry: a high half
// of 0xFFFF marks a solid entry holding its value in the low half, otherwise the entry is an index.
#define BT_REGION_SIZE 0x1000
#define BT_PAGE_SIZE 64
#define BT_TABLE_SIZE 8
#define BT_TABLE_ENTRIES (BT_TABLE_SIZE*BT_TABLE_SIZE*BT_TABLE_SIZE)
#define BT_PAGE_SLICES (BT_PAGE_SIZE/BLENDSLICE_SIZE)
#define BT_TILE_TEXELS (BT_PAGE_SIZE/BLENDSLICE_SIZE)
#define BT_TILE_STRIDE (BT_TILE_TEXELS + 2)
#define BT_ATLAS_TILES 128
#define BT_INDEX_WIDTH 2048
#define BT_MAX_ENTRIES (1<<24) // indices must stay exact as floats in the shader
#define BT_SOLID 0xFFFF0000U
#define BT_UNPAINTED (BT_SOLID | 0xFF)

static struct blendtexstate
{
    vector<ushort> index; // two halves per entry, high then low
    vector<uchar> atlas;
    int rootsize = 0, numtiles = 0, indexrows = 0, atlasrows = 0;
    int dirtymin = INT_MAX, dirtymax = -1, atlasdirtymin = INT_MAX, atlasdirtymax = -1;
    // edits don't reuse the tables and tiles they free, so running out (full) rebuilds them all, unless even a rebuild
    // from scratch ran out (overfull)
    bool built = false, full = false, overfull = false;
    GLuint indextex = 0, atlastex = 0;
} blendtex;

void clearblendtextures()
{
    if(blendtex.indextex) { glDeleteTextures(1, &blendtex.indextex); blendtex.indextex = 0; }
    if(blendtex.atlastex) { glDeleteTextures(1, &blendtex.atlastex); blendtex.atlastex = 0; }
    blendtex.index.setsize(0);
    blendtex.atlas.setsize(0);
    blendtex.rootsize = blendtex.numtiles = blendtex.indexrows = blendtex.atlasrows = 0;
    blendtex.dirtymin = blendtex.atlasdirtymin = INT_MAX;
    blendtex.dirtymax = blendtex.atlasdirtymax = -1;
    blendtex.built = blendtex.full = blendtex.overfull = false;
}

// the value the samples in the rect share, or -1 if they differ
static int uniformblendmap(uchar type, const BlendMapNode &node, int bmx, int bmy, int bmsize, int x1, int y1, int x2, int y2)
{
    if(type == BM_SOLID) return node.solid->val;
    if(type == BM_BRANCH)
    {
        int half = bmsize/2, val = -2;
        loopi(4)
        {
            int cx = bmx + (i&1 ? half : 0), cy = bmy + (i&2 ? half : 0);
            if(x2 <= cx || y2 <= cy || x1 >= cx + half || y1 >= cy + half) continue;
            int cval = uniformblendmap(node.branch->type[i], node.branch->children[i], cx, cy, half, x1, y1, x2, y2);
            if(cval < 0 || (val >= 0 && cval != val)) return -1;
            val = cval;
        }
        return val;
    }
    int sx1 = max(x1 - bmx, 0), sy1 = max(y1 - bmy, 0), sx2 = min(x2 - bmx, bmsize), sy2 = min(y2 - bmy, bmsize);
    const uchar *src = &node.image->data[sy1*BM_IMAGE_SIZE + sx1];
    int val = src[0];
    loopj(sy2-sy1)
    {
        loopi(sx2-sx1) if(src[i] != val) return -1;
        src += BM_IMAGE_SIZE;
    }
    return val;
}

static int maxblendtexrows() { return max(min(hwtexsize, 1<<14), 1<<10); }

// uploads what changed, remaking a texture when it has outgrown its rows
static void uploadblendtextures()
{
    int indexrows = (blendtex.index.length()/2 + BT_INDEX_WIDTH-1)/BT_INDEX_WIDTH, atlasrows = (blendtex.numtiles + BT_ATLAS_TILES-1)/BT_ATLAS_TILES;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if(!blendtex.indextex || indexrows > blendtex.indexrows)
    {
        if(!blendtex.indextex) glGenTextures(1, &blendtex.indextex);
        blendtex.indexrows = min(max(max(indexrows, 1), blendtex.indexrows*2), maxblendtexrows());
        blendtex.index.pad(2*blendtex.indexrows*BT_INDEX_WIDTH - blendtex.index.length());
        glBindTexture(GL_TEXTURE_2D, blendtex.indextex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16, BT_INDEX_WIDTH, blendtex.indexrows, 0, GL_RG, GL_UNSIGNED_SHORT, blendtex.index.getbuf());
        blendtex.dirtymin = INT_MAX;
        blendtex.dirtymax = -1;
    }
    else if(blendtex.dirtymax >= blendtex.dirtymin)
    {
        int row1 = blendtex.dirtymin/BT_INDEX_WIDTH, row2 = blendtex.dirtymax/BT_INDEX_WIDTH + 1;
        glBindTexture(GL_TEXTURE_2D, blendtex.indextex);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, row1, BT_INDEX_WIDTH, row2-row1, GL_RG, GL_UNSIGNED_SHORT, &blendtex.index[2*row1*BT_INDEX_WIDTH]);
        blendtex.dirtymin = INT_MAX;
        blendtex.dirtymax = -1;
    }
    int atlaswidth = BT_ATLAS_TILES*BT_TILE_STRIDE;
    if(!blendtex.atlastex || atlasrows > blendtex.atlasrows)
    {
        if(!blendtex.atlastex) glGenTextures(1, &blendtex.atlastex);
        blendtex.atlasrows = min(max(max(atlasrows, 1), blendtex.atlasrows*2), maxblendtexrows()/BT_TILE_STRIDE);
        blendtex.atlas.pad(blendtex.atlasrows*BT_TILE_STRIDE*atlaswidth - blendtex.atlas.length());
        glBindTexture(GL_TEXTURE_2D, blendtex.atlastex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, hasTRG ? GL_R8 : GL_LUMINANCE8, atlaswidth, blendtex.atlasrows*BT_TILE_STRIDE, 0, hasTRG ? GL_RED : GL_LUMINANCE, GL_UNSIGNED_BYTE, blendtex.atlas.getbuf());
        blendtex.atlasdirtymin = INT_MAX;
        blendtex.atlasdirtymax = -1;
    }
    else if(blendtex.atlasdirtymax >= blendtex.atlasdirtymin)
    {
        int row1 = blendtex.atlasdirtymin/BT_ATLAS_TILES*BT_TILE_STRIDE, row2 = (blendtex.atlasdirtymax/BT_ATLAS_TILES + 1)*BT_TILE_STRIDE;
        glBindTexture(GL_TEXTURE_2D, blendtex.atlastex);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, row1, atlaswidth, row2-row1, hasTRG ? GL_RED : GL_LUMINANCE, GL_UNSIGNED_BYTE, &blendtex.atlas[row1*atlaswidth]);
        blendtex.atlasdirtymin = INT_MAX;
        blendtex.atlasdirtymax = -1;
    }
}

static inline uint getblendindex(int i) { return (uint(blendtex.index[2*i])<<16) | blendtex.index[2*i+1]; }

static inline void setblendindex(int i, uint val)
{
    if(getblendindex(i) == val) return;
    blendtex.index[2*i] = ushort(val>>16);
    blendtex.index[2*i+1] = ushort(val);
    blendtex.dirtymin = min(blendtex.dirtymin, i);
    blendtex.dirtymax = max(blendtex.dirtymax, i);
}

// a run of entries all holding val, or -1 when the index is full
static int allocblendindex(int n, uint val)
{
    int base = blendtex.index.length()/2;
    if(base + n > min(BT_MAX_ENTRIES, maxblendtexrows()*BT_INDEX_WIDTH)) { blendtex.full = true; return -1; }
    loopi(n) { blendtex.index.add(ushort(val>>16)); blendtex.index.add(ushort(val)); }
    blendtex.dirtymin = min(blendtex.dirtymin, base);
    blendtex.dirtymax = max(blendtex.dirtymax, base + n - 1);
    return base;
}

// the texels of one slice across one page, as an atlas tile, or the value they all share
static int buildblendtile(int dim, int k, int r0, int c0, uchar *texels)
{
    BlendMapRoot *root = findblendslice(dim, k);
    if(!root) return 0xFF;
    int bmsize = blendmapsize(), s0 = (r0>>BM_SCALE) - (BLENDSLICE_SIZE>>BM_SCALE), t0 = (c0>>BM_SCALE) - (BLENDSLICE_SIZE>>BM_SCALE),
        span = BT_TILE_STRIDE*(BLENDSLICE_SIZE>>BM_SCALE);
    int val = uniformblendmap(root->type, *root, 0, 0, bmsize, max(s0, 0), max(t0, 0), min(s0 + span, bmsize), min(t0 + span, bmsize));
    if(val >= 0) return val;
    int step = BLENDSLICE_SIZE>>BM_SCALE;
    loop(j, BT_TILE_STRIDE) loop(i, BT_TILE_STRIDE)
    {
        int sum = 0;
        loop(y, step) loop(x, step) sum += lookupblendmap(*root, clamp(s0 + i*step + x, 0, bmsize-1), clamp(t0 + j*step + y, 0, bmsize-1));
        texels[j*BT_TILE_STRIDE + i] = uchar((sum + step*step/2)/(step*step));
    }
    return -1;
}

static bool storeblendtile(int tile, const uchar *texels)
{
    int atlaswidth = BT_ATLAS_TILES*BT_TILE_STRIDE, row = tile/BT_ATLAS_TILES, needed = (row + 1)*BT_TILE_STRIDE*atlaswidth;
    if((row + 1)*BT_TILE_STRIDE > maxblendtexrows()) { blendtex.full = true; return false; }
    if(blendtex.atlas.length() < needed) blendtex.atlas.pad(needed - blendtex.atlas.length());
    uchar *dst = &blendtex.atlas[row*BT_TILE_STRIDE*atlaswidth + (tile%BT_ATLAS_TILES)*BT_TILE_STRIDE];
    loopj(BT_TILE_STRIDE) memcpy(&dst[j*atlaswidth], &texels[j*BT_TILE_STRIDE], BT_TILE_STRIDE);
    blendtex.atlasdirtymin = min(blendtex.atlasdirtymin, tile);
    blendtex.atlasdirtymax = max(blendtex.atlasdirtymax, tile);
    return true;
}

// the entry of a table one level down from entry, at a position inside it, making the table if the entry is solid
// and make is set; -1 if there is none
static int blendtableentry(int entry, const ivec &pos, bool make)
{
    uint val = getblendindex(entry);
    if(val >= BT_SOLID)
    {
        if(!make) return -1;
        int table = allocblendindex(BT_TABLE_ENTRIES, val);
        if(table < 0) return -1;
        setblendindex(entry, table);
        val = table;
    }
    return int(val) + (pos.z*BT_TABLE_SIZE + pos.y)*BT_TABLE_SIZE + pos.x;
}

static inline int blendrootentry(int dim, const ivec &region)
{
    int n = blendtex.rootsize;
    return (dim*n + region.z)*n*n + region.y*n + region.x;
}

// the entry for a page (in pages from the world's origin) along an axis, making the tables above it if make is set
static int blendpageentry(int dim, const ivec &page, bool make)
{
    const int pagesper = BT_REGION_SIZE/BT_PAGE_SIZE, chunkpages = pagesper/BT_TABLE_SIZE;
    ivec region = ivec(page).div(pagesper), inregion = ivec(page).sub(ivec(region).mul(pagesper));
    int entry = blendtableentry(blendrootentry(dim, region), ivec(inregion).div(chunkpages), make);
    if(entry < 0) return -1;
    return blendtableentry(entry, ivec(inregion).sub(ivec(inregion).div(chunkpages).mul(chunkpages)), make);
}

// rebuilds the slice block of one page along one axis, reusing its block and tiles where it already has them
static void buildblendpage(int dim, const ivec &page)
{
    int k0 = page[dim]*BT_PAGE_SLICES, r0 = page[R[dim]]*BT_PAGE_SIZE, c0 = page[C[dim]]*BT_PAGE_SIZE;
    uint slices[BT_PAGE_SLICES];
    uchar texels[BT_TILE_STRIDE*BT_TILE_STRIDE];
    int entry = blendpageentry(dim, page, false);
    uint old = entry >= 0 ? getblendindex(entry) : BT_UNPAINTED;
    int block = old >= BT_SOLID ? -1 : int(old);
    bool solid = true;
    loopi(BT_PAGE_SLICES)
    {
        int val = buildblendtile(dim, k0 + i, r0, c0, texels);
        if(val >= 0) slices[i] = BT_SOLID | val;
        else
        {
            uint prev = block >= 0 ? getblendindex(block + i) : BT_UNPAINTED;
            int tile = prev >= BT_SOLID ? blendtex.numtiles : int(prev);
            if(!storeblendtile(tile, texels)) { slices[i] = BT_UNPAINTED; continue; }
            if(tile == blendtex.numtiles) blendtex.numtiles++;
            slices[i] = tile;
        }
        if(slices[i] != slices[0]) solid = false;
    }
    if(solid && slices[0] >= BT_SOLID)
    {
        if(slices[0] == old) return;
        if(entry < 0) entry = blendpageentry(dim, page, true);
        if(entry >= 0) setblendindex(entry, slices[0]);
        return;
    }
    if(entry < 0) entry = blendpageentry(dim, page, true);
    if(entry < 0) return;
    if(block < 0)
    {
        block = allocblendindex(BT_PAGE_SLICES, BT_UNPAINTED);
        if(block < 0) return;
        setblendindex(entry, block);
    }
    loopi(BT_PAGE_SLICES) setblendindex(block + i, slices[i]);
}

// adds the pages (along dim, in pages) whose tiles read samples of the node inside the rect, unless they are unpainted
static void findblendpages(uchar type, const BlendMapNode &node, int bmx, int bmy, int size, int x1, int y1, int x2, int y2, int dim, int pk, hashset<ivec> &pages)
{
    if(bmx >= x2 || bmy >= y2 || bmx + size <= x1 || bmy + size <= y1) return;
    if(type == BM_SOLID && node.solid->val == 0xFF) return;
    if(type == BM_BRANCH)
    {
        int half = size/2;
        loopi(4) findblendpages(node.branch->type[i], node.branch->children[i], bmx + (i&1 ? half : 0), bmy + (i&2 ? half : 0), half, x1, y1, x2, y2, dim, pk, pages);
        return;
    }
    // a tile's border reaches one texel past its page
    const int pagesamples = BT_PAGE_SIZE>>BM_SCALE, border = BLENDSLICE_SIZE>>BM_SCALE;
    int s1 = max(bmx, x1) - border, t1 = max(bmy, y1) - border, s2 = min(bmx + size, x2) + border, t2 = min(bmy + size, y2) + border;
    int pages1 = max(s1, 0)/pagesamples, paget1 = max(t1, 0)/pagesamples, pages2 = (min(s2, blendmapsize()) - 1)/pagesamples, paget2 = (min(t2, blendmapsize()) - 1)/pagesamples;
    for(int pt = paget1; pt <= paget2; pt++) for(int ps = pages1; ps <= pages2; ps++)
    {
        ivec page;
        page[dim] = pk;
        page[R[dim]] = ps;
        page[C[dim]] = pt;
        pages.add(page);
    }
}

// adds the pages inside the box (in pages) that the index has as painted
static void findblendindexpages(int dim, const ivec &lo, const ivec &hi, hashset<ivec> &pages)
{
    const int pagesper = BT_REGION_SIZE/BT_PAGE_SIZE, chunkpages = pagesper/BT_TABLE_SIZE;
    ivec rlo = ivec(lo).div(pagesper), rhi = ivec(hi).sub(1).div(pagesper);
    for(int rz = rlo.z; rz <= rhi.z; rz++) for(int ry = rlo.y; ry <= rhi.y; ry++) for(int rx = rlo.x; rx <= rhi.x; rx++)
    {
        ivec region(rx, ry, rz);
        uint rval = getblendindex(blendrootentry(dim, region));
        if(rval >= BT_SOLID) continue;
        loopk(BT_TABLE_ENTRIES)
        {
            uint cval = getblendindex(rval + k);
            if(cval >= BT_SOLID) continue;
            ivec chunk = ivec(region).mul(pagesper).add(ivec(k%BT_TABLE_SIZE, (k/BT_TABLE_SIZE)%BT_TABLE_SIZE, k/(BT_TABLE_SIZE*BT_TABLE_SIZE)).mul(chunkpages));
            if(chunk.x >= hi.x || chunk.y >= hi.y || chunk.z >= hi.z || chunk.x + chunkpages <= lo.x || chunk.y + chunkpages <= lo.y || chunk.z + chunkpages <= lo.z) continue;
            loopj(BT_TABLE_ENTRIES)
            {
                if(getblendindex(cval + j) == BT_UNPAINTED) continue;
                ivec page = ivec(chunk).add(ivec(j%BT_TABLE_SIZE, (j/BT_TABLE_SIZE)%BT_TABLE_SIZE, j/(BT_TABLE_SIZE*BT_TABLE_SIZE)));
                if(page.x >= lo.x && page.y >= lo.y && page.z >= lo.z && page.x < hi.x && page.y < hi.y && page.z < hi.z) pages.add(page);
            }
        }
    }
}

void updateblendtextures(const ivec &bbmin, const ivec &bbmax)
{
    if(!blendtex.built) return;
    // the pages whose tiles can read a sample in the box, borders included
    const int border = BLENDSLICE_SIZE, numpages = worldsize/BT_PAGE_SIZE;
    ivec lo = ivec(bbmin).sub(border).max(0).div(BT_PAGE_SIZE), hi = ivec(bbmax).add(border + BT_PAGE_SIZE - 1).div(BT_PAGE_SIZE).min(numpages);
    if(lo.x >= hi.x || lo.y >= hi.y || lo.z >= hi.z) return;
    hashset<ivec> pages;
    loopk(3)
    {
        pages.clear();
        findblendindexpages(k, lo, hi, pages);
        const int pagesamples = BT_PAGE_SIZE>>BM_SCALE;
        int x1 = lo[R[k]]*pagesamples, y1 = lo[C[k]]*pagesamples, x2 = hi[R[k]]*pagesamples, y2 = hi[C[k]]*pagesamples;
        for(int sk = max(lo[k]*BT_PAGE_SLICES, 0); sk < min(hi[k]*BT_PAGE_SLICES, numblendslices()); sk++)
        {
            BlendMapRoot *root = findblendslice(k, sk);
            if(root) findblendpages(root->type, *root, 0, 0, blendmapsize(), x1, y1, x2, y2, k, sk/BT_PAGE_SLICES, pages);
        }
        enumerate(pages, ivec, page, buildblendpage(k, page));
    }
    // the next bindblendtexture rebuilds them
    if(blendtex.full && !blendtex.overfull) { clearblendtextures(); return; }
    uploadblendtextures();
}

static void buildblendtextures()
{
    clearblendtextures();
    blendtex.rootsize = max(worldsize/BT_REGION_SIZE, 1);
    allocblendindex(3*blendtex.rootsize*blendtex.rootsize*blendtex.rootsize, BT_UNPAINTED);
    blendtex.built = blendtex.overfull = true;
    updateblendtextures(ivec(0, 0, 0), ivec(worldsize, worldsize, worldsize));
    blendtex.overfull = blendtex.full;
}

void bindblendtexture()
{
    if(!blendtex.built) buildblendtextures();
    int atlaswidth = BT_ATLAS_TILES*BT_TILE_STRIDE;
    GLOBALPARAMF(blendmapparams, blendtex.rootsize, BT_REGION_SIZE/BT_PAGE_SIZE, BT_TABLE_SIZE, BT_PAGE_SLICES);
    GLOBALPARAMF(blendindexparams, BT_INDEX_WIDTH, 1.0f/BT_INDEX_WIDTH, 1.0f/max(blendtex.indexrows, 1));
    GLOBALPARAMF(blendatlasparams, BT_ATLAS_TILES, 1.0f/atlaswidth, 1.0f/max(blendtex.atlasrows*BT_TILE_STRIDE, 1));
    glBindTexture(GL_TEXTURE_2D, blendtex.atlastex);
    GLint active = 0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
    glActiveTexture_(GL_TEXTURE0 + TEX_BLENDINDEX);
    glBindTexture(GL_TEXTURE_2D, blendtex.indextex);
    glActiveTexture_(active);
}

void dumpblendtexs()
{
    if(blendtex.atlas.empty()) return;
    int atlaswidth = BT_ATLAS_TILES*BT_TILE_STRIDE;
    ImageData temp(atlaswidth, blendtex.atlas.length()/atlaswidth, 1, blendtex.atlas.getbuf());
    const char *map = mapname, *name = strrchr(map, '/');
    defformatstring(buf, "blendatlas_%s.png", name ? name+1 : map);
    savepng(buf, temp, true);
    conoutf(colourwhite, "Blendmap: %d tiles, %d index entries", blendtex.numtiles, blendtex.index.length()/2);
}

COMMAND(0, dumpblendtexs, "");

void cleanupblendmap()
{
    loopv(brushes) brushes[i]->cleanup();
    clearblendtextures();
}

ICOMMAND(0, clearblendbrushes, "", (),
{
    while(brushes.length()) delete brushes.pop();
    curbrush = -1;
});

void delblendbrush(const char *name)
{
    loopv(brushes) if(!strcmp(brushes[i]->name, name))
    {
        delete brushes[i];
        brushes.remove(i--);
    }
    curbrush = brushes.empty() ? -1 : clamp(curbrush, 0, brushes.length()-1);
}

COMMAND(0, delblendbrush, "s");

void addblendbrush(const char *name, const char *imgname)
{
    delblendbrush(name);

    ImageData s;
    if(!loadimage(imgname, s)) { conoutf(colourred, "Could not load blend brush image %s", imgname); return; }
    if(max(s.w, s.h) > (1<<12))
    {
        conoutf(colourred, "Blend brush image size exceeded %dx%d pixels: %s", 1<<12, 1<<12, imgname);
        return;
    }

    BlendBrush *brush = new BlendBrush(name, s.w, s.h);

    uchar *dst = brush->data, *srcrow = s.data;
    loopi(s.h)
    {
        for(uchar *src = srcrow, *end = &srcrow[s.w*s.bpp]; src < end; src += s.bpp)
            *dst++ = src[0];
        srcrow += s.pitch;
    }

    brushes.add(brush);
    if(curbrush < 0) curbrush = 0;
    else if(curbrush >= brushes.length()) curbrush = brushes.length()-1;

}

COMMAND(0, addblendbrush, "ss");

ICOMMAND(0, blendbrushdelta, "i", (int *dir),
{
    if(brushes.empty()) { curbrush = -1; return; }
    curbrush += *dir;
    while(curbrush >= brushes.length()) curbrush -= brushes.length();
    while(curbrush < 0) curbrush += brushes.length();
});

ICOMMAND(0, getblendbrushname, "i", (int *n), result(brushes.inrange(*n) ? brushes[*n]->name : ""));

ICOMMAND(0, blendbrush, "sN$", (char *s, int *numargs, ident *id),
{
    if(*numargs > 0)
    {
        int n = -1;
        if(s != NULL)
        {
            if(isnumeric(*s)) n = atoi(s);
            else loopv(brushes) if(!strcmp(brushes[i]->name, s)) { n = i; break; }
        }
        if(brushes.inrange(n)) curbrush = n;
        else if(n == -1) intret(brushes.length());
        else curbrush = -1;
    }
    else if(*numargs < 0) intret(curbrush);
    else printvar(id, curbrush, brushes.inrange(curbrush) ? brushes[curbrush]->name : "");
});

bool canpaintblendmap(bool brush = true, bool sel = false, bool msg = true)
{
    if(noedit(!sel, msg)) return false;
    if(!blendpaintmode)
    {
        conoutf(colourred, "Operation only allowed in blend paint mode");
        return false;
    }
    if(brush && !brushes.inrange(curbrush))
    {
        if(msg) conoutf(colourred, "No blend brush selected");
        return false;
    }
    return true;
}

ICOMMAND(0, rotateblendbrush, "i", (int *val),
{
    if(!canpaintblendmap()) return;

    BlendBrush *brush = brushes[curbrush];
    const texrotation &r = texrotations[*val < 0 ? 3 : clamp(*val, 1, 7)];
    brush->reorient(r.flipx, r.flipy, r.swapxy);
});

// A blend region is a copy of the paint around a box, carried by undo records, the clipboard and prefabs. Along each
// axis it holds every slice crossing the box and one more beyond each side, so faces on the box's edge keep their
// paint, over the box's extent across the slice. Missing slices were unpainted. The data is the box, then per slice
// its axis and index and its image-sized blocks in order: runs of solid blocks (a count and their value) and blocks
// that aren't (their samples inside the box); then a zero.
static void blendregionrange(const ivec &bbmin, const ivec &bbmax, int dim, int &k1, int &k2, int &s1, int &t1, int &s2, int &t2)
{
    k1 = blendboxslicemin(bbmin, dim);
    k2 = blendboxslicemax(bbmax, dim);
    s1 = bbmin[R[dim]] >> BM_SCALE;
    t1 = bbmin[C[dim]] >> BM_SCALE;
    s2 = (bbmax[R[dim]] + (1<<BM_SCALE) - 1) >> BM_SCALE;
    t2 = (bbmax[C[dim]] + (1<<BM_SCALE) - 1) >> BM_SCALE;
}

void copyblendregion(const ivec &bbmin, const ivec &bbmax, vector<uchar> &buf)
{
    ivec lo = ivec(bbmin).max(0), hi = ivec(bbmax).min(worldsize);
    loopi(3) { putint(buf, lo[i]); }
    loopi(3) { putint(buf, hi[i]); }
    int bmsize = blendmapsize();
    loopk(3)
    {
        int k1, k2, s1, t1, s2, t2;
        blendregionrange(lo, hi, k, k1, k2, s1, t1, s2, t2);
        if(s1 >= s2 || t1 >= t2) continue;
        for(int sl = k1; sl <= k2; sl++)
        {
            BlendMapRoot *root = findblendslice(k, sl);
            if(!root || uniformblendmap(root->type, *root, 0, 0, bmsize, s1, t1, s2, t2) == 0xFF) continue;
            buf.add(k+1);
            putint(buf, sl);
            int run = 0, runval = 0;
            auto endrun = [&]()
            {
                if(!run) return;
                buf.add(0);
                putint(buf, run);
                buf.add(runval);
                run = 0;
            };
            for(int bt = t1 - t1%BM_IMAGE_SIZE; bt < t2; bt += BM_IMAGE_SIZE)
                for(int bs = s1 - s1%BM_IMAGE_SIZE; bs < s2; bs += BM_IMAGE_SIZE)
                {
                    int x1 = max(bs, s1), y1 = max(bt, t1), x2 = min(bs + BM_IMAGE_SIZE, s2), y2 = min(bt + BM_IMAGE_SIZE, t2),
                        val = uniformblendmap(root->type, *root, 0, 0, bmsize, x1, y1, x2, y2);
                    if(val >= 0)
                    {
                        if(run && val != runval) endrun();
                        runval = val;
                        run++;
                        continue;
                    }
                    endrun();
                    buf.add(1);
                    for(int y = y1; y < y2; y++) for(int x = x1; x < x2; x++) buf.add(lookupblendmap(*root, x, y));
                }
            endrun();
        }
    }
    buf.add(0);
}

bool blendregionbox(const uchar *data, int len, ivec &bbmin, ivec &bbmax)
{
    ucharbuf p((uchar *)data, len);
    loopi(3) bbmin[i] = getint(p);
    loopi(3) bbmax[i] = getint(p);
    // the box came from a map up to the largest world size, maybe another client's
    loopi(3) if(bbmin[i] < 0 || bbmin[i] > bbmax[i] || bbmax[i] > (1<<16)) return false;
    return !p.overread();
}

// p' = sign * p[perm] + offset, per axis
BlendXform::BlendXform()
{
    loopi(3) { perm[i] = i; sign[i] = 1; offset[i] = 0; }
}

vec BlendXform::apply(const vec &p) const
{
    vec r;
    loopi(3) r[i] = sign[i]*p[perm[i]] + offset[i];
    return r;
}

void BlendXform::then(const BlendXform &next)
{
    BlendXform r;
    loopi(3)
    {
        r.perm[i] = perm[next.perm[i]];
        r.sign[i] = next.sign[i]*sign[next.perm[i]];
        r.offset[i] = next.sign[i]*offset[next.perm[i]] + next.offset[i];
    }
    *this = r;
}

BlendXform BlendXform::translate(const ivec &d)
{
    BlendXform x;
    loopi(3) x.offset[i] = d[i];
    return x;
}

BlendXform BlendXform::flip(int dim, int lo, int hi)
{
    BlendXform x;
    x.sign[dim] = -1;
    x.offset[dim] = lo + hi;
    return x;
}

// a quarter turn about dim within the square from o of size along the two other axes, as rotatecube turns cubes
BlendXform BlendXform::rotate(int dim, const ivec &o, int size)
{
    BlendXform x;
    int r = R[dim], c = C[dim];
    x.perm[r] = c;
    x.sign[r] = -1;
    x.offset[r] = o[r] + o[c] + size;
    x.perm[c] = r;
    x.offset[c] = o[c] - o[r];
    return x;
}

// the slice and samples a world box (one sample across, one slice deep) lands in
static void blendxformsample(const BlendXform &xf, int dim, int k, int s, int t, int &ndim, int &nk, int &ns, int &nt)
{
    vec centre;
    centre[dim] = k*BLENDSLICE_SIZE + 0.5f*BLENDSLICE_SIZE;
    centre[R[dim]] = (s<<BM_SCALE) + 0.5f*(1<<BM_SCALE);
    centre[C[dim]] = (t<<BM_SCALE) + 0.5f*(1<<BM_SCALE);
    vec n = xf.apply(centre);
    loopi(3) if(xf.perm[i] == dim) ndim = i;
    nk = int(floorf(n[ndim]/BLENDSLICE_SIZE));
    ns = int(floorf(n[R[ndim]]/(1<<BM_SCALE)));
    nt = int(floorf(n[C[ndim]]/(1<<BM_SCALE)));
}

// replaces the paint around the destination box with the region's, moved by the transform; the destination box,
// grown by a slice each way, is returned for updating
bool pasteblendregion(const uchar *data, int len, const BlendXform &xf, ivec &dstmin, ivec &dstmax)
{
    ivec srcmin, srcmax;
    if(!blendregionbox(data, len, srcmin, srcmax)) return false;
    vec a = xf.apply(vec(srcmin)), b = xf.apply(vec(srcmax));
    loopi(3)
    {
        dstmin[i] = clamp(int(floorf(min(a[i], b[i]) + 0.5f)), 0, worldsize);
        dstmax[i] = clamp(int(floorf(max(a[i], b[i]) + 0.5f)), 0, worldsize);
    }
    loopk(3)
    {
        int k1, k2, s1, t1, s2, t2;
        blendregionrange(dstmin, dstmax, k, k1, k2, s1, t1, s2, t2);
        for(int sl = k1; sl <= k2; sl++) fillblendmap(k, sl, s1, t1, s2, t2, 0xFF);
    }
    ucharbuf p((uchar *)data, len);
    loopi(6) getint(p);
    for(;;)
    {
        int dim = p.get() - 1;
        if(dim < 0 || dim > 2) break;
        int sl = getint(p), k1, k2, s1, t1, s2, t2, run = 0;
        uchar runval = 0xFF;
        blendregionrange(srcmin, srcmax, dim, k1, k2, s1, t1, s2, t2);
        for(int bt = t1 - t1%BM_IMAGE_SIZE; bt < t2; bt += BM_IMAGE_SIZE)
            for(int bs = s1 - s1%BM_IMAGE_SIZE; bs < s2; bs += BM_IMAGE_SIZE)
            {
                int x1 = max(bs, s1), y1 = max(bt, t1), x2 = min(bs + BM_IMAGE_SIZE, s2), y2 = min(bt + BM_IMAGE_SIZE, t2), type = 0;
                if(!run)
                {
                    type = p.get();
                    if(type == 0)
                    {
                        run = getint(p);
                        runval = p.get();
                        if(run <= 0) return false;
                    }
                }
                if(type == 0)
                {
                    run--;
                    uchar val = runval;
                    if(val == 0xFF) continue;
                    // a rect transforms to a rect: map its corner samples and fill between them
                    int d1, k1n, sa, ta, d2, k2n, sb, tb;
                    blendxformsample(xf, dim, sl, x1, y1, d1, k1n, sa, ta);
                    blendxformsample(xf, dim, sl, x2-1, y2-1, d2, k2n, sb, tb);
                    fillblendmap(d1, k1n, min(sa, sb), min(ta, tb), max(sa, sb)+1, max(ta, tb)+1, val);
                }
                else if(type == 1) for(int y = y1; y < y2; y++) for(int x = x1; x < x2; x++)
                {
                    uchar val = p.get();
                    if(val == 0xFF) continue;
                    int nd, nk, ns, nt;
                    blendxformsample(xf, dim, sl, x, y, nd, nk, ns, nt);
                    fillblendmap(nd, nk, ns, nt, ns+1, nt+1, val);
                }
                else return false;
                if(p.overread()) return false;
            }
        if(run) return false;
    }
    dstmin.sub(BLENDSLICE_SIZE).max(0);
    dstmax.add(BLENDSLICE_SIZE).min(worldsize);
    return true;
}

// writes the samples a delta from another client holds, returning the box they lie in
bool applyblenddelta(const uchar *data, int len, ivec &bbmin, ivec &bbmax)
{
    ucharbuf p((uchar *)data, len);
    int bmsize = blendmapsize();
    bool changed = false;
    bbmin = ivec(worldsize, worldsize, worldsize);
    bbmax = ivec(0, 0, 0);
    for(;;)
    {
        int dim = p.get() - 1;
        if(dim < 0 || dim > 2) break;
        int k = getint(p), bs = getint(p), bt = getint(p);
        uchar bits[BM_IMAGE_SIZE*BM_IMAGE_SIZE/8];
        p.get(bits, sizeof(bits));
        if(p.overread() || !validblendslice(k) || bs < 0 || bt < 0 || bs >= bmsize || bt >= bmsize || bs%BM_IMAGE_SIZE || bt%BM_IMAGE_SIZE) break;
        loopi(BM_IMAGE_SIZE*BM_IMAGE_SIZE) if(bits[i>>3]&(1<<(i&7)))
        {
            int s = bs + i%BM_IMAGE_SIZE, t = bt + i/BM_IMAGE_SIZE;
            fillblendmap(dim, k, s, t, s+1, t+1, p.get());
        }
        if(p.overread()) break;
        ivec lo, hi;
        lo[dim] = k*BLENDSLICE_SIZE;
        hi[dim] = lo[dim] + BLENDSLICE_SIZE;
        lo[R[dim]] = bs<<BM_SCALE;
        hi[R[dim]] = (bs + BM_IMAGE_SIZE)<<BM_SCALE;
        lo[C[dim]] = bt<<BM_SCALE;
        hi[C[dim]] = (bt + BM_IMAGE_SIZE)<<BM_SCALE;
        bbmin.min(lo);
        bbmax.max(hi);
        changed = true;
    }
    bbmin.max(0);
    bbmax.min(worldsize);
    return changed;
}

// A stamp projects the brush from the camera. The brush lies across the axis of the face being aimed at, at the aim
// point, blendbrushscale world units per brush pixel; each sample a blend layer face reads, that the camera can see and
// that lies within reach of the aim point along that axis, takes the brush value its line of sight passes through.
struct BlendStamp
{
    vec aim, camera;
    int dim, w, h, reach;
    float r0, c0, texelsize;
};

static bool aimblendstamp(BlendBrush *brush, BlendStamp &st)
{
    vec dir = vec(worldpos).sub(camera1->o);
    if(dir.iszero()) return false;
    dir.normalize();
    float dist = raycube(camera1->o, dir, 0, 0);
    if(dist < 0) return false;
    st.camera = camera1->o;
    st.aim = vec(dir).mul(dist).add(camera1->o);
    const vec &n = hitsurface;
    st.dim = fabs(n.x) > fabs(n.y) ? (fabs(n.x) > fabs(n.z) ? 0 : 2) : (fabs(n.y) > fabs(n.z) ? 1 : 2);
    if(fabs(st.camera[st.dim] - st.aim[st.dim]) < 1) return false;
    st.w = brush->w;
    st.h = brush->h;
    st.reach = blendbrushreach;
    st.texelsize = (1<<BM_SCALE)*blendbrushscale;
    st.r0 = st.aim[R[st.dim]] - 0.5f*st.w*st.texelsize;
    st.c0 = st.aim[C[st.dim]] - 0.5f*st.h*st.texelsize;
    if(blendselsnap)
    {
        st.r0 = floorf(st.r0/sel.grid)*sel.grid;
        st.c0 = floorf(st.c0/sel.grid)*sel.grid;
    }
    return true;
}

// the box a stamp can reach: the brush's footprint grown by perspective over the reach, and the reach along its axis
static void blendstampbox(const BlendStamp &st, ivec &bbmin, ivec &bbmax)
{
    int r = R[st.dim], c = C[st.dim];
    float dist = fabs(st.camera[st.dim] - st.aim[st.dim]), grow = (dist + st.reach)/dist,
          hw = 0.5f*st.w*st.texelsize, hh = 0.5f*st.h*st.texelsize, mr = st.r0 + hw, mc = st.c0 + hh;
    bbmin[r] = int(floorf(mr - hw*grow)) - st.reach;
    bbmin[c] = int(floorf(mc - hh*grow)) - st.reach;
    bbmin[st.dim] = int(floorf(st.aim[st.dim])) - st.reach;
    bbmax[r] = int(ceilf(mr + hw*grow)) + st.reach;
    bbmax[c] = int(ceilf(mc + hh*grow)) + st.reach;
    bbmax[st.dim] = int(ceilf(st.aim[st.dim])) + st.reach;
    bbmin.max(0);
    bbmax.min(worldsize);
}

// the brush value the camera sees through a world position, or -1 if the brush doesn't cover it
static int blendstampvalue(const BlendStamp &st, const uchar *data, const vec &p)
{
    vec d = vec(p).sub(st.camera);
    if(fabs(d[st.dim]) < 1e-4f) return -1;
    float t = (st.aim[st.dim] - st.camera[st.dim])/d[st.dim];
    if(t <= 0) return -1;
    vec q = vec(d).mul(t).add(st.camera);
    float u = (q[R[st.dim]] - st.r0)/st.texelsize - 0.5f, v = (q[C[st.dim]] - st.c0)/st.texelsize - 0.5f;
    if(u < -0.5f || v < -0.5f || u > st.w - 0.5f || v > st.h - 0.5f) return -1;
    int x = clamp(int(floorf(u)), 0, st.w-1), y = clamp(int(floorf(v)), 0, st.h-1), x2 = min(x+1, st.w-1), y2 = min(y+1, st.h-1);
    float fx = clamp(u - x, 0.0f, 1.0f), fy = clamp(v - y, 0.0f, 1.0f);
    return int((1-fy)*((1-fx)*data[y*st.w + x] + fx*data[y*st.w + x2]) + fy*((1-fx)*data[y2*st.w + x] + fx*data[y2*st.w + x2]) + 0.5f);
}

// calls hit(face, s, t, slice, value) for each sample the stamp paints
template<class F> static void castblendstamp(BlendBrush *brush, const BlendStamp &st, F hit)
{
    ivec bbmin, bbmax;
    blendstampbox(st, bbmin, bbmax);
    auto visit = [&](const BlendFace &face, int s, int t, int k)
    {
        vec p = face.samplepos(s, t);
        if(fabs(p[st.dim] - st.aim[st.dim]) > st.reach) return;
        // the face must face the camera, and nothing may stand between them
        if((st.camera[face.dim] - p[face.dim])*(dimcoord(face.orient) ? 1 : -1) <= 0) return;
        int val = blendstampvalue(st, brush->data, p);
        if(val < 0) return;
        vec dir = vec(p).sub(st.camera);
        float dist = dir.magnitude();
        if(dist <= 0) return;
        dir.div(dist);
        float blocked = raycube(st.camera, dir, dist, 0);
        if(blocked >= 0 && blocked < dist - 1) return;
        hit(face, s, t, k, val);
    };
    loopblendfaces(worldroot, ivec(0, 0, 0), worldsize>>1, bbmin, bbmax, visit);
}

static void stampblendmap(BlendBrush *brush, const BlendStamp &st, int smode)
{
    bool painted = false;
    castblendstamp(brush, st, [&](const BlendFace &face, int s, int t, int k, int val)
    {
        uchar cur = lookupblendmap(face.dim, k, s, t), next = blendpaint(cur, uchar(val), smode);
        if(next != cur) fillblendmap(face.dim, k, s, t, s+1, t+1, next);
        painted = true;
    });

    ivec bbmin, bbmax;
    blendstampbox(st, bbmin, bbmax);
    if(painted)
    {
        BlendMarks marks;
        int pad = BM_EDGE_FILL<<BM_SCALE;
        bbmin.sub(pad).max(0);
        bbmax.add(pad).min(worldsize);
        markblendfaces(worldroot, ivec(0, 0, 0), worldsize>>1, ivec(bbmin).sub(pad), ivec(bbmax).add(pad), marks);
        fillblendedges(marks, bbmin, bbmax);
    }
    previewblends(bbmin, bbmax);
}

// Blendmap edits reach other clients one of two ways. A stamp sends the samples it wrote as N_EDITPAINT (see
// BlendDelta), since other clients may not have the brush (players can add their own, and rotating one changes it).
// The other edits need no brush and send N_EDITBLEND with BLENDEDIT_ARGS ints: the edit, then its arguments.
enum { BLENDEDIT_CLEARSEL = 0, BLENDEDIT_INVERTSEL, BLENDEDIT_INVERT, BLENDEDIT_CLEAR, BLENDEDIT_MOVE };

// the box an edit can change, for recording undo
static void blendeditbox(const int *args, ivec &bbmin, ivec &bbmax)
{
    switch(args[0])
    {
        case BLENDEDIT_CLEARSEL: case BLENDEDIT_INVERTSEL:
            bbmin = ivec(args[1], args[2], args[3]);
            bbmax = ivec(args[4], args[5], args[6]);
            break;
        default:
            bbmin = ivec(0, 0, 0);
            bbmax = ivec(worldsize, worldsize, worldsize);
            break;
    }
    bbmin.max(0);
    bbmax.min(worldsize);
}

static void applyblendedit(const int *args)
{
    switch(args[0])
    {
        case BLENDEDIT_CLEARSEL: case BLENDEDIT_INVERTSEL:
        {
            ivec bbmin(args[1], args[2], args[3]), bbmax(args[4], args[5], args[6]);
            if(args[0] == BLENDEDIT_CLEARSEL) editblendfaces(bbmin, bbmax, [](uchar val) { return uchar(0xFF); });
            else editblendfaces(bbmin, bbmax, [](uchar val) { return uchar(0xFF - val); });
            previewblends(bbmin, bbmax);
            break;
        }
        case BLENDEDIT_INVERT:
            editblendfaces(ivec(0, 0, 0), ivec(worldsize, worldsize, worldsize), [](uchar val) { return uchar(0xFF - val); });
            previewblends(ivec(0, 0, 0), ivec(worldsize, worldsize, worldsize));
            break;
        case BLENDEDIT_CLEAR:
            resetblendmap();
            previewblends(ivec(0, 0, 0), ivec(worldsize, worldsize, worldsize));
            break;
        case BLENDEDIT_MOVE:
        {
            ivec d(args[1], args[2], args[3]);
            int step = BM_IMAGE_SIZE<<BM_SCALE;
            if(d.x%step || d.y%step || d.z%step) break;
            if(max(abs(d.x), max(abs(d.y), abs(d.z))) >= worldsize) resetblendmap();
            else moveblendmap(d);
            previewblends(ivec(0, 0, 0), ivec(worldsize, worldsize, worldsize));
            break;
        }
    }
}

// an edit made here: recorded for undo, applied, and sent to other clients
static void localblendedit(const int *args)
{
    ivec bbmin, bbmax;
    blendeditbox(args, bbmin, bbmax);
    if(bbmin.x < bbmax.x && bbmin.y < bbmax.y && bbmin.z < bbmax.z) makeblendundo(bbmin, bbmax);
    applyblendedit(args);
    client::editblend(args);
}

// an edit made by another client
void mpeditblend(const int *args)
{
    applyblendedit(args);
}

int paintingblendmap = 0, lastpaintblendmap = 0;

void paintblendmap(bool msg)
{
    if(!canpaintblendmap(true, false, msg)) return;

    BlendStamp st;
    if(!aimblendstamp(brushes[curbrush], st)) return;
    ivec bbmin, bbmax;
    blendstampbox(st, bbmin, bbmax);
    int pad = BM_EDGE_FILL<<BM_SCALE;
    bbmin.sub(pad).max(0);
    bbmax.add(pad).min(worldsize);
    // stamps in one stroke undo together
    makeblendundo(bbmin, bbmax, paintingblendmap);
    bool mp = multiplayer(false);
    BlendDelta delta;
    if(mp) blenddelta = &delta;
    stampblendmap(brushes[curbrush], st, blendpaintmode);
    blenddelta = NULL;
    if(!mp) return;
    vector<uchar> buf;
    delta.write(buf);
    // a stamp other players can't be sent is taken back, so everyone keeps the same paint
    if(!sendblendpaint(buf))
    {
        cancelblendundo();
        static int warned = 0;
        if(warned != paintingblendmap || msg) conoutf(colourred, "Blend paint too large to send to other players, try a smaller brush scale");
        warned = paintingblendmap;
    }
}

VAR(0, paintblendmapdelay, 1, 125, 3000);
VAR(0, paintblendmapinterval, 1, 50, 3000);

void stoppaintblendmap()
{
    paintingblendmap = 0;
    lastpaintblendmap = 0;
}

void trypaintblendmap()
{
    if(!paintingblendmap || totalmillis-paintingblendmap < paintblendmapdelay) return;
    if(lastpaintblendmap)
    {
        int diff = totalmillis - lastpaintblendmap;
        if(diff < paintblendmapinterval) return;
        lastpaintblendmap = (diff - diff%paintblendmapinterval) + lastpaintblendmap;
    }
    else lastpaintblendmap = totalmillis;
    paintblendmap(false);
}

ICOMMAND(0, paintblendmap, "D", (int *isdown),
{
    if(*isdown)
    {
        if(!paintingblendmap) { paintingblendmap = max(totalmillis, 1); paintblendmap(true); }
    }
    else stoppaintblendmap();
});

static void selblendbox(ivec &bbmin, ivec &bbmax)
{
    bbmin = sel.o;
    bbmax = ivec(sel.s).mul(sel.grid).add(sel.o);
}

static void blendseledit(int op)
{
    if(noedit(false)) return;
    ivec bbmin, bbmax;
    selblendbox(bbmin, bbmax);
    int args[BLENDEDIT_ARGS] = { op, bbmin.x, bbmin.y, bbmin.z, bbmax.x, bbmax.y, bbmax.z };
    localblendedit(args);
}

ICOMMAND(0, clearblendmapsel, "", (), blendseledit(BLENDEDIT_CLEARSEL));
ICOMMAND(0, invertblendmapsel, "", (), blendseledit(BLENDEDIT_INVERTSEL));

ICOMMAND(0, invertblendmap, "", (),
{
    if(noedit(false)) return;
    int args[BLENDEDIT_ARGS] = { BLENDEDIT_INVERT };
    localblendedit(args);
});

void showblendmap()
{
    if(noedit(false)) return;
    previewblends(ivec(0, 0, 0), ivec(worldsize, worldsize, worldsize));
}

COMMAND(0, showblendmap, "");
ICOMMAND(0, clearblendmap, "", (),
{
    if(noedit(true)) return;
    int args[BLENDEDIT_ARGS] = { BLENDEDIT_CLEAR };
    localblendedit(args);
});

static void moveblendmapcmd(int *dx, int *dy, int *dz)
{
    if(noedit(true)) return;
    int step = BM_IMAGE_SIZE<<BM_SCALE;
    if(*dx%step || *dy%step || *dz%step)
    {
        conoutf(colourred, "Blendmap movement must be in multiples of %d", step);
        return;
    }
    int args[BLENDEDIT_ARGS] = { BLENDEDIT_MOVE, *dx, *dy, *dz };
    localblendedit(args);
}
COMMANDN(0, moveblendmap, moveblendmapcmd, "iii");

// the brush preview projects the brush from the camera as a stamp does, over everything within reach
void renderblendbrush()
{
    if(!blendpaintmode || !brushes.inrange(curbrush)) return;

    BlendBrush *brush = brushes[curbrush];
    BlendStamp st;
    if(!aimblendstamp(brush, st)) return;
    if(!brush->tex) brush->gentex();
    ivec bbmin, bbmax;
    blendstampbox(st, bbmin, bbmax);
    renderblendbrush(brush->tex, st.dim, st.aim[st.dim], st.reach, st.r0, st.c0, st.w*st.texelsize, st.h*st.texelsize, bbmin, bbmax);
}

// size is the node's width in samples: images must be exactly an image wide, and branches wider
static bool loadblendmap(stream *f, uchar &type, BlendMapNode &node, int size, bool legacy = false)
{
    type = f->getchar();
    if((type == BM_IMAGE && size != (legacy ? BM_LEGACY_IMAGE_SIZE : BM_IMAGE_SIZE)) || (type == BM_BRANCH && size <= (legacy ? BM_LEGACY_IMAGE_SIZE : BM_IMAGE_SIZE)))
    {
        type = BM_SOLID;
        node.solid = &bmsolids[0xFF];
        return false;
    }
    if(legacy && type == BM_IMAGE)
    {
        // version 1 images were larger; split each into a subtree of today's
        uchar data[BM_LEGACY_IMAGE_SIZE*BM_LEGACY_IMAGE_SIZE];
        type = BM_SOLID;
        node.solid = &bmsolids[0xFF];
        if(f->read(data, sizeof(data)) != sizeof(data)) return false;
        blitblendmap(type, node, 0, 0, BM_LEGACY_IMAGE_SIZE, data, NULL, 0, 0, BM_LEGACY_IMAGE_SIZE, BM_LEGACY_IMAGE_SIZE, 1);
        optimizeblendmap(type, node);
        return true;
    }
    switch(type)
    {
        case BM_SOLID:
        {
            int val = f->getchar();
            if(val<0 || val>0xFF) return false;
            node.solid = &bmsolids[val];
            break;
        }

        case BM_IMAGE:
            node.image = new BlendMapImage;
            if(f->read(node.image->data, sizeof(node.image->data)) != sizeof(node.image->data))
                return false;
            break;

        case BM_BRANCH:
            node.branch = new BlendMapBranch;
            loopi(4) { node.branch->type[i] = BM_SOLID; node.branch->children[i].solid = &bmsolids[0xFF]; }
            loopi(4) if(!loadblendmap(f, node.branch->type[i], node.branch->children[i], size/2, legacy))
                return false;
            break;

        default:
            type = BM_SOLID;
            node.solid = &bmsolids[0xFF];
            return false;
    }
    return true;
}

// info is the map header's blendmap field: 1 is the top-down blendmap of older maps, converted once the map's
// geometry and textures are ready (convertblendmap); 2 is the slices, per axis a count then each slice's index and tree
bool loadblendmap(stream *f, int info)
{
    resetblendmap();
    if(info == 1)
    {
        blendmap.legacy = new BlendMapRoot;
        return loadblendmap(f, blendmap.legacy->type, *blendmap.legacy, blendmapsize(), true);
    }
    if(info != 2) return false;
    loopk(3)
    {
        int numslices = f->getlil<int>();
        if(numslices < 0 || numslices > numblendslices()+2) return false;
        loopi(numslices)
        {
            int slice = f->getlil<int>();
            // a slice given twice would leak the first
            if(findblendslice(k, slice)) return false;
            BlendMapRoot *root = makeblendslice(k, slice), discard;
            BlendMapRoot &dst = root ? *root : discard;
            bool loaded = loadblendmap(f, dst.type, dst, blendmapsize());
            if(!root) discard.cleanup();
            if(!loaded) return false;
        }
    }
    return true;
}

static void saveblendmap(stream *f, uchar type, BlendMapNode &node)
{
    f->putchar(type);
    switch(type)
    {
        case BM_SOLID:
            f->putchar(node.solid->val);
            break;

        case BM_IMAGE:
            f->write(node.image->data, sizeof(node.image->data));
            break;

        case BM_BRANCH:
            loopi(4) saveblendmap(f, node.branch->type[i], node.branch->children[i]);
            break;
    }
}

void saveblendmap(stream *f)
{
    loopk(3)
    {
        vector<BlendMapRoot *> &slices = blendmap.slices[k];
        int numslices = 0;
        loopv(slices) if(slices[i]) numslices++;
        f->putlil<int>(numslices);
        loopv(slices) if(slices[i])
        {
            f->putlil<int>(i-1);
            saveblendmap(f, slices[i]->type, *slices[i]);
        }
    }
}

uchar shouldsaveblendmap()
{
    loopk(3) loopv(blendmap.slices[k]) if(blendmap.slices[k][i]) return 2;
    return 0;
}
