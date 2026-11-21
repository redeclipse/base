// implementation of generic tools

#include "cube.h"

void *operator new(size_t size)
{
    void *p = malloc(size);
    if(!p) abort();
    return p;
}

void *operator new[](size_t size)
{
    void *p = malloc(size);
    if(!p) abort();
    return p;
}

void operator delete(void *p) { if(p) free(p); }

void operator delete[](void *p) { if(p) free(p); }

void *operator new(size_t size, bool err)
{
    void *p = malloc(size);
    if(!p && err) abort();
    return p;
}

void *operator new[](size_t size, bool err)
{
    void *p = malloc(size);
    if(!p && err) abort();
    return p;
}

////////////////////////// strings ////////////////////////////////////////

static string tmpstr[4];
static int tmpidx = 0;

char *tempformatstring(const char *fmt, ...)
{
    tmpidx = (tmpidx+1)%4;

    va_list v;
    va_start(v, fmt);
    vformatstring(tmpstr[tmpidx], fmt, v);
    va_end(v);

    return tmpstr[tmpidx];
}

////////////////////////// rnd numbers ////////////////////////////////////////

#define N (624)
#define M (397)
#define K (0x9908B0DFU)

static uint state[N];
static int next = N;

void seedMT(uint seed)
{
    state[0] = seed;
    for(uint i = 1; i < N; i++)
        state[i] = seed = 1812433253U * (seed ^ (seed >> 30)) + i;
    next = 0;
}

uint randomMT()
{
    int cur = next;
    if(++next >= N)
    {
        if(next > N) { seedMT(5489U + time(NULL)); cur = next++; }
        else next = 0;
    }
    uint y = (state[cur] & 0x80000000U) | (state[next] & 0x7FFFFFFFU);
    state[cur] = y = state[cur < N-M ? cur + M : cur + M-N] ^ (y >> 1) ^ (-int(y & 1U) & K);
    y ^= (y >> 11);
    y ^= (y <<  7) & 0x9D2C5680U;
    y ^= (y << 15) & 0xEFC60000U;
    y ^= (y >> 18);
    return y;
}

#undef N
#undef M
#undef K

///////////////////////// network ///////////////////////

// all network traffic is in 32bit ints, which are then compressed using the following simple scheme (assumes that most values are small).

template<class T>
static inline void putint_(T &p, int n)
{
    if(n<128 && n>-127) p.put(n);
    else if(n<0x8000 && n>=-0x8000) { p.put(0x80); p.put(n); p.put(n>>8); }
    else { p.put(0x81); p.put(n); p.put(n>>8); p.put(n>>16); p.put(n>>24); }
}
void putint(ucharbuf &p, int n) { putint_(p, n); }
void putint(packetbuf &p, int n) { putint_(p, n); }
void putint(vector<uchar> &p, int n) { putint_(p, n); }

int getint(ucharbuf &p)
{
    int c = (schar)p.get();
    if(c==-128) { int n = p.get(); n |= ((schar)p.get())<<8; return n; }
    else if(c==-127) { int n = p.get(); n |= p.get()<<8; n |= p.get()<<16; return n|(p.get()<<24); }
    else return c;
}

// much smaller encoding for unsigned integers up to 28 bits, but can handle signed
template<class T>
static inline void putuint_(T &p, int n)
{
    if(n < 0 || n >= (1<<21))
    {
        p.put(0x80 | (n & 0x7F));
        p.put(0x80 | ((n >> 7) & 0x7F));
        p.put(0x80 | ((n >> 14) & 0x7F));
        p.put(n >> 21);
    }
    else if(n < (1<<7)) p.put(n);
    else if(n < (1<<14))
    {
        p.put(0x80 | (n & 0x7F));
        p.put(n >> 7);
    }
    else
    {
        p.put(0x80 | (n & 0x7F));
        p.put(0x80 | ((n >> 7) & 0x7F));
        p.put(n >> 14);
    }
}
void putuint(ucharbuf &p, int n) { putuint_(p, n); }
void putuint(packetbuf &p, int n) { putuint_(p, n); }
void putuint(vector<uchar> &p, int n) { putuint_(p, n); }

int getuint(ucharbuf &p)
{
    int n = p.get();
    if(n & 0x80)
    {
        n += (p.get() << 7) - 0x80;
        if(n & (1<<14)) n += (p.get() << 14) - (1<<14);
        if(n & (1<<21)) n += (p.get() << 21) - (1<<21);
        if(n & (1<<28)) n |= ~0U<<28;
    }
    return n;
}

template<class T>
static inline void putfloat_(T &p, float f)
{
    lilswap(&f, 1);
    p.put((uchar *)&f, sizeof(float));
}
void putfloat(ucharbuf &p, float f) { putfloat_(p, f); }
void putfloat(packetbuf &p, float f) { putfloat_(p, f); }
void putfloat(vector<uchar> &p, float f) { putfloat_(p, f); }

float getfloat(ucharbuf &p)
{
    float f;
    p.get((uchar *)&f, sizeof(float));
    return lilswap(f);
}

template<class T>
static inline void sendstring_(const char *t, T &p)
{
    while(*t) putint(p, *t++);
    putint(p, 0);
}
void sendstring(const char *t, ucharbuf &p) { sendstring_(t, p); }
void sendstring(const char *t, packetbuf &p) { sendstring_(t, p); }
void sendstring(const char *t, vector<uchar> &p) { sendstring_(t, p); }

void getstring(char *text, ucharbuf &p, size_t len)
{
    char *t = text;
    do
    {
        if(t>=&text[len]) { text[len-1] = 0; return; }
        if(!p.remaining()) { *t = 0; return; }
        *t = getint(p);
    }
    while(*t++);
}
void ipmask::parse(const char *name)
{
    union { uchar b[sizeof(enet_uint32)]; enet_uint32 i; } ipconv, maskconv;
    ipconv.i = 0;
    maskconv.i = 0;
    loopi(4)
    {
        char *end = NULL;
        int n = strtol(name, &end, 10);
        if(!end) break;
        if(end > name) { ipconv.b[i] = n; maskconv.b[i] = 0xFF; }
        name = end;
        while(int c = *name)
        {
            ++name;
            if(c == '.') break;
            if(c == '/')
            {
                int range = clamp(int(strtol(name, NULL, 10)), 0, 32);
                mask = range ? ENET_HOST_TO_NET_32(0xFFffFFff << (32 - range)) : maskconv.i;
                ip = ipconv.i & mask;
                return;
            }
        }
    }
    ip = ipconv.i;
    mask = maskconv.i;
}

int ipmask::print(char *buf) const
{
    char *start = buf;
    union { uchar b[sizeof(enet_uint32)]; enet_uint32 i; } ipconv, maskconv;
    ipconv.i = ip;
    maskconv.i = mask;
    int lastdigit = -1;
    loopi(4) if(maskconv.b[i])
    {
        if(lastdigit >= 0) *buf++ = '.';
        loopj(i - lastdigit - 1) { *buf++ = '*'; *buf++ = '.'; }
        buf += sprintf(buf, "%d", ipconv.b[i]);
        lastdigit = i;
    }
    enet_uint32 bits = ~ENET_NET_TO_HOST_32(mask);
    int range = 32;
    for(; (bits&0xFF) == 0xFF; bits >>= 8) range -= 8;
    for(; bits&1; bits >>= 1) --range;
    if(!bits && range%8) buf += sprintf(buf, "/%d", range);
    return int(buf-start);
}

// Iterative wildcard match: '*' matches any run of characters, '?' any one character
// Only the last '*' is kept as a backtrack point, so there is no recursion and no exponential blowup
bool cubematchstr(const char *str, const char *match, bool nocase)
{
    if(!match || !*match) return !str || !*str;
    if(!str) str = "";
    const char *starmatch = NULL, *starstr = NULL;
    while(*str)
    {
        if(*match == '*')
        {
            starmatch = ++match;
            starstr = str;
            continue;
        }
        if(*match && (*match == '?' || (nocase ? cubelower(*str) == cubelower(*match) : *str == *match)))
        {
            str++;
            match++;
            continue;
        }
        if(starmatch)
        {
            match = starmatch;
            str = ++starstr;
            continue;
        }
        return false;
    }
    // what is left of the pattern only matches the end of the string if it is all wildcards
    while(*match == '*') match++;
    return !*match;
}

int cubepattern(const char *str, const char *pattern, bool nocase)
{
    if(!pattern || !*pattern) return !str || !*str;
    vector<char *> match;
    while(*pattern)
    {
        const char *start = pattern, *p = strchr(pattern, '|');
        int len = p ? p-start : strlen(start);
        match.add(newstring(start, len));
        pattern += len;
        if(p) pattern++;
    }
    int ret = -1;
    loopv(match) if(cubematchstr(str, match[i], nocase))
    {
        ret = i;
        break;
    }
    match.deletearrays();
    return ret;
}

// Linear-time Thompson NFA regex matcher (no exceptions, immune to catastrophic backtracking).
// Supports: . ^ $ * + ? | (...) [...] [^...] and \d \D \w \W \s \S escapes.
#define RX_MAX_NODES 512
#define RX_MAX_DEPTH 64
namespace
{
    enum { RX_CHAR = 0, RX_ANY, RX_CLASS, RX_BOL, RX_EOL, RX_SPLIT, RX_MATCH };

    struct rxnode
    {
        uchar op;
        uchar ch;
        bool neg;
        int out, out1;
        uchar bits[32]; // 256-bit character class set for RX_CLASS

        void setbit(uchar c) { bits[c >> 3] |= 1 << (c & 7); }
        bool hasbit(uchar c) const { return (bits[c >> 3] & (1 << (c & 7))) != 0; }
    };

    struct rxfrag
    {
        int start;
        vector<int> outs; // encoded as (node_index << 1) | which_out (0=out, 1=out1)
    };

    struct rxcompiler
    {
        vector<rxnode> nodes;
        const char *p;
        bool nocase, bad;
        int depth;

        rxcompiler(const char *pat, bool nc) : p(pat), nocase(nc), bad(false), depth(0) {}

        int addnode(uchar op, uchar ch = 0)
        {
            if(nodes.length() >= RX_MAX_NODES) { bad = true; return -1; }
            rxnode &n = nodes.add();
            n.op = op;
            n.ch = ch;
            n.neg = false;
            n.out = n.out1 = -1;
            memset(n.bits, 0, sizeof(n.bits));
            return nodes.length() - 1;
        }

        void patch(const vector<int> &outs, int target)
        {
            loopv(outs)
            {
                int idx = outs[i] >> 1;
                if(outs[i] & 1) nodes[idx].out1 = target;
                else nodes[idx].out = target;
            }
        }

        void addcharclass(rxnode &n, uchar c)
        {
            if(nocase) { n.setbit(uchar(cubelower(c))); n.setbit(uchar(cubeupper(c))); }
            else n.setbit(c);
        }

        void addshortcut(rxnode &n, char esc, bool invert = false)
        {
            for(int c = 1; c < 256; c++)
            {
                bool m = false;
                switch(esc)
                {
                    case 'd': m = c >= '0' && c <= '9'; break;
                    case 'w': m = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'; break;
                    case 's': m = c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v'; break;
                }
                if(invert ? !m : m) n.setbit(uchar(c));
            }
        }

        bool parseatom(rxfrag &f)
        {
            if(!*p || *p == ')' || *p == '|') return false;
            if(*p == '(')
            {
                if(depth >= RX_MAX_DEPTH) { bad = true; return false; }
                p++;
                depth++;
                bool ok = parsealt(f);
                depth--;
                if(!ok) return false;
                if(*p != ')') { bad = true; return false; }
                p++;
                return true;
            }
            if(*p == '^')
            {
                p++;
                int id = addnode(RX_BOL);
                if(id < 0) return false;
                f.start = id;
                f.outs.add(id << 1);
                return true;
            }
            if(*p == '$')
            {
                p++;
                int id = addnode(RX_EOL);
                if(id < 0) return false;
                f.start = id;
                f.outs.add(id << 1);
                return true;
            }
            if(*p == '.')
            {
                p++;
                int id = addnode(RX_ANY);
                if(id < 0) return false;
                f.start = id;
                f.outs.add(id << 1);
                return true;
            }
            if(*p == '[')
            {
                p++;
                int id = addnode(RX_CLASS);
                if(id < 0) return false;
                rxnode &n = nodes[id];
                if(*p == '^') { n.neg = true; p++; }
                bool first = true;
                while(*p && (first || *p != ']'))
                {
                    first = false;
                    uchar c = uchar(*p++);
                    if(c == '\\' && *p)
                    {
                        char e = *p++;
                        if(e == 'd' || e == 'w' || e == 's') { addshortcut(n, e, false); continue; }
                        if(e == 'D' || e == 'W' || e == 'S') { addshortcut(n, cubelower(e), true); continue; }
                        c = uchar(e);
                    }
                    if(*p == '-' && p[1] && p[1] != ']')
                    {
                        p++;
                        uchar hi = uchar(*p++);
                        if(hi == '\\' && *p) hi = uchar(*p++);
                        if(c <= hi) for(int k = c; k <= hi; k++) addcharclass(n, uchar(k));
                    }
                    else addcharclass(n, c);
                }
                if(*p != ']') { bad = true; return false; }
                p++;
                f.start = id;
                f.outs.add(id << 1);
                return true;
            }
            if(*p == '*' || *p == '+' || *p == '?') { bad = true; return false; }
            if(*p == '\\')
            {
                p++;
                if(!*p) { bad = true; return false; }
                char e = *p++;
                if(e == 'd' || e == 'w' || e == 's' || e == 'D' || e == 'W' || e == 'S')
                {
                    int id = addnode(RX_CLASS);
                    if(id < 0) return false;
                    addshortcut(nodes[id], cubelower(e), e >= 'A' && e <= 'Z');
                    f.start = id;
                    f.outs.add(id << 1);
                    return true;
                }
                int id = addnode(RX_CHAR, nocase ? uchar(cubelower(e)) : uchar(e));
                if(id < 0) return false;
                f.start = id;
                f.outs.add(id << 1);
                return true;
            }
            uchar c = uchar(*p++);
            int id = addnode(RX_CHAR, nocase ? uchar(cubelower(c)) : c);
            if(id < 0) return false;
            f.start = id;
            f.outs.add(id << 1);
            return true;
        }

        bool parsepiece(rxfrag &f)
        {
            if(!parseatom(f)) return false;
            while(*p == '*' || *p == '+' || *p == '?')
            {
                char q = *p++;
                int s = addnode(RX_SPLIT);
                if(s < 0) return false;
                if(q == '?')
                {
                    nodes[s].out = f.start;
                    f.start = s;
                    f.outs.add((s << 1) | 1);
                }
                else if(q == '*')
                {
                    nodes[s].out = f.start;
                    patch(f.outs, s);
                    f.start = s;
                    f.outs.setsize(0);
                    f.outs.add((s << 1) | 1);
                }
                else // '+'
                {
                    nodes[s].out = f.start;
                    patch(f.outs, s);
                    f.outs.setsize(0);
                    f.outs.add((s << 1) | 1);
                }
            }
            return true;
        }

        bool parseseq(rxfrag &f)
        {
            bool any = false;
            while(*p && *p != ')' && *p != '|')
            {
                rxfrag next;
                if(!parsepiece(next)) return false;
                if(!any) { f.start = next.start; f.outs.setsize(0); f.outs.move(next.outs); any = true; }
                else { patch(f.outs, next.start); f.outs.setsize(0); f.outs.move(next.outs); }
            }
            if(!any)
            {
                // empty alternative: epsilon split to itself's single out
                int id = addnode(RX_SPLIT);
                if(id < 0) return false;
                f.start = id;
                f.outs.add(id << 1);
            }
            return !bad;
        }

        bool parsealt(rxfrag &f)
        {
            if(!parseseq(f)) return false;
            while(*p == '|')
            {
                p++;
                rxfrag rhs;
                if(!parseseq(rhs)) return false;
                int s = addnode(RX_SPLIT);
                if(s < 0) return false;
                nodes[s].out = f.start;
                nodes[s].out1 = rhs.start;
                f.start = s;
                loopv(rhs.outs) f.outs.add(rhs.outs[i]);
            }
            return !bad;
        }

        int compile()
        {
            rxfrag f;
            if(!parsealt(f) || *p || bad) return -1;
            int m = addnode(RX_MATCH);
            if(m < 0) return -1;
            patch(f.outs, m);
            return f.start;
        }
    };

    void rxaddstate(const vector<rxnode> &nodes, vector<int> &list, vector<int> &seen, int gen, int id, bool atbol, bool ateol)
    {
        if(id < 0 || seen[id] == gen) return;
        seen[id] = gen;
        const rxnode &n = nodes[id];
        if(n.op == RX_SPLIT)
        {
            rxaddstate(nodes, list, seen, gen, n.out, atbol, ateol);
            rxaddstate(nodes, list, seen, gen, n.out1, atbol, ateol);
            return;
        }
        if(n.op == RX_BOL) { if(atbol) rxaddstate(nodes, list, seen, gen, n.out, atbol, ateol); return; }
        if(n.op == RX_EOL) { if(ateol) rxaddstate(nodes, list, seen, gen, n.out, atbol, ateol); return; }
        list.add(id);
    }
}

bool cuberegex(const char *str, const char *pattern, bool nocase)
{
    if(!str || !pattern || !*pattern) return false;
    rxcompiler c(pattern, nocase);
    int start = c.compile();
    if(start < 0) return false;
    vector<int> cur, next, seen;
    seen.reserve(c.nodes.length());
    loopv(c.nodes) seen.add(0);
    int gen = 0;
    const char *s = str;
    for(;;)
    {
        uchar ch = uchar(*s);
        bool atbol = (s == str), ateol = (ch == '\0');
        // Unanchored search: also seed start state at every position without duplicating active states
        gen++;
        loopv(cur) seen[cur[i]] = gen;
        rxaddstate(c.nodes, cur, seen, gen, start, atbol, ateol);
        loopv(cur) if(c.nodes[cur[i]].op == RX_MATCH) return true;
        if(ateol) break;
        next.setsize(0);
        gen++;
        uchar cmp = nocase ? uchar(cubelower(ch)) : ch;
        bool nexteol = (s[1] == '\0');
        loopv(cur)
        {
            const rxnode &n = c.nodes[cur[i]];
            bool matched = false;
            if(n.op == RX_ANY) matched = true;
            else if(n.op == RX_CHAR) matched = (cmp == n.ch);
            else if(n.op == RX_CLASS) matched = n.neg ? !n.hasbit(ch) : n.hasbit(ch);
            if(matched) rxaddstate(c.nodes, next, seen, gen, n.out, false, nexteol);
        }
        cur.setsize(0);
        cur.move(next);
        s++;
    }
    return false;
}
