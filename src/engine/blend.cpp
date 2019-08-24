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
#define BM_IMAGE_SIZE 64

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

    void shrink(int quadrant)
    {
        if(type == BM_BRANCH)
        {
            BlendMapRoot oldroot = *this;
            type = branch->shrink(*this, quadrant);
            oldroot.cleanup();
        }
    }
};

static BlendMapRoot blendmap;

struct BlendMapCache
{
    BlendMapRoot node;
    int scale;
    ivec2 origin;
};

BlendMapCache *newblendmapcache() { return new BlendMapCache; }

void freeblendmapcache(BlendMapCache *&cache) { delete cache; cache = NULL; }

bool setblendmaporigin(BlendMapCache *cache, const ivec &o, int size)
{
    if(blendmap.type!=BM_BRANCH)
    {
        cache->node = blendmap;
        cache->scale = worldscale-BM_SCALE;
        cache->origin = ivec2(0, 0);
        return cache->node.solid!=&bmsolids[0xFF];
    }

    BlendMapBranch *bm = blendmap.branch;
    int bmscale = worldscale-BM_SCALE, bmsize = 1<<bmscale,
        x = o.x>>BM_SCALE, y = o.y>>BM_SCALE,
        x1 = max(x-1, 0), y1 = max(y-1, 0),
        x2 = min(((o.x + size + (1<<BM_SCALE)-1)>>BM_SCALE) + 1, bmsize),
        y2 = min(((o.y + size + (1<<BM_SCALE)-1)>>BM_SCALE) + 1, bmsize),
        diff = (x1^x2)|(y1^y2);
    if(diff < bmsize) while(!(diff&(1<<(bmscale-1))))
    {
        bmscale--;
        int n = (((y1>>bmscale)&1)<<1) | ((x1>>bmscale)&1);
        if(bm->type[n]!=BM_BRANCH)
        {
            cache->node = BlendMapRoot(bm->type[n], bm->children[n]);
            cache->scale = bmscale;
            cache->origin = ivec2(x1&(~0U<<bmscale), y1&(~0U<<bmscale));
            return cache->node.solid!=&bmsolids[0xFF];
        }
        bm = bm->children[n].branch;
    }

    cache->node.type = BM_BRANCH;
    cache->node.branch = bm;
    cache->scale = bmscale;
    cache->origin = ivec2(x1&(~0U<<bmscale), y1&(~0U<<bmscale));
    return true;
}

bool hasblendmap(BlendMapCache *cache)
{
    return cache->node.solid!=&bmsolids[0xFF];
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

uchar lookupblendmap(BlendMapCache *cache, const vec &pos)
{
    if(cache->node.type==BM_SOLID) return cache->node.solid->val;

    uchar vals[4], *val = vals;
    float bx = pos.x/(1<<BM_SCALE) - 0.5f, by = pos.y/(1<<BM_SCALE) - 0.5f;
    int ix = (int)floor(bx), iy = (int)floor(by),
        rx = ix-cache->origin.x, ry = iy-cache->origin.y;
    loop(vy, 2) loop(vx, 2)
    {
        int cx = clamp(rx+vx, 0, (1<<cache->scale)-1), cy = clamp(ry+vy, 0, (1<<cache->scale)-1);
        if(cache->node.type==BM_IMAGE)
            *val++ = cache->node.image->data[cy*BM_IMAGE_SIZE + cx];
        else *val++ = lookupblendmap(cx, cy, cache->node.branch, cache->scale);
    }
    float fx = bx - ix, fy = by - iy;
    return uchar((1-fy)*((1-fx)*vals[0] + fx*vals[1]) +
                 fy*((1-fx)*vals[2] + fx*vals[3]));
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

void fillblendmap(int x, int y, int w, int h, uchar val)
{
    int bmsize = worldsize>>BM_SCALE,
        x1 = clamp(x, 0, bmsize),
        y1 = clamp(y, 0, bmsize),
        x2 = clamp(x+w, 0, bmsize),
        y2 = clamp(y+h, 0, bmsize);
    if(max(x1, y1) >= bmsize || min(x2, y2) <= 0 || x1>=x2 || y1>=y2) return;
    fillblendmap(blendmap.type, blendmap, bmsize, val, x1, y1, x2, y2);
}

static void invertblendmap(uchar &type, BlendMapNode &node, int size, int x1, int y1, int x2, int y2)
{
    if(type==BM_BRANCH)
    {
        size /= 2;
        if(y1 < size)
        {
            if(x1 < size) invertblendmap(node.branch->type[0], node.branch->children[0], size,
                                        x1, y1, min(x2, size), min(y2, size));
            if(x2 > size) invertblendmap(node.branch->type[1], node.branch->children[1], size,
                                        max(x1-size, 0), y1, x2-size, min(y2, size));
        }
        if(y2 > size)
        {
            if(x1 < size) invertblendmap(node.branch->type[2], node.branch->children[2], size,
                                        x1, max(y1-size, 0), min(x2, size), y2-size);
            if(x2 > size) invertblendmap(node.branch->type[3], node.branch->children[3], size,
                                        max(x1-size, 0), max(y1-size, 0), x2-size, y2-size);
        }
        return;
    }
    else if(type==BM_SOLID)
    {
        fillblendmap(type, node, size, 255-node.solid->val, x1, y1, x2, y2);
    }
    else if(type==BM_IMAGE)
    {
        uchar *dst = &node.image->data[y1*BM_IMAGE_SIZE + x1];
        loopi(y2-y1)
        {
            loopj(x2-x1) dst[j] = 255-dst[j];
            dst += BM_IMAGE_SIZE;
        }
    }
}

void invertblendmap(int x, int y, int w, int h)
{
    int bmsize = worldsize>>BM_SCALE,
        x1 = clamp(x, 0, bmsize),
        y1 = clamp(y, 0, bmsize),
        x2 = clamp(x+w, 0, bmsize),
        y2 = clamp(y+h, 0, bmsize);
    if(max(x1, y1) >= bmsize || min(x2, y2) <= 0 || x1>=x2 || y1>=y2) return;
    invertblendmap(blendmap.type, blendmap, bmsize, x1, y1, x2, y2);
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
    optimizeblendmap(blendmap.type, blendmap);
}

ICOMMAND(0, optimizeblendmap, "", (), optimizeblendmap());

VARF(0, blendpaintmode, 0, 0, 5,
{
    if(!blendpaintmode) stoppaintblendmap();
});

static void blitblendmap(uchar &type, BlendMapNode &node, int bmx, int bmy, int bmsize, uchar *src, int sx, int sy, int sw, int sh, int smode)
{
    if(type==BM_BRANCH)
    {
        bmsize /= 2;
        if(sy < bmy + bmsize)
        {
            if(sx < bmx + bmsize) blitblendmap(node.branch->type[0], node.branch->children[0], bmx, bmy, bmsize, src, sx, sy, sw, sh, smode);
            if(sx + sw > bmx + bmsize) blitblendmap(node.branch->type[1], node.branch->children[1], bmx+bmsize, bmy, bmsize, src, sx, sy, sw, sh, smode);
        }
        if(sy + sh > bmy + bmsize)
        {
            if(sx < bmx + bmsize) blitblendmap(node.branch->type[2], node.branch->children[2], bmx, bmy+bmsize, bmsize, src, sx, sy, sw, sh, smode);
            if(sx + sw > bmx + bmsize) blitblendmap(node.branch->type[3], node.branch->children[3], bmx+bmsize, bmy+bmsize, bmsize, src, sx, sy, sw, sh, smode);
        }
        return;
    }
    if(type==BM_SOLID)
    {
        uchar val = node.solid->val;
        if(bmsize > BM_IMAGE_SIZE)
        {
            node.splitsolid(type, val);
            blitblendmap(type, node, bmx, bmy, bmsize, src, sx, sy, sw, sh, smode);
            return;
        }

        type = BM_IMAGE;
        node.image = new BlendMapImage;
        memset(node.image->data, val, sizeof(node.image->data));
    }

    int x1 = clamp(sx - bmx, 0, bmsize), y1 = clamp(sy - bmy, 0, bmsize),
        x2 = clamp(sx+sw - bmx, 0, bmsize), y2 = clamp(sy+sh - bmy, 0, bmsize);
    uchar *dst = &node.image->data[y1*BM_IMAGE_SIZE + x1];
    src += max(bmy - sy, 0)*sw + max(bmx - sx, 0);
    loopi(y2-y1)
    {
        switch(smode)
        {
            case 1:
                memcpy(dst, src, x2 - x1);
                break;

            case 2:
                loopi(x2 - x1) dst[i] = min(dst[i], src[i]);
                break;

            case 3:
                loopi(x2 - x1) dst[i] = max(dst[i], src[i]);
                break;

            case 4:
                loopi(x2 - x1) dst[i] = min(dst[i], uchar(0xFF - src[i]));
                break;

            case 5:
                loopi(x2 - x1) dst[i] = max(dst[i], uchar(0xFF - src[i]));
                break;
        }
        dst += BM_IMAGE_SIZE;
        src += sw;
    }
}

void blitblendmap(uchar *src, int sx, int sy, int sw, int sh, int smode)
{
    int bmsize = worldsize>>BM_SCALE;
    if(max(sx, sy) >= bmsize || min(sx+sw, sy+sh) <= 0 || min(sw, sh) <= 0) return;
    blitblendmap(blendmap.type, blendmap, 0, 0, bmsize, src, sx, sy, sw, sh, smode);
}

void resetblendmap()
{
    clearblendtextures();
    blendmap.cleanup();
    blendmap.type = BM_SOLID;
    blendmap.solid = &bmsolids[0xFF];
}

void enlargeblendmap()
{
    if(blendmap.type == BM_SOLID) return;
    BlendMapBranch *branch = new BlendMapBranch;
    branch->type[0] = blendmap.type;
    branch->children[0] = blendmap;
    loopi(3)
    {
        branch->type[i+1] = BM_SOLID;
        branch->children[i+1].solid = &bmsolids[0xFF];
    }
    blendmap.type = BM_BRANCH;
    blendmap.branch = branch;
}

void shrinkblendmap(int octant)
{
    blendmap.shrink(octant&3);
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

int calcblendlayer(int x1, int y1, int x2, int y2)
{
    int bmsize = worldsize>>BM_SCALE,
        ux1 = max(x1, 0) >> BM_SCALE,
        ux2 = (min(x2, worldsize) + (1<<BM_SCALE)-1) >> BM_SCALE,
        uy1 = max(y1, 0) >> BM_SCALE,
        uy2 = (min(y2, worldsize) + (1<<BM_SCALE)-1) >> BM_SCALE;
    if(ux1 >= ux2 || uy1 >= uy2) return LAYER_TOP;
    return calcblendlayer(blendmap.type, blendmap, 0, 0, bmsize, ux1, uy1, ux2-ux1, uy2-uy1);
}

void moveblendmap(uchar type, BlendMapNode &node, int size, int x, int y, int dx, int dy)
{
    if(type == BM_BRANCH)
    {
        size /= 2;
        moveblendmap(node.branch->type[0], node.branch->children[0], size, x, y, dx, dy);
        moveblendmap(node.branch->type[1], node.branch->children[1], size, x + size, y, dx, dy);
        moveblendmap(node.branch->type[2], node.branch->children[2], size, x, y + size, dx, dy);
        moveblendmap(node.branch->type[3], node.branch->children[3], size, x + size, y + size, dx, dy);
        return;
    }
    else if(type == BM_SOLID)
    {
        fillblendmap(x+dx, y+dy, size, size, node.solid->val);
    }
    else if(type == BM_IMAGE)
    {
        blitblendmap(node.image->data, x+dx, y+dy, size, size, 1);
    }
}

void moveblendmap(int dx, int dy)
{
    BlendMapRoot old = blendmap;
    blendmap.type = BM_SOLID;
    blendmap.solid = &bmsolids[0xFF];
    moveblendmap(old.type, old, worldsize>>BM_SCALE, 0, 0, dx, dy);
    old.cleanup();
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

VARF(IDF_PERSIST, blendtexsize, 0, 9, 12-BM_SCALE, { clearblendtextures(); updateblendtextures(); });

struct BlendTexture
{
    int x, y, size;
    uchar *data;
    GLuint tex;
    GLenum format;
    bool valid;

    BlendTexture() : x(0), y(0), size(0), data(NULL), tex(0), format(GL_FALSE), valid(false)
    {}

    ~BlendTexture()
    {
        cleanup();
    }

    bool setup(int sz)
    {
        if(!tex) glGenTextures(1, &tex);
        sz = min(sz, maxtexsize ? min(maxtexsize, hwtexsize) : hwtexsize);
        while(sz&(sz-1)) sz &= sz-1;
        if(sz == size) return false;
        size = sz;
        if(data) delete[] data;
        data = new uchar[size*size];
        format = hasTRG ? GL_RED : GL_LUMINANCE;
        createtexture(tex, size, size, NULL, 3, 1, hasTRG ? GL_R8 : GL_LUMINANCE8);
        valid = false;
        return true;
    }

    void cleanup()
    {
        if(tex) { glDeleteTextures(1, &tex); tex = 0; }
        if(data) { delete[] data; data = NULL; }
        size = 0;
        valid = false;
    }

    bool contains(int px, int py) const
    {
        return px >= x && py >= y && px < x + 0x1000 && py < y + 0x1000;
    }

    bool contains(const ivec &p) const { return contains(p.x, p.y); }
};

static vector<BlendTexture> blendtexs;

void dumpblendtexs()
{
    loopv(blendtexs)
    {
        BlendTexture &bt = blendtexs[i];
        if(!bt.size || !bt.valid) continue;
        ImageData temp(bt.size, bt.size, 1, blendtexs[i].data);
        const char *map = mapname, *name = strrchr(map, '/');
        defformatstring(buf, "blendtex_%s_%d.png", name ? name+1 : map, i);
        savepng(buf, temp, true);
    }
}

COMMAND(0, dumpblendtexs, "");

static void renderblendtexture(uchar &type, BlendMapNode &node, int bmx, int bmy, int bmsize, uchar *dst, int dsize, int dx, int dy, int dw, int dh)
{
    if(type==BM_BRANCH)
    {
        bmsize /= 2;
        if(dy < bmy + bmsize)
        {
            if(dx < bmx + bmsize) renderblendtexture(node.branch->type[0], node.branch->children[0], bmx, bmy, bmsize, dst, dsize, dx, dy, dw, dh);
            if(dx + dw > bmx + bmsize) renderblendtexture(node.branch->type[1], node.branch->children[1], bmx+bmsize, bmy, bmsize, dst, dsize, dx, dy, dw, dh);
        }
        if(dy + dh > bmy + bmsize)
        {
            if(dx < bmx + bmsize) renderblendtexture(node.branch->type[2], node.branch->children[2], bmx, bmy+bmsize, bmsize, dst, dsize, dx, dy, dw, dh);
            if(dx + dw > bmx + bmsize) renderblendtexture(node.branch->type[3], node.branch->children[3], bmx+bmsize, bmy+bmsize, bmsize, dst, dsize, dx, dy, dw, dh);
        }
        return;
    }

    int x1 = clamp(dx - bmx, 0, bmsize), y1 = clamp(dy - bmy, 0, bmsize),
        x2 = clamp(dx+dw - bmx, 0, bmsize), y2 = clamp(dy+dh - bmy, 0, bmsize),
        tsize = 1<<(min(worldscale, 12)-BM_SCALE),
        step = tsize/dsize, stepw = (x2 - x1)/step, steph = (y2 - y1)/step;
    dst += max(bmy - dy, 0)/step*dsize + max(bmx - dx, 0)/step;
    if(type == BM_SOLID) loopi(steph)
    {
        memset(dst, node.solid->val, stepw);
        dst += dsize;
    }
    else
    {
        uchar *src = &node.image->data[y1*BM_IMAGE_SIZE + x1];
        loopi(steph)
        {
            if(step <= 1) memcpy(dst, src, stepw);
            else for(int j = 0, k = 0; j < stepw; j++, k += step) dst[j] = src[k];
            src += step*BM_IMAGE_SIZE;
            dst += dsize;
        }
    }
}

void renderblendtexture(uchar *dst, int dsize, int dx, int dy, int dw, int dh)
{
    int bmsize = worldsize>>BM_SCALE;
    if(max(dx, dy) >= bmsize || min(dx+dw, dy+dh) <= 0 || min(dw, dh) <= 0) return;
    renderblendtexture(blendmap.type, blendmap, 0, 0, bmsize, dst, dsize, dx, dy, dw, dh);
}

static bool usesblendmap(uchar &type, BlendMapNode &node, int bmx, int bmy, int bmsize, int ux, int uy, int uw, int uh)
{
    if(type==BM_BRANCH)
    {
        bmsize /= 2;
        if(uy < bmy + bmsize)
        {
            if(ux < bmx + bmsize && usesblendmap(node.branch->type[0], node.branch->children[0], bmx, bmy, bmsize, ux, uy, uw, uh))
                return true;
            if(ux + uw > bmx + bmsize && usesblendmap(node.branch->type[1], node.branch->children[1], bmx+bmsize, bmy, bmsize, ux, uy, uw, uh))
                return true;
        }
        if(uy + uh > bmy + bmsize)
        {
            if(ux < bmx + bmsize && usesblendmap(node.branch->type[2], node.branch->children[2], bmx, bmy+bmsize, bmsize, ux, uy, uw, uh))
                return true;
            if(ux + uw > bmx + bmsize && usesblendmap(node.branch->type[3], node.branch->children[3], bmx+bmsize, bmy+bmsize, bmsize, ux, uy, uw, uh))
                return true;
        }
        return false;
    }
    return type!=BM_SOLID || node.solid->val != 0xFF;
}

bool usesblendmap(int x1, int y1, int x2, int y2)
{
    int bmsize = worldsize>>BM_SCALE,
        ux1 = max(x1, 0) >> BM_SCALE,
        ux2 = (min(x2, worldsize) + (1<<BM_SCALE)-1) >> BM_SCALE,
        uy1 = max(y1, 0) >> BM_SCALE,
        uy2 = (min(y2, worldsize) + (1<<BM_SCALE)-1) >> BM_SCALE;
    if(ux1 >= ux2 || uy1 >= uy2) return false;
    return usesblendmap(blendmap.type, blendmap, 0, 0, bmsize, ux1, uy1, ux2-ux1, uy2-uy1);
}

void bindblendtexture(const ivec &p)
{
    loopv(blendtexs) if(blendtexs[i].contains(p))
    {
        BlendTexture &bt = blendtexs[i];
        int tsize = 1<<min(worldscale, 12);
        GLOBALPARAMF(blendmapparams, bt.x, bt.y, 1.0f/tsize, 1.0f/tsize);
        glBindTexture(GL_TEXTURE_2D, bt.tex);
        break;
    }
}

static void updateblendtextures(uchar &type, BlendMapNode &node, int bmx, int bmy, int bmsize, int ux, int uy, int uw, int uh)
{
    if(type==BM_BRANCH)
    {
        if(bmsize > 0x1000>>BM_SCALE)
        {
            bmsize /= 2;
            if(uy < bmy + bmsize)
            {
                if(ux < bmx + bmsize) updateblendtextures(node.branch->type[0], node.branch->children[0], bmx, bmy, bmsize, ux, uy, uw, uh);
                if(ux + uw > bmx + bmsize) updateblendtextures(node.branch->type[1], node.branch->children[1], bmx+bmsize, bmy, bmsize, ux, uy, uw, uh);
            }
            if(uy + uh > bmy + bmsize)
            {
                if(ux < bmx + bmsize) updateblendtextures(node.branch->type[2], node.branch->children[2], bmx, bmy+bmsize, bmsize, ux, uy, uw, uh);
                if(ux + uw > bmx + bmsize) updateblendtextures(node.branch->type[3], node.branch->children[3], bmx+bmsize, bmy+bmsize, bmsize, ux, uy, uw, uh);
            }
            return;
        }
        if(!usesblendmap(type, node, bmx, bmy, bmsize, ux, uy, uw, uh)) return;
    }
    else if(type==BM_SOLID && node.solid->val == 0xFF) return;

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    int tx1 = max(bmx, ux)&~((0x1000>>BM_SCALE)-1),
        tx2 = (min(bmx+bmsize, ux+uw) + (0x1000>>BM_SCALE)-1)&~((0x1000>>BM_SCALE)-1),
        ty1 = max(bmy, uy)&~((0x1000>>BM_SCALE)-1),
        ty2 = (min(bmy+bmsize, uy+uh) + (0x1000>>BM_SCALE)-1)&~((0x1000>>BM_SCALE)-1);
    for(int ty = ty1; ty < ty2; ty += 0x1000>>BM_SCALE) for(int tx = tx1; tx < tx2; tx += 0x1000>>BM_SCALE)
    {
        BlendTexture *bt = NULL;
        loopv(blendtexs) if(blendtexs[i].contains(tx<<BM_SCALE, ty<<BM_SCALE)) { bt = &blendtexs[i]; break; }
        if(!bt)
        {
            bt = &blendtexs.add();
            bt->x = tx<<BM_SCALE;
            bt->y = ty<<BM_SCALE;
        }
        bt->setup(1<<min(worldscale-BM_SCALE, blendtexsize));
        int tsize = 1<<(min(worldscale, 12)-BM_SCALE),
            ux1 = tx, ux2 = tx + tsize, uy1 = ty, uy2 = ty + tsize,
            step = tsize/bt->size;
        if(!bt->valid)
        {
            ux1 = max(ux1, ux&~(step-1));
            ux2 = min(ux2, (ux+uw+step-1)&~(step-1));
            uy1 = max(uy1, uy&~(step-1));
            uy2 = min(uy2, (uy+uh+step-1)&~(step-1));
            bt->valid = true;
        }
        uchar *data = bt->data + (uy1-ty)/step*bt->size + (ux1-tx)/step;
        renderblendtexture(type, node, bmx, bmy, bmsize, data, bt->size, ux1, uy1, ux2-ux1, uy2-uy1);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, bt->size);
        glBindTexture(GL_TEXTURE_2D, bt->tex);
        glTexSubImage2D(GL_TEXTURE_2D, 0, (ux1-tx)/step, (uy1-ty)/step, (ux2-ux1)/step, (uy2-uy1)/step, bt->format, GL_UNSIGNED_BYTE, data);
    }

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}

void updateblendtextures(int x1, int y1, int x2, int y2)
{
    int bmsize = worldsize>>BM_SCALE,
        ux1 = max(x1, 0) >> BM_SCALE,
        ux2 = (min(x2, worldsize) + (1<<BM_SCALE)-1) >> BM_SCALE,
        uy1 = max(y1, 0) >> BM_SCALE,
        uy2 = (min(y2, worldsize) + (1<<BM_SCALE)-1) >> BM_SCALE;
    if(ux1 >= ux2 || uy1 >= uy2) return;
    updateblendtextures(blendmap.type, blendmap, 0, 0, bmsize, ux1, uy1, ux2-ux1, uy2-uy1);
}

void clearblendtextures()
{
    loopv(blendtexs) blendtexs[i].cleanup();
    blendtexs.shrink(0);
}

void cleanupblendmap()
{
    loopv(brushes) brushes[i]->cleanup();
    loopv(blendtexs) blendtexs[i].cleanup();
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
    if(!loadimage(imgname, s)) { conoutf("\frCould not load blend brush image %s", imgname); return; }
    if(max(s.w, s.h) > (1<<12))
    {
        conoutf("\frBlend brush image size exceeded %dx%d pixels: %s", 1<<12, 1<<12, imgname);
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

ICOMMAND(0, nextblendbrush, "i", (int *dir),
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
        else curbrush = -1;
    }
    else if(*numargs < 0) intret(curbrush);
    else printvar(id, curbrush, brushes.inrange(curbrush) ? brushes[curbrush]->name : "");
});

bool canpaintblendmap(bool brush = true, bool sel = false, bool msg = true)
{
    if(noedit(!sel, msg) || (nompedit && multiplayer())) return false;
    if(!blendpaintmode)
    {
        conoutft(CON_DEBUG, "\frOperation only allowed in blend paint mode");
        return false;
    }
    if(brush && !brushes.inrange(curbrush))
    {
        if(msg) conoutft(CON_DEBUG, "\frNo blend brush selected");
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

void paintblendmap(bool msg)
{
    if(!canpaintblendmap(true, false, msg)) return;

    BlendBrush *brush = brushes[curbrush];
    int x = (int)floor(clamp(worldpos.x, 0.0f, float(worldsize))/(1<<BM_SCALE) - 0.5f*brush->w),
        y = (int)floor(clamp(worldpos.y, 0.0f, float(worldsize))/(1<<BM_SCALE) - 0.5f*brush->h);
    blitblendmap(brush->data, x, y, brush->w, brush->h, blendpaintmode);
    previewblends(ivec((x-1)<<BM_SCALE, (y-1)<<BM_SCALE, 0),
                  ivec((x+brush->w+1)<<BM_SCALE, (y+brush->h+1)<<BM_SCALE, worldsize));
}

VAR(0, paintblendmapdelay, 1, 125, 3000);
VAR(0, paintblendmapinterval, 1, 50, 3000);

int paintingblendmap = 0, lastpaintblendmap = 0;

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
        if(!paintingblendmap) { paintblendmap(true); paintingblendmap = totalmillis; }
    }
    else stoppaintblendmap();
});

void clearblendmapsel()
{
    if(noedit(false) || (nompedit && multiplayer())) return;
    int x1 = sel.o.x>>BM_SCALE, y1 = sel.o.y>>BM_SCALE,
        x2 = (sel.o.x+sel.s.x*sel.grid+(1<<BM_SCALE)-1)>>BM_SCALE,
        y2 = (sel.o.y+sel.s.y*sel.grid+(1<<BM_SCALE)-1)>>BM_SCALE;
    fillblendmap(x1, y1, x2-x1, y2-y1, 0xFF);
    previewblends(ivec(x1<<BM_SCALE, y1<<BM_SCALE, 0),
                  ivec(x2<<BM_SCALE, y2<<BM_SCALE, worldsize));
}

COMMAND(0, clearblendmapsel, "");

void invertblendmapsel()
{
    if(noedit(false) || (nompedit && multiplayer())) return;
    int x1 = sel.o.x>>BM_SCALE, y1 = sel.o.y>>BM_SCALE,
        x2 = (sel.o.x+sel.s.x*sel.grid+(1<<BM_SCALE)-1)>>BM_SCALE,
        y2 = (sel.o.y+sel.s.y*sel.grid+(1<<BM_SCALE)-1)>>BM_SCALE;
    invertblendmap(x1, y1, x2-x1, y2-y1);
    previewblends(ivec(x1<<BM_SCALE, y1<<BM_SCALE, 0),
                  ivec(x2<<BM_SCALE, y2<<BM_SCALE, worldsize));
}

COMMAND(0, invertblendmapsel, "");

ICOMMAND(0, invertblendmap, "", (),
{
    if(noedit(false) || (nompedit && multiplayer())) return;
    invertblendmap(0, 0, worldsize>>BM_SCALE, worldsize>>BM_SCALE);
    previewblends(ivec(0, 0, 0), ivec(worldsize, worldsize, worldsize));
});

void showblendmap()
{
    if(noedit(false) || (nompedit && multiplayer())) return;
    previewblends(ivec(0, 0, 0), ivec(worldsize, worldsize, worldsize));
}

COMMAND(0, showblendmap, "");
ICOMMAND(0, clearblendmap, "", (),
{
    if(noedit(true) || (nompedit && multiplayer())) return;
    resetblendmap();
    showblendmap();
});

ICOMMAND(0, moveblendmap, "ii", (int *dx, int *dy),
{
    if(noedit(true) || (nompedit && multiplayer())) return;
    if(*dx%(BM_IMAGE_SIZE<<BM_SCALE) || *dy%(BM_IMAGE_SIZE<<BM_SCALE))
    {
        conoutf("^frBlendmap movement must be in multiples of %d", BM_IMAGE_SIZE<<BM_SCALE);
        return;
    }
    if(*dx <= -worldsize || *dx >= worldsize || *dy <= -worldsize || *dy >= worldsize)
        resetblendmap();
    else
        moveblendmap(*dx>>BM_SCALE, *dy>>BM_SCALE);
    showblendmap();
});

void renderblendbrush()
{
    if(!blendpaintmode || !brushes.inrange(curbrush)) return;

    BlendBrush *brush = brushes[curbrush];
    int x1 = (int)floor(clamp(worldpos.x, 0.0f, float(worldsize))/(1<<BM_SCALE) - 0.5f*brush->w) << BM_SCALE,
        y1 = (int)floor(clamp(worldpos.y, 0.0f, float(worldsize))/(1<<BM_SCALE) - 0.5f*brush->h) << BM_SCALE,
        x2 = x1 + (brush->w << BM_SCALE),
        y2 = y1 + (brush->h << BM_SCALE);

    if(max(x1, y1) >= worldsize || min(x2, y2) <= 0 || x1>=x2 || y1>=y2) return;

    if(!brush->tex) brush->gentex();
    renderblendbrush(brush->tex, x1, y1, x2 - x1, y2 - y1);
}

bool loadblendmap(stream *f, uchar &type, BlendMapNode &node)
{
    type = f->getchar();
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
            loopi(4) if(!loadblendmap(f, node.branch->type[i], node.branch->children[i]))
                return false;
            break;

        default:
            type = BM_SOLID;
            node.solid = &bmsolids[0xFF];
            return false;
    }
    return true;
}

bool loadblendmap(stream *f, int info)
{
    resetblendmap();
    return loadblendmap(f, blendmap.type, blendmap);
}

void saveblendmap(stream *f, uchar type, BlendMapNode &node)
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
    saveblendmap(f, blendmap.type, blendmap);
}

uchar shouldsaveblendmap()
{
    return blendmap.solid!=&bmsolids[0xFF] ? 1 : 0;
}

