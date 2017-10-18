struct stainrenderer;

struct BIH
{
    struct node
    {
        short split[2];
        ushort child[2];

        int axis() const { return child[0]>>14; }
        int childindex(int which) const { return child[which]&0x3FFF; }
        bool isleaf(int which) const { return (child[1]&(1<<(14+which)))!=0; }
    };

    struct tri
    {
        ushort vert[3];
    };

    struct tribb
    {
        svec center, radius;

        bool outside(const ivec &bo, const ivec &br) const
        {
            return abs(bo.x - center.x) > br.x + radius.x ||
                   abs(bo.y - center.y) > br.y + radius.y ||
                   abs(bo.z - center.z) > br.z + radius.z;
        }
    };

    enum { MESH_RENDER = 1<<1, MESH_NOCLIP = 1<<2, MESH_ALPHA = 1<<3, MESH_COLLIDE = 1<<4, MESH_CULLFACE = 1<<5 };

    struct mesh
    {
        matrix4x3 xform, invxform;
        matrix3 xformnorm, invxformnorm;
        float scale, invscale;
        node *nodes;
        int numnodes;
        const tri *tris;
        const tribb *tribbs;
        int numtris;
        const uchar *pos, *tc;
        int posstride, tcstride;
        Texture *tex;
        int flags;
        vec bbmin, bbmax;

        mesh() : numnodes(0), numtris(0), tex(NULL), flags(0) {}

        vec getpos(int i) const { return *(const vec *)(pos + i*posstride); }
        vec2 gettc(int i) const { return *(const vec2 *)(tc + i*tcstride); }
    };

    mesh *meshes;
    int nummeshes;
    node *nodes;
    int numnodes;
    tribb *tribbs;
    int numtris;
    vec bbmin, bbmax, center;
    float radius, entradius;

    BIH(vector<mesh> &buildmeshes);

    ~BIH();

    void build(mesh &m, ushort *indices, int numindices, const ivec &vmin, const ivec &vmax);

    bool traverse(const vec &o, const vec &ray, float maxdist, float &dist, int mode);
    bool traverse(const mesh &m, const vec &o, const vec &ray, const vec &invray, float maxdist, float &dist, int mode, node *curnode, float tmin, float tmax);
    bool triintersect(const mesh &m, int tidx, const vec &mo, const vec &mray, float maxdist, float &dist, int mode);

    bool boxcollide(physent *d, const vec &dir, float cutoff, const vec &o, int yaw, int pitch, int roll, float scale = 1);
    bool ellipsecollide(physent *d, const vec &dir, float cutoff, const vec &o, int yaw, int pitch, int roll, float scale = 1);

    template<int C>
    void collide(const mesh &m, physent *d, const vec &dir, float cutoff, const vec &center, const vec &radius, const matrix4x3 &orient, float &dist, node *curnode, const ivec &bo, const ivec &br);
    template<int C>
    void tricollide(const mesh &m, int tidx, physent *d, const vec &dir, float cutoff, const vec &center, const vec &radius, const matrix4x3 &orient, float &dist, const ivec &bo, const ivec &br);

    void genstaintris(stainrenderer *s, const vec &staincenter, float stainradius, const vec &o, int yaw, int pitch, int roll, float scale = 1);
    void genstaintris(stainrenderer *s, const mesh &m, const vec &center, float radius, const matrix4x3 &orient, node *curnode, const ivec &bo, const ivec &br);
    void genstaintris(stainrenderer *s, const mesh &m, int tidx, const vec &center, float radius, const matrix4x3 &orient, const ivec &bo, const ivec &br);
 
    void preload();
};

extern bool mmintersect(const extentity &e, const vec &o, const vec &ray, float maxdist, int mode, float &dist);

