#include "cube.h"

///////////////////////// cryptography /////////////////////////////////

/* Based off the reference implementation of Tiger, a cryptographically
 * secure 192 bit hash function by Ross Anderson and Eli Biham. More info at:
 * http://www.cs.technion.ac.il/~biham/Reports/Tiger/
 */

#define TIGER_PASSES 3

namespace tiger
{
    typedef unsigned long long int chunk;

    union hashval
    {
        uchar bytes[3*8];
        chunk chunks[3];
    };

    chunk sboxes[4*256];

    void compress(const chunk *str, chunk state[3])
    {
        chunk a, b, c;
        chunk aa, bb, cc;
        chunk x0, x1, x2, x3, x4, x5, x6, x7;

        a = state[0];
        b = state[1];
        c = state[2];

        x0=str[0]; x1=str[1]; x2=str[2]; x3=str[3];
        x4=str[4]; x5=str[5]; x6=str[6]; x7=str[7];

        aa = a;
        bb = b;
        cc = c;

        loop(pass_no, TIGER_PASSES)
        {
            if(pass_no)
            {
                x0 -= x7 ^ 0xA5A5A5A5A5A5A5A5ULL; x1 ^= x0; x2 += x1; x3 -= x2 ^ ((~x1)<<19);
                x4 ^= x3; x5 += x4; x6 -= x5 ^ ((~x4)>>23); x7 ^= x6;
                x0 += x7; x1 -= x0 ^ ((~x7)<<19); x2 ^= x1; x3 += x2;
                x4 -= x3 ^ ((~x2)>>23); x5 ^= x4; x6 += x5; x7 -= x6 ^ 0x0123456789ABCDEFULL;
            }

#define sb1 (sboxes)
#define sb2 (sboxes+256)
#define sb3 (sboxes+256*2)
#define sb4 (sboxes+256*3)

#define round(a, b, c, x) \
      c ^= x; \
      a -= sb1[((c)>>(0*8))&0xFF] ^ sb2[((c)>>(2*8))&0xFF] ^ \
       sb3[((c)>>(4*8))&0xFF] ^ sb4[((c)>>(6*8))&0xFF] ; \
      b += sb4[((c)>>(1*8))&0xFF] ^ sb3[((c)>>(3*8))&0xFF] ^ \
       sb2[((c)>>(5*8))&0xFF] ^ sb1[((c)>>(7*8))&0xFF] ; \
      b *= mul;

            uint mul = !pass_no ? 5 : (pass_no==1 ? 7 : 9);
            round(a, b, c, x0) round(b, c, a, x1) round(c, a, b, x2) round(a, b, c, x3)
            round(b, c, a, x4) round(c, a, b, x5) round(a, b, c, x6) round(b, c, a, x7)

            chunk tmp = a; a = c; c = b; b = tmp;
        }

        a ^= aa;
        b -= bb;
        c += cc;

        state[0] = a;
        state[1] = b;
        state[2] = c;
    }

    void gensboxes()
    {
        const char *str = "Tiger - A Fast New Hash Function, by Ross Anderson and Eli Biham";
        chunk state[3] = { 0x0123456789ABCDEFULL, 0xFEDCBA9876543210ULL, 0xF096A5B4C3B2E187ULL };
        uchar temp[64];

        if(!islittleendian()) loopj(64) temp[j^7] = str[j];
        else loopj(64) temp[j] = str[j];
        loopi(1024) loop(col, 8) ((uchar *)&sboxes[i])[col] = i&0xFF;

        int abc = 2;
        loop(pass, 5) loopi(256) for(int sb = 0; sb < 1024; sb += 256)
        {
            abc++;
            if(abc >= 3) { abc = 0; compress((chunk *)temp, state); }
            loop(col, 8)
            {
                uchar val = ((uchar *)&sboxes[sb+i])[col];
                ((uchar *)&sboxes[sb+i])[col] = ((uchar *)&sboxes[sb + ((uchar *)&state[abc])[col]])[col];
                ((uchar *)&sboxes[sb + ((uchar *)&state[abc])[col]])[col] = val;
            }
        }
    }

    void hash(const uchar *str, int length, hashval &val)
    {
        static bool init = false;
        if(!init) { gensboxes(); init = true; }

        uchar temp[64];

        val.chunks[0] = 0x0123456789ABCDEFULL;
        val.chunks[1] = 0xFEDCBA9876543210ULL;
        val.chunks[2] = 0xF096A5B4C3B2E187ULL;

        int i = length;
        for(; i >= 64; i -= 64, str += 64)
        {
            if(!islittleendian())
            {
                loopj(64) temp[j^7] = str[j];
                compress((chunk *)temp, val.chunks);
            }
            else compress((chunk *)str, val.chunks);
        }

        int j;
        if(!islittleendian())
        {
            for(j = 0; j < i; j++) temp[j^7] = str[j];
            temp[j^7] = 0x01;
            while(++j&7) temp[j^7] = 0;
        }
        else
        {
            for(j = 0; j < i; j++) temp[j] = str[j];
            temp[j] = 0x01;
            while(++j&7) temp[j] = 0;
        }

        if(j > 56)
        {
            while(j < 64) temp[j++] = 0;
            compress((chunk *)temp, val.chunks);
            j = 0;
        }
        while(j < 56) temp[j++] = 0;
        *(chunk *)(temp+56) = (chunk)length<<3;
        compress((chunk *)temp, val.chunks);
        if(!islittleendian())
        {
            loopk(3)
            {
                uchar *c = &val.bytes[k*sizeof(chunk)];
                loopl(sizeof(chunk)/2) swap(c[l], c[sizeof(chunk)-1-l]);
            }
        }
    }
}

/* Elliptic curve cryptography based on NIST DSS prime curves. */

#define BI_DIGIT_BITS 16
#define BI_DIGIT_MASK ((1<<BI_DIGIT_BITS)-1)

template<int BI_DIGITS> struct bigint
{
    typedef ushort digit;
    typedef uint dbldigit;

    int len;
    digit digits[BI_DIGITS];

    bigint() {}
    bigint(digit n) { if(n) { len = 1; digits[0] = n; } else len = 0; }
    bigint(const char *s) { parse(s); }
    template<int Y_DIGITS> bigint(const bigint<Y_DIGITS> &y) { *this = y; }

    static int parsedigits(ushort *digits, int maxlen, const char *s)
    {
        int slen = 0;
        while(isxdigit(s[slen])) slen++;
        int len = (slen+2*sizeof(ushort)-1)/(2*sizeof(ushort));
        if(len>maxlen) return 0;
        memset(digits, 0, len*sizeof(ushort));
        loopi(slen)
        {
            int c = s[slen-i-1];
            if(isalpha(c)) c = toupper(c) - 'A' + 10;
            else if(isdigit(c)) c -= '0';
            else return 0;
            digits[i/(2*sizeof(ushort))] |= c<<(4*(i%(2*sizeof(ushort))));
        }
        return len;
    }

    void parse(const char *s)
    {
        len = parsedigits(digits, BI_DIGITS, s);
        shrink();
    }

    void zero() { len = 0; }

    void print(stream *out) const
    {
        vector<char> buf;
        printdigits(buf);
        out->write(buf.getbuf(), buf.length());
    }

    void printdigits(vector<char> &buf) const
    {
        loopi(len)
        {
            digit d = digits[len-i-1];
            loopj(BI_DIGIT_BITS/4)
            {
                uint shift = BI_DIGIT_BITS - (j+1)*4;
                int val = (d >> shift) & 0xF;
                if(val < 10) buf.add('0' + val);
                else buf.add('a' + val - 10);
            }
        }
    }

    template<int Y_DIGITS> bigint &operator=(const bigint<Y_DIGITS> &y)
    {
        len = y.len;
        memcpy(digits, y.digits, len*sizeof(digit));
        return *this;
    }

    bool iszero() const { return !len; }
    bool isone() const { return len==1 && digits[0]==1; }

    int numbits() const
    {
        if(!len) return 0;
        int bits = len*BI_DIGIT_BITS;
        digit last = digits[len-1], mask = 1<<(BI_DIGIT_BITS-1);
        while(mask)
        {
            if(last&mask) return bits;
            bits--;
            mask >>= 1;
        }
        return 0;
    }

    bool hasbit(int n) const { return n/BI_DIGIT_BITS < len && ((digits[n/BI_DIGIT_BITS]>>(n%BI_DIGIT_BITS))&1); }

    bool morebits(int n) const { return len > n/BI_DIGIT_BITS; }

    template<int X_DIGITS, int Y_DIGITS> bigint &add(const bigint<X_DIGITS> &x, const bigint<Y_DIGITS> &y)
    {
        dbldigit carry = 0;
        int maxlen = max(x.len, y.len), i;
        for(i = 0; i < y.len || carry; i++)
        {
             carry += (i < x.len ? (dbldigit)x.digits[i] : 0) + (i < y.len ? (dbldigit)y.digits[i] : 0);
             digits[i] = (digit)carry;
             carry >>= BI_DIGIT_BITS;
        }
        if(i < x.len && this != &x) memcpy(&digits[i], &x.digits[i], (x.len - i)*sizeof(digit));
        len = max(i, maxlen);
        return *this;
    }
    template<int Y_DIGITS> bigint &add(const bigint<Y_DIGITS> &y) { return add(*this, y); }

    template<int X_DIGITS, int Y_DIGITS> bigint &sub(const bigint<X_DIGITS> &x, const bigint<Y_DIGITS> &y)
    {
        ASSERT(x >= y);
        dbldigit borrow = 0;
        int i;
        for(i = 0; i < y.len || borrow; i++)
        {
             borrow = (1<<BI_DIGIT_BITS) + (dbldigit)x.digits[i] - (i<y.len ? (dbldigit)y.digits[i] : 0) - borrow;
             digits[i] = (digit)borrow;
             borrow = (borrow>>BI_DIGIT_BITS)^1;
        }
        if(i < x.len && this != &x) memcpy(&digits[i], &x.digits[i], (x.len - i)*sizeof(digit));
        len = x.len;
        shrink();
        return *this;
    }
    template<int Y_DIGITS> bigint &sub(const bigint<Y_DIGITS> &y) { return sub(*this, y); }

    void shrink() { while(len > 0 && !digits[len-1]) len--; }
    void shrinkdigits(int n) { len = n; shrink(); }
    void shrinkbits(int n) { shrinkdigits(n/BI_DIGIT_BITS); }

    template<int Y_DIGITS> void copyshrinkdigits(const bigint<Y_DIGITS> &y, int n)
    {
        len = clamp(y.len, 0, n);
        memcpy(digits, y.digits, len*sizeof(digit));
        shrink();
    }
    template<int Y_DIGITS> void copyshrinkbits(const bigint<Y_DIGITS> &y, int n)
    {
        copyshrinkdigits(y, n/BI_DIGIT_BITS);
    }

    template<int X_DIGITS, int Y_DIGITS> bigint &mul(const bigint<X_DIGITS> &x, const bigint<Y_DIGITS> &y)
    {
        if(!x.len || !y.len) { len = 0; return *this; }
        memset(digits, 0, y.len*sizeof(digit));
        loopi(x.len)
        {
            dbldigit carry = 0;
            loopj(y.len)
            {
                carry += (dbldigit)x.digits[i] * (dbldigit)y.digits[j] + (dbldigit)digits[i+j];
                digits[i+j] = (digit)carry;
                carry >>= BI_DIGIT_BITS;
            }
            digits[i+y.len] = carry;
        }
        len = x.len + y.len;
        shrink();
        return *this;
    }

    bigint &rshift(int n)
    {
        assert(len <= BI_DIGITS);
        if(!len || n<=0) return *this;
        if(n >= len*BI_DIGIT_BITS) { len = 0; return *this; }
        int dig = (n-1)/BI_DIGIT_BITS;
        n = ((n-1) % BI_DIGIT_BITS)+1;
        digit carry = digit(digits[dig]>>n);
        for(int i = dig+1; i < len; i++)
        {
            dbldigit tmp = digits[i];
            digits[i-dig-1] = digit((tmp<<(BI_DIGIT_BITS-n)) | carry);
            carry = digit(tmp>>n);
        }
        digits[len-dig-1] = carry;
        len -= dig + (n/BI_DIGIT_BITS);
        shrink();
        return *this;
    }

    bigint &lshift(int n)
    {
        if(!len || n<=0) return *this;
        int dig = n/BI_DIGIT_BITS;
        n %= BI_DIGIT_BITS;
        digit carry = 0;
        loopi(len)
        {
            dbldigit tmp = digits[i];
            digits[i+dig] = digit((tmp<<n) | carry);
            carry = digit(tmp>>(BI_DIGIT_BITS-n));
        }
        len += dig;
        if(carry) digits[len++] = carry;
        if(dig) memset(digits, 0, dig*sizeof(digit));
        return *this;
    }

    void zerodigits(int i, int n)
    {
        memset(&digits[i], 0, n*sizeof(digit));
    }
    void zerobits(int i, int n)
    {
        zerodigits(i/BI_DIGIT_BITS, n/BI_DIGIT_BITS);
    }

    template<int Y_DIGITS> void copydigits(int to, const bigint<Y_DIGITS> &y, int from, int n)
    {
        int avail = clamp(y.len-from, 0, n);
        memcpy(&digits[to], &y.digits[from], avail*sizeof(digit));
        if(avail < n) memset(&digits[to+avail], 0, (n-avail)*sizeof(digit));
    }
    template<int Y_DIGITS> void copybits(int to, const bigint<Y_DIGITS> &y, int from, int n)
    {
        copydigits(to/BI_DIGIT_BITS, y, from/BI_DIGIT_BITS, n/BI_DIGIT_BITS);
    }

    void dupdigits(int to, int from, int n)
    {
        memcpy(&digits[to], &digits[from], n*sizeof(digit));
    }
    void dupbits(int to, int from, int n)
    {
        dupdigits(to/BI_DIGIT_BITS, from/BI_DIGIT_BITS, n/BI_DIGIT_BITS);
    }

    template<int Y_DIGITS> bool operator==(const bigint<Y_DIGITS> &y) const
    {
        if(len!=y.len) return false;
        loopirev(len) if(digits[i]!=y.digits[i]) return false;
        return true;
    }
    template<int Y_DIGITS> bool operator!=(const bigint<Y_DIGITS> &y) const { return !(*this==y); }
    template<int Y_DIGITS> bool operator<(const bigint<Y_DIGITS> &y) const
    {
        if(len<y.len) return true;
        if(len>y.len) return false;
        loopirev(len)
        {
            if(digits[i]<y.digits[i]) return true;
            if(digits[i]>y.digits[i]) return false;
        }
        return false;
    }
    template<int Y_DIGITS> bool operator>(const bigint<Y_DIGITS> &y) const { return y<*this; }
    template<int Y_DIGITS> bool operator<=(const bigint<Y_DIGITS> &y) const { return !(y<*this); }
    template<int Y_DIGITS> bool operator>=(const bigint<Y_DIGITS> &y) const { return !(*this<y); }
};

#define GF_BITS         192
#define GF_DIGITS       ((GF_BITS+BI_DIGIT_BITS-1)/BI_DIGIT_BITS)

typedef bigint<GF_DIGITS+1> gfint;

/* NIST prime Galois fields.
 * Currently only supports NIST P-192, where P=2^192-2^64-1, and P-256, where P=2^256-2^224+2^192+2^96-1.
 */
struct gfield : gfint
{
    static const gfield P;

    gfield() {}
    gfield(digit n) : gfint(n) {}
    gfield(const char *s) : gfint(s) {}

    template<int Y_DIGITS> gfield(const bigint<Y_DIGITS> &y) : gfint(y) {}

    template<int Y_DIGITS> gfield &operator=(const bigint<Y_DIGITS> &y)
    {
        gfint::operator=(y);
        return *this;
    }

    template<int X_DIGITS, int Y_DIGITS> gfield &add(const bigint<X_DIGITS> &x, const bigint<Y_DIGITS> &y)
    {
        gfint::add(x, y);
        if(*this >= P) gfint::sub(*this, P);
        return *this;
    }
    template<int Y_DIGITS> gfield &add(const bigint<Y_DIGITS> &y) { return add(*this, y); }

    template<int X_DIGITS> gfield &mul2(const bigint<X_DIGITS> &x) { return add(x, x); }
    gfield &mul2() { return mul2(*this); }

    gfield &div2()
    {
        if(hasbit(0)) gfint::add(*this, P);
        rshift(1);
        return *this;
    }

    template<int X_DIGITS, int Y_DIGITS> gfield &sub(const bigint<X_DIGITS> &x, const bigint<Y_DIGITS> &y)
    {
        if(x < y)
        {
            gfint tmp; /* necessary if this==&y, using this instead would clobber y */
            tmp.add(x, P);
            gfint::sub(tmp, y);
        }
        else gfint::sub(x, y);
        return *this;
    }
    template<int Y_DIGITS> gfield &sub(const bigint<Y_DIGITS> &y) { return sub(*this, y); }

    template<int X_DIGITS> gfield &neg(const bigint<X_DIGITS> &x)
    {
        gfint::sub(P, x);
        return *this;
    }
    gfield &neg() { return neg(*this); }

    template<int X_DIGITS> gfield &square(const bigint<X_DIGITS> &x) { return mul(x, x); }
    gfield &square() { return square(*this); }

    template<int X_DIGITS, int Y_DIGITS> gfield &mul(const bigint<X_DIGITS> &x, const bigint<Y_DIGITS> &y)
    {
        bigint<X_DIGITS+Y_DIGITS> result;
        result.mul(x, y);
        reduce(result);
        return *this;
    }
    template<int Y_DIGITS> gfield &mul(const bigint<Y_DIGITS> &y) { return mul(*this, y); }

    template<int RESULT_DIGITS> void reduce(const bigint<RESULT_DIGITS> &result)
    {
#if GF_BITS==192
        // B = T + S1 + S2 + S3 mod p
        copyshrinkdigits(result, GF_DIGITS); // T

        if(result.morebits(192))
        {
            gfield s;
            s.copybits(0, result, 192, 64);
            s.dupbits(64, 0, 64);
            s.shrinkbits(128);
            add(s); // S1

            if(result.morebits(256))
            {
                s.zerobits(0, 64);
                s.copybits(64, result, 256, 64);
                s.dupbits(128, 64, 64);
                s.shrinkdigits(GF_DIGITS);
                add(s); // S2

                if(result.morebits(320))
                {
                    s.copybits(0, result, 320, 64);
                    s.dupbits(64, 0, 64);
                    s.dupbits(128, 0, 64);
                    s.shrinkdigits(GF_DIGITS);
                    add(s); // S3
                }
            }
        }
        else if(*this >= P) gfint::sub(*this, P);
#elif GF_BITS==256
        // B = T + 2*S1 + 2*S2 + S3 + S4 - D1 - D2 - D3 - D4 mod p
        copyshrinkdigits(result, GF_DIGITS); // T

        if(result.morebits(256))
        {
            gfield s;
            if(result.morebits(352))
            {
                s.zerobits(0, 96);
                s.copybits(96, result, 352, 160);
                s.shrinkdigits(GF_DIGITS);
                add(s); add(s); // S1

                if(result.morebits(384))
                {
                    //s.zerobits(0, 96);
                    s.copybits(96, result, 384, 128);
                    s.shrinkbits(224);
                    add(s); add(s); // S2
                }
            }

            s.copybits(0, result, 256, 96);
            s.zerobits(96, 96);
            s.copybits(192, result, 448, 64);
            s.shrinkdigits(GF_DIGITS);
            add(s); // S3

            s.copybits(0, result, 288, 96);
            s.copybits(96, result, 416, 96);
            s.dupbits(192, 96, 32);
            s.copybits(224, result, 256, 32);
            s.shrinkdigits(GF_DIGITS);
            add(s); // S4

            s.copybits(0, result, 352, 96);
            s.zerobits(96, 96);
            s.copybits(192, result, 256, 32);
            s.copybits(224, result, 320, 32);
            s.shrinkdigits(GF_DIGITS);
            sub(s); // D1

            s.copybits(0, result, 384, 128);
            //s.zerobits(128, 64);
            s.copybits(192, result, 288, 32);
            s.copybits(224, result, 352, 32);
            s.shrinkdigits(GF_DIGITS);
            sub(s); // D2

            s.copybits(0, result, 416, 96);
            s.copybits(96, result, 256, 96);
            s.zerobits(192, 32);
            s.copybits(224, result, 384, 32);
            s.shrinkdigits(GF_DIGITS);
            sub(s); // D3

            s.copybits(0, result, 448, 64);
            s.zerobits(64, 32);
            s.copybits(96, result, 288, 96);
            //s.zerobits(192, 32);
            s.copybits(224, result, 416, 32);
            s.shrinkdigits(GF_DIGITS);
            sub(s); // D4
        }
        else if(*this >= P) gfint::sub(*this, P);
#else
#error Unsupported GF
#endif
    }

    template<int X_DIGITS, int Y_DIGITS> gfield &pow(const bigint<X_DIGITS> &x, const bigint<Y_DIGITS> &y)
    {
        gfield a(x);
        if(y.hasbit(0)) *this = a;
        else
        {
            len = 1;
            digits[0] = 1;
            if(!y.len) return *this;
        }
        for(int i = 1, j = y.numbits(); i < j; i++)
        {
            a.square();
            if(y.hasbit(i)) mul(a);
        }
        return *this;
    }
    template<int Y_DIGITS> gfield &pow(const bigint<Y_DIGITS> &y) { return pow(*this, y); }

    bool invert(const gfield &x)
    {
        if(!x.len) return false;
        gfint u(x), v(P), A((gfint::digit)1), C((gfint::digit)0);
        while(!u.iszero())
        {
            int ushift = 0, ashift = 0;
            while(!u.hasbit(ushift))
            {
                ushift++;
                if(A.hasbit(ashift))
                {
                    if(ashift) { A.rshift(ashift); ashift = 0; }
                    A.add(P);
                }
                ashift++;
            }
            if(ushift) u.rshift(ushift);
            if(ashift) A.rshift(ashift);
            int vshift = 0, cshift = 0;
            while(!v.hasbit(vshift))
            {
                vshift++;
                if(C.hasbit(cshift))
                {
                    if(cshift) { C.rshift(cshift); cshift = 0; }
                    C.add(P);
                }
                cshift++;
            }
            if(vshift) v.rshift(vshift);
            if(cshift) C.rshift(cshift);
            if(u >= v)
            {
                u.sub(v);
                if(A < C) A.add(P);
                A.sub(C);
            }
            else
            {
                v.sub(v, u);
                if(C < A) C.add(P);
                C.sub(A);
            }
        }
        if(C >= P) gfint::sub(C, P);
        else { len = C.len; memcpy(digits, C.digits, len*sizeof(digit)); }
        ASSERT(*this < P);
        return true;
    }
    void invert() { invert(*this); }

    template<int X_DIGITS> static int legendre(const bigint<X_DIGITS> &x)
    {
        static const gfint Psub1div2(gfint(P).sub(bigint<1>(1)).rshift(1));
        gfield L;
        L.pow(x, Psub1div2);
        if(!L.len) return 0;
        if(L.len==1) return 1;
        return -1;
    }
    int legendre() const { return legendre(*this); }

    bool sqrt(const gfield &x)
    {
        if(!x.len) { len = 0; return true; }
#if GF_BITS==224
#error Unsupported GF
#else
        ASSERT((P.digits[0]%4)==3);
        static const gfint Padd1div4(gfint(P).add(bigint<1>(1)).rshift(2));
        switch(legendre(x))
        {
            case 0: len = 0; return true;
            case -1: return false;
            default: pow(x, Padd1div4); return true;
        }
#endif
    }
    bool sqrt() { return sqrt(*this); }
};

struct ecjacobian
{
    static const gfield B;
    static const gfint order;
    static const ecjacobian base;
    static const ecjacobian origin;

    gfield x, y, z;

    ecjacobian() {}
    ecjacobian(const gfield &x, const gfield &y) : x(x), y(y), z(bigint<1>(1)) {}
    ecjacobian(const gfield &x, const gfield &y, const gfield &z) : x(x), y(y), z(z) {}

    void mul2()
    {
        if(z.iszero()) return;
        else if(y.iszero()) { *this = origin; return; }
        gfield a, b, c, d;
        d.sub(x, c.square(z));
        d.mul(c.add(x));
        c.mul2(d).add(d);
        z.mul(y).add(z);
        a.square(y);
        b.mul2(a);
        d.mul2(x).mul(b);
        x.square(c).sub(d).sub(d);
        a.square(b).add(a);
        y.sub(d, x).mul(c).sub(a);
    }

    void add(const ecjacobian &q)
    {
        if(q.z.iszero()) return;
        else if(z.iszero()) { *this = q; return; }
        gfield a, b, c, d, e, f;
        a.square(z);
        b.mul(q.y, a).mul(z);
        a.mul(q.x);
        if(q.z.isone())
        {
            c.add(x, a);
            d.add(y, b);
            a.sub(x, a);
            b.sub(y, b);
        }
        else
        {
            f.mul(y, e.square(q.z)).mul(q.z);
            e.mul(x);
            c.add(e, a);
            d.add(f, b);
            a.sub(e, a);
            b.sub(f, b);
        }
        if(a.iszero()) { if(b.iszero()) mul2(); else *this = origin; return; }
        if(!q.z.isone()) z.mul(q.z);
        z.mul(a);
        x.square(b).sub(f.mul(c, e.square(a)));
        y.sub(f, x).sub(x).mul(b).sub(e.mul(a).mul(d)).div2();
    }

    template<int Q_DIGITS> void mul(const ecjacobian &p, const bigint<Q_DIGITS> &q)
    {
        *this = origin;
        loopirev(q.numbits())
        {
            mul2();
            if(q.hasbit(i)) add(p);
        }
    }
    template<int Q_DIGITS> void mul(const bigint<Q_DIGITS> &q) { ecjacobian tmp(*this); mul(tmp, q); }

    void normalize()
    {
        if(z.iszero() || z.isone()) return;
        gfield tmp;
        z.invert();
        tmp.square(z);
        x.mul(tmp);
        y.mul(tmp).mul(z);
        z = bigint<1>(1);
    }

    bool calcy(bool ybit)
    {
        gfield y2, tmp;
        y2.square(x).mul(x).sub(tmp.add(x, x).add(x)).add(B);
        if(!y.sqrt(y2)) { y.zero(); return false; }
        if(y.hasbit(0) != ybit) y.neg();
        return true;
    }

    void print(vector<char> &buf)
    {
        normalize();
        buf.add(y.hasbit(0) ? '-' : '+');
        x.printdigits(buf);
    }

    bool parse(const char *s)
    {
        // refuse anything but a sign and an x below P, every gfield routine assumes x < P
        *this = origin;
        if(*s != '+' && *s != '-') return false;
        bool ybit = *s++ == '-';
        int slen = 0;
        while(isxdigit(s[slen])) slen++;
        if(!slen || slen > GF_DIGITS*BI_DIGIT_BITS/4) return false;
        gfield px;
        px.parse(s);
        if(px >= gfield::P) return false;
        x = px;
        if(!calcy(ybit)) { *this = origin; return false; }
        z = bigint<1>(1);
        return true;
    }
};

const ecjacobian ecjacobian::origin(gfield((gfield::digit)1), gfield((gfield::digit)1), gfield((gfield::digit)0));

#if GF_BITS==192
const gfield gfield::P("fffffffffffffffffffffffffffffffeffffffffffffffff");
const gfield ecjacobian::B("64210519e59c80e70fa7e9ab72243049feb8deecc146b9b1");
const gfint ecjacobian::order("ffffffffffffffffffffffff99def836146bc9b1b4d22831");
const ecjacobian ecjacobian::base(
    gfield("188da80eb03090f67cbf20eb43a18800f4ff0afd82ff1012"),
    gfield("07192b95ffc8da78631011ed6b24cdd573f977a11e794811")
);
#elif GF_BITS==224
const gfield gfield::P("ffffffffffffffffffffffffffffffff000000000000000000000001");
const gfield ecjacobian::B("b4050a850c04b3abf54132565044b0b7d7bfd8ba270b39432355ffb4");
const gfint ecjacobian::order("ffffffffffffffffffffffffffff16a2e0b8f03e13dd29455c5c2a3d");
const ecjacobian ecjacobian::base(
    gfield("b70e0cbd6bb4bf7f321390b94a03c1d356c21122343280d6115c1d21"),
    gfield("bd376388b5f723fb4c22dfe6cd4375a05a07476444d5819985007e34")
);
#elif GF_BITS==256
const gfield gfield::P("ffffffff00000001000000000000000000000000ffffffffffffffffffffffff");
const gfield ecjacobian::B("5ac635d8aa3a93e7b3ebbd55769886bc651d06b0cc53b0f63bce3c3e27d2604b");
const gfint ecjacobian::order("ffffffff00000000ffffffffffffffffbce6faada7179e84f3b9cac2fc632551");
const ecjacobian ecjacobian::base(
    gfield("6b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296"),
    gfield("4fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5")
);
#elif GF_BITS==384
const gfield gfield::P("fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffeffffffff0000000000000000ffffffff");
const gfield ecjacobian::B("b3312fa7e23ee7e4988e056be3f82d19181d9c6efe8141120314088f5013875ac656398d8a2ed19d2a85c8edd3ec2aef");
const gfint ecjacobian::order("ffffffffffffffffffffffffffffffffffffffffffffffffc7634d81f4372ddf581a0db248b0a77aecec196accc52973");
const ecjacobian ecjacobian::base(
    gfield("aa87ca22be8b05378eb1c71ef320ad746e1d3b628ba79b9859f741e082542a385502f25dbf55296c3a545e3872760ab7"),
    gfield("3617de4a96262c6f5d9e98bf9292dc29f8f41dbd289a147ce9da3113b5f0b8c00a60b1ce1d7e819d7a431d7c90ea0e5f")
);
#elif GF_BITS==521
const gfield gfield::P("1ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
const gfield ecjacobian::B("051953eb968e1c9a1f929a21a0b68540eea2da725b99b315f3b8b489918ef109e156193951ec7e937b1652c0bd3bb1bf073573df883d2c34f1ef451fd46b503f00");
const gfint ecjacobian::order("1fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffa51868783bf2f966b7fcc0148f709a5d03bb5c9b8899c47aebb6fb71e91386409");
const ecjacobian ecjacobian::base(
    gfield("c6858e06b70404e9cd9e3ecb662395b4429c648139053fb521f828af606b4d3dbaa14b5e77efe75928fe1dc127a2ffa8de3348b3c1856a429bf97e7e31c2e5bd66"),
    gfield("11839296a789a3bc0045c8a5fb42c7d1bd998f54449579b446817afbd17273e662c97ee72995ef42640c550b9013fad0761353c7086a272c24088be94769fd16650")
);
#else
#error Unsupported GF
#endif

void calcpubkey(gfint privkey, vector<char> &pubstr)
{
    ecjacobian c(ecjacobian::base);
    c.mul(privkey);
    c.normalize();
    c.print(pubstr);
    pubstr.add('\0');
}

bool calcpubkey(const char *privstr, vector<char> &pubstr)
{
    if(!privstr[0]) return false;
    gfint privkey;
    privkey.parse(privstr);
    calcpubkey(privkey, pubstr);
    return true;
}

void genprivkey(const char *seed, vector<char> &privstr, vector<char> &pubstr)
{
    tiger::hashval hash;
    tiger::hash((const uchar *)seed, (int)strlen(seed), hash);
    bigint<8*sizeof(hash.bytes)/BI_DIGIT_BITS> privkey;
    memcpy(privkey.digits, hash.bytes, sizeof(hash.bytes));
    privkey.len = 8*sizeof(hash.bytes)/BI_DIGIT_BITS;
    privkey.shrink();
    privkey.printdigits(privstr);
    privstr.add('\0');

    calcpubkey(privkey, pubstr);
}

bool hashstring(const char *str, char *result, int maxlen)
{
    tiger::hashval hv;
    if(maxlen < 2*(int)sizeof(hv.bytes) + 1) return false;
    tiger::hash((uchar *)str, strlen(str), hv);
    loopi(sizeof(hv.bytes))
    {
        uchar c = hv.bytes[i];
        *result++ = "0123456789abcdef"[c&0xF];
        *result++ = "0123456789abcdef"[c>>4];
    }
    *result = '\0';
    return true;
}

bool answerchallenge(const char *privstr, const char *challenge, vector<char> &answerstr)
{
    gfint privkey;
    privkey.parse(privstr);
    ecjacobian answer;
    if(!answer.parse(challenge)) return false;
    answer.mul(privkey);
    answer.normalize();
    answer.x.printdigits(answerstr);
    answerstr.add('\0');
    return true;
}

void *parsepubkey(const char *pubstr)
{
    ecjacobian *pubkey = new ecjacobian;
    pubkey->parse(pubstr);
    return pubkey;
}

void freepubkey(void *pubkey)
{
    delete (ecjacobian *)pubkey;
}

void *genchallenge(void *pubkey, const void *seed, int seedlen, vector<char> &challengestr)
{
    tiger::hashval hash;
    tiger::hash((const uchar *)seed, seedlen, hash);
    gfint challenge;
    memcpy(challenge.digits, hash.bytes, sizeof(hash.bytes));
    challenge.len = 8*sizeof(hash.bytes)/BI_DIGIT_BITS;
    challenge.shrink();

    ecjacobian answer(*(ecjacobian *)pubkey);
    answer.mul(challenge);
    answer.normalize();

    ecjacobian secret(ecjacobian::base);
    secret.mul(challenge);
    secret.normalize();

    secret.print(challengestr);
    challengestr.add('\0');

    return new gfield(answer.x);
}

void freechallenge(void *answer)
{
    delete (gfint *)answer;
}

// the answer genchallenge expects, in hex as answerchallenge writes it
void challengeanswer(void *correct, vector<char> &answerstr)
{
    ((gfint *)correct)->printdigits(answerstr);
    answerstr.add('\0');
}

bool checkchallenge(const char *answerstr, void *correct)
{
    gfint answer(answerstr);
    return answer == *(gfint *)correct;
}

namespace sha256impl
{
    static inline uint rotr(uint x, int n) { return (x >> n) | (x << (32 - n)); }

    static const uint k[64] =
    {
        0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
        0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
        0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
        0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
        0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
        0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
        0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
        0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
    };

    static void compress(const uchar block[64], uint h[8])
    {
        uint w[64];
        loopi(16) w[i] = (uint(block[i*4])<<24) | (uint(block[i*4+1])<<16) | (uint(block[i*4+2])<<8) | uint(block[i*4+3]);
        for(int i = 16; i < 64; i++)
        {
            uint s0 = rotr(w[i-15], 7) ^ rotr(w[i-15], 18) ^ (w[i-15] >> 3);
            uint s1 = rotr(w[i-2], 17) ^ rotr(w[i-2], 19) ^ (w[i-2] >> 10);
            w[i] = w[i-16] + s0 + w[i-7] + s1;
        }
        uint a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        loopi(64)
        {
            uint s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            uint ch = (e & f) ^ ((~e) & g);
            uint t1 = hh + s1 + ch + k[i] + w[i];
            uint s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            uint maj = (a & b) ^ (a & c) ^ (b & c);
            uint t2 = s0 + maj;
            hh = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }

    static void hash(const uchar *data, int len, uchar out[32])
    {
        uint h[8] = { 0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU, 0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U };
        int pos = 0;
        for(; pos + 64 <= len; pos += 64) compress(data + pos, h);
        uchar pad[64];
        int rem = len - pos;
        if(rem > 0) memcpy(pad, data + pos, rem);
        pad[rem++] = 0x80;
        if(rem > 56)
        {
            if(rem < 64) memset(pad + rem, 0, 64 - rem);
            compress(pad, h);
            rem = 0;
        }
        if(rem < 56) memset(pad + rem, 0, 56 - rem);
        ullong bits = ullong(uint(len)) << 3;
        loopi(8) pad[56 + i] = uchar(bits >> ((7 - i) * 8));
        compress(pad, h);
        loopi(8)
        {
            out[i*4 + 0] = uchar(h[i] >> 24);
            out[i*4 + 1] = uchar(h[i] >> 16);
            out[i*4 + 2] = uchar(h[i] >> 8);
            out[i*4 + 3] = uchar(h[i]);
        }
    }

    static void hex(const uchar *data, int len, char out[65])
    {
        uchar digest[32];
        hash(data, len, digest);
        loopi(32)
        {
            out[i*2] = "0123456789abcdef"[digest[i] >> 4];
            out[i*2 + 1] = "0123456789abcdef"[digest[i] & 0xF];
        }
        out[64] = '\0';
    }
}

#define GF_HEXCHARS (GF_DIGITS*BI_DIGIT_BITS/4)

template<int X_DIGITS> static void modorder(gfint &out, const bigint<X_DIGITS> &x)
{
    out.zero();
    loopirev(x.numbits())
    {
        out.lshift(1);
        if(x.hasbit(i))
        {
            if(!out.len) { out.len = 1; out.digits[0] = 1; }
            else out.digits[0] |= 1;
        }
        if(out >= ecjacobian::order) out.sub(ecjacobian::order);
    }
}

template<int X_DIGITS> static void printhexpad(const bigint<X_DIGITS> &v, char out[GF_HEXCHARS+1])
{
    loopi(GF_DIGITS)
    {
        uint d = (GF_DIGITS - 1 - i) < v.len ? v.digits[GF_DIGITS - 1 - i] : 0;
        loopj(BI_DIGIT_BITS/4)
        {
            uint shift = BI_DIGIT_BITS - (j + 1)*4;
            out[i*(BI_DIGIT_BITS/4) + j] = "0123456789abcdef"[(d >> shift) & 0xF];
        }
    }
    out[GF_HEXCHARS] = '\0';
}

static bool validnamearg(const char *name)
{
    if(!name || !*name) return false;
    for(const char *p = name; *p; p++) if(uchar(*p) <= ' ' || uchar(*p) == 0x7F) return false;
    return true;
}

static bool lowerhexarg(const char *src, int maxlen, vector<char> &out)
{
    if(!src || !*src) return false;
    for(const char *p = src; *p; p++)
    {
        if(!isxdigit(uchar(*p)) || out.length() >= maxlen) return false;
        out.add(char(tolower(uchar(*p))));
    }
    return true;
}

static void buildsigninmsg(const char *name, const vector<char> &challow, const char *rstr, vector<char> &msg)
{
    msg.setsize(0);
    msg.put(AUTH_SIGNINLABEL, strlen(AUTH_SIGNINLABEL));
    msg.put(name, strlen(name));
    msg.add(' ');
    msg.put(challow.getbuf(), challow.length());
    msg.add(' ');
    msg.put(rstr, strlen(rstr));
}

template<int X_DIGITS> static void wipebigint(bigint<X_DIGITS> &v)
{
    memset(v.digits, 0, sizeof(v.digits));
    v.len = 0;
}

bool signmaster(const char *privstr, const char *name, const char *challenge, char *out, const uchar *randseed, int randlen)
{
    if(!privstr || !validnamearg(name)) return false;
    int plen = 0;
    while(isxdigit(uchar(privstr[plen]))) plen++;
    if(!plen || plen > GF_HEXCHARS || privstr[plen]) return false;
    vector<char> challow;
    if(!lowerhexarg(challenge, 128, challow)) return false;

    gfint rawpriv, d;
    rawpriv.parse(privstr);
    modorder(d, rawpriv);
    wipebigint(rawpriv);
    if(d.iszero()) return false;

    char dhex[GF_HEXCHARS+1];
    printhexpad(d, dhex);
    bool ok = false;
    for(int attempt = 0; attempt < 256; attempt++)
    {
        vector<char> nmsg;
        static const char * const noncelabel = "Master Sign-in Nonce ";
        nmsg.put(noncelabel, strlen(noncelabel));
        nmsg.put(dhex, GF_HEXCHARS);
        nmsg.add(' ');
        nmsg.put(name, strlen(name));
        nmsg.add(' ');
        nmsg.put(challow.getbuf(), challow.length());
        nmsg.add(' ');
        if(randseed && randlen > 0) loopi(randlen)
        {
            nmsg.add("0123456789abcdef"[randseed[i] >> 4]);
            nmsg.add("0123456789abcdef"[randseed[i] & 0xF]);
        }
        nmsg.add("0123456789abcdef"[(attempt >> 4) & 0xF]);
        nmsg.add("0123456789abcdef"[attempt & 0xF]);

        char khex[65];
        sha256impl::hex((const uchar *)nmsg.getbuf(), nmsg.length(), khex);
        if(nmsg.length()) memset(nmsg.getbuf(), 0, nmsg.length());
        bigint<17> kbig;
        kbig.parse(khex);
        memset(khex, 0, sizeof(khex));
        gfint k;
        modorder(k, kbig);
        wipebigint(kbig);
        if(k.iszero()) continue;

        ecjacobian r(ecjacobian::base);
        r.mul(k);
        r.normalize();
        if(r.z.iszero()) { wipebigint(k); continue; }

        char rstr[GF_HEXCHARS+2];
        rstr[0] = r.y.hasbit(0) ? '-' : '+';
        printhexpad(r.x, rstr + 1);

        vector<char> emsg;
        buildsigninmsg(name, challow, rstr, emsg);
        char ehex[65];
        sha256impl::hex((const uchar *)emsg.getbuf(), emsg.length(), ehex);

        bigint<17> ebig;
        ebig.parse(ehex);
        gfint escalar;
        modorder(escalar, ebig);

        bigint<2*GF_DIGITS+2> edprod;
        edprod.mul(escalar, d);
        gfint edmod;
        modorder(edmod, edprod);
        wipebigint(edprod);

        gfint s(k);
        wipebigint(k);
        if(s < edmod) s.add(ecjacobian::order);
        s.sub(edmod);
        wipebigint(edmod);

        memcpy(out, ehex, 64);
        printhexpad(s, out + 64);
        wipebigint(s);
        ok = true;
        break;
    }
    memset(dhex, 0, sizeof(dhex));
    wipebigint(d);
    return ok;
}

bool checkmastersignin(void *pubkey, const char *name, const char *challenge, const char *answer)
{
    if(!pubkey || !validnamearg(name) || !answer) return false;
    vector<char> challow;
    if(!lowerhexarg(challenge, 128, challow)) return false;

    const int siglen = 64 + GF_HEXCHARS;
    int alen = 0;
    while(alen <= siglen && answer[alen])
    {
        if(!isxdigit(uchar(answer[alen]))) return false;
        alen++;
    }
    if(alen != siglen) return false;

    char ehex[65], shex[GF_HEXCHARS+1];
    memcpy(ehex, answer, 64);
    ehex[64] = '\0';
    memcpy(shex, answer + 64, GF_HEXCHARS);
    shex[GF_HEXCHARS] = '\0';

    gfint s;
    s.parse(shex);
    if(s >= ecjacobian::order) return false;

    bigint<17> ebig;
    ebig.parse(ehex);
    gfint escalar;
    modorder(escalar, ebig);

    const ecjacobian &pub = *(const ecjacobian *)pubkey;
    if(pub.z.iszero()) return false;

    ecjacobian r1, r2;
    r1.mul(ecjacobian::base, s);
    r2.mul(pub, escalar);
    r1.add(r2);
    r1.normalize();
    if(r1.z.iszero()) return false;

    char rstr[GF_HEXCHARS+2];
    rstr[0] = r1.y.hasbit(0) ? '-' : '+';
    printhexpad(r1.x, rstr + 1);

    vector<char> emsg;
    buildsigninmsg(name, challow, rstr, emsg);
    char wante[65];
    sha256impl::hex((const uchar *)emsg.getbuf(), emsg.length(), wante);

    int diff = 0;
    loopi(64) diff |= tolower(uchar(ehex[i])) ^ wante[i];
    return !diff;
}

static void hashlabelledraw(const char *label, const char *mid1, const char *mid2, const char *raw, char out[AUTH_HASHANSWER])
{
    while(*raw == '0' && raw[1]) raw++;
    if(!*raw) raw = "0";
    vector<char> msg;
    msg.put(label, strlen(label));
    if(mid1)
    {
        msg.put(mid1, strlen(mid1));
        msg.add(' ');
    }
    if(mid2)
    {
        msg.put(mid2, strlen(mid2));
        msg.add(' ');
    }
    for(; *raw; raw++) msg.add(char(tolower(uchar(*raw))));
    sha256impl::hex((const uchar *)msg.getbuf(), msg.length(), out);
}

static bool matchhash64(const char *answer, const char want[AUTH_HASHANSWER])
{
    if(!answer) return false;
    char got[AUTH_HASHANSWER];
    memset(got, 0, sizeof(got));
    size_t len = strlen(answer);
    memcpy(got, answer, min(len, sizeof(got) - 1));
    int diff = len != 64;
    loopi(64) diff |= tolower(uchar(got[i])) ^ want[i];
    return !diff;
}

bool answergamechallenge(const char *privstr, const char *challenge, const char *name, const char *target, char *out)
{
    if(!validnamearg(name) || !validnamearg(target)) return false;
    vector<char> raw;
    if(!answerchallenge(privstr, challenge, raw)) return false;
    hashlabelledraw(AUTH_GAMELABEL, target, name, raw.getbuf(), out);
    if(raw.length()) memset(raw.getbuf(), 0, raw.length());
    return true;
}

bool checkgamechallenge(const char *answer, void *correct, const char *name, const char *host, int port)
{
    if(!correct || !validnamearg(name) || !host || !*host || port <= 0) return false;
    vector<char> raw;
    challengeanswer(correct, raw);
    defformatstring(pubtarget, "%s:%d", host, port);
    defformatstring(loctarget, "local:%d", port);
    char wantpub[AUTH_HASHANSWER], wantloc[AUTH_HASHANSWER];
    hashlabelledraw(AUTH_GAMELABEL, pubtarget, name, raw.getbuf(), wantpub);
    hashlabelledraw(AUTH_GAMELABEL, loctarget, name, raw.getbuf(), wantloc);
    if(raw.length()) memset(raw.getbuf(), 0, raw.length());
    bool okpub = matchhash64(answer, wantpub), okloc = matchhash64(answer, wantloc);
    return okpub || okloc;
}

bool answerserverchallenge(const char *privstr, const char *challenge, const char *name, char *out)
{
    if(!validnamearg(name)) return false;
    vector<char> raw;
    if(!answerchallenge(privstr, challenge, raw)) return false;
    hashlabelledraw(AUTH_SERVERLABEL, name, NULL, raw.getbuf(), out);
    if(raw.length()) memset(raw.getbuf(), 0, raw.length());
    return true;
}

bool checkserverchallenge(const char *answer, void *correct, const char *name)
{
    if(!correct || !validnamearg(name)) return false;
    vector<char> raw;
    challengeanswer(correct, raw);
    char want[AUTH_HASHANSWER];
    hashlabelledraw(AUTH_SERVERLABEL, name, NULL, raw.getbuf(), want);
    if(raw.length()) memset(raw.getbuf(), 0, raw.length());
    return matchhash64(answer, want);
}

