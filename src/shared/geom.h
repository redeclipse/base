struct vec;
struct vec4;

struct vec2
{
    union
    {
        struct { float x, y; };
        float v[2];
    };

    vec2() {}
    vec2(float x, float y) : x(x), y(y) {}
    explicit vec2(const vec &v);
    explicit vec2(const vec4 &v);

    float &operator[](int i)       { return v[i]; }
    float  operator[](int i) const { return v[i]; }

    bool operator==(const vec2 &o) const { return x == o.x && y == o.y; }
    bool operator!=(const vec2 &o) const { return x != o.x || y != o.y; }

    bool iszero() const { return x==0 && y==0; }
    float dot(const vec2 &o) const  { return x*o.x + y*o.y; }
    float squaredlen() const { return dot(*this); }
    float magnitude() const  { return sqrtf(squaredlen()); }
    vec2 &normalize() { mul(1/magnitude()); return *this; }
    vec2 &safenormalize() { float m = magnitude(); if(m) mul(1/m); return *this; }
    float cross(const vec2 &o) const { return x*o.y - y*o.x; }
    float squaredist(const vec2 &e) const { return vec2(*this).sub(e).squaredlen(); }
    float dist(const vec2 &e) const { return sqrtf(squaredist(e)); }

    vec2 &mul(float f)       { x *= f; y *= f; return *this; }
    vec2 &mul(const vec2 &o) { x *= o.x; y *= o.y; return *this; }
    vec2 &square()           { mul(*this); return *this; }
    vec2 &div(float f)       { x /= f; y /= f; return *this; }
    vec2 &div(const vec2 &o) { x /= o.x; y /= o.y; return *this; }
    vec2 &recip()            { x = 1/x; y = 1/y; return *this; }
    vec2 &add(float f)       { x += f; y += f; return *this; }
    vec2 &add(const vec2 &o) { x += o.x; y += o.y; return *this; }
    vec2 &sub(float f)       { x -= f; y -= f; return *this; }
    vec2 &sub(const vec2 &o) { x -= o.x; y -= o.y; return *this; }
    vec2 &neg()              { x = -x; y = -y; return *this; }
    vec2 &min(const vec2 &o) { x = ::min(x, o.x); y = ::min(y, o.y); return *this; }
    vec2 &max(const vec2 &o) { x = ::max(x, o.x); y = ::max(y, o.y); return *this; }
    vec2 &min(float f)       { x = ::min(x, f); y = ::min(y, f); return *this; }
    vec2 &max(float f)       { x = ::max(x, f); y = ::max(y, f); return *this; }
    vec2 &abs() { x = fabs(x); y = fabs(y); return *this; }
    vec2 &clamp(float l, float h) { x = ::clamp(x, l, h); y = ::clamp(y, l, h); return *this; }
    vec2 &reflect(const vec2 &n) { float k = 2*dot(n); x -= k*n.x; y -= k*n.y; return *this; }
    vec2 &lerp(const vec2 &b, float t) { x += (b.x-x)*t; y += (b.y-y)*t; return *this; }
    vec2 &lerp(const vec2 &a, const vec2 &b, float t) { x = a.x + (b.x-a.x)*t; y = a.y + (b.y-a.y)*t; return *this; }
    vec2 &avg(const vec2 &b) { add(b); mul(0.5f); return *this; }
    template<class B> vec2 &madd(const vec2 &a, const B &b) { return add(vec2(a).mul(b)); }
    template<class B> vec2 &msub(const vec2 &a, const B &b) { return sub(vec2(a).mul(b)); }

    vec2 &rotate_around_z(float c, float s) { float rx = x, ry = y; x = c*rx-s*ry; y = c*ry+s*rx; return *this; }
    vec2 &rotate_around_z(float angle) { return rotate_around_z(cosf(angle), sinf(angle)); }
    vec2 &rotate_around_z(const vec2 &sc) { return rotate_around_z(sc.x, sc.y); }
};

static inline bool htcmp(const vec2 &x, const vec2 &y)
{
    return x == y;
}

static inline uint hthash(const vec2 &k)
{
    union { uint i; float f; } x, y;
    x.f = k.x; y.f = k.y;
    uint v = x.i^y.i;
    return v + (v>>12);
}

struct ivec;
struct usvec;
struct svec;

struct vec
{
    union
    {
        struct { float x, y, z; };
        struct { float r, g, b; };
        float v[3];
    };

    vec() {}
    explicit vec(int a) : x(a), y(a), z(a) {}
    explicit vec(float a) : x(a), y(a), z(a) {}
    vec(float a, float b, float c) : x(a), y(b), z(c) {}
    explicit vec(int v[3]) : x(v[0]), y(v[1]), z(v[2]) {}
    explicit vec(const float *v) : x(v[0]), y(v[1]), z(v[2]) {}
    explicit vec(const vec2 &v, float z = 0) : x(v.x), y(v.y), z(z) {}
    explicit vec(const vec4 &v);
    explicit vec(const ivec &v);
    explicit vec(const usvec &v);
    explicit vec(const svec &v);

    vec(float yaw, float pitch) : x(-sinf(yaw)*cosf(pitch)), y(cosf(yaw)*cosf(pitch)), z(sinf(pitch)) {}

    float &operator[](int i)       { return v[i]; }
    float  operator[](int i) const { return v[i]; }

    vec &set(int i, float f) { v[i] = f; return *this; }

    bool operator==(const vec &o) const { return x == o.x && y == o.y && z == o.z; }
    bool operator!=(const vec &o) const { return x != o.x || y != o.y || z != o.z; }

    bool iszero() const { return x==0 && y==0 && z==0; }
    float squaredlen() const { return x*x + y*y + z*z; }
    float dot2(const vec2 &o) const { return x*o.x + y*o.y; }
    float dot2(const vec &o) const { return x*o.x + y*o.y; }
    float dot(const vec &o) const { return x*o.x + y*o.y + z*o.z; }
    float squaredot(const vec &o) const { float k = dot(o); return k*k; }
    float absdot(const vec &o) const { return fabs(x*o.x) + fabs(y*o.y) + fabs(z*o.z); }
    float zdot(const vec &o) const { return z*o.z; }
    vec &mul(const vec &o)   { x *= o.x; y *= o.y; z *= o.z; return *this; }
    vec &mul(float f)        { x *= f; y *= f; z *= f; return *this; }
    vec &mul2(float f)       { x *= f; y *= f; return *this; }
    vec &square()            { mul(*this); return *this; }
    vec &div(const vec &o)   { x /= o.x; y /= o.y; z /= o.z; return *this; }
    vec &div(float f)        { x /= f; y /= f; z /= f; return *this; }
    vec &div2(float f)       { x /= f; y /= f; return *this; }
    vec &recip()             { x = 1/x; y = 1/y; z = 1/z; return *this; }
    vec &add(const vec &o)   { x += o.x; y += o.y; z += o.z; return *this; }
    vec &add(float f)        { x += f; y += f; z += f; return *this; }
    vec &add2(float f)       { x += f; y += f; return *this; }
    vec &addz(float f)       { z += f; return *this; }
    vec &sub(const vec &o)   { x -= o.x; y -= o.y; z -= o.z; return *this; }
    vec &sub(float f)        { x -= f; y -= f; z -= f; return *this; }
    vec &sub2(float f)       { x -= f; y -= f; return *this; }
    vec &subz(float f)       { z -= f; return *this; }
    vec &neg2()              { x = -x; y = -y; return *this; }
    vec &neg()               { x = -x; y = -y; z = -z; return *this; }
    vec &min(const vec &o)   { x = ::min(x, o.x); y = ::min(y, o.y); z = ::min(z, o.z); return *this; }
    vec &max(const vec &o)   { x = ::max(x, o.x); y = ::max(y, o.y); z = ::max(z, o.z); return *this; }
    vec &min(float f)        { x = ::min(x, f); y = ::min(y, f); z = ::min(z, f); return *this; }
    vec &max(float f)        { x = ::max(x, f); y = ::max(y, f); z = ::max(z, f); return *this; }
    vec &abs() { x = fabs(x); y = fabs(y); z = fabs(z); return *this; }
    vec &clamp(float l, float h) { x = ::clamp(x, l, h); y = ::clamp(y, l, h); z = ::clamp(z, l, h); return *this; }
    float magnitude2() const { return sqrtf(dot2(*this)); }
    float magnitude() const  { return sqrtf(squaredlen()); }
    vec &normalize()         { div(magnitude()); return *this; }
    vec &safenormalize()     { float m = magnitude(); if(m) div(m); return *this; }
    bool isnormalized() const { float m = squaredlen(); return (m>0.99f && m<1.01f); }
    float squaredist(const vec &e) const { return vec(*this).sub(e).squaredlen(); }
    float dist(const vec &e) const { return sqrtf(squaredist(e)); }
    float dist(const vec &e, vec &t) const { t = *this; t.sub(e); return t.magnitude(); }
    float dist2(const vec &o) const { float dx = x-o.x, dy = y-o.y; return sqrtf(dx*dx + dy*dy); }
    template<class T>
    bool reject(const T &o, float r) { return x>o.x+r || x<o.x-r || y>o.y+r || y<o.y-r; }
    float distrange(const vec &o, float r = 1e16f, float m = 0.f) const { float dt = dist(o); return r > m && dt > m ? (dt < r ? (dt-m)/(r-m) : 1.f) : 0.f; }
    bool rejectxyz(const vec &o, float rx, float ry, float rz1, float rz2) { return x>o.x+rx || x<o.x-rx || y>o.y+ry || y<o.y-ry || z>o.z+rz2 || z<o.z-rz1; }
    template<class A, class B>
    vec &cross(const A &a, const B &b) { x = a.y*b.z-a.z*b.y; y = a.z*b.x-a.x*b.z; z = a.x*b.y-a.y*b.x; return *this; }
    vec &cross(const vec &o, const vec &a, const vec &b) { return cross(vec(a).sub(o), vec(b).sub(o)); }
    float scalartriple(const vec &a, const vec &b) const { return x*(a.y*b.z-a.z*b.y) + y*(a.z*b.x-a.x*b.z) + z*(a.x*b.y-a.y*b.x); }
    float zscalartriple(const vec &a, const vec &b) const { return z*(a.x*b.y-a.y*b.x); }
    vec &reflectz(float rz) { z = 2*rz - z; return *this; }
    vec &reflect(const vec &n) { float k = 2*dot(n); x -= k*n.x; y -= k*n.y; z -= k*n.z; return *this; }
    vec &project(const vec &n) { float k = dot(n); x -= k*n.x; y -= k*n.y; z -= k*n.z; return *this; }
    vec &projectxydir(const vec &n) { if(n.z) z = -(x*n.x/n.z + y*n.y/n.z); return *this; }
    vec &projectxy(const vec &n)
    {
        float m = squaredlen(), k = dot(n);
        projectxydir(n);
        rescale(sqrtf(::max(m - k*k, 0.0f)));
        return *this;
    }
    vec &projectxy(const vec &n, float threshold)
    {
        float m = squaredlen(), k = ::min(dot(n), threshold);
        projectxydir(n);
        rescale(sqrtf(::max(m - k*k, 0.0f)));
        return *this;
    }
    vec &lerp(const vec &b, float t) { x += (b.x-x)*t; y += (b.y-y)*t; z += (b.z-z)*t; return *this; }
    vec &lerp(const vec &a, const vec &b, float t) { x = a.x + (b.x-a.x)*t; y = a.y + (b.y-a.y)*t; z = a.z + (b.z-a.z)*t; return *this; }
    vec &avg(const vec &b) { add(b); mul(0.5f); return *this; }
    template<class B> vec &madd(const vec &a, const B &b) { return add(vec(a).mul(b)); }
    template<class B> vec &msub(const vec &a, const B &b) { return sub(vec(a).mul(b)); }

    vec &rescale(float k)
    {
        float mag = magnitude();
        if(mag > 1e-6f) mul(k / mag);
        return *this;
    }

    vec &rotate_around_z(float c, float s) { float rx = x, ry = y; x = c*rx-s*ry; y = c*ry+s*rx; return *this; }
    vec &rotate_around_x(float c, float s) { float ry = y, rz = z; y = c*ry-s*rz; z = c*rz+s*ry; return *this; }
    vec &rotate_around_y(float c, float s) { float rx = x, rz = z; x = c*rx+s*rz; z = c*rz-s*rx; return *this; }

    vec &rotate_around_z(float angle) { return rotate_around_z(cosf(angle), sinf(angle)); }
    vec &rotate_around_x(float angle) { return rotate_around_x(cosf(angle), sinf(angle)); }
    vec &rotate_around_y(float angle) { return rotate_around_y(cosf(angle), sinf(angle)); }

    vec &rotate_around_z(const vec2 &sc) { return rotate_around_z(sc.x, sc.y); }
    vec &rotate_around_x(const vec2 &sc) { return rotate_around_x(sc.x, sc.y); }
    vec &rotate_around_y(const vec2 &sc) { return rotate_around_y(sc.x, sc.y); }

    vec &rotate(float c, float s, const vec &d)
    {
        *this = vec(x*(d.x*d.x*(1-c)+c) + y*(d.x*d.y*(1-c)-d.z*s) + z*(d.x*d.z*(1-c)+d.y*s),
                    x*(d.y*d.x*(1-c)+d.z*s) + y*(d.y*d.y*(1-c)+c) + z*(d.y*d.z*(1-c)-d.x*s),
                    x*(d.x*d.z*(1-c)-d.y*s) + y*(d.y*d.z*(1-c)+d.x*s) + z*(d.z*d.z*(1-c)+c));
        return *this;
    }
    vec &rotate(float angle, const vec &d) { return rotate(cosf(angle), sinf(angle), d); }
    vec &rotate(const vec2 &sc, const vec &d) { return rotate(sc.x, sc.y, d); }

    void orthogonal(const vec &d)
    {
        *this = fabs(d.x) > fabs(d.z) ? vec(-d.y, d.x, 0) : vec(0, -d.z, d.y);
    }

    void orthonormalize(vec &s, vec &t) const
    {
        s.project(*this);
        t.project(*this).project(s);
    }

    template<class T> bool insidebb(const T &bbmin, const T &bbmax) const
    {
        return x >= bbmin.x && x <= bbmax.x && y >= bbmin.y && y <= bbmax.y && z >= bbmin.z && z <= bbmax.z;
    }

    template<class T, class U> bool insidebb(const T &bbmin, const T &bbmax, U margin) const
    {
        return x >= bbmin.x-margin && x <= bbmax.x+margin && y >= bbmin.y-margin && y <= bbmax.y+margin && z >= bbmin.z-margin && z <= bbmax.z+margin;
    }

    template<class T, class U> bool insidebb(const T &o, U size) const
    {
        return x >= o.x && x <= o.x + size && y >= o.y && y <= o.y + size && z >= o.z && z <= o.z + size;
    }

    template<class T, class U> bool insidebb(const T &o, U size, U margin) const
    {
        size += margin;
        return x >= o.x-margin && x <= o.x + size && y >= o.y-margin && y <= o.y + size && z >= o.z-margin && z <= o.z + size;
    }

    template<class T> float dist_to_bb(const T &min, const T &max) const
    {
        float sqrdist = 0;
        loopi(3)
        {
            if     (v[i] < min[i]) { float delta = v[i]-min[i]; sqrdist += delta*delta; }
            else if(v[i] > max[i]) { float delta = max[i]-v[i]; sqrdist += delta*delta; }
        }
        return sqrtf(sqrdist);
    }

    template<class T, class S> float dist_to_bb(const T &o, S size) const
    {
        return dist_to_bb(o, T(o).add(size));
    }

    template<class T> float project_bb(const T &min, const T &max) const
    {
        return x*(x < 0 ? max.x : min.x) + y*(y < 0 ? max.y : min.y) + z*(z < 0 ? max.z : min.z);
    }

    static vec fromcolor(int color)
    {
        return vec(((color>>16)&0xFF)*(1.0f/255.0f), ((color>>8)&0xFF)*(1.0f/255.0f), (color&0xFF)*(1.0f/255.0f));
    }

    int tohexcolor() const { return (int(::clamp(r, 0.0f, 1.0f)*255)<<16)|(int(::clamp(g, 0.0f, 1.0f)*255)<<8)|int(::clamp(b, 0.0f, 1.0f)*255); }

    vec &minbounds(float rx, float ry, float rz)
    {
        x = ::clamp(x, -rx, rx);
        y = ::clamp(y, -ry, ry);
        z = ::clamp(z, -rz, rz);
        return *this;
    }
    vec &minbounds(const vec &n) { return minbounds(n.x, n.y, n.z); }
};

inline vec2::vec2(const vec &v) : x(v.x), y(v.y) {}

static inline bool htcmp(const vec &x, const vec &y)
{
    return x == y;
}

static inline uint hthash(const vec &k)
{
    union { uint i; float f; } x, y, z;
    x.f = k.x; y.f = k.y; z.f = k.z;
    uint v = x.i^y.i^z.i;
    return v + (v>>12);
}

struct vec4
{
    union
    {
        struct { float x, y, z, w; };
        struct { float r, g, b, a; };
        float v[4];
    };

    vec4() {}
    explicit vec4(const vec &p, float w = 0) : x(p.x), y(p.y), z(p.z), w(w) {}
    explicit vec4(const vec2 &p, float z = 0, float w = 0) : x(p.x), y(p.y), z(z), w(w) {}
    vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    explicit vec4(const float *v) : x(v[0]), y(v[1]), z(v[2]), w(v[3]) {}
    explicit vec4(int color, float trans = 1) : x(((color>>16)&0xFF)/255.f), y(((color>>8)&0xFF)/255.f), z((color&0xFF)/255.f), w(trans) {}

    float &operator[](int i)       { return v[i]; }
    float  operator[](int i) const { return v[i]; }

    bool operator==(const vec4 &o) const { return x == o.x && y == o.y && z == o.z && w == o.w; }
    bool operator!=(const vec4 &o) const { return x != o.x || y != o.y || z != o.z || w != o.w; }

    float dot3(const vec4 &o) const { return x*o.x + y*o.y + z*o.z; }
    float dot3(const vec &o) const { return x*o.x + y*o.y + z*o.z; }
    float dot(const vec4 &o) const { return dot3(o) + w*o.w; }
    float dot(const vec &o) const  { return x*o.x + y*o.y + z*o.z + w; }
    float squaredlen() const { return dot(*this); }
    float magnitude() const  { return sqrtf(squaredlen()); }
    float magnitude3() const { return sqrtf(dot3(*this)); }
    vec4 &normalize() { mul(1/magnitude()); return *this; }
    vec4 &safenormalize() { float m = magnitude(); if(m) mul(1/m); return *this; }

    vec4 &lerp(const vec4 &b, float t)
    {
        x += (b.x-x)*t;
        y += (b.y-y)*t;
        z += (b.z-z)*t;
        w += (b.w-w)*t;
        return *this;
    }
    vec4 &lerp(const vec4 &a, const vec4 &b, float t)
    {
        x = a.x+(b.x-a.x)*t;
        y = a.y+(b.y-a.y)*t;
        z = a.z+(b.z-a.z)*t;
        w = a.w+(b.w-a.w)*t;
        return *this;
    }
    vec4 &avg(const vec4 &b) { add(b); mul(0.5f); return *this; }
    template<class B> vec4 &madd(const vec4 &a, const B &b) { return add(vec4(a).mul(b)); }
    template<class B> vec4 &msub(const vec4 &a, const B &b) { return sub(vec4(a).mul(b)); }

    vec4 &mul3(float f)      { x *= f; y *= f; z *= f; return *this; }
    vec4 &mul(float f)       { mul3(f); w *= f; return *this; }
    vec4 &mul(const vec4 &o) { x *= o.x; y *= o.y; z *= o.z; w *= o.w; return *this; }
    vec4 &mul(const vec &o)  { x *= o.x; y *= o.y; z *= o.z; return *this; }
    vec4 &square()           { mul(*this); return *this; }
    vec4 &div3(float f)      { x /= f; y /= f; z /= f; return *this; }
    vec4 &div(float f)       { div3(f); w /= f; return *this; }
    vec4 &div(const vec4 &o) { x /= o.x; y /= o.y; z /= o.z; w /= o.w; return *this; }
    vec4 &div(const vec &o)  { x /= o.x; y /= o.y; z /= o.z; return *this; }
    vec4 &recip()            { x = 1/x; y = 1/y; z = 1/z; w = 1/w; return *this; }
    vec4 &add(const vec4 &o) { x += o.x; y += o.y; z += o.z; w += o.w; return *this; }
    vec4 &add(const vec &o)  { x += o.x; y += o.y; z += o.z; return *this; }
    vec4 &add3(float f)      { x += f; y += f; z += f; return *this; }
    vec4 &add(float f)       { add3(f); w += f; return *this; }
    vec4 &addw(float f)      { w += f; return *this; }
    vec4 &sub(const vec4 &o) { x -= o.x; y -= o.y; z -= o.z; w -= o.w; return *this; }
    vec4 &sub(const vec &o)  { x -= o.x; y -= o.y; z -= o.z; return *this; }
    vec4 &sub3(float f)      { x -= f; y -= f; z -= f; return *this; }
    vec4 &sub(float f)       { sub3(f); w -= f; return *this; }
    vec4 &subw(float f)      { w -= f; return *this; }
    vec4 &neg3()             { x = -x; y = -y; z = -z; return *this; }
    vec4 &neg()              { neg3(); w = -w; return *this; }
    vec4 &clamp(float l, float h) { x = ::clamp(x, l, h); y = ::clamp(y, l, h); z = ::clamp(z, l, h); w = ::clamp(w, l, h); return *this; }

    template<class A, class B>
    vec4 &cross(const A &a, const B &b) { x = a.y*b.z-a.z*b.y; y = a.z*b.x-a.x*b.z; z = a.x*b.y-a.y*b.x; return *this; }
    vec4 &cross(const vec &o, const vec &a, const vec &b) { return cross(vec(a).sub(o), vec(b).sub(o)); }

    void setxyz(const vec &v) { x = v.x; y = v.y; z = v.z; }

    vec4 &rotate_around_z(float c, float s) { float rx = x, ry = y; x = c*rx-s*ry; y = c*ry+s*rx; return *this; }
    vec4 &rotate_around_x(float c, float s) { float ry = y, rz = z; y = c*ry-s*rz; z = c*rz+s*ry; return *this; }
    vec4 &rotate_around_y(float c, float s) { float rx = x, rz = z; x = c*rx-s*rz; z = c*rz+s*rx; return *this; }

    vec4 &rotate_around_z(float angle) { return rotate_around_z(cosf(angle), sinf(angle)); }
    vec4 &rotate_around_x(float angle) { return rotate_around_x(cosf(angle), sinf(angle)); }
    vec4 &rotate_around_y(float angle) { return rotate_around_y(cosf(angle), sinf(angle)); }

    vec4 &rotate_around_z(const vec2 &sc) { return rotate_around_z(sc.x, sc.y); }
    vec4 &rotate_around_x(const vec2 &sc) { return rotate_around_x(sc.x, sc.y); }
    vec4 &rotate_around_y(const vec2 &sc) { return rotate_around_y(sc.x, sc.y); }

    static vec4 fromcolor(const vec &v, float trans) { return vec4(v.x, v.y, v.z, trans); }
    static vec4 fromcolor(const vec &v) { return fromcolor(v, 1); }
    static vec4 fromcolor(int color) { return vec4(((color>>16)&0xFF)/255.f, ((color>>8)&0xFF)/255.f, (color&0xFF)/255.f, 1); }
};

inline vec2::vec2(const vec4 &v) : x(v.x), y(v.y) {}
inline vec::vec(const vec4 &v) : x(v.x), y(v.y), z(v.z) {}

struct matrix3;
struct matrix4x3;
struct matrix4;

struct quat : vec4
{
    quat() {}
    quat(float x, float y, float z, float w) : vec4(x, y, z, w) {}
    quat(const vec &axis, float angle)
    {
        w = cosf(angle/2);
        float s = sinf(angle/2);
        x = s*axis.x;
        y = s*axis.y;
        z = s*axis.z;
    }
    quat(const vec &u, const vec &v)
    {
        w = sqrtf(u.squaredlen() * v.squaredlen()) + u.dot(v);
        cross(u, v);
        normalize();
    }
    explicit quat(const vec &v)
    {
        x = v.x;
        y = v.y;
        z = v.z;
        restorew();
    }
    explicit quat(const matrix3 &m) { convertmatrix(m); }
    explicit quat(const matrix4x3 &m) { convertmatrix(m); }
    explicit quat(const matrix4 &m) { convertmatrix(m); }

    void restorew() { w = 1.0f-x*x-y*y-z*z; w = w<0 ? 0 : -sqrtf(w); }

    quat &add(const vec4 &o) { vec4::add(o); return *this; }
    quat &sub(const vec4 &o) { vec4::sub(o); return *this; }
    quat &mul(float k) { vec4::mul(k); return *this; }
    template<class B> quat &madd(const vec4 &a, const B &b) { return add(vec4(a).mul(b)); }
    template<class B> quat &msub(const vec4 &a, const B &b) { return sub(vec4(a).mul(b)); }

    quat &mul(const quat &p, const quat &o)
    {
        x = p.w*o.x + p.x*o.w + p.y*o.z - p.z*o.y;
        y = p.w*o.y - p.x*o.z + p.y*o.w + p.z*o.x;
        z = p.w*o.z + p.x*o.y - p.y*o.x + p.z*o.w;
        w = p.w*o.w - p.x*o.x - p.y*o.y - p.z*o.z;
        return *this;
    }
    quat &mul(const quat &o) { return mul(quat(*this), o); }

    quat &invert() { neg3(); return *this; }

    quat &normalize() { vec4::normalize(); return *this; }

    void calcangleaxis(float &angle, vec &axis) const
    {
        float rr = dot3(*this);
        if(rr>0)
        {
            angle = 2*acosf(w);
            axis = vec(x, y, z).mul(1/rr);
        }
        else { angle = 0; axis = vec(0, 0, 1); }
    }

    vec calcangles() const
    {
        vec4 qq = vec4(*this).square();
        float rr = qq.x + qq.y + qq.z + qq.w,
              t = x*y + z*w;
        if(fabs(t) > 0.49999f*rr) return t < 0 ? vec(-2*atan2f(x, w), -M_PI/2, 0) : vec(2*atan2f(x, w), M_PI/2, 0);
        return vec(atan2f(2*(y*w - x*z), qq.x - qq.y - qq.z + qq.w),
                   asinf(2*t/rr),
                   atan2f(2*(x*w - y*z), -qq.x + qq.y - qq.z + qq.w));
    }

    vec rotate(const vec &v) const
    {
        return vec().cross(*this, vec().cross(*this, v).madd(v, w)).mul(2).add(v);
    }

    vec invertedrotate(const vec &v) const
    {
        return vec().cross(*this, vec().cross(*this, v).msub(v, w)).mul(2).add(v);
    }

    template<class M>
    void convertmatrix(const M &m)
    {
        float trace = m.a.x + m.b.y + m.c.z;
        if(trace>0)
        {
            float r = sqrtf(1 + trace), inv = 0.5f/r;
            w = 0.5f*r;
            x = (m.b.z - m.c.y)*inv;
            y = (m.c.x - m.a.z)*inv;
            z = (m.a.y - m.b.x)*inv;
        }
        else if(m.a.x > m.b.y && m.a.x > m.c.z)
        {
            float r = sqrtf(1 + m.a.x - m.b.y - m.c.z), inv = 0.5f/r;
            x = 0.5f*r;
            y = (m.a.y + m.b.x)*inv;
            z = (m.c.x + m.a.z)*inv;
            w = (m.b.z - m.c.y)*inv;
        }
        else if(m.b.y > m.c.z)
        {
            float r = sqrtf(1 + m.b.y - m.a.x - m.c.z), inv = 0.5f/r;
            x = (m.a.y + m.b.x)*inv;
            y = 0.5f*r;
            z = (m.b.z + m.c.y)*inv;
            w = (m.c.x - m.a.z)*inv;
        }
        else
        {
            float r = sqrtf(1 + m.c.z - m.a.x - m.b.y), inv = 0.5f/r;
            x = (m.c.x + m.a.z)*inv;
            y = (m.b.z + m.c.y)*inv;
            z = 0.5f*r;
            w = (m.a.y - m.b.x)*inv;
        }
    }
};

struct dualquat
{
    quat real, dual;

    dualquat() {}
    dualquat(const quat &q, const vec &p)
        : real(q),
          dual(0.5f*( p.x*q.w + p.y*q.z - p.z*q.y),
               0.5f*(-p.x*q.z + p.y*q.w + p.z*q.x),
               0.5f*( p.x*q.y - p.y*q.x + p.z*q.w),
              -0.5f*( p.x*q.x + p.y*q.y + p.z*q.z))
    {
    }
    explicit dualquat(const quat &q) : real(q), dual(0, 0, 0, 0) {}
    explicit dualquat(const matrix4x3 &m);

    dualquat &mul(float k) { real.mul(k); dual.mul(k); return *this; }
    dualquat &add(const dualquat &d) { real.add(d.real); dual.add(d.dual); return *this; }

    dualquat &lerp(const dualquat &to, float t)
    {
        float k = real.dot(to.real) < 0 ? -t : t;
        real.mul(1-t).madd(to.real, k);
        dual.mul(1-t).madd(to.dual, k);
        return *this;
    }
    dualquat &lerp(const dualquat &from, const dualquat &to, float t)
    {
        float k = from.real.dot(to.real) < 0 ? -t : t;
        (real = from.real).mul(1-t).madd(to.real, k);
        (dual = from.dual).mul(1-t).madd(to.dual, k);
        return *this;
    }

    dualquat &invert()
    {
        real.invert();
        dual.invert();
        dual.msub(real, 2*real.dot(dual));
        return *this;
    }

    void mul(const dualquat &p, const dualquat &o)
    {
        real.mul(p.real, o.real);
        dual.mul(p.real, o.dual).add(quat().mul(p.dual, o.real));
    }
    void mul(const dualquat &o) { mul(dualquat(*this), o); }

    void mulorient(const quat &q)
    {
        real.mul(q, quat(real));
        dual.mul(quat(q).invert(), quat(dual));
    }

    void mulorient(const quat &q, const dualquat &base)
    {
        quat trans;
        trans.mul(base.dual, quat(base.real).invert());
        dual.mul(quat(q).invert(), quat().mul(real, trans).add(dual));

        real.mul(q, quat(real));
        dual.add(quat().mul(real, trans.invert())).msub(real, 2*base.real.dot(base.dual));
    }

    void normalize()
    {
        float invlen = 1/real.magnitude();
        real.mul(invlen);
        dual.mul(invlen);
    }

    void translate(const vec &p)
    {
        dual.x +=  0.5f*( p.x*real.w + p.y*real.z - p.z*real.y);
        dual.y +=  0.5f*(-p.x*real.z + p.y*real.w + p.z*real.x);
        dual.z +=  0.5f*( p.x*real.y - p.y*real.x + p.z*real.w);
        dual.w += -0.5f*( p.x*real.x + p.y*real.y + p.z*real.z);
    }

    void scale(float k)
    {
        dual.mul(k);
    }

    void fixantipodal(const dualquat &d)
    {
        if(real.dot(d.real) < 0)
        {
            real.neg();
            dual.neg();
        }
    }

    void accumulate(const dualquat &d, float k)
    {
        if(real.dot(d.real) < 0) k = -k;
        real.madd(d.real, k);
        dual.madd(d.dual, k);
    }

    vec transform(const vec &v) const
    {
        return vec().cross(real, vec().cross(real, v).madd(v, real.w).add(vec(dual))).madd(vec(dual), real.w).msub(vec(real), dual.w).mul(2).add(v);
    }

    quat transform(const quat &q) const
    {
        return quat().mul(real, q);
    }

    vec transposedtransform(const vec &v) const
    {
        return dualquat(*this).invert().transform(v);
    }

    vec transformnormal(const vec &v) const
    {
        return real.rotate(v);
    }

    vec transposedtransformnormal(const vec &v) const
    {
        return real.invertedrotate(v);
    }

    vec gettranslation() const
    {
        return vec().cross(real, dual).madd(vec(dual), real.w).msub(vec(real), dual.w).mul(2);
    }
};

struct matrix3
{
    vec a, b, c;

    matrix3() {}
    matrix3(const vec &a, const vec &b, const vec &c) : a(a), b(b), c(c) {}
    explicit matrix3(float angle, const vec &axis) { rotate(angle, axis); }
    explicit matrix3(const quat &q)
    {
        float x = q.x, y = q.y, z = q.z, w = q.w,
              tx = 2*x, ty = 2*y, tz = 2*z,
              txx = tx*x, tyy = ty*y, tzz = tz*z,
              txy = tx*y, txz = tx*z, tyz = ty*z,
              twx = w*tx, twy = w*ty, twz = w*tz;
        a = vec(1 - (tyy + tzz), txy + twz, txz - twy);
        b = vec(txy - twz, 1 - (txx + tzz), tyz + twx);
        c = vec(txz + twy, tyz - twx, 1 - (txx + tyy));
    }
    explicit matrix3(const matrix4x3 &m);
    explicit matrix3(const matrix4 &m);

    void mul(const matrix3 &m, const matrix3 &n)
    {
        a = vec(m.a).mul(n.a.x).madd(m.b, n.a.y).madd(m.c, n.a.z);
        b = vec(m.a).mul(n.b.x).madd(m.b, n.b.y).madd(m.c, n.b.z);
        c = vec(m.a).mul(n.c.x).madd(m.b, n.c.y).madd(m.c, n.c.z);
    }
    void mul(const matrix3 &n) { mul(matrix3(*this), n); }

    void multranspose(const matrix3 &m, const matrix3 &n)
    {
        a = vec(m.a).mul(n.a.x).madd(m.b, n.b.x).madd(m.c, n.c.x);
        b = vec(m.a).mul(n.a.y).madd(m.b, m.b.y).madd(m.c, n.c.y);
        c = vec(m.a).mul(n.a.z).madd(m.b, n.b.z).madd(m.c, n.c.z);
    }
    void multranspose(const matrix3 &n) { multranspose(matrix3(*this), n); }

    void transposemul(const matrix3 &m, const matrix3 &n)
    {
        a = vec(m.a.dot(n.a), m.b.dot(n.a), m.c.dot(n.a));
        b = vec(m.a.dot(n.b), m.b.dot(n.b), m.c.dot(n.b));
        c = vec(m.a.dot(n.c), m.b.dot(n.c), m.c.dot(n.c));
    }
    void transposemul(const matrix3 &n) { transposemul(matrix3(*this), n); }

    void transpose()
    {
        swap(a.y, b.x); swap(a.z, c.x);
        swap(b.z, c.y);
    }

    template<class M>
    void transpose(const M &m)
    {
        a = vec(m.a.x, m.b.x, m.c.x);
        b = vec(m.a.y, m.b.y, m.c.y);
        c = vec(m.a.z, m.b.z, m.c.z);
    }

    void invert(const matrix3 &o)
    {
        vec unscale(1/o.a.squaredlen(), 1/o.b.squaredlen(), 1/o.c.squaredlen());
        transpose(o);
        a.mul(unscale);
        b.mul(unscale);
        c.mul(unscale);
    }
    void invert() { invert(matrix3(*this)); }

    void normalize()
    {
        a.normalize();
        b.normalize();
        c.normalize();
    }

    void scale(float k)
    {
        a.mul(k);
        b.mul(k);
        c.mul(k);
    }

    void rotate(float angle, const vec &axis)
    {
        rotate(cosf(angle), sinf(angle), axis);
    }

    void rotate(float ck, float sk, const vec &axis)
    {
        a = vec(axis.x*axis.x*(1-ck)+ck, axis.x*axis.y*(1-ck)+axis.z*sk, axis.x*axis.z*(1-ck)-axis.y*sk);
        b = vec(axis.x*axis.y*(1-ck)-axis.z*sk, axis.y*axis.y*(1-ck)+ck, axis.y*axis.z*(1-ck)+axis.x*sk);
        c = vec(axis.x*axis.z*(1-ck)+axis.y*sk, axis.y*axis.z*(1-ck)-axis.x*sk, axis.z*axis.z*(1-ck)+ck);
    }

    void setyaw(float ck, float sk)
    {
        a = vec(ck, sk, 0);
        b = vec(-sk, ck, 0);
        c = vec(0, 0, 1);
    }

    void setyaw(float angle)
    {
        setyaw(cosf(angle), sinf(angle));
    }

    float trace() const { return a.x + b.y + c.z; }

    bool calcangleaxis(float tr, float &angle, vec &axis, float threshold = 1e-16f) const
    {
        if(tr <= -1)
        {
            if(a.x >= b.y && a.x >= c.z)
            {
                float r = 1 + a.x - b.y - c.z;
                if(r <= threshold) return false;
                r = sqrtf(r);
                axis.x = 0.5f*r;
                axis.y = b.x/r;
                axis.z = c.x/r;
            }
            else if(b.y >= c.z)
            {
                float r = 1 + b.y - a.x - c.z;
                if(r <= threshold) return false;
                r = sqrtf(r);
                axis.y = 0.5f*r;
                axis.x = b.x/r;
                axis.z = c.y/r;
            }
            else
            {
                float r = 1 + b.y - a.x - c.z;
                if(r <= threshold) return false;
                r = sqrtf(r);
                axis.z = 0.5f*r;
                axis.x = c.x/r;
                axis.y = c.y/r;
            }
            angle = M_PI;
        }
        else if(tr >= 3)
        {
            axis = vec(0, 0, 1);
            angle = 0;
        }
        else
        {
            axis = vec(b.z - c.y, c.x - a.z, a.y - b.x);
            float r = axis.squaredlen();
            if(r <= threshold) return false;
            axis.mul(1/sqrtf(r));
            angle = acosf(0.5f*(tr - 1));
        }
        return true;
    }

    bool calcangleaxis(float &angle, vec &axis, float threshold = 1e-16f) const { return calcangleaxis(trace(), angle, axis, threshold); }

    vec transform(const vec &o) const
    {
        return vec(a).mul(o.x).madd(b, o.y).madd(c, o.z);
    }
    vec transposedtransform(const vec &o) const { return vec(a.dot(o), b.dot(o), c.dot(o)); }
    vec abstransform(const vec &o) const
    {
        return vec(a).mul(o.x).abs().add(vec(b).mul(o.y).abs()).add(vec(c).mul(o.z).abs());
    }
    vec abstransposedtransform(const vec &o) const
    {
        return vec(a.absdot(o), b.absdot(o), c.absdot(o));
    }

    void identity()
    {
        a = vec(1, 0, 0);
        b = vec(0, 1, 0);
        c = vec(0, 0, 1);
    }

    void rotate_around_x(float ck, float sk)
    {
        vec rb = vec(b).mul(ck).madd(c, sk),
            rc = vec(c).mul(ck).msub(b, sk);
        b = rb;
        c = rc;
    }
    void rotate_around_x(float angle) { rotate_around_x(cosf(angle), sinf(angle)); }
    void rotate_around_x(const vec2 &sc) { rotate_around_x(sc.x, sc.y); }

    void rotate_around_y(float ck, float sk)
    {
        vec rc = vec(c).mul(ck).madd(a, sk),
            ra = vec(a).mul(ck).msub(c, sk);
        c = rc;
        a = ra;
    }
    void rotate_around_y(float angle) { rotate_around_y(cosf(angle), sinf(angle)); }
    void rotate_around_y(const vec2 &sc) { rotate_around_y(sc.x, sc.y); }

    void rotate_around_z(float ck, float sk)
    {
        vec ra = vec(a).mul(ck).madd(b, sk),
            rb = vec(b).mul(ck).msub(a, sk);
        a = ra;
        b = rb;
    }
    void rotate_around_z(float angle) { rotate_around_z(cosf(angle), sinf(angle)); }
    void rotate_around_z(const vec2 &sc) { rotate_around_z(sc.x, sc.y); }

    vec transform(const vec2 &o) { return vec(a).mul(o.x).madd(b, o.y); }
    vec transposedtransform(const vec2 &o) const { return vec(a.dot2(o), b.dot2(o), c.dot2(o)); }

    vec rowx() const { return vec(a.x, b.x, c.x); }
    vec rowy() const { return vec(a.y, b.y, c.y); }
    vec rowz() const { return vec(a.z, b.z, c.z); }
};

struct matrix4x3
{
    vec a, b, c, d;

    matrix4x3() {}
    matrix4x3(const vec &a, const vec &b, const vec &c, const vec &d) : a(a), b(b), c(c), d(d) {}
    matrix4x3(const matrix3 &rot, const vec &trans) : a(rot.a), b(rot.b), c(rot.c), d(trans) {}
    matrix4x3(const dualquat &dq)
    {
        vec4 r = vec4(dq.real).mul(1/dq.real.squaredlen()), rr = vec4(r).mul(dq.real);
        r.mul(2);
        float xy = r.x*dq.real.y, xz = r.x*dq.real.z, yz = r.y*dq.real.z,
              wx = r.w*dq.real.x, wy = r.w*dq.real.y, wz = r.w*dq.real.z;
        a = vec(rr.w + rr.x - rr.y - rr.z, xy + wz, xz - wy);
        b = vec(xy - wz, rr.w + rr.y - rr.x - rr.z, yz + wx);
        c = vec(xz + wy, yz - wx, rr.w + rr.z - rr.x - rr.y);
        d = vec(-(dq.dual.w*r.x - dq.dual.x*r.w + dq.dual.y*r.z - dq.dual.z*r.y),
                -(dq.dual.w*r.y - dq.dual.x*r.z - dq.dual.y*r.w + dq.dual.z*r.x),
                -(dq.dual.w*r.z + dq.dual.x*r.y - dq.dual.y*r.x - dq.dual.z*r.w));

    }
    explicit matrix4x3(const matrix4 &m);

    void mul(float k)
    {
        a.mul(k);
        b.mul(k);
        c.mul(k);
        d.mul(k);
    }

    void setscale(float x, float y, float z) { a.x = x; b.y = y; c.z = z; }
    void setscale(const vec &v) { setscale(v.x, v.y, v.z); }
    void setscale(float n) { setscale(n, n, n); }

    void scale(float x, float y, float z)
    {
        a.mul(x);
        b.mul(y);
        c.mul(z);
    }
    void scale(const vec &v) { scale(v.x, v.y, v.z); }
    void scale(float n) { scale(n, n, n); }

    void settranslation(const vec &p) { d = p; }
    void settranslation(float x, float y, float z) { d = vec(x, y, z); }

    void translate(const vec &p) { d.madd(a, p.x).madd(b, p.y).madd(c, p.z); }
    void translate(float x, float y, float z) { translate(vec(x, y, z)); }
    void translate(const vec &p, float scale) { translate(vec(p).mul(scale)); }

    void accumulate(const matrix4x3 &m, float k)
    {
        a.madd(m.a, k);
        b.madd(m.b, k);
        c.madd(m.c, k);
        d.madd(m.d, k);
    }

    void normalize()
    {
        a.normalize();
        b.normalize();
        c.normalize();
    }

    void lerp(const matrix4x3 &to, float t)
    {
        a.lerp(to.a, t);
        b.lerp(to.b, t);
        c.lerp(to.c, t);
        d.lerp(to.d, t);
    }
    void lerp(const matrix4x3 &from, const matrix4x3 &to, float t)
    {
        a.lerp(from.a, to.a, t);
        b.lerp(from.b, to.b, t);
        c.lerp(from.c, to.c, t);
        d.lerp(from.d, to.d, t);
    }

    void identity()
    {
        a = vec(1, 0, 0);
        b = vec(0, 1, 0);
        c = vec(0, 0, 1);
        d = vec(0, 0, 0);
    }

    void mul(const matrix4x3 &m, const matrix4x3 &n)
    {
        a = vec(m.a).mul(n.a.x).madd(m.b, n.a.y).madd(m.c, n.a.z);
        b = vec(m.a).mul(n.b.x).madd(m.b, n.b.y).madd(m.c, n.b.z);
        c = vec(m.a).mul(n.c.x).madd(m.b, n.c.y).madd(m.c, n.c.z);
        d = vec(m.d).madd(m.a, n.d.x).madd(m.b, n.d.y).madd(m.c, n.d.z);
    }
    void mul(const matrix4x3 &n) { mul(matrix4x3(*this), n); }

    void mul(const matrix3 &m, const matrix4x3 &n)
    {
        a = vec(m.a).mul(n.a.x).madd(m.b, n.a.y).madd(m.c, n.a.z);
        b = vec(m.a).mul(n.b.x).madd(m.b, n.b.y).madd(m.c, n.b.z);
        c = vec(m.a).mul(n.c.x).madd(m.b, n.c.y).madd(m.c, n.c.z);
        d = vec(m.a).mul(n.d.x).madd(m.b, n.d.y).madd(m.c, n.d.z);
    }

    void mul(const matrix3 &rot, const vec &trans, const matrix4x3 &n)
    {
        mul(rot, n);
        d.add(trans);
    }

    void transpose()
    {
        d = vec(a.dot(d), b.dot(d), c.dot(d)).neg();
        swap(a.y, b.x); swap(a.z, c.x);
        swap(b.z, c.y);
    }

    void transpose(const matrix4x3 &o)
    {
        a = vec(o.a.x, o.b.x, o.c.x);
        b = vec(o.a.y, o.b.y, o.c.y);
        c = vec(o.a.z, o.b.z, o.c.z);
        d = vec(o.a.dot(o.d), o.b.dot(o.d), o.c.dot(o.d)).neg();
    }

    void transposemul(const matrix4x3 &m, const matrix4x3 &n)
    {
        vec t(m.a.dot(m.d), m.b.dot(m.d), m.c.dot(m.d));
        a = vec(m.a.dot(n.a), m.b.dot(n.a), m.c.dot(n.a));
        b = vec(m.a.dot(n.b), m.b.dot(n.b), m.c.dot(n.b));
        c = vec(m.a.dot(n.c), m.b.dot(n.c), m.c.dot(n.c));
        d = vec(m.a.dot(n.d), m.b.dot(n.d), m.c.dot(n.d)).sub(t);
    }

    void multranspose(const matrix4x3 &m, const matrix4x3 &n)
    {
        vec t(n.a.dot(n.d), n.b.dot(n.d), n.c.dot(n.d));
        a = vec(m.a).mul(n.a.x).madd(m.b, n.b.x).madd(m.c, n.c.x);
        b = vec(m.a).mul(n.a.y).madd(m.b, m.b.y).madd(m.c, n.c.y);
        c = vec(m.a).mul(n.a.z).madd(m.b, n.b.z).madd(m.c, n.c.z);
        d = vec(m.d).msub(m.a, t.x).msub(m.b, t.y).msub(m.c, t.z);
    }

    void invert(const matrix4x3 &o)
    {
        vec unscale(1/o.a.squaredlen(), 1/o.b.squaredlen(), 1/o.c.squaredlen());
        transpose(o);
        a.mul(unscale);
        b.mul(unscale);
        c.mul(unscale);
        d.mul(unscale);
    }
    void invert() { invert(matrix4x3(*this)); }

    void rotate(float angle, const vec &d)
    {
        rotate(cosf(angle), sinf(angle), d);
    }

    void rotate(float ck, float sk, const vec &axis)
    {
        matrix3 m;
        m.rotate(ck, sk, axis);
        *this = matrix4x3(m, vec(0, 0, 0));
    }

    void rotate_around_x(float ck, float sk)
    {
        vec rb = vec(b).mul(ck).madd(c, sk),
            rc = vec(c).mul(ck).msub(b, sk);
        b = rb;
        c = rc;
    }
    void rotate_around_x(float angle) { rotate_around_x(cosf(angle), sinf(angle)); }
    void rotate_around_x(const vec2 &sc) { rotate_around_x(sc.x, sc.y); }

    void rotate_around_y(float ck, float sk)
    {
        vec rc = vec(c).mul(ck).madd(a, sk),
            ra = vec(a).mul(ck).msub(c, sk);
        c = rc;
        a = ra;
    }
    void rotate_around_y(float angle) { rotate_around_y(cosf(angle), sinf(angle)); }
    void rotate_around_y(const vec2 &sc) { rotate_around_y(sc.x, sc.y); }

    void rotate_around_z(float ck, float sk)
    {
        vec ra = vec(a).mul(ck).madd(b, sk),
            rb = vec(b).mul(ck).msub(a, sk);
        a = ra;
        b = rb;
    }
    void rotate_around_z(float angle) { rotate_around_z(cosf(angle), sinf(angle)); }
    void rotate_around_z(const vec2 &sc) { rotate_around_z(sc.x, sc.y); }

    vec transform(const vec &o) const { return vec(d).madd(a, o.x).madd(b, o.y).madd(c, o.z); }
    vec transposedtransform(const vec &o) const { vec p = vec(o).sub(d); return vec(a.dot(p), b.dot(p), c.dot(p)); }
    vec transformnormal(const vec &o) const { return vec(a).mul(o.x).madd(b, o.y).madd(c, o.z); }
    vec transposedtransformnormal(const vec &o) const { return vec(a.dot(o), b.dot(o), c.dot(o)); }
    vec transform(const vec2 &o) const { return vec(d).madd(a, o.x).madd(b, o.y); }

    vec4 rowx() const { return vec4(a.x, b.x, c.x, d.x); }
    vec4 rowy() const { return vec4(a.y, b.y, c.y, d.y); }
    vec4 rowz() const { return vec4(a.z, b.z, c.z, d.z); }
};

inline dualquat::dualquat(const matrix4x3 &m) : real(m)
{
    dual.x =  0.5f*( m.d.x*real.w + m.d.y*real.z - m.d.z*real.y);
    dual.y =  0.5f*(-m.d.x*real.z + m.d.y*real.w + m.d.z*real.x);
    dual.z =  0.5f*( m.d.x*real.y - m.d.y*real.x + m.d.z*real.w);
    dual.w = -0.5f*( m.d.x*real.x + m.d.y*real.y + m.d.z*real.z);
}

inline matrix3::matrix3(const matrix4x3 &m) : a(m.a), b(m.b), c(m.c) {}

struct plane : vec
{
    float offset;

    float dist(const vec &p) const { return dot(p)+offset; }
    float dist(const vec4 &p) const { return p.dot3(*this) + p.w*offset; }
    bool operator==(const plane &p) const { return x==p.x && y==p.y && z==p.z && offset==p.offset; }
    bool operator!=(const plane &p) const { return x!=p.x || y!=p.y || z!=p.z || offset!=p.offset; }

    plane() {}
    plane(const vec &c, float off) : vec(c), offset(off) {}
    plane(const vec4 &p) : vec(p), offset(p.w) {}
    plane(int d, float off)
    {
        x = y = z = 0.0f;
        v[d] = 1.0f;
        offset = -off;
    }
    plane(float a, float b, float c, float d) : vec(a, b, c), offset(d) {}

    void toplane(const vec &n, const vec &p)
    {
        x = n.x; y = n.y; z = n.z;
        offset = -dot(p);
    }

    bool toplane(const vec &a, const vec &b, const vec &c)
    {
        cross(vec(b).sub(a), vec(c).sub(a));
        float mag = magnitude();
        if(!mag) return false;
        div(mag);
        offset = -dot(a);
        return true;
    }

    bool rayintersect(const vec &o, const vec &ray, float &dist)
    {
        float cosalpha = dot(ray);
        if(cosalpha==0) return false;
        float deltac = offset+dot(o);
        dist -= deltac/cosalpha;
        return true;
    }

    plane &reflectz(float rz)
    {
        offset += 2*rz*z;
        z = -z;
        return *this;
    }

    plane &invert()
    {
        neg();
        offset = -offset;
        return *this;
    }

    plane &scale(float k)
    {
        mul(k);
        return *this;
    }

    plane &translate(const vec &p)
    {
        offset += dot(p);
        return *this;
    }

    plane &normalize()
    {
        float mag = magnitude();
        div(mag);
        offset /= mag;
        return *this;
    }

    float zintersect(const vec &p) const { return -(x*p.x+y*p.y+offset)/z; }
    float zdelta(const vec &p) const { return -(x*p.x+y*p.y)/z; }
    float zdist(const vec &p) const { return p.z-zintersect(p); }
};

struct triangle
{
    vec a, b, c;

    triangle(const vec &a, const vec &b, const vec &c) : a(a), b(b), c(c) {}
    triangle() {}

    triangle &add(const vec &o) { a.add(o); b.add(o); c.add(o); return *this; }
    triangle &sub(const vec &o) { a.sub(o); b.sub(o); c.sub(o); return *this; }

    bool operator==(const triangle &t) const { return a == t.a && b == t.b && c == t.c; }
};

/**

The engine uses 3 different linear coordinate systems
which are oriented around each of the axis dimensions.

So any point within the game can be defined by four coordinates: (d, x, y, z)

d is the reference axis dimension
x is the coordinate of the ROW dimension
y is the coordinate of the COL dimension
z is the coordinate of the reference dimension (DEPTH)

typically, if d is not used, then it is implicitly the Z dimension.
ie: d=z => x=x, y=y, z=z

**/

// DIM: X=0 Y=1 Z=2.
const int R[3]  = {1, 2, 0}; // row
const int C[3]  = {2, 0, 1}; // col
const int D[3]  = {0, 1, 2}; // depth

struct ivec4;
struct ivec2;

struct ivec
{
    union
    {
        struct { int x, y, z; };
        struct { int r, g, b; };
        int v[3];
    };

    ivec() {}
    explicit ivec(const vec &v) : x(int(v.x)), y(int(v.y)), z(int(v.z)) {}
    ivec(int a, int b, int c) : x(a), y(b), z(c) {}
    ivec(int d, int row, int col, int depth)
    {
        v[R[d]] = row;
        v[C[d]] = col;
        v[D[d]] = depth;
    }
    ivec(int i, const ivec &co, int size) : x(co.x+((i&1)>>0)*size), y(co.y+((i&2)>>1)*size), z(co.z +((i&4)>>2)*size) {}
    explicit ivec(const ivec4 &v);
    explicit ivec(const ivec2 &v, int z = 0);
    explicit ivec(const usvec &v);
    explicit ivec(const svec &v);

    int &operator[](int i)       { return v[i]; }
    int  operator[](int i) const { return v[i]; }

    //int idx(int i) { return v[i]; }
    bool operator==(const ivec &v) const { return x==v.x && y==v.y && z==v.z; }
    bool operator!=(const ivec &v) const { return x!=v.x || y!=v.y || z!=v.z; }
    bool iszero() const { return x==0 && y==0 && z==0; }
    ivec &shl(int n) { x<<= n; y<<= n; z<<= n; return *this; }
    ivec &shr(int n) { x>>= n; y>>= n; z>>= n; return *this; }
    ivec &mul(int n) { x *= n; y *= n; z *= n; return *this; }
    ivec &div(int n) { x /= n; y /= n; z /= n; return *this; }
    ivec &add(int n) { x += n; y += n; z += n; return *this; }
    ivec &sub(int n) { x -= n; y -= n; z -= n; return *this; }
    ivec &mul(const ivec &v) { x *= v.x; y *= v.y; z *= v.z; return *this; }
    ivec &div(const ivec &v) { x /= v.x; y /= v.y; z /= v.z; return *this; }
    ivec &add(const ivec &v) { x += v.x; y += v.y; z += v.z; return *this; }
    ivec &sub(const ivec &v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
    ivec &mask(int n) { x &= n; y &= n; z &= n; return *this; }
    ivec &neg() { x = -x; y = -y; z = -z; return *this; }
    ivec &min(const ivec &o) { x = ::min(x, o.x); y = ::min(y, o.y); z = ::min(z, o.z); return *this; }
    ivec &max(const ivec &o) { x = ::max(x, o.x); y = ::max(y, o.y); z = ::max(z, o.z); return *this; }
    ivec &min(int n) { x = ::min(x, n); y = ::min(y, n); z = ::min(z, n); return *this; }
    ivec &max(int n) { x = ::max(x, n); y = ::max(y, n); z = ::max(z, n); return *this; }
    ivec &abs() { x = ::abs(x); y = ::abs(y); z = ::abs(z); return *this; }
    ivec &clamp(int l, int h) { x = ::clamp(x, l, h); y = ::clamp(y, l, h); z = ::clamp(z, l, h); return *this; }
    ivec &cross(const ivec &a, const ivec &b) { x = a.y*b.z-a.z*b.y; y = a.z*b.x-a.x*b.z; z = a.x*b.y-a.y*b.x; return *this; }
    int dot(const ivec &o) const { return x*o.x + y*o.y + z*o.z; }
    float dist(const plane &p) const { return x*p.x + y*p.y + z*p.z + p.offset; }
    static ivec fromcolor(int color) { return ivec(int((color>>16)&0xFF), int((color>>8)&0xFF), int(color&0xFF)); }

    static inline ivec floor(const vec &o) { return ivec(int(::floor(o.x)), int(::floor(o.y)), int(::floor(o.z))); }
    static inline ivec ceil(const vec &o) { return ivec(int(::ceil(o.x)), int(::ceil(o.y)), int(::ceil(o.z))); }
};

inline vec::vec(const ivec &v) : x(v.x), y(v.y), z(v.z) {}

static inline bool htcmp(const ivec &x, const ivec &y)
{
    return x == y;
}

static inline uint hthash(const ivec &k)
{
    return k.x^k.y^k.z;
}

struct ivec2
{
    union
    {
        struct { int x, y; };
        int v[2];
    };

    ivec2() {}
    ivec2(int x, int y) : x(x), y(y) {}
    explicit ivec2(const vec2 &v) : x(int(v.x)), y(int(v.y)) {}
    explicit ivec2(const ivec &v) : x(v.x), y(v.y) {}

    int &operator[](int i)       { return v[i]; }
    int  operator[](int i) const { return v[i]; }

    bool operator==(const ivec2 &o) const { return x == o.x && y == o.y; }
    bool operator!=(const ivec2 &o) const { return x != o.x || y != o.y; }

    bool iszero() const { return x==0 && y==0; }
    ivec2 &shl(int n) { x<<= n; y<<= n; return *this; }
    ivec2 &shr(int n) { x>>= n; y>>= n; return *this; }
    ivec2 &mul(int n) { x *= n; y *= n; return *this; }
    ivec2 &div(int n) { x /= n; y /= n; return *this; }
    ivec2 &add(int n) { x += n; y += n; return *this; }
    ivec2 &sub(int n) { x -= n; y -= n; return *this; }
    ivec2 &mul(const ivec2 &v) { x *= v.x; y *= v.y; return *this; }
    ivec2 &div(const ivec2 &v) { x /= v.x; y /= v.y; return *this; }
    ivec2 &add(const ivec2 &v) { x += v.x; y += v.y; return *this; }
    ivec2 &sub(const ivec2 &v) { x -= v.x; y -= v.y; return *this; }
    ivec2 &mask(int n) { x &= n; y &= n; return *this; }
    ivec2 &neg() { x = -x; y = -y; return *this; }
    ivec2 &min(const ivec2 &o) { x = ::min(x, o.x); y = ::min(y, o.y); return *this; }
    ivec2 &max(const ivec2 &o) { x = ::max(x, o.x); y = ::max(y, o.y); return *this; }
    ivec2 &min(int n) { x = ::min(x, n); y = ::min(y, n); return *this; }
    ivec2 &max(int n) { x = ::max(x, n); y = ::max(y, n); return *this; }
    ivec2 &abs() { x = ::abs(x); y = ::abs(y); return *this; }
    int dot(const ivec2 &o) const { return x*o.x + y*o.y; }
    int cross(const ivec2 &o) const { return x*o.y - y*o.x; }
};

inline ivec::ivec(const ivec2 &v, int z) : x(v.x), y(v.y), z(z) {}

static inline bool htcmp(const ivec2 &x, const ivec2 &y)
{
    return x == y;
}

static inline uint hthash(const ivec2 &k)
{
    return k.x^k.y;
}

struct ivec4
{
    union
    {
        struct { int x, y, z, w; };
        struct { int r, g, b, a; };
        int v[4];
    };

    ivec4() {}
    explicit ivec4(const ivec &p, int w = 0) : x(p.x), y(p.y), z(p.z), w(w) {}
    ivec4(int x, int y, int z, int w) : x(x), y(y), z(z), w(w) {}
    explicit ivec4(const vec4 &v) : x(int(v.x)), y(int(v.y)), z(int(v.z)), w(int(v.w)) {}

    bool operator==(const ivec4 &o) const { return x == o.x && y == o.y && z == o.z && w == o.w; }
    bool operator!=(const ivec4 &o) const { return x != o.x || y != o.y || z != o.z || w != o.w; }
};

inline ivec::ivec(const ivec4 &v) : x(v.x), y(v.y), z(v.z) {}

static inline bool htcmp(const ivec4 &x, const ivec4 &y)
{
    return x == y;
}

static inline uint hthash(const ivec4 &k)
{
    return k.x^k.y^k.z^k.w;
}

struct bvec4;

struct bvec
{
    union
    {
        struct { uchar x, y, z; };
        struct { uchar r, g, b; };
        uchar v[3];
    };

    bvec() {}
    bvec(uchar x, uchar y, uchar z) : x(x), y(y), z(z) {}
    explicit bvec(const vec &v) : x(uchar((v.x+1)*(255.0f/2.0f))), y(uchar((v.y+1)*(255.0f/2.0f))), z(uchar((v.z+1)*(255.0f/2.0f))) {}
    explicit bvec(int color) : x(uchar((color>>16)&0xFF)), y(uchar((color>>8)&0xFF)), z(uchar(color&0xFF)) {}
    explicit bvec(const bvec4 &v);

    uchar &operator[](int i)       { return v[i]; }
    uchar  operator[](int i) const { return v[i]; }

    bool operator==(const bvec &v) const { return x==v.x && y==v.y && z==v.z; }
    bool operator!=(const bvec &v) const { return x!=v.x || y!=v.y || z!=v.z; }

    bool iszero() const { return x==0 && y==0 && z==0; }

    bvec &mul(int n) { x *= n; y *= n; z *= n; return *this; }
    bvec &div(int n) { x /= n; y /= n; z /= n; return *this; }
    bvec &add(int n) { x += n; y += n; z += n; return *this; }
    bvec &sub(int n) { x -= n; y -= n; z -= n; return *this; }
    bvec &add(const bvec &v) { x += v.x; y += v.y; z += v.z; return *this; }
    bvec &sub(const bvec &v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
    bvec &min(const bvec &o)   { x = ::min(x, o.x); y = ::min(y, o.y); z = ::min(z, o.z); return *this; }
    bvec &max(const bvec &o)   { x = ::max(x, o.x); y = ::max(y, o.y); z = ::max(z, o.z); return *this; }
    bvec &min(int f)        { x = ::min(int(x), f); y = ::min(int(y), f); z = ::min(int(z), f); return *this; }
    bvec &max(int f)        { x = ::max(int(x), f); y = ::max(int(y), f); z = ::max(int(z), f); return *this; }
    bvec &abs() { return *this; }
    bvec &clamp(int l, int h) { x = ::clamp(int(x), l, h); y = ::clamp(int(y), l, h); z = ::clamp(int(z), l, h); return *this; }

    vec tonormal() const { return vec(x*(2.0f/255.0f)-1.0f, y*(2.0f/255.0f)-1.0f, z*(2.0f/255.0f)-1.0f); }

    bvec &normalize()
    {
        vec n(x-127.5f, y-127.5f, z-127.5f);
        float mag = 127.5f/n.magnitude();
        x = uchar(n.x*mag+127.5f);
        y = uchar(n.y*mag+127.5f);
        z = uchar(n.z*mag+127.5f);
        return *this;
    }

    void lerp(const bvec &a, const bvec &b, float t)
    {
        x = uchar(a.x + (b.x-a.x)*t);
        y = uchar(a.y + (b.y-a.y)*t);
        z = uchar(a.z + (b.z-a.z)*t);
    }

    void lerp(const bvec &a, const bvec &b, int ka, int kb, int d)
    {
        x = uchar((a.x*ka + b.x*kb)/d);
        y = uchar((a.y*ka + b.y*kb)/d);
        z = uchar((a.z*ka + b.z*kb)/d);
    }

    void flip() { x ^= 0x80; y ^= 0x80; z ^= 0x80; }

    void scale(int k, int d) { x = uchar((x*k)/d); y = uchar((y*k)/d); z = uchar((z*k)/d); }

    bvec &shl(int n) { x<<= n; y<<= n; z<<= n; return *this; }
    bvec &shr(int n) { x>>= n; y>>= n; z>>= n; return *this; }

    static bvec fromcolor(const vec &v) { return bvec(uchar(v.x*255.0f), uchar(v.y*255.0f), uchar(v.z*255.0f)); }
    static bvec fromcolor(int color) { return bvec(int((color>>16)&0xFF), int((color>>8)&0xFF), int(color&0xFF)); }
    vec tocolor() const { return vec(x*(1.0f/255.0f), y*(1.0f/255.0f), z*(1.0f/255.0f)); }

    static bvec from565(ushort c) { return bvec((((c>>11)&0x1F)*527 + 15) >> 6, (((c>>5)&0x3F)*259 + 35) >> 6, ((c&0x1F)*527 + 15) >> 6); }

    static bvec hexcolor(int color)
    {
        return bvec((color>>16)&0xFF, (color>>8)&0xFF, color&0xFF);
    }
    int tohexcolor() const { return (int(r)<<16)|(int(g)<<8)|int(b); }
};

struct bvec4
{
    union
    {
        struct { uchar x, y, z, w; };
        struct { uchar r, g, b, a; };
        uchar v[4];
        uint mask;
    };

    bvec4() {}
    bvec4(uchar x, uchar y, uchar z, uchar w = 0) : x(x), y(y), z(z), w(w) {}
    bvec4(const bvec &v, uchar w = 0) : x(v.x), y(v.y), z(v.z), w(w) {}

    uchar &operator[](int i)       { return v[i]; }
    uchar  operator[](int i) const { return v[i]; }

    bool operator==(const bvec4 &v) const { return mask==v.mask; }
    bool operator!=(const bvec4 &v) const { return mask!=v.mask; }

    bool iszero() const { return mask==0; }

    bvec4 &mul(int n) { x *= n; y *= n; z *= n; w *= n; return *this; }
    bvec4 &div(int n) { x /= n; y /= n; z /= n; w /= n; return *this; }
    bvec4 &add(int n) { x += n; y += n; z += n; w += n; return *this; }
    bvec4 &sub(int n) { x -= n; y -= n; z -= n; w -= n; return *this; }
    bvec4 &mul(const bvec4 &v) { x *= v.x; y *= v.y; z *= v.z; w *= v.w; return *this; }
    bvec4 &div(const bvec4 &v) { x /= v.x; y /= v.y; z /= v.z; w /= v.w; return *this; }
    bvec4 &add(const bvec4 &v) { x += v.x; y += v.y; z += v.z; w += v.w; return *this; }
    bvec4 &sub(const bvec4 &v) { x -= v.x; y -= v.y; z -= v.z; w -= v.w; return *this; }
    bvec4 &mul(float xx, float yy, float zz, float ww = 1) { x = uchar(x*xx); y = uchar(y*yy); z = uchar(z*zz); w = uchar(w*ww); return *this; }
    bvec4 &div(float xx, float yy, float zz, float ww = 1) { x = uchar(x/xx); y = uchar(y/yy); z = uchar(z/zz); w = uchar(w/ww); return *this; }
    bvec4 &muld(float xx, float yy, float zz, float ss = 1) { x = uchar(x*xx/ss); y = uchar(y*yy/ss); z = uchar(z*zz/ss); return *this; }
    bvec4 &divd(float xx, float yy, float zz, float ss = 1) { x = uchar(x*ss/xx); y = uchar(y*ss/yy); z = uchar(z*ss/zz); return *this; }
    bvec4 &mulda(float xx, float yy, float zz, float ww = 1, float ss = 1) { x = uchar(x*xx/ss); y = uchar(y*yy/ss); z = uchar(z*zz/ss); w = uchar(w*ww/ss); return *this; }
    bvec4 &divda(float xx, float yy, float zz, float ww = 1, float ss = 1) { x = uchar(x*ss/xx); y = uchar(y*ss/yy); z = uchar(z*ss/zz); w = uchar(w*ss/ww); return *this; }
    bvec4 &min(const bvec4 &o)   { x = ::min(x, o.x); y = ::min(y, o.y); z = ::min(z, o.z); w = ::min(w, o.w); return *this; }
    bvec4 &max(const bvec4 &o)   { x = ::max(x, o.x); y = ::max(y, o.y); z = ::max(z, o.z); z = ::max(w, o.w); return *this; }
    bvec4 &min(int f)        { x = ::min(int(x), f); y = ::min(int(y), f); z = ::min(int(z), f); w = ::min(int(w), f); return *this; }
    bvec4 &max(int f)        { x = ::max(int(x), f); y = ::max(int(y), f); z = ::max(int(z), f); w = ::max(int(w), f); return *this; }

    vec tonormal() const { return vec(x*(2.0f/255.0f)-1.0f, y*(2.0f/255.0f)-1.0f, z*(2.0f/255.0f)-1.0f); }

    void lerp(const bvec4 &a, const bvec4 &b, float t)
    {
        x = uchar(a.x + (b.x-a.x)*t);
        y = uchar(a.y + (b.y-a.y)*t);
        z = uchar(a.z + (b.z-a.z)*t);
        w = a.w;
    }

    void lerp(const bvec4 &a, const bvec4 &b, int ka, int kb, int d)
    {
        x = uchar((a.x*ka + b.x*kb)/d);
        y = uchar((a.y*ka + b.y*kb)/d);
        z = uchar((a.z*ka + b.z*kb)/d);
        w = a.w;
    }

    bvec4 &lighten(int intensity)
    {
        x = uchar(x*(255-intensity)/255 + intensity);
        y = uchar(y*(255-intensity)/255 + intensity);
        z = uchar(z*(255-intensity)/255 + intensity);
        return *this;
    }

    void lerp(const bvec4 &a, const bvec4 &b, const bvec4 &c, float ta, float tb, float tc)
    {
        x = uchar(a.x*ta + b.x*tb + c.x*tc);
        y = uchar(a.y*ta + b.y*tb + c.y*tc);
        z = uchar(a.z*ta + b.z*tb + c.z*tc);
        w = uchar(a.w*ta + b.w*tb + c.w*tc);
    }

    void flip() { mask ^= 0x80808080; }

    bvec4 &alpha(uchar n) { a = n; return *this; }
    static bvec4 fromcolor(int color) { return bvec4(uchar((color>>16)&0xFF), uchar((color>>8)&0xFF), uchar(color&0xFF), 255); }
};

inline bvec::bvec(const bvec4 &v) : x(v.x), y(v.y), z(v.z) {}

struct usvec
{
    union
    {
        struct { ushort x, y, z; };
        ushort v[3];
    };

    ushort &operator[](int i) { return v[i]; }
    ushort operator[](int i) const { return v[i]; }
};

inline vec::vec(const usvec &v) : x(v.x), y(v.y), z(v.z) {}
inline ivec::ivec(const usvec &v) : x(v.x), y(v.y), z(v.z) {}

struct svec
{
    union
    {
        struct { short x, y, z; };
        short v[3];
    };

    svec() {}
    svec(short x, short y, short z) : x(x), y(y), z(z) {}
    explicit svec(const ivec &v) : x(v.x), y(v.y), z(v.z) {}

    short &operator[](int i) { return v[i]; }
    short operator[](int i) const { return v[i]; }
};

inline vec::vec(const svec &v) : x(v.x), y(v.y), z(v.z) {}
inline ivec::ivec(const svec &v) : x(v.x), y(v.y), z(v.z) {}

struct svec2
{
    union
    {
        struct { short x, y; };
        short v[2];
    };

    svec2() {}
    svec2(short x, short y) : x(x), y(y) {}

    short &operator[](int i) { return v[i]; }
    short operator[](int i) const { return v[i]; }

    bool operator==(const svec2 &o) const { return x == o.x && y == o.y; }
    bool operator!=(const svec2 &o) const { return x != o.x || y != o.y; }

    bool iszero() const { return x==0 && y==0; }
};

struct dvec4
{
    double x, y, z, w;

    dvec4() {}
    dvec4(double x, double y, double z, double w) : x(x), y(y), z(z), w(w) {}
    dvec4(const vec4 &v) : x(v.x), y(v.y), z(v.z), w(v.w) {}

    template<class B> dvec4 &madd(const dvec4 &a, const B &b) { return add(dvec4(a).mul(b)); }
    dvec4 &mul(double f)       { x *= f; y *= f; z *= f; w *= f; return *this; }
    dvec4 &mul(const dvec4 &o) { x *= o.x; y *= o.y; z *= o.z; w *= o.w; return *this; }
    dvec4 &add(double f)       { x += f; y += f; z += f; w += f; return *this; }
    dvec4 &add(const dvec4 &o) { x += o.x; y += o.y; z += o.z; w += o.w; return *this; }

    operator vec4() const { return vec4(x, y, z, w); }
};

struct matrix4
{
    vec4 a, b, c, d;

    matrix4() {}
    matrix4(const float *m) : a(m), b(m+4), c(m+8), d(m+12) {}
    matrix4(const vec &a, const vec &b, const vec &c = vec(0, 0, 1))
        : a(a.x, b.x, c.x, 0), b(a.y, b.y, c.y, 0), c(a.z, b.z, c.z, 0), d(0, 0, 0, 1)
    {}
    matrix4(const vec4 &a, const vec4 &b, const vec4 &c, const vec4 &d = vec4(0, 0, 0, 1))
        : a(a), b(b), c(c), d(d)
    {}
    matrix4(const matrix4x3 &m)
        : a(m.a, 0), b(m.b, 0), c(m.c, 0), d(m.d, 1)
    {}
    matrix4(const matrix3 &rot, const vec &trans)
        : a(rot.a, 0), b(rot.b, 0), c(rot.c, 0), d(trans, 1)
    {}

    void mul(const matrix4 &x, const matrix3 &y)
    {
        a = vec4(x.a).mul(y.a.x).madd(x.b, y.a.y).madd(x.c, y.a.z);
        b = vec4(x.a).mul(y.b.x).madd(x.b, y.b.y).madd(x.c, y.b.z);
        c = vec4(x.a).mul(y.c.x).madd(x.b, y.c.y).madd(x.c, y.c.z);
        d = x.d;
    }
    void mul(const matrix3 &y) { mul(matrix4(*this), y); }

    template<class T> void mult(const matrix4 &x, const matrix4 &y)
    {
        a = T(x.a).mul(y.a.x).madd(x.b, y.a.y).madd(x.c, y.a.z).madd(x.d, y.a.w);
        b = T(x.a).mul(y.b.x).madd(x.b, y.b.y).madd(x.c, y.b.z).madd(x.d, y.b.w);
        c = T(x.a).mul(y.c.x).madd(x.b, y.c.y).madd(x.c, y.c.z).madd(x.d, y.c.w);
        d = T(x.a).mul(y.d.x).madd(x.b, y.d.y).madd(x.c, y.d.z).madd(x.d, y.d.w);
    }

    void mul(const matrix4 &x, const matrix4 &y) { mult<vec4>(x, y); }
    void mul(const matrix4 &y) { mult<vec4>(matrix4(*this), y); }

    void muld(const matrix4 &x, const matrix4 &y) { mult<dvec4>(x, y); }
    void muld(const matrix4 &y) { mult<dvec4>(matrix4(*this), y); }

    void rotate_around_x(float ck, float sk)
    {
        vec4 rb = vec4(b).mul(ck).madd(c, sk),
             rc = vec4(c).mul(ck).msub(b, sk);
        b = rb;
        c = rc;
    }
    void rotate_around_x(float angle) { rotate_around_x(cosf(angle), sinf(angle)); }
    void rotate_around_x(const vec2 &sc) { rotate_around_x(sc.x, sc.y); }

    void rotate_around_y(float ck, float sk)
    {
        vec4 rc = vec4(c).mul(ck).madd(a, sk),
             ra = vec4(a).mul(ck).msub(c, sk);
        c = rc;
        a = ra;
    }
    void rotate_around_y(float angle) { rotate_around_y(cosf(angle), sinf(angle)); }
    void rotate_around_y(const vec2 &sc) { rotate_around_y(sc.x, sc.y); }

    void rotate_around_z(float ck, float sk)
    {
        vec4 ra = vec4(a).mul(ck).madd(b, sk),
             rb = vec4(b).mul(ck).msub(a, sk);
        a = ra;
        b = rb;
    }
    void rotate_around_z(float angle) { rotate_around_z(cosf(angle), sinf(angle)); }
    void rotate_around_z(const vec2 &sc) { rotate_around_z(sc.x, sc.y); }

    void rotate(float ck, float sk, const vec &axis)
    {
        matrix3 m;
        m.rotate(ck, sk, axis);
        mul(m);
    }
    void rotate(float angle, const vec &dir) { rotate(cosf(angle), sinf(angle), dir); }
    void rotate(const vec2 &sc, const vec &dir) { rotate(sc.x, sc.y, dir); }

    void identity()
    {
        a = vec4(1, 0, 0, 0);
        b = vec4(0, 1, 0, 0);
        c = vec4(0, 0, 1, 0);
        d = vec4(0, 0, 0, 1);
    }

    void settranslation(const vec &v) { d.setxyz(v); }
    void settranslation(float x, float y, float z) { d.x = x; d.y = y; d.z = z; }

    void translate(const vec &p) { d.madd(a, p.x).madd(b, p.y).madd(c, p.z); }
    void translate(float x, float y, float z) { translate(vec(x, y, z)); }
    void translate(const vec &p, float scale) { translate(vec(p).mul(scale)); }

    void setscale(float x, float y, float z) { a.x = x; b.y = y; c.z = z; }
    void setscale(const vec &v) { setscale(v.x, v.y, v.z); }
    void setscale(float n) { setscale(n, n, n); }

    void scale(float x, float y, float z)
    {
        a.mul(x);
        b.mul(y);
        c.mul(z);
    }
    void scale(const vec &v) { scale(v.x, v.y, v.z); }
    void scale(float n) { scale(n, n, n); }

    void scalexy(float x, float y)
    {
        a.x *= x; a.y *= y;
        b.x *= x; b.y *= y;
        c.x *= x; c.y *= y;
        d.x *= x; d.y *= y;
    }

    void scalez(float k)
    {
        a.z *= k;
        b.z *= k;
        c.z *= k;
        d.z *= k;
    }

    void reflectz(float z)
    {
        d.add(vec4(c).mul(2*z));
        c.neg();
    }

    void projective(float zscale = 0.5f, float zoffset = 0.5f)
    {
        a.x = 0.5f*(a.x + a.w);
        a.y = 0.5f*(a.y + a.w);
        b.x = 0.5f*(b.x + b.w);
        b.y = 0.5f*(b.y + b.w);
        c.x = 0.5f*(c.x + c.w);
        c.y = 0.5f*(c.y + c.w);
        d.x = 0.5f*(d.x + d.w);
        d.y = 0.5f*(d.y + d.w);
        a.z = zscale*a.z + zoffset*a.w;
        b.z = zscale*b.z + zoffset*b.w;
        c.z = zscale*c.z + zoffset*c.w;
        d.z = zscale*d.z + zoffset*d.w;
    }

    void jitter(float x, float y)
    {
        a.x += x * a.w;
        a.y += y * a.w;
        b.x += x * b.w;
        b.y += y * b.w;
        c.x += x * c.w;
        c.y += y * c.w;
        d.x += x * d.w;
        d.y += y * d.w;
    }

    void transpose()
    {
        swap(a.y, b.x); swap(a.z, c.x); swap(a.w, d.x);
        swap(b.z, c.y); swap(b.w, d.y);
        swap(c.w, d.z);
    }

    void transpose(const matrix4 &m)
    {
        a = vec4(m.a.x, m.b.x, m.c.x, m.d.x);
        b = vec4(m.a.y, m.b.y, m.c.y, m.d.y);
        c = vec4(m.a.z, m.b.z, m.c.z, m.d.z);
        d = vec4(m.a.w, m.b.w, m.c.w, m.d.w);
    }

    void frustum(float left, float right, float bottom, float top, float znear, float zfar)
    {
        float width = right - left, height = top - bottom, zrange = znear - zfar;
        a = vec4(2*znear/width, 0, 0, 0);
        b = vec4(0, 2*znear/height, 0, 0);
        c = vec4((right + left)/width, (top + bottom)/height, (zfar + znear)/zrange, -1);
        d = vec4(0, 0, 2*znear*zfar/zrange, 0);
    }

    void perspective(float fovy, float aspect, float znear, float zfar)
    {
        float ydist = znear * tan(fovy/2*RAD), xdist = ydist * aspect;
        frustum(-xdist, xdist, -ydist, ydist, znear, zfar);
    }

    void ortho(float left, float right, float bottom, float top, float znear, float zfar)
    {
        float width = right - left, height = top - bottom, zrange = znear - zfar;
        a = vec4(2/width, 0, 0, 0);
        b = vec4(0, 2/height, 0, 0);
        c = vec4(0, 0, 2/zrange, 0);
        d = vec4(-(right+left)/width, -(top+bottom)/height, (zfar+znear)/zrange, 1);
    }

    void clip(const plane &p, const matrix4 &m)
    {
        float x = ((p.x<0 ? -1 : (p.x>0 ? 1 : 0)) + m.c.x) / m.a.x,
              y = ((p.y<0 ? -1 : (p.y>0 ? 1 : 0)) + m.c.y) / m.b.y,
              w = (1 + m.c.z) / m.d.z,
            scale = 2 / (x*p.x + y*p.y - p.z + w*p.offset);
        a = vec4(m.a.x, m.a.y, p.x*scale, m.a.w);
        b = vec4(m.b.x, m.b.y, p.y*scale, m.b.w);
        c = vec4(m.c.x, m.c.y, p.z*scale + 1.0f, m.c.w);
        d = vec4(m.d.x, m.d.y, p.offset*scale, m.d.w);
    }

    void transform(const vec &in, vec &out) const
    {
        out = vec(a).mul(in.x).add(vec(b).mul(in.y)).add(vec(c).mul(in.z)).add(vec(d));
    }

    void transform(const vec4 &in, vec &out) const
    {
        out = vec(a).mul(in.x).add(vec(b).mul(in.y)).add(vec(c).mul(in.z)).add(vec(d).mul(in.w));
    }

    void transform(const vec &in, vec4 &out) const
    {
        out = vec4(a).mul(in.x).madd(b, in.y).madd(c, in.z).add(d);
    }

    void transform(const vec4 &in, vec4 &out) const
    {
        out = vec4(a).mul(in.x).madd(b, in.y).madd(c, in.z).madd(d, in.w);
    }

    template<class T, class U> T transform(const U &in) const
    {
        T v;
        transform(in, v);
        return v;
    }

    template<class T> vec perspectivetransform(const T &in) const
    {
        vec4 v;
        transform(in, v);
        return vec(v).div(v.w);
    }

    void transformnormal(const vec &in, vec &out) const
    {
        out = vec(a).mul(in.x).add(vec(b).mul(in.y)).add(vec(c).mul(in.z));
    }

    void transformnormal(const vec &in, vec4 &out) const
    {
        out = vec4(a).mul(in.x).madd(b, in.y).madd(c, in.z);
    }

    template<class T, class U> T transformnormal(const U &in) const
    {
        T v;
        transformnormal(in, v);
        return v;
    }

    void transposedtransform(const vec &in, vec &out) const
    {
        vec p = vec(in).sub(vec(d));
        out.x = a.dot3(p);
        out.y = b.dot3(p);
        out.z = c.dot3(p);
    }

    void transposedtransformnormal(const vec &in, vec &out) const
    {
        out.x = a.dot3(in);
        out.y = b.dot3(in);
        out.z = c.dot3(in);
    }

    void transposedtransform(const plane &in, plane &out) const
    {
        out.x = in.dist(a);
        out.y = in.dist(b);
        out.z = in.dist(c);
        out.offset = in.dist(d);
    }

    float getscale() const
    {
        return sqrtf(a.x*a.y + b.x*b.x + c.x*c.x);
    }

    vec gettranslation() const
    {
        return vec(d);
    }

    vec4 rowx() const { return vec4(a.x, b.x, c.x, d.x); }
    vec4 rowy() const { return vec4(a.y, b.y, c.y, d.y); }
    vec4 rowz() const { return vec4(a.z, b.z, c.z, d.z); }
    vec4 roww() const { return vec4(a.w, b.w, c.w, d.w); }

    bool invert(const matrix4 &m, double mindet = 1.0e-12);

    vec2 lineardepthscale() const
    {
        return vec2(d.w, -d.z).div(c.z*d.w - d.z*c.w);
    }
};

inline matrix3::matrix3(const matrix4 &m)
    : a(m.a), b(m.b), c(m.c)
{}

inline matrix4x3::matrix4x3(const matrix4 &m)
    : a(m.a), b(m.b), c(m.c), d(m.d)
{}

struct matrix2
{
    vec2 a, b;

    matrix2() {}
    matrix2(const vec2 &a, const vec2 &b) : a(a), b(b) {}
    explicit matrix2(const matrix4 &m) : a(m.a), b(m.b) {}
    explicit matrix2(const matrix3 &m) : a(m.a), b(m.b) {}
};

struct half
{
    ushort val;

    half() {}
    half(float f)
    {
        union { int i; float f; } conv;
        conv.f = f;
        ushort signbit = (conv.i>>(31-15)) & (1<<15), mantissa = (conv.i>>(23-10)) & 0x3FF;
        int exponent = ((conv.i>>23)&0xFF) - 127 + 15;
        if(exponent <= 0)
        {
            mantissa |= 0x400;
            mantissa >>= min(1-exponent, 10+1);
            exponent = 0;
        }
        else if(exponent >= 0x1F)
        {
            mantissa = 0;
            exponent = 0x1F;
        }
        val = signbit | (ushort(exponent)<<10) | mantissa;
    }

    bool operator==(const half &h) const { return val == h.val; }
    bool operator!=(const half &h) const { return val != h.val; }
};

struct hvec2
{
    half x, y;

    hvec2() {}
    hvec2(float x, float y) : x(x), y(y) {}
    hvec2(const vec2 &v) : x(v.x), y(v.y) {}

    bool operator==(const hvec2 &h) const { return x == h.x && y == h.y; }
    bool operator!=(const hvec2 &h) const { return x != h.x || y != h.y; }
};

struct hvec
{
    half x, y, z;

    hvec() {}
    hvec(float x, float y, float z) : x(x), y(y), z(z) {}
    hvec(const vec &v) : x(v.x), y(v.y), z(v.z) {}

    bool operator==(const hvec &h) const { return x == h.x && y == h.y && z == h.z; }
    bool operator!=(const hvec &h) const { return x != h.x || y != h.y || z != h.z; }
};

struct hvec4
{
    half x, y, z, w;

    hvec4() {}
    hvec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    hvec4(const vec &v, float w) : x(v.x), y(v.y), z(v.z), w(w) {}
    hvec4(const vec4 &v) : x(v.x), y(v.y), z(v.z), w(v.w) {}

    bool operator==(const hvec4 &h) const { return x == h.x && y == h.y && z == h.z && w == h.w; }
    bool operator!=(const hvec4 &h) const { return x != h.x || y != h.y || z != h.z || w != h.w; }
};

struct squat
{
    short x, y, z, w;

    squat() {}
    squat(const vec4 &q) { convert(q); }

    void convert(const vec4 &q)
    {
        x = short(q.x*32767.5f-0.5f);
        y = short(q.y*32767.5f-0.5f);
        z = short(q.z*32767.5f-0.5f);
        w = short(q.w*32767.5f-0.5f);
    }

    void lerp(const vec4 &a, const vec4 &b, float t)
    {
        vec4 q;
        q.lerp(a, b, t);
        convert(q);
    }
};

extern void vecfromyaw(float yaw, int move, int strafe, vec2 &m);
extern void vecfromyawpitch(float yaw, float pitch, int move, int strafe, vec &m);
extern void vectoyawpitch(const vec &v, float &yaw, float &pitch);
extern bool raysphereintersect(const vec &center, float radius, const vec &o, const vec &ray, float &dist);
extern bool rayboxintersect(const vec &b, const vec &s, const vec &o, const vec &ray, float &dist, int &orient);
extern bool linecylinderintersect(const vec &from, const vec &to, const vec &start, const vec &end, float radius, float &dist);
extern vec closestpointcylinder(const vec &center, const vec &start, const vec &end, float radius);
extern int polyclip(const vec *in, int numin, const vec &dir, float below, float above, vec *out);

extern const vec2 sincos360[];
static inline int mod360(int angle)
{
    if(angle < 0) angle = 360 + (angle <= -360 ? angle%360 : angle);
    else if(angle >= 360) angle %= 360;
    return angle;
}
static inline const vec2 &sincosmod360(int angle) { return sincos360[mod360(angle)]; }
static inline float cos360(int angle) { return sincos360[angle].x; }
static inline float sin360(int angle) { return sincos360[angle].y; }
static inline float tan360(int angle) { const vec2 &sc = sincos360[angle]; return sc.y/sc.x; }
static inline float cotan360(int angle) { const vec2 &sc = sincos360[angle]; return sc.x/sc.y; }

