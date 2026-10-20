// numberformat.cpp: checks the number conversions in shared/command.h against printf and strtod, which they must match
// byte for byte: every int, every float, and a large set of decimal and malformed strings. Build and run from src with
// the game's own flags:
//   g++ -O3 -ffast-math -fno-finite-math-only -std=c++17 -DSTANDALONE -I. -Ishared -Iengine -Igame -Ienet/include -Isupport tests/numberformat.cpp -o numberformat -lpthread -lz
//   ./numberformat
#include "cube.h"
#include <thread>
#include <atomic>

// the conversions as they were before the fast paths
#define OLDPARSEFLOAT(name, type) \
    static inline type oldparse##name(const char *s) \
    { \
        char *end; \
        double val = strtod(s, &end); \
        return val || end==s || (*end!='x' && *end!='X') ? type(val) : type(parseint(s)); \
    }
OLDPARSEFLOAT(float, float)
OLDPARSEFLOAT(number, double)

static std::atomic<llong> failures(0), fastformats(0), kindfailures[3];

static void fail(const char *what, const char *got, const char *want)
{
    failures++;
    int kind = what[0] == 'i' ? 0 : what[0] == 'f' ? 1 : 2;
    if(kindfailures[kind]++ < 10) printf("FAIL %s: got \"%s\" want \"%s\"\n", what, got, want);
}

static void checkrange(ullong start, ullong end)
{
    char got[20], want[20], what[64];
    llong fast = 0;
    for(ullong b = start; b < end; b++)
    {
        uint bits = uint(b);
        int i = int(bits);
        intformat(got, i);
        snprintf(want, sizeof(want), "%d", i);
        if(strcmp(got, want)) { snprintf(what, sizeof(what), "intformat %d", i); fail(what, got, want); }

        float v;
        memcpy(&v, &bits, sizeof(v));
        // floatformat only asks for whole numbers' "%.1f" from printf, so only the rest need to match
        if(v != int(v) && formatfloat6g(got, v, sizeof(got)))
        {
            fast++;
            snprintf(want, sizeof(want), "%.6g", v);
            if(strcmp(got, want)) { snprintf(what, sizeof(what), "formatfloat6g %08x", bits); fail(what, got, want); }
        }
    }
    fastformats += fast;
}

static void checkparse(const char *s)
{
    double got, want = oldparsenumber(s);
    char gs[64], ws[64], what[128];
    got = parsenumber(s);
    if(memcmp(&got, &want, sizeof(got)))
    {
        snprintf(gs, sizeof(gs), "%a", got); snprintf(ws, sizeof(ws), "%a", want); snprintf(what, sizeof(what), "parsenumber \"%s\"", s);
        fail(what, gs, ws);
    }
    float gotf = parsefloat(s), wantf = oldparsefloat(s);
    if(memcmp(&gotf, &wantf, sizeof(gotf)))
    {
        snprintf(gs, sizeof(gs), "%a", gotf); snprintf(ws, sizeof(ws), "%a", wantf); snprintf(what, sizeof(what), "parsefloat \"%s\"", s);
        fail(what, gs, ws);
    }
}

int main()
{
    int numthreads = max(int(std::thread::hardware_concurrency()), 1);
    ullong total = 1ULL<<32, step = total/numthreads;
    std::thread *threads[256];
    numthreads = min(numthreads, 256);
    loopi(numthreads) threads[i] = new std::thread(checkrange, i*step, i == numthreads-1 ? total : (i+1)*step);
    loopi(numthreads) { threads[i]->join(); delete threads[i]; }
    printf("ints and floats checked, %lld floats took the fast path\n", (llong)fastformats);

    static const char *fixed[] =
    {
        "", "-", ".", "-.", "5.", ".5", "-.5", "-5.", "0", "-0", "00", "-00.000", "1e5", "1E5", "1.5e", "1.5e-3", "0x1A", "0X1a", "-0x10", "0x",
        "1.5abc", " 1", "\t1.5", "+1", "+.5", "inf", "-inf", "nan", "infinity", "1.2.3", "00012", "000000000000001", "0000000000000001",
        "0.000000000000001", "0.0000000000000001", "123456789012345", "1234567890123456", "999999999999999", "9999999999999999",
        "0.1", "0.2", "0.3", "-0.1", "3.14159", "2.718281828459045", "1.0000001", "16777217", "16777216.5", "0.30000000000000004",
        "1-", "1x", "1.x", "1 2", "1,5", "--1", "-+1", "١", "1\xc2\xa0"
    };
    loopi(sizeof(fixed)/sizeof(fixed[0])) checkparse(fixed[i]);

    // what floatstr writes, and the longer forms other code or players write
    char buf[64];
    for(ullong b = 0; b < (1ULL<<32); b += 997)
    {
        uint bits = uint(b);
        float v;
        memcpy(&v, &bits, sizeof(v));
        floatformat(buf, v); checkparse(buf);
        snprintf(buf, sizeof(buf), "%.9g", v); checkparse(buf);
        snprintf(buf, sizeof(buf), "%.3f", v); checkparse(buf);
    }
    // random plain decimals of every length up to 20 digits, with leading and trailing zeros, the point anywhere and a sign
    uint seed = 12345;
    loopk(20000000)
    {
        seed = seed*1664525U + 1013904223U;
        int len = 1 + (seed>>8)%20, dot = (seed>>16)%(len+2) - 1, n = 0;
        if((seed>>28)&1) buf[n++] = '-';
        loopi(len)
        {
            if(i == dot) buf[n++] = '.';
            seed = seed*1664525U + 1013904223U;
            int r = (seed>>24)%14;
            buf[n++] = r >= 10 ? '0' : '0' + r;
        }
        if(dot == len) buf[n++] = '.';
        buf[n] = '\0';
        checkparse(buf);
    }
    printf("strings checked\n");
    printf("%s: %lld failures\n", failures ? "FAILED" : "PASSED", (llong)failures);
    return failures ? 1 : 0;
}
