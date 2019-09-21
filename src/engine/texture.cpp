// texture.cpp: texture slot management

#include "engine.h"
#include "SDL_image.h"

TVAR(IDF_PERSIST|IDF_PRELOAD, notexturetex, "textures/notexture", 3);
TVAR(IDF_PERSIST|IDF_PRELOAD, blanktex, "textures/blank", 3);
TVAR(IDF_PERSIST|IDF_PRELOAD, logotex, "textures/logo", 3);
TVAR(IDF_PERSIST|IDF_PRELOAD, emblemtex, "textures/emblem", 3);
TVAR(IDF_PERSIST|IDF_PRELOAD, nothumbtex, "textures/nothumb", 3);

template<int BPP> static void halvetexture(uchar * RESTRICT src, uint sw, uint sh, uint stride, uchar * RESTRICT dst)
{
    for(uchar *yend = &src[sh*stride]; src < yend;)
    {
        for(uchar *xend = &src[sw*BPP], *xsrc = src; xsrc < xend; xsrc += 2*BPP, dst += BPP)
        {
            loopi(BPP) dst[i] = (uint(xsrc[i]) + uint(xsrc[i+BPP]) + uint(xsrc[stride+i]) + uint(xsrc[stride+i+BPP]))>>2;
        }
        src += 2*stride;
    }
}

template<int BPP> static void shifttexture(uchar * RESTRICT src, uint sw, uint sh, uint stride, uchar * RESTRICT dst, uint dw, uint dh)
{
    uint wfrac = sw/dw, hfrac = sh/dh, wshift = 0, hshift = 0;
    while(dw<<wshift < sw) wshift++;
    while(dh<<hshift < sh) hshift++;
    uint tshift = wshift + hshift;
    for(uchar *yend = &src[sh*stride]; src < yend;)
    {
        for(uchar *xend = &src[sw*BPP], *xsrc = src; xsrc < xend; xsrc += wfrac*BPP, dst += BPP)
        {
            uint t[BPP] = {0};
            for(uchar *ycur = xsrc, *xend = &ycur[wfrac*BPP], *yend = &src[hfrac*stride];
                ycur < yend;
                ycur += stride, xend += stride)
            {
                for(uchar *xcur = ycur; xcur < xend; xcur += BPP)
                    loopi(BPP) t[i] += xcur[i];
            }
            loopi(BPP) dst[i] = t[i] >> tshift;
        }
        src += hfrac*stride;
    }
}

template<int BPP> static void scaletexture(uchar * RESTRICT src, uint sw, uint sh, uint stride, uchar * RESTRICT dst, uint dw, uint dh)
{
    uint wfrac = (sw<<12)/dw, hfrac = (sh<<12)/dh, darea = dw*dh, sarea = sw*sh;
    int over, under;
    for(over = 0; (darea>>over) > sarea; over++);
    for(under = 0; (darea<<under) < sarea; under++);
    uint cscale = clamp(under, over - 12, 12),
         ascale = clamp(12 + under - over, 0, 24),
         dscale = ascale + 12 - cscale,
         area = ((ullong)darea<<ascale)/sarea;
    dw *= wfrac;
    dh *= hfrac;
    for(uint y = 0; y < dh; y += hfrac)
    {
        const uint yn = y + hfrac - 1, yi = y>>12, h = (yn>>12) - yi, ylow = ((yn|(-int(h)>>24))&0xFFFU) + 1 - (y&0xFFFU), yhigh = (yn&0xFFFU) + 1;
        const uchar *ysrc = &src[yi*stride];
        for(uint x = 0; x < dw; x += wfrac, dst += BPP)
        {
            const uint xn = x + wfrac - 1, xi = x>>12, w = (xn>>12) - xi, xlow = ((w+0xFFFU)&0x1000U) - (x&0xFFFU), xhigh = (xn&0xFFFU) + 1;
            const uchar *xsrc = &ysrc[xi*BPP], *xend = &xsrc[w*BPP];
            uint t[BPP] = {0};
            for(const uchar *xcur = &xsrc[BPP]; xcur < xend; xcur += BPP)
                loopi(BPP) t[i] += xcur[i];
            loopi(BPP) t[i] = (ylow*(t[i] + ((xsrc[i]*xlow + xend[i]*xhigh)>>12)))>>cscale;
            if(h)
            {
                xsrc += stride;
                xend += stride;
                for(uint hcur = h; --hcur; xsrc += stride, xend += stride)
                {
                    uint c[BPP] = {0};
                    for(const uchar *xcur = &xsrc[BPP]; xcur < xend; xcur += BPP)
                        loopi(BPP) c[i] += xcur[i];
                    loopi(BPP) t[i] += ((c[i]<<12) + xsrc[i]*xlow + xend[i]*xhigh)>>cscale;
                }
                uint c[BPP] = {0};
                for(const uchar *xcur = &xsrc[BPP]; xcur < xend; xcur += BPP)
                    loopi(BPP) c[i] += xcur[i];
                loopi(BPP) t[i] += (yhigh*(c[i] + ((xsrc[i]*xlow + xend[i]*xhigh)>>12)))>>cscale;
            }
            loopi(BPP) dst[i] = (t[i] * area)>>dscale;
        }
    }
}

static void scaletexture(uchar * RESTRICT src, uint sw, uint sh, uint bpp, uint pitch, uchar * RESTRICT dst, uint dw, uint dh)
{
    if(sw == dw*2 && sh == dh*2)
    {
        switch(bpp)
        {
            case 1: return halvetexture<1>(src, sw, sh, pitch, dst);
            case 2: return halvetexture<2>(src, sw, sh, pitch, dst);
            case 3: return halvetexture<3>(src, sw, sh, pitch, dst);
            case 4: return halvetexture<4>(src, sw, sh, pitch, dst);
        }
    }
    else if(sw < dw || sh < dh || sw&(sw-1) || sh&(sh-1) || dw&(dw-1) || dh&(dh-1))
    {
        switch(bpp)
        {
            case 1: return scaletexture<1>(src, sw, sh, pitch, dst, dw, dh);
            case 2: return scaletexture<2>(src, sw, sh, pitch, dst, dw, dh);
            case 3: return scaletexture<3>(src, sw, sh, pitch, dst, dw, dh);
            case 4: return scaletexture<4>(src, sw, sh, pitch, dst, dw, dh);
        }
    }
    else
    {
        switch(bpp)
        {
            case 1: return shifttexture<1>(src, sw, sh, pitch, dst, dw, dh);
            case 2: return shifttexture<2>(src, sw, sh, pitch, dst, dw, dh);
            case 3: return shifttexture<3>(src, sw, sh, pitch, dst, dw, dh);
            case 4: return shifttexture<4>(src, sw, sh, pitch, dst, dw, dh);
        }
    }
}

static void reorientnormals(uchar * RESTRICT src, int sw, int sh, int bpp, int stride, uchar * RESTRICT dst, bool flipx, bool flipy, bool swapxy)
{
    int stridex = bpp, stridey = bpp;
    if(swapxy) stridex *= sh; else stridey *= sw;
    if(flipx) { dst += (sw-1)*stridex; stridex = -stridex; }
    if(flipy) { dst += (sh-1)*stridey; stridey = -stridey; }
    uchar *srcrow = src;
    loopi(sh)
    {
        for(uchar *curdst = dst, *src = srcrow, *end = &srcrow[sw*bpp]; src < end;)
        {
            uchar nx = *src++, ny = *src++;
            if(flipx) nx = 255-nx;
            if(flipy) ny = 255-ny;
            if(swapxy) swap(nx, ny);
            curdst[0] = nx;
            curdst[1] = ny;
            curdst[2] = *src++;
            if(bpp > 3) curdst[3] = *src++;
            curdst += stridex;
        }
        srcrow += stride;
        dst += stridey;
    }
}

template<int BPP>
static inline void reorienttexture(uchar * RESTRICT src, int sw, int sh, int stride, uchar * RESTRICT dst, bool flipx, bool flipy, bool swapxy)
{
    int stridex = BPP, stridey = BPP;
    if(swapxy) stridex *= sh; else stridey *= sw;
    if(flipx) { dst += (sw-1)*stridex; stridex = -stridex; }
    if(flipy) { dst += (sh-1)*stridey; stridey = -stridey; }
    uchar *srcrow = src;
    loopi(sh)
    {
        for(uchar *curdst = dst, *src = srcrow, *end = &srcrow[sw*BPP]; src < end;)
        {
            loopk(BPP) curdst[k] = *src++;
            curdst += stridex;
        }
        srcrow += stride;
        dst += stridey;
    }
}

static void reorienttexture(uchar * RESTRICT src, int sw, int sh, int bpp, int stride, uchar * RESTRICT dst, bool flipx, bool flipy, bool swapxy)
{
    switch(bpp)
    {
        case 1: return reorienttexture<1>(src, sw, sh, stride, dst, flipx, flipy, swapxy);
        case 2: return reorienttexture<2>(src, sw, sh, stride, dst, flipx, flipy, swapxy);
        case 3: return reorienttexture<3>(src, sw, sh, stride, dst, flipx, flipy, swapxy);
        case 4: return reorienttexture<4>(src, sw, sh, stride, dst, flipx, flipy, swapxy);
    }
}

static void reorients3tc(GLenum format, int blocksize, int w, int h, uchar *src, uchar *dst, bool flipx, bool flipy, bool swapxy, bool normals = false)
{
    int bx1 = 0, by1 = 0, bx2 = min(w, 4), by2 = min(h, 4), bw = (w+3)/4, bh = (h+3)/4, stridex = blocksize, stridey = blocksize;
    if(swapxy) stridex *= bw; else stridey *= bh;
    if(flipx) { dst += (bw-1)*stridex; stridex = -stridex; bx1 += 4-bx2; bx2 = 4; }
    if(flipy) { dst += (bh-1)*stridey; stridey = -stridey; by1 += 4-by2; by2 = 4; }
    loopi(bh)
    {
        for(uchar *curdst = dst, *end = &src[bw*blocksize]; src < end; src += blocksize, curdst += stridex)
        {
            if(format == GL_COMPRESSED_RGBA_S3TC_DXT3_EXT)
            {
                ullong salpha = lilswap(*(const ullong *)src), dalpha = 0;
                uint xmask = flipx ? 15 : 0, ymask = flipy ? 15 : 0, xshift = 2, yshift = 4;
                if(swapxy) swap(xshift, yshift);
                for(int y = by1; y < by2; y++) for(int x = bx1; x < bx2; x++)
                {
                    dalpha |= ((salpha&15) << (((xmask^x)<<xshift) + ((ymask^y)<<yshift)));
                    salpha >>= 4;
                }
                *(ullong *)curdst = lilswap(dalpha);
                src += 8;
                curdst += 8;
            }
            else if(format == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT)
            {
                uchar alpha1 = src[0], alpha2 = src[1];
                ullong salpha = lilswap(*(const ushort *)&src[2]) + ((ullong)lilswap(*(const uint *)&src[4]) << 16), dalpha = 0;
                uint xmask = flipx ? 7 : 0, ymask = flipy ? 7 : 0, xshift = 0, yshift = 2;
                if(swapxy) swap(xshift, yshift);
                for(int y = by1; y < by2; y++) for(int x = bx1; x < bx2; x++)
                {
                    dalpha |= ((salpha&7) << (3*((xmask^x)<<xshift) + ((ymask^y)<<yshift)));
                    salpha >>= 3;
                }
                curdst[0] = alpha1;
                curdst[1] = alpha2;
                *(ushort *)&curdst[2] = lilswap(ushort(dalpha));
                *(ushort *)&curdst[4] = lilswap(ushort(dalpha>>16));
                *(ushort *)&curdst[6] = lilswap(ushort(dalpha>>32));
                src += 8;
                curdst += 8;
            }

            ushort color1 = lilswap(*(const ushort *)src), color2 = lilswap(*(const ushort *)&src[2]);
            uint sbits = lilswap(*(const uint *)&src[4]);
            if(normals)
            {
                ushort ncolor1 = color1, ncolor2 = color2;
                if(flipx)
                {
                    ncolor1 = (ncolor1 & ~0xF800) | (0xF800 - (ncolor1 & 0xF800));
                    ncolor2 = (ncolor2 & ~0xF800) | (0xF800 - (ncolor2 & 0xF800));
                }
                if(flipy)
                {
                    ncolor1 = (ncolor1 & ~0x7E0) | (0x7E0 - (ncolor1 & 0x7E0));
                    ncolor2 = (ncolor2 & ~0x7E0) | (0x7E0 - (ncolor2 & 0x7E0));
                }
                if(swapxy)
                {
                    ncolor1 = (ncolor1 & 0x1F) | (((((ncolor1 >> 11) & 0x1F) * 0x3F) / 0x1F) << 5) | (((((ncolor1 >> 5) & 0x3F) * 0x1F) / 0x3F) << 11);
                    ncolor2 = (ncolor2 & 0x1F) | (((((ncolor2 >> 11) & 0x1F) * 0x3F) / 0x1F) << 5) | (((((ncolor2 >> 5) & 0x3F) * 0x1F) / 0x3F) << 11);
                }
                if(color1 <= color2 && ncolor1 > ncolor2) { color1 = ncolor2; color2 = ncolor1; }
                else { color1 = ncolor1; color2 = ncolor2; }
            }
            uint dbits = 0, xmask = flipx ? 3 : 0, ymask = flipy ? 3 : 0, xshift = 1, yshift = 3;
            if(swapxy) swap(xshift, yshift);
            for(int y = by1; y < by2; y++) for(int x = bx1; x < bx2; x++)
            {
                dbits |= ((sbits&3) << (((xmask^x)<<xshift) + ((ymask^y)<<yshift)));
                sbits >>= 2;
            }
            *(ushort *)curdst = lilswap(color1);
            *(ushort *)&curdst[2] = lilswap(color2);
            *(uint *)&curdst[4] = lilswap(dbits);

            if(blocksize > 8) { src -= 8; curdst -= 8; }
        }
        dst += stridey;
    }
}

static void reorientrgtc(GLenum format, int blocksize, int w, int h, uchar *src, uchar *dst, bool flipx, bool flipy, bool swapxy)
{
    int bx1 = 0, by1 = 0, bx2 = min(w, 4), by2 = min(h, 4), bw = (w+3)/4, bh = (h+3)/4, stridex = blocksize, stridey = blocksize;
    if(swapxy) stridex *= bw; else stridey *= bh;
    if(flipx) { dst += (bw-1)*stridex; stridex = -stridex; bx1 += 4-bx2; bx2 = 4; }
    if(flipy) { dst += (bh-1)*stridey; stridey = -stridey; by1 += 4-by2; by2 = 4; }
    stridex -= blocksize;
    loopi(bh)
    {
        for(uchar *curdst = dst, *end = &src[bw*blocksize]; src < end; curdst += stridex)
        {
            loopj(blocksize/8)
            {
                uchar val1 = src[0], val2 = src[1];
                ullong sval = lilswap(*(const ushort *)&src[2]) + ((ullong)lilswap(*(const uint *)&src[4] )<< 16), dval = 0;
                uint xmask = flipx ? 7 : 0, ymask = flipy ? 7 : 0, xshift = 0, yshift = 2;
                if(swapxy) swap(xshift, yshift);
                for(int y = by1; y < by2; y++) for(int x = bx1; x < bx2; x++)
                {
                    dval |= ((sval&7) << (3*((xmask^x)<<xshift) + ((ymask^y)<<yshift)));
                    sval >>= 3;
                }
                curdst[0] = val1;
                curdst[1] = val2;
                *(ushort *)&curdst[2] = lilswap(ushort(dval));
                *(ushort *)&curdst[4] = lilswap(ushort(dval>>16));
                *(ushort *)&curdst[6] = lilswap(ushort(dval>>32));
                src += 8;
                curdst += 8;
            }
        }
        dst += stridey;
    }
}

#define writetex(t, body) do \
    { \
        uchar *dstrow = t.data; \
        loop(y, t.h) \
        { \
            for(uchar *dst = dstrow, *end = &dstrow[t.w*t.bpp]; dst < end; dst += t.bpp) \
            { \
                body; \
            } \
            dstrow += t.pitch; \
        } \
    } while(0)

#define readwritetex(t, s, body) do \
    { \
        uchar *dstrow = t.data, *srcrow = s.data; \
        loop(y, t.h) \
        { \
            for(uchar *dst = dstrow, *src = srcrow, *end = &srcrow[s.w*s.bpp]; src < end; dst += t.bpp, src += s.bpp) \
            { \
                body; \
            } \
            dstrow += t.pitch; \
            srcrow += s.pitch; \
        } \
    } while(0)

#define read2writetex(t, s1, src1, s2, src2, body) do \
    { \
        uchar *dstrow = t.data, *src1row = s1.data, *src2row = s2.data; \
        loop(y, t.h) \
        { \
            for(uchar *dst = dstrow, *end = &dstrow[t.w*t.bpp], *src1 = src1row, *src2 = src2row; dst < end; dst += t.bpp, src1 += s1.bpp, src2 += s2.bpp) \
            { \
                body; \
            } \
            dstrow += t.pitch; \
            src1row += s1.pitch; \
            src2row += s2.pitch; \
        } \
    } while(0)

#define readwritergbtex(t, s, body) \
    { \
        if(t.bpp >= 3) readwritetex(t, s, body); \
        else \
        { \
            ImageData rgb(t.w, t.h, 3); \
            read2writetex(rgb, t, orig, s, src, { dst[0] = dst[1] = dst[2] = orig[0]; body; }); \
            t.replace(rgb); \
        } \
    }

void forcergbimage(ImageData &s)
{
    if(s.bpp >= 3) return;
    ImageData d(s.w, s.h, 3);
    readwritetex(d, s, { dst[0] = dst[1] = dst[2] = src[0]; });
    s.replace(d);
}

#define readwritergbatex(t, s, body) \
    { \
        if(t.bpp >= 4) { readwritetex(t, s, body); } \
        else \
        { \
            ImageData rgba(t.w, t.h, 4); \
            if(t.bpp==3) read2writetex(rgba, t, orig, s, src, { dst[0] = orig[0]; dst[1] = orig[1]; dst[2] = orig[2]; body; }); \
            else read2writetex(rgba, t, orig, s, src, { dst[0] = dst[1] = dst[2] = orig[0]; body; }); \
            t.replace(rgba); \
        } \
    }

void forcergbaimage(ImageData &s)
{
    if(s.bpp >= 4) return;
    ImageData d(s.w, s.h, 4);
    if(s.bpp==3) readwritetex(d, s, { dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2]; });
    else readwritetex(d, s, { dst[0] = dst[1] = dst[2] = src[0]; });
    s.replace(d);
}

void swizzleimage(ImageData &s)
{
    if(s.bpp==2)
    {
        ImageData d(s.w, s.h, 4);
        readwritetex(d, s, { dst[0] = dst[1] = dst[2] = src[0]; dst[3] = src[1]; });
        s.replace(d);
    }
    else if(s.bpp==1)
    {
        ImageData d(s.w, s.h, 3);
        readwritetex(d, s, { dst[0] = dst[1] = dst[2] = src[0]; });
        s.replace(d);
    }
}

void scaleimage(ImageData &s, int w, int h)
{
    ImageData d(w, h, s.bpp);
    scaletexture(s.data, s.w, s.h, s.bpp, s.pitch, d.data, w, h);
    s.replace(d);
}

void texreorient(ImageData &s, bool flipx, bool flipy, bool swapxy, int type = TEX_DIFFUSE)
{
    ImageData d(swapxy ? s.h : s.w, swapxy ? s.w : s.h, s.bpp, s.levels, s.align, s.compressed);
    switch(s.compressed)
    {
    case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        {
            uchar *dst = d.data, *src = s.data;
            loopi(s.levels)
            {
                reorients3tc(s.compressed, s.bpp, max(s.w>>i, 1), max(s.h>>i, 1), src, dst, flipx, flipy, swapxy, type==TEX_NORMAL);
                src += s.calclevelsize(i);
                dst += d.calclevelsize(i);
            }
            break;
        }
    case GL_COMPRESSED_RED_RGTC1:
    case GL_COMPRESSED_RG_RGTC2:
    case GL_COMPRESSED_LUMINANCE_LATC1_EXT:
    case GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT:
        {
            uchar *dst = d.data, *src = s.data;
            loopi(s.levels)
            {
                reorientrgtc(s.compressed, s.bpp, max(s.w>>i, 1), max(s.h>>i, 1), src, dst, flipx, flipy, swapxy);
                src += s.calclevelsize(i);
                dst += d.calclevelsize(i);
            }
            break;
        }
    default:
        if(type==TEX_NORMAL && s.bpp >= 3) reorientnormals(s.data, s.w, s.h, s.bpp, s.pitch, d.data, flipx, flipy, swapxy);
        else reorienttexture(s.data, s.w, s.h, s.bpp, s.pitch, d.data, flipx, flipy, swapxy);
        break;
    }
    s.replace(d);
}

extern const texrotation texrotations[8] =
{
    { false, false, false }, // 0: default
    { false,  true,  true }, // 1: 90 degrees
    {  true,  true, false }, // 2: 180 degrees
    {  true, false,  true }, // 3: 270 degrees
    {  true, false, false }, // 4: flip X
    { false,  true, false }, // 5: flip Y
    { false, false,  true }, // 6: transpose
    {  true,  true,  true }, // 7: flipped transpose
};

void texrotate(ImageData &s, int numrots, int type = TEX_DIFFUSE)
{
    if(numrots >= 1 && numrots <= 7)
    {
        const texrotation &r = texrotations[numrots];
        texreorient(s, r.flipx, r.flipy, r.swapxy, type);
    }
}

void texoffset(ImageData &s, int xoffset, int yoffset)
{
    xoffset = max(xoffset, 0);
    xoffset %= s.w;
    yoffset = max(yoffset, 0);
    yoffset %= s.h;
    if(!xoffset && !yoffset) return;
    ImageData d(s.w, s.h, s.bpp);
    uchar *src = s.data;
    loop(y, s.h)
    {
        uchar *dst = (uchar *)d.data+((y+yoffset)%d.h)*d.pitch;
        memcpy(dst+xoffset*s.bpp, src, (s.w-xoffset)*s.bpp);
        memcpy(dst, src+(s.w-xoffset)*s.bpp, xoffset*s.bpp);
        src += s.pitch;
    }
    s.replace(d);
}

void texcrop(ImageData &s, ImageData &d, int x, int y, int w, int h)
{
    x = clamp(x, 0, s.w);
    y = clamp(y, 0, s.h);
    w = min(w < 0 ? s.w : w, s.w - x);
    h = min(h < 0 ? s.h : h, s.h - y);
    if(!w || !h) return;
    d.setdata(NULL, w, h, s.bpp);
    uchar *dst = d.data, *src = &s.data[y*s.pitch + x*s.bpp];
    loopi(h)
    {
        memcpy(dst, src, w*s.bpp);
        src += s.pitch;
        dst += d.pitch;
    }
}

void texcrop(ImageData &s, int x, int y, int w, int h)
{
    x = clamp(x, 0, s.w);
    y = clamp(y, 0, s.h);
    w = min(w < 0 ? s.w : w, s.w - x);
    h = min(h < 0 ? s.h : h, s.h - y);
    if(!w || !h) return;
    ImageData d;
    texcrop(s, d, x, y, w, h);
    s.replace(d);
}

void texmad(ImageData &s, const vec &mul, const vec &add)
{
    if(s.bpp < 3 && (mul.x != mul.y || mul.y != mul.z || add.x != add.y || add.y != add.z))
        swizzleimage(s);
    int maxk = min(int(s.bpp), 3);
    writetex(s,
        loopk(maxk) dst[k] = uchar(clamp(dst[k]*mul[k] + 255*add[k], 0.0f, 255.0f));
    );
}

void texcolorify(ImageData &s, const vec &color, vec weights)
{
    if(s.bpp < 3) return;
    if(weights.iszero()) weights = vec(0.21f, 0.72f, 0.07f);
    writetex(s,
        float lum = dst[0]*weights.x + dst[1]*weights.y + dst[2]*weights.z;
        loopk(3) dst[k] = uchar(clamp(lum*color[k], 0.0f, 255.0f));
    );
}

void texcolormask(ImageData &s, const vec &color1, const vec &color2)
{
    if(s.bpp < 4) return;
    ImageData d(s.w, s.h, 3);
    readwritetex(d, s,
        vec color;
        color.lerp(color2, color1, src[3]/255.0f);
        loopk(3) dst[k] = uchar(clamp(color[k]*src[k], 0.0f, 255.0f));
    );
    s.replace(d);
}

void texinvert(ImageData &d)
{
    writetex(d,
        if(d.bpp >= 3) loopk(3) dst[k] = 255-dst[k];
        else dst[0] = 255-dst[0];
    );
}

void texdup(ImageData &s, int srcchan, int dstchan)
{
    if(srcchan==dstchan || max(srcchan, dstchan) >= s.bpp) return;
    writetex(s, dst[dstchan] = dst[srcchan]);
}

void texmix(ImageData &s, int c1, int c2, int c3, int c4)
{
    int numchans = c1 < 0 ? 0 : (c2 < 0 ? 1 : (c3 < 0 ? 2 : (c4 < 0 ? 3 : 4)));
    if(numchans <= 0) return;
    ImageData d(s.w, s.h, numchans);
    readwritetex(d, s,
        switch(numchans)
        {
            case 4: dst[3] = c4 != 255 ? src[c4] : 255;
            case 3: dst[2] = c3 != 255 ? src[c3] : 255;
            case 2: dst[1] = c2 != 255 ? src[c2] : 255;
            case 1: dst[0] = c1 != 255 ? src[c1] : 255;
        }
    );
    s.replace(d);
}

void texgrey(ImageData &s)
{
    if(s.bpp <= 2) return;
    ImageData d(s.w, s.h, s.bpp >= 4 ? 2 : 1);
    if(s.bpp >= 4)
    {
        readwritetex(d, s,
            dst[0] = src[0];
            dst[1] = src[3];
        );
    }
    else
    {
        readwritetex(d, s, dst[0] = src[0]);
    }
    s.replace(d);
}

void texpremul(ImageData &s)
{
    switch(s.bpp)
    {
        case 2:
            writetex(s,
                dst[0] = uchar((uint(dst[0])*uint(dst[1]))/255);
            );
            break;
        case 4:
            writetex(s,
                uint alpha = dst[3];
                dst[0] = uchar((uint(dst[0])*alpha)/255);
                dst[1] = uchar((uint(dst[1])*alpha)/255);
                dst[2] = uchar((uint(dst[2])*alpha)/255);
            );
            break;
    }
}

void texagrad(ImageData &s, float x2, float y2, float x1, float y1)
{
    if(s.bpp != 2 && s.bpp != 4) return;
    y1 = 1 - y1;
    y2 = 1 - y2;
    float minx = 1, miny = 1, maxx = 1, maxy = 1;
    if(x1 != x2)
    {
        minx = (0 - x1) / (x2 - x1);
        maxx = (1 - x1) / (x2 - x1);
    }
    if(y1 != y2)
    {
        miny = (0 - y1) / (y2 - y1);
        maxy = (1 - y1) / (y2 - y1);
    }
    float dx = (maxx - minx)/max(s.w-1, 1),
          dy = (maxy - miny)/max(s.h-1, 1),
          cury = miny;
    for(uchar *dstrow = s.data + s.bpp - 1, *endrow = dstrow + s.h*s.pitch; dstrow < endrow; dstrow += s.pitch)
    {
        float curx = minx;
        for(uchar *dst = dstrow, *end = &dstrow[s.w*s.bpp]; dst < end; dst += s.bpp)
        {
            dst[0] = uchar(dst[0]*clamp(curx, 0.0f, 1.0f)*clamp(cury, 0.0f, 1.0f));
            curx += dx;
        }
        cury += dy;
    }
}

void texblend(ImageData &d, ImageData &s, ImageData &m)
{
    if(s.w != d.w || s.h != d.h) scaleimage(s, d.w, d.h);
    if(m.w != d.w || m.h != d.h) scaleimage(m, d.w, d.h);
    if(&s == &m)
    {
        if(s.bpp == 2)
        {
            if(d.bpp >= 3) swizzleimage(s);
        }
        else if(s.bpp == 4)
        {
            if(d.bpp < 3) swizzleimage(d);
        }
        else return;
        if(d.bpp < 3) readwritetex(d, s,
            int srcblend = src[1];
            int dstblend = 255 - srcblend;
            dst[0] = uchar((dst[0]*dstblend + src[0]*srcblend)/255);
        );
        else readwritetex(d, s,
            int srcblend = src[3];
            int dstblend = 255 - srcblend;
            dst[0] = uchar((dst[0]*dstblend + src[0]*srcblend)/255);
            dst[1] = uchar((dst[1]*dstblend + src[1]*srcblend)/255);
            dst[2] = uchar((dst[2]*dstblend + src[2]*srcblend)/255);
        );
    }
    else
    {
        if(s.bpp < 3)
        {
            if(d.bpp >= 3) swizzleimage(s);
        }
        else if(d.bpp < 3) swizzleimage(d);
        if(d.bpp < 3) read2writetex(d, s, src, m, mask,
            int srcblend = mask[0];
            int dstblend = 255 - srcblend;
            dst[0] = uchar((dst[0]*dstblend + src[0]*srcblend)/255);
        );
        else read2writetex(d, s, src, m, mask,
            int srcblend = mask[0];
            int dstblend = 255 - srcblend;
            dst[0] = uchar((dst[0]*dstblend + src[0]*srcblend)/255);
            dst[1] = uchar((dst[1]*dstblend + src[1]*srcblend)/255);
            dst[2] = uchar((dst[2]*dstblend + src[2]*srcblend)/255);
        );
    }
}

VAR(0, hwtexsize, 1, 0, 0);
VAR(0, hwcubetexsize, 1, 0, 0);
VAR(0, hwmaxaniso, 1, 0, 0);
VAR(0, hwtexunits, 1, 0, 0);
VAR(0, hwvtexunits, 1, 0, 0);
VARF(IDF_PERSIST, maxtexsize, 0, 0, 1<<12, initwarning("texture quality", INIT_LOAD));
VARF(IDF_PERSIST, reducefilter, 0, 1, 1, initwarning("texture quality", INIT_LOAD));
VARF(IDF_PERSIST, texreduce, 0, 0, 12, initwarning("texture quality", INIT_LOAD));
VARF(IDF_PERSIST, texcompress, 0, 1536, 1<<12, initwarning("texture quality", INIT_LOAD));
VARF(IDF_PERSIST, texcompressquality, -1, -1, 1, setuptexcompress());
VARF(0, trilinear, 0, 1, 1, initwarning("texture filtering", INIT_LOAD));
VARF(0, bilinear, 0, 1, 1, initwarning("texture filtering", INIT_LOAD));
VARF(IDF_PERSIST, aniso, 0, 0, 16, initwarning("texture filtering", INIT_LOAD));

extern int usetexcompress;

void setuptexcompress()
{
    if(!usetexcompress) return;

    GLenum hint = GL_DONT_CARE;
    switch(texcompressquality)
    {
        case 1: hint = GL_NICEST; break;
        case 0: hint = GL_FASTEST; break;
    }
    glHint(GL_TEXTURE_COMPRESSION_HINT, hint);
}

GLenum compressedformat(GLenum format, int w, int h, int force = 0)
{
    if(usetexcompress && texcompress && force >= 0 && (force || max(w, h) >= texcompress)) switch(format)
    {
        case GL_RGB5:
        case GL_RGB8:
        case GL_RGB: return usetexcompress > 1 ? GL_COMPRESSED_RGB_S3TC_DXT1_EXT : GL_COMPRESSED_RGB;
        case GL_RGB5_A1: return usetexcompress > 1 ? GL_COMPRESSED_RGBA_S3TC_DXT1_EXT : GL_COMPRESSED_RGBA;
        case GL_RGBA: return usetexcompress > 1 ? GL_COMPRESSED_RGBA_S3TC_DXT5_EXT : GL_COMPRESSED_RGBA;
        case GL_RED:
        case GL_R8: return hasRGTC ? (usetexcompress > 1 ? GL_COMPRESSED_RED_RGTC1 : GL_COMPRESSED_RED) : (usetexcompress > 1 ? GL_COMPRESSED_RGB_S3TC_DXT1_EXT : GL_COMPRESSED_RGB);
        case GL_RG:
        case GL_RG8: return hasRGTC ? (usetexcompress > 1 ? GL_COMPRESSED_RG_RGTC2 : GL_COMPRESSED_RG) : (usetexcompress > 1 ? GL_COMPRESSED_RGBA_S3TC_DXT5_EXT : GL_COMPRESSED_RGBA);
        case GL_LUMINANCE:
        case GL_LUMINANCE8: return hasLATC ? (usetexcompress > 1 ? GL_COMPRESSED_LUMINANCE_LATC1_EXT : GL_COMPRESSED_LUMINANCE) : (usetexcompress > 1 ? GL_COMPRESSED_RGB_S3TC_DXT1_EXT : GL_COMPRESSED_RGB);
        case GL_LUMINANCE_ALPHA:
        case GL_LUMINANCE8_ALPHA8: return hasLATC ? (usetexcompress > 1 ? GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT : GL_COMPRESSED_LUMINANCE_ALPHA) : (usetexcompress > 1 ? GL_COMPRESSED_RGBA_S3TC_DXT5_EXT : GL_COMPRESSED_RGBA);
    }
    return format;
}

int formatsize(GLenum format)
{
    switch(format)
    {
        case GL_RED:
        case GL_LUMINANCE:
        case GL_ALPHA: return 1;
        case GL_RG:
        case GL_LUMINANCE_ALPHA: return 2;
        case GL_RGB: return 3;
        case GL_RGBA: return 4;
        default: return 4;
    }
}

VARF(IDF_PERSIST, usenp2, 0, 1, 1, initwarning("texture quality", INIT_LOAD));

void resizetexture(int w, int h, bool mipmap, bool canreduce, GLenum target, int compress, int &tw, int &th)
{
    int hwlimit = target==GL_TEXTURE_CUBE_MAP ? hwcubetexsize : hwtexsize,
        sizelimit = mipmap && maxtexsize ? min(maxtexsize, hwlimit) : hwlimit;
    if(compress > 0 && !usetexcompress)
    {
        w = max(w/compress, 1);
        h = max(h/compress, 1);
    }
    if(canreduce && texreduce)
    {
        w = max(w>>texreduce, 1);
        h = max(h>>texreduce, 1);
    }
    w = min(w, sizelimit);
    h = min(h, sizelimit);
    if(!usenp2 && target!=GL_TEXTURE_RECTANGLE && (w&(w-1) || h&(h-1)))
    {
        tw = th = 1;
        while(tw < w) tw *= 2;
        while(th < h) th *= 2;
        if(w < tw - tw/4) tw /= 2;
        if(h < th - th/4) th /= 2;
    }
    else
    {
        tw = w;
        th = h;
    }
}

void uploadtexture(GLenum target, GLenum internal, int tw, int th, GLenum format, GLenum type, const void *pixels, int pw, int ph, int pitch, bool mipmap)
{
    int bpp = formatsize(format), row = 0, rowalign = 0;
    if(!pitch) pitch = pw*bpp;
    uchar *buf = NULL;
    if(pw!=tw || ph!=th)
    {
        buf = new uchar[tw*th*bpp];
        scaletexture((uchar *)pixels, pw, ph, bpp, pitch, buf, tw, th);
    }
    else if(tw*bpp != pitch)
    {
        row = pitch/bpp;
        rowalign = texalign(pixels, pitch, 1);
        while(rowalign > 0 && ((row*bpp + rowalign - 1)/rowalign)*rowalign != pitch) rowalign >>= 1;
        if(!rowalign)
        {
            row = 0;
            buf = new uchar[tw*th*bpp];
            loopi(th) memcpy(&buf[i*tw*bpp], &((uchar *)pixels)[i*pitch], tw*bpp);
        }
    }
    for(int level = 0, align = 0;; level++)
    {
        uchar *src = buf ? buf : (uchar *)pixels;
        if(buf) pitch = tw*bpp;
        int srcalign = row > 0 ? rowalign : texalign(src, pitch, 1);
        if(align != srcalign) glPixelStorei(GL_UNPACK_ALIGNMENT, align = srcalign);
        if(row > 0) glPixelStorei(GL_UNPACK_ROW_LENGTH, row);
        if(target==GL_TEXTURE_1D) glTexImage1D(target, level, internal, tw, 0, format, type, src);
        else glTexImage2D(target, level, internal, tw, th, 0, format, type, src);
        if(row > 0) glPixelStorei(GL_UNPACK_ROW_LENGTH, row = 0);
        if(!mipmap || max(tw, th) <= 1) break;
        int srcw = tw, srch = th;
        if(tw > 1) tw /= 2;
        if(th > 1) th /= 2;
        if(src)
        {
            if(!buf) buf = new uchar[tw*th*bpp];
            scaletexture(src, srcw, srch, bpp, pitch, buf, tw, th);
        }
    }
    if(buf) delete[] buf;
}

void uploadcompressedtexture(GLenum target, GLenum subtarget, GLenum format, int w, int h, const uchar *data, int align, int blocksize, int levels, bool mipmap)
{
    int hwlimit = target==GL_TEXTURE_CUBE_MAP ? hwcubetexsize : hwtexsize,
        sizelimit = levels > 1 && maxtexsize ? min(maxtexsize, hwlimit) : hwlimit;
    int level = 0;
    loopi(levels)
    {
        int size = ((w + align-1)/align) * ((h + align-1)/align) * blocksize;
        if(w <= sizelimit && h <= sizelimit)
        {
            if(target==GL_TEXTURE_1D) glCompressedTexImage1D_(subtarget, level, format, w, 0, size, data);
            else glCompressedTexImage2D_(subtarget, level, format, w, h, 0, size, data);
            level++;
            if(!mipmap) break;
        }
        if(max(w, h) <= 1) break;
        if(w > 1) w /= 2;
        if(h > 1) h /= 2;
        data += size;
    }
}

GLenum textarget(GLenum subtarget)
{
    switch(subtarget)
    {
        case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
            return GL_TEXTURE_CUBE_MAP;
    }
    return subtarget;
}

GLenum uncompressedformat(GLenum format)
{
    switch(format)
    {
        case GL_COMPRESSED_ALPHA:
            return GL_ALPHA;
        case GL_COMPRESSED_LUMINANCE:
        case GL_COMPRESSED_LUMINANCE_LATC1_EXT:
            return GL_LUMINANCE;
        case GL_COMPRESSED_LUMINANCE_ALPHA:
        case GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT:
            return GL_LUMINANCE_ALPHA;
        case GL_COMPRESSED_RED:
        case GL_COMPRESSED_RED_RGTC1:
            return GL_RED;
        case GL_COMPRESSED_RG:
        case GL_COMPRESSED_RG_RGTC2:
            return GL_RG;
        case GL_COMPRESSED_RGB:
        case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
            return GL_RGB;
        case GL_COMPRESSED_RGBA:
        case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
            return GL_RGBA;
    }
    return GL_FALSE;
}

const GLint *swizzlemask(GLenum format)
{
    static const GLint luminance[4] = { GL_RED, GL_RED, GL_RED, GL_ONE };
    static const GLint luminancealpha[4] = { GL_RED, GL_RED, GL_RED, GL_GREEN };
    switch(format)
    {
        case GL_RED: return luminance;
        case GL_RG: return luminancealpha;
    }
    return NULL;
}

void setuptexparameters(int tnum, const void *pixels, int clamp, int filter, GLenum format, GLenum target, bool swizzle)
{
    glBindTexture(target, tnum);
    glTexParameteri(target, GL_TEXTURE_WRAP_S, clamp&0x001 ? GL_CLAMP_TO_EDGE : (clamp&0x1000 ? GL_CLAMP_TO_BORDER : (clamp&0x100 ? GL_MIRRORED_REPEAT : GL_REPEAT)));
    if(target!=GL_TEXTURE_1D) glTexParameteri(target, GL_TEXTURE_WRAP_T, clamp&0x002 ? GL_CLAMP_TO_EDGE : (clamp&0x2000 ? GL_CLAMP_TO_BORDER : (clamp&0x200 ? GL_MIRRORED_REPEAT : GL_REPEAT)));
    if(target==GL_TEXTURE_3D) glTexParameteri(target, GL_TEXTURE_WRAP_R, clamp&0x004 ? GL_CLAMP_TO_EDGE : (clamp&0x4000 ? GL_CLAMP_TO_BORDER : (clamp&0x400 ? GL_MIRRORED_REPEAT : GL_REPEAT)));
    if(target==GL_TEXTURE_2D && hasAF && min(aniso, hwmaxaniso) > 0 && filter > 1) glTexParameteri(target, GL_TEXTURE_MAX_ANISOTROPY_EXT, min(aniso, hwmaxaniso));
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, !(clamp&0x8000) && filter && bilinear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER,
        !(clamp&0x8000) && filter > 1 ?
            (trilinear ?
                (bilinear ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR) :
                (bilinear ? GL_LINEAR_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_NEAREST)) :
            (!(clamp&0x8000) && filter && bilinear ? GL_LINEAR : GL_NEAREST));
    if(swizzle && hasTRG && hasTSW)
    {
        const GLint *mask = swizzlemask(format);
        if(mask) glTexParameteriv(target, GL_TEXTURE_SWIZZLE_RGBA, mask);
    }
}

static GLenum textype(GLenum &component, GLenum &format)
{
    GLenum type = GL_UNSIGNED_BYTE;
    switch(component)
    {
        case GL_R16F:
        case GL_R32F:
            if(!format) format = GL_RED;
            type = GL_FLOAT;
            break;

        case GL_RG16F:
        case GL_RG32F:
            if(!format) format = GL_RG;
            type = GL_FLOAT;
            break;

        case GL_RGB16F:
        case GL_RGB32F:
        case GL_R11F_G11F_B10F:
            if(!format) format = GL_RGB;
            type = GL_FLOAT;
            break;

        case GL_RGBA16F:
        case GL_RGBA32F:
            if(!format) format = GL_RGBA;
            type = GL_FLOAT;
            break;

        case GL_DEPTH_COMPONENT16:
        case GL_DEPTH_COMPONENT24:
        case GL_DEPTH_COMPONENT32:
            if(!format) format = GL_DEPTH_COMPONENT;
            break;

        case GL_DEPTH_STENCIL:
        case GL_DEPTH24_STENCIL8:
            if(!format) format = GL_DEPTH_STENCIL;
            type = GL_UNSIGNED_INT_24_8;
            break;

        case GL_R8:
        case GL_R16:
        case GL_COMPRESSED_RED:
        case GL_COMPRESSED_RED_RGTC1:
            if(!format) format = GL_RED;
            break;

        case GL_RG8:
        case GL_RG16:
        case GL_COMPRESSED_RG:
        case GL_COMPRESSED_RG_RGTC2:
            if(!format) format = GL_RG;
            break;

        case GL_RGB5:
        case GL_RGB8:
        case GL_RGB16:
        case GL_RGB10:
        case GL_COMPRESSED_RGB:
        case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
            if(!format) format = GL_RGB;
            break;

        case GL_RGB5_A1:
        case GL_RGBA8:
        case GL_RGBA16:
        case GL_RGB10_A2:
        case GL_COMPRESSED_RGBA:
        case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
            if(!format) format = GL_RGBA;
            break;

        case GL_LUMINANCE8:
        case GL_LUMINANCE16:
        case GL_COMPRESSED_LUMINANCE:
        case GL_COMPRESSED_LUMINANCE_LATC1_EXT:
            if(!format) format = GL_LUMINANCE;
            break;

        case GL_LUMINANCE8_ALPHA8:
        case GL_LUMINANCE16_ALPHA16:
        case GL_COMPRESSED_LUMINANCE_ALPHA:
        case GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT:
            if(!format) format = GL_LUMINANCE_ALPHA;
            break;

        case GL_ALPHA8:
        case GL_ALPHA16:
        case GL_COMPRESSED_ALPHA:
            if(!format) format = GL_ALPHA;
            break;

        case GL_RGB8UI:
        case GL_RGB16UI:
        case GL_RGB32UI:
        case GL_RGB8I:
        case GL_RGB16I:
        case GL_RGB32I:
            if(!format) format = GL_RGB_INTEGER;
            break;

        case GL_RGBA8UI:
        case GL_RGBA16UI:
        case GL_RGBA32UI:
        case GL_RGBA8I:
        case GL_RGBA16I:
        case GL_RGBA32I:
            if(!format) format = GL_RGBA_INTEGER;
            break;

        case GL_R8UI:
        case GL_R16UI:
        case GL_R32UI:
        case GL_R8I:
        case GL_R16I:
        case GL_R32I:
            if(!format) format = GL_RED_INTEGER;
            break;

        case GL_RG8UI:
        case GL_RG16UI:
        case GL_RG32UI:
        case GL_RG8I:
        case GL_RG16I:
        case GL_RG32I:
            if(!format) format = GL_RG_INTEGER;
            break;
    }
    if(!format) format = component;
    return type;
}

void createtexture(int tnum, int w, int h, const void *pixels, int clamp, int filter, GLenum component, GLenum subtarget, int pw, int ph, int pitch, bool resize, GLenum format, bool swizzle)
{
    GLenum target = textarget(subtarget), type = textype(component, format);
    if(tnum) setuptexparameters(tnum, pixels, clamp, filter, format, target, swizzle);
    if(!pw) pw = w;
    if(!ph) ph = h;
    int tw = w, th = h;
    bool mipmap = filter > 1;
    if(resize && pixels)
    {
        resizetexture(w, h, mipmap, false, target, 0, tw, th);
        if(mipmap) component = compressedformat(component, tw, th);
    }
    uploadtexture(subtarget, component, tw, th, format, type, pixels, pw, ph, pitch, mipmap);
}

void createcompressedtexture(int tnum, int w, int h, const uchar *data, int align, int blocksize, int levels, int clamp, int filter, GLenum format, GLenum subtarget, bool swizzle = false)
{
    GLenum target = textarget(subtarget);
    if(tnum) setuptexparameters(tnum, data, clamp, filter, format, target, swizzle);
    uploadcompressedtexture(target, subtarget, format, w, h, data, align, blocksize, levels, filter > 1);
}

void create3dtexture(int tnum, int w, int h, int d, const void *pixels, int clamp, int filter, GLenum component, GLenum target, bool swizzle)
{
    GLenum format = GL_FALSE, type = textype(component, format);
    if(tnum) setuptexparameters(tnum, pixels, clamp, filter, format, target, swizzle);
    glTexImage3D_(target, 0, component, w, h, d, 0, format, type, pixels);
}

hashnameset<Texture> textures;
vector<Texture *> animtextures;

Texture *notexture = NULL, *blanktexture = NULL; // used as default, ensured to be loaded

static void updatetexture(Texture *t)
{
    if(t->frames.length() <= 1 || t->delay <= 0) return;
    int elapsed = lastmillis-t->last;
    if(elapsed < t->delay) return;
    int animlen = t->throb ? (t->frames.length()-1)*2 : t->frames.length();
    t->frame += elapsed/t->delay;
    t->frame %= animlen;
    t->last = lastmillis-(lastmillis%t->delay);
    int frame = t->throb && t->frame >= t->frames.length() ? animlen-t->frame : t->frame;
    t->id = t->frames.inrange(frame) ? t->frames[frame] : 0;
}

void updatetextures()
{
    loopv(animtextures) updatetexture(animtextures[i]);
}

void preloadtextures(uint flags)
{
    enumerate(idents, ident, id, {
        if(id.type == ID_SVAR && (id.flags&IDF_TEXTURE) && (id.flags&(IDF_PRELOAD|IDF_GAMEPRELOAD)) == flags)
            id.changed();
    });
}

static GLenum texformat(int bpp, bool swizzle = false)
{
    switch(bpp)
    {
        case 1: return hasTRG && (hasTSW || !glcompat || !swizzle) ? GL_RED : GL_LUMINANCE;
        case 2: return hasTRG && (hasTSW || !glcompat || !swizzle) ? GL_RG : GL_LUMINANCE_ALPHA;
        case 3: return GL_RGB;
        case 4: return GL_RGBA;
        default: return 0;
    }
}

static bool alphaformat(GLenum format)
{
    switch(format)
    {
        case GL_ALPHA:
        case GL_LUMINANCE_ALPHA:
        case GL_RG:
        case GL_RGBA:
            return true;
        default:
            return false;
    }
}

int texalign(const void *data, int w, int bpp)
{
    int stride = w*bpp;
    if(stride&1) return 1;
    if(stride&2) return 2;
    return 4;
}

bool floatformat(GLenum format)
{
    switch(format)
    {
        case GL_R16F:
        case GL_R32F:
        case GL_RG16F:
        case GL_RG32F:
        case GL_RGB16F:
        case GL_RGB32F:
        case GL_R11F_G11F_B10F:
        case GL_RGBA16F:
        case GL_RGBA32F:
            return true;
        default:
            return false;
    }
}

static Texture *newtexture(Texture *t, const char *rname, ImageData &s, int clamp = 0, bool mipit = true, bool canreduce = false, bool transient = false, int compress = 0, TextureAnim *anim = NULL)
{
    if(!t)
    {
        char *key = newstring(rname);
        t = &textures[key];
        t->name = key;
        t->frames.shrink(0);
        t->frame = 0;
    }

    t->clamp = clamp;
    t->mipmap = mipit;
    t->type = Texture::IMAGE;
    if(transient) t->type |= Texture::TRANSIENT;
    if(clamp&0x300) t->type |= Texture::MIRROR;
    if(!s.data)
    {
        t->type |= Texture::STUB;
        t->w = t->h = t->xs = t->ys = t->bpp = 0;
        return t;
    }

    bool swizzle = !(clamp&0x10000);
    GLenum format;
    if(s.compressed)
    {
        format = uncompressedformat(s.compressed);
        t->bpp = formatsize(format);
        t->type |= Texture::COMPRESSED;
    }
    else
    {
        format = texformat(s.bpp, swizzle);
        t->bpp = s.bpp;
        if(swizzle && hasTRG && !hasTSW && swizzlemask(format))
        {
            swizzleimage(s);
            format = texformat(s.bpp, swizzle);
            t->bpp = s.bpp;
        }
    }
    if(alphaformat(format)) t->type |= Texture::ALPHA;

    bool hasanim = anim && anim->count;
    t->delay = hasanim ? anim->delay : 0;
    t->throb = hasanim ? anim->throb : false;

    t->w = t->xs = hasanim ? anim->w : s.w;
    t->h = t->ys = hasanim ? anim->h : s.h;
    if(t->frames.empty()) t->frames.add(0);

    int filter = !canreduce || reducefilter ? (mipit ? 2 : 1) : 0;
    if(t->frames.empty()) t->frames.add(0);
    glGenTextures(1, &t->frames[0]);
    if(s.compressed)
    {
        uchar *data = s.data;
        int levels = s.levels, level = 0;
        if(canreduce && texreduce) loopi(min(texreduce, s.levels-1))
        {
            data += s.calclevelsize(level++);
            levels--;
            if(t->w > 1) t->w /= 2;
            if(t->h > 1) t->h /= 2;
        }
        int sizelimit = mipit && maxtexsize ? min(maxtexsize, hwtexsize) : hwtexsize;
        while(t->w > sizelimit || t->h > sizelimit)
        {
            data += s.calclevelsize(level++);
            levels--;
            if(t->w > 1) t->w /= 2;
            if(t->h > 1) t->h /= 2;
        }
        createcompressedtexture(t->frames[0], t->w, t->h, data, s.align, s.bpp, levels, clamp, filter, s.compressed, GL_TEXTURE_2D, swizzle);
    }
    else
    {
        resizetexture(t->w, t->h, mipit, canreduce, GL_TEXTURE_2D, compress, t->w, t->h);

        GLenum component = compressedformat(format, t->w, t->h, compress);

        loopi(hasanim ? anim->count : 1)
        {
            while(!t->frames.inrange(i)) t->frames.add(0);
            glGenTextures(1, &t->frames[i]);

            ImageData cropped;
            uchar *data = s.data;
            int pitch = s.pitch;
            if(hasanim)
            {
                int sx = (i%anim->x)*anim->w, sy = (((i-(i%anim->x))/anim->x)%anim->y)*anim->h;
                texcrop(s, cropped, sx, sy, anim->w, anim->h);
                data = cropped.data;
                pitch = cropped.pitch;
            }
            createtexture(t->frames[i], t->w, t->h, data, clamp, filter, component, GL_TEXTURE_2D, t->xs, t->ys, pitch, false, format, swizzle);
            if(verbose >= 3)
                conoutf("\faAdding frame: %s (%d) [%d,%d:%d,%d]", t->name, i+1, t->w, t->h, t->xs, t->ys);
        }
    }
    t->id = t->frames.length() ? t->frames[0] : 0;
    if(t->frames.length() > 1 && t->delay > 0) animtextures.add(t);
    return t;
}

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
#define RGBAMASKS 0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff
#define RGBMASKS  0xff0000, 0x00ff00, 0x0000ff, 0
#else
#define RGBAMASKS 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000
#define RGBMASKS  0x0000ff, 0x00ff00, 0xff0000, 0
#endif

SDL_Surface *wrapsurface(void *data, int width, int height, int bpp)
{
    switch(bpp)
    {
        case 3: return SDL_CreateRGBSurfaceFrom(data, width, height, 8*bpp, bpp*width, RGBMASKS);
        case 4: return SDL_CreateRGBSurfaceFrom(data, width, height, 8*bpp, bpp*width, RGBAMASKS);
    }
    return NULL;
}

SDL_Surface *creatergbsurface(SDL_Surface *os)
{
    SDL_Surface *ns = SDL_CreateRGBSurface(SDL_SWSURFACE, os->w, os->h, 24, RGBMASKS);
    if(ns) SDL_BlitSurface(os, NULL, ns, NULL);
    SDL_FreeSurface(os);
    return ns;
}

SDL_Surface *creatergbasurface(SDL_Surface *os)
{
    SDL_Surface *ns = SDL_CreateRGBSurface(SDL_SWSURFACE, os->w, os->h, 32, RGBAMASKS);
    if(ns)
    {
        SDL_SetSurfaceBlendMode(os, SDL_BLENDMODE_NONE);
        SDL_BlitSurface(os, NULL, ns, NULL);
    }
    SDL_FreeSurface(os);
    return ns;
}

bool checkgrayscale(SDL_Surface *s)
{
    // gray scale images have 256 levels, no colorkey, and the palette is a ramp
    if(s->format->palette)
    {
        if(s->format->palette->ncolors != 256 || SDL_GetColorKey(s, NULL) >= 0) return false;
        const SDL_Color *colors = s->format->palette->colors;
        loopi(256) if(colors[i].r != i || colors[i].g != i || colors[i].b != i) return false;
    }
    return true;
}

SDL_Surface *fixsurfaceformat(SDL_Surface *s)
{
    if(!s) return NULL;
    if(!s->pixels || min(s->w, s->h) <= 0 || s->format->BytesPerPixel <= 0)
    {
        SDL_FreeSurface(s);
        return NULL;
    }
    static const uint rgbmasks[] = { RGBMASKS }, rgbamasks[] = { RGBAMASKS };
    switch(s->format->BytesPerPixel)
    {
        case 1:
            if(!checkgrayscale(s)) return SDL_GetColorKey(s, NULL) >= 0 ? creatergbasurface(s) : creatergbsurface(s);
            break;
        case 3:
            if(s->format->Rmask != rgbmasks[0] || s->format->Gmask != rgbmasks[1] || s->format->Bmask != rgbmasks[2])
                return creatergbsurface(s);
            break;
        case 4:
            if(s->format->Rmask != rgbamasks[0] || s->format->Gmask != rgbamasks[1] || s->format->Bmask != rgbamasks[2] || s->format->Amask != rgbamasks[3])
                return s->format->Amask ? creatergbasurface(s) : creatergbsurface(s);
            break;
    }
    return s;
}

void texflip(ImageData &s)
{
    ImageData d(s.w, s.h, s.bpp);
    uchar *dst = d.data, *src = &s.data[s.pitch*s.h];
    loopi(s.h)
    {
        src -= s.pitch;
        memcpy(dst, src, s.bpp*s.w);
        dst += d.pitch;
    }
    s.replace(d);
}

void texnormal(ImageData &s, int emphasis)
{
    ImageData d(s.w, s.h, 3);
    uchar *src = s.data, *dst = d.data;
    loop(y, s.h) loop(x, s.w)
    {
        vec normal(0.0f, 0.0f, 255.0f/emphasis);
        normal.x += src[y*s.pitch + ((x+s.w-1)%s.w)*s.bpp];
        normal.x -= src[y*s.pitch + ((x+1)%s.w)*s.bpp];
        normal.y += src[((y+s.h-1)%s.h)*s.pitch + x*s.bpp];
        normal.y -= src[((y+1)%s.h)*s.pitch + x*s.bpp];
        normal.normalize();
        *dst++ = uchar(127.5f + normal.x*127.5f);
        *dst++ = uchar(127.5f + normal.y*127.5f);
        *dst++ = uchar(127.5f + normal.z*127.5f);
    }
    s.replace(d);
}

template<int n, int bpp, bool normals>
static void blurtexture(int w, int h, uchar *dst, const uchar *src, int margin)
{
    static const int weights3x3[9] =
    {
        0x10, 0x20, 0x10,
        0x20, 0x40, 0x20,
        0x10, 0x20, 0x10
    };
    static const int weights5x5[25] =
    {
        0x05, 0x05, 0x09, 0x05, 0x05,
        0x05, 0x0A, 0x14, 0x0A, 0x05,
        0x09, 0x14, 0x28, 0x14, 0x09,
        0x05, 0x0A, 0x14, 0x0A, 0x05,
        0x05, 0x05, 0x09, 0x05, 0x05
    };
    const int *mat = n > 1 ? weights5x5 : weights3x3;
    int mstride = 2*n + 1,
        mstartoffset = n*(mstride + 1),
        stride = bpp*w,
        startoffset = n*bpp,
        nextoffset1 = stride + mstride*bpp,
        nextoffset2 = stride - mstride*bpp;
    src += margin*(stride + bpp);
    for(int y = margin; y < h-margin; y++)
    {
        for(int x = margin; x < w-margin; x++)
        {
            int dr = 0, dg = 0, db = 0;
            const uchar *p = src - startoffset;
            const int *m = mat + mstartoffset;
            for(int t = y; t >= y-n; t--, p -= nextoffset1, m -= mstride)
            {
                if(t < 0) p += stride;
                int a = 0;
                if(n > 1) { a += m[-2]; if(x >= 2) { dr += p[0] * a; dg += p[1] * a; db += p[2] * a; a = 0; } p += bpp; }
                a += m[-1]; if(x >= 1) { dr += p[0] * a; dg += p[1] * a; db += p[2] * a; a = 0; } p += bpp;
                int cr = p[0], cg = p[1], cb = p[2]; a += m[0]; dr += cr * a; dg += cg * a; db += cb * a; p += bpp;
                if(x+1 < w) { cr = p[0]; cg = p[1]; cb = p[2]; } dr += cr * m[1]; dg += cg * m[1]; db += cb * m[1]; p += bpp;
                if(n > 1) { if(x+2 < w) { cr = p[0]; cg = p[1]; cb = p[2]; } dr += cr * m[2]; dg += cg * m[2]; db += cb * m[2]; p += bpp; }
            }
            p = src - startoffset + stride;
            m = mat + mstartoffset + mstride;
            for(int t = y+1; t <= y+n; t++, p += nextoffset2, m += mstride)
            {
                if(t >= h) p -= stride;
                int a = 0;
                if(n > 1) { a += m[-2]; if(x >= 2) { dr += p[0] * a; dg += p[1] * a; db += p[2] * a; a = 0; } p += bpp; }
                a += m[-1]; if(x >= 1) { dr += p[0] * a; dg += p[1] * a; db += p[2] * a; a = 0; } p += bpp;
                int cr = p[0], cg = p[1], cb = p[2]; a += m[0]; dr += cr * a; dg += cg * a; db += cb * a; p += bpp;
                if(x+1 < w) { cr = p[0]; cg = p[1]; cb = p[2]; } dr += cr * m[1]; dg += cg * m[1]; db += cb * m[1]; p += bpp;
                if(n > 1) { if(x+2 < w) { cr = p[0]; cg = p[1]; cb = p[2]; } dr += cr * m[2]; dg += cg * m[2]; db += cb * m[2]; p += bpp; }
            }
            if(normals)
            {
                vec v(dr-0x7F80, dg-0x7F80, db-0x7F80);
                float mag = 127.5f/v.magnitude();
                dst[0] = uchar(v.x*mag + 127.5f);
                dst[1] = uchar(v.y*mag + 127.5f);
                dst[2] = uchar(v.z*mag + 127.5f);
            }
            else
            {
                dst[0] = dr>>8;
                dst[1] = dg>>8;
                dst[2] = db>>8;
            }
            if(bpp > 3) dst[3] = src[3];
            dst += bpp;
            src += bpp;
        }
        src += 2*margin*bpp;
    }
}

void blurtexture(int n, int bpp, int w, int h, uchar *dst, const uchar *src, int margin)
{
    switch((clamp(n, 1, 2)<<4) | bpp)
    {
        case 0x13: blurtexture<1, 3, false>(w, h, dst, src, margin); break;
        case 0x23: blurtexture<2, 3, false>(w, h, dst, src, margin); break;
        case 0x14: blurtexture<1, 4, false>(w, h, dst, src, margin); break;
        case 0x24: blurtexture<2, 4, false>(w, h, dst, src, margin); break;
    }
}

void blurnormals(int n, int w, int h, bvec *dst, const bvec *src, int margin)
{
    switch(clamp(n, 1, 2))
    {
        case 1: blurtexture<1, 3, true>(w, h, dst->v, src->v, margin); break;
        case 2: blurtexture<2, 3, true>(w, h, dst->v, src->v, margin); break;
    }
}

void texblur(ImageData &s, int n, int r)
{
    if(s.bpp < 3) return;
    loopi(r)
    {
        ImageData d(s.w, s.h, s.bpp);
        blurtexture(n, s.bpp, s.w, s.h, d.data, s.data);
        s.replace(d);
    }
}

bool canloadsurface(const char *name)
{
    stream *f = openfile(name, "rb");
    if(!f) return false;
    delete f;
    return true;
}

#if 0
SDL_Surface *loadsurface(const char *name)
{
    SDL_Surface *s = NULL;
    stream *z = openzipfile(name, "rb");
    if(z)
    {
        SDL_RWops *rw = z->rwops();
        if(rw)
        {
            const char *ext = strrchr(name, '.');
            if(ext) ++ext;
            s = IMG_LoadTyped_RW(rw, 0, ext);
            SDL_FreeRW(rw);
        }
        delete z;
    }
    if(!s) s = IMG_Load(findfile(name, "rb"));
    return fixsurfaceformat(s);
}
#endif

static vec parsevec(const char *arg)
{
    vec v(0, 0, 0);
    int i = 0;
    for(; arg[0] && (!i || arg[0]=='/') && i<3; arg += strcspn(arg, "/,><"), i++)
    {
        if(i) arg++;
        v[i] = atof(arg);
    }
    if(i==1) v.y = v.z = v.x;
    return v;
}

VAR(0, usedds, 0, 1, 1);
VAR(0, dbgdds, 0, 0, 1);
VAR(0, scaledds, 0, 2, 4);

static bool texturedata(ImageData &d, const char *tname, Slot::Tex *tex = NULL, bool msg = true, int *compress = NULL, int *wrap = NULL, TextureAnim *anim = NULL)
{
    const char *cmds = NULL, *file = tname;
    if(!tname && tex) file = tname = tex->name;
    if(tname && tname[0]=='<')
    {
        cmds = tname;
        file = strrchr(tname, '>');
        if(!file) file = tex ? tex->name : NULL;
        else file++;
    }

    if(!file) { if(msg) conoutf("\frCould not load texture: %s", tname); return false; }

    bool raw = !usedds || !compress, dds = false;
    for(const char *pcmds = cmds; pcmds;)
    {
        #define PARSETEXCOMMANDS(cmds) \
            const char *cmd = NULL, *end = NULL, *arg[4] = { NULL, NULL, NULL, NULL }; \
            cmd = &cmds[1]; \
            end = strchr(cmd, '>'); \
            if(!end) break; \
            cmds = strchr(cmd, '<'); \
            size_t len = strcspn(cmd, ":,><"); \
            loopi(4) \
            { \
                arg[i] = strchr(i ? arg[i-1] : cmd, i ? ',' : ':'); \
                if(!arg[i] || arg[i] >= end) arg[i] = ""; \
                else arg[i]++; \
            }
        #define COPYTEXARG(dst, src) copystring(dst, stringslice(src, strcspn(src, ":,><")))
        PARSETEXCOMMANDS(pcmds);
        if(matchstring(cmd, len, "dds")) dds = true;
        else if(matchstring(cmd, len, "thumbnail")) raw = true;
        else if(matchstring(cmd, len, "stub")) return canloadsurface(file);
    }

    if(msg)
    {
        defformatstring(text, "Loading texture: %s", file);
        progress(loadprogress, text);
    }

    int flen = strlen(file);
    if(flen >= 4 && (!strcasecmp(file + flen - 4, ".dds") || (dds && !raw)))
    {
        string dfile;
        copystring(dfile, file);
        memcpy(dfile + flen - 4, ".dds", 4);
        if(!loaddds(dfile, d, raw ? 1 : (dds ? 0 : -1)) && (!dds || raw))
        {
            if(msg) conoutf("\frCould not load texture %s", dfile);
            return false;
        }
        if(d.data && !d.compressed && !dds && compress) *compress = scaledds;
    }

    if(!d.data)
    {
        SDL_Surface *s = loadsurface(file);
        if(!s) { if(msg) conoutf("\frCould not load texture %s", file); return false; }
        int bpp = s->format->BitsPerPixel;
        if(bpp%8 || !texformat(bpp/8)) { SDL_FreeSurface(s); conoutf("\frTexture must be 8, 16, 24, or 32 bpp: %s", file); return false; }
        if(max(s->w, s->h) > (1<<12)) { SDL_FreeSurface(s); conoutf("\frTexture size exceeded %dx%d pixels: %s", 1<<12, 1<<12, file); return false; }
        d.wrap(s);
    }

    while(cmds)
    {
        PARSETEXCOMMANDS(cmds);
        if(d.compressed) goto compressed;
        if(matchstring(cmd, len, "mad")) texmad(d, parsevec(arg[0]), parsevec(arg[1]));
        else if(matchstring(cmd, len, "colorify")) texcolorify(d, parsevec(arg[0]), parsevec(arg[1]));
        else if(matchstring(cmd, len, "colormask")) texcolormask(d, parsevec(arg[0]), *arg[1] ? parsevec(arg[1]) : vec(1, 1, 1));
        else if(matchstring(cmd, len, "invert")) texinvert(d);
        else if(matchstring(cmd, len, "normal"))
        {
            int emphasis = atoi(arg[0]);
            texnormal(d, emphasis > 0 ? emphasis : 3);
        }
        else if(matchstring(cmd, len, "dup")) texdup(d, atoi(arg[0]), atoi(arg[1]));
        else if(matchstring(cmd, len, "offset")) texoffset(d, atoi(arg[0]), atoi(arg[1]));
        else if(matchstring(cmd, len, "rotate")) texrotate(d, atoi(arg[0]), tex ? tex->type : 0);
        else if(matchstring(cmd, len, "reorient")) texreorient(d, atoi(arg[0])>0, atoi(arg[1])>0, atoi(arg[2])>0, tex ? tex->type : TEX_DIFFUSE);
        else if(matchstring(cmd, len, "crop")) texcrop(d, atoi(arg[0]), atoi(arg[1]), *arg[2] ? atoi(arg[2]) : -1, *arg[3] ? atoi(arg[3]) : -1);
        else if(matchstring(cmd, len, "mix")) texmix(d, *arg[0] ? atoi(arg[0]) : -1, *arg[1] ? atoi(arg[1]) : -1, *arg[2] ? atoi(arg[2]) : -1, *arg[3] ? atoi(arg[3]) : -1);
        else if(matchstring(cmd, len, "grey")) texgrey(d);
        else if(matchstring(cmd, len, "blur"))
        {
            int emphasis = atoi(arg[0]), repeat = atoi(arg[1]);
            texblur(d, emphasis > 0 ? clamp(emphasis, 1, 2) : 1, repeat > 0 ? repeat : 1);
        }
        else if(matchstring(cmd, len, "premul")) texpremul(d);
        else if(matchstring(cmd, len, "agrad")) texagrad(d, atof(arg[0]), atof(arg[1]), atof(arg[2]), atof(arg[3]));
        else if(matchstring(cmd, len, "blend"))
        {
            ImageData src, mask;
            string srcname, maskname;
            COPYTEXARG(srcname, arg[0]);
            COPYTEXARG(maskname, arg[1]);
            if(srcname[0] && texturedata(src, srcname, NULL, false) && (!maskname[0] || texturedata(mask, maskname, NULL, false)))
                texblend(d, src, maskname[0] ? mask : src);
        }
        else if(matchstring(cmd, len, "thumbnail"))
        {
            int w = atoi(arg[0]), h = atoi(arg[1]);
            if(w <= 0 || w > (1<<12)) w = 64;
            if(h <= 0 || h > (1<<12)) h = w;
            if(d.w > w || d.h > h) scaleimage(d, w, h);
        }
        else if(matchstring(cmd, len, "compress") || matchstring(cmd, len, "dds"))
        {
            int scale = atoi(arg[0]);
            if(scale <= 0) scale = scaledds;
            if(compress) *compress = scale;
        }
        else if(matchstring(cmd, len, "nocompress"))
        {
            if(compress) *compress = -1;
        }
        else if(matchstring(cmd, len, "anim"))
        {
            if(anim)
            {
                anim->delay = *arg[0] ? atoi(arg[0]) : 50;
                anim->x = max(1, *arg[1] ? atoi(arg[1]) : 1);
                anim->y = max(1, *arg[2] ? atoi(arg[2]) : 2);
                anim->throb = *arg[3] && atoi(arg[3]);
                anim->w = d.w/anim->x;
                anim->h = d.h/anim->y;
                anim->count = anim->x*anim->y;
            }
        }
        else
    compressed:
        if(matchstring(cmd, len, "mirror"))
        {
            if(wrap) *wrap |= 0x300;
        }
        else if(matchstring(cmd, len, "noswizzle"))
        {
            if(wrap) *wrap |= 0x10000;
        }
    }

    if((anim && anim->count ? max(anim->w, anim->h) : max(d.w, d.h)) > (1<<12))
    {
        d.cleanup();
        conoutf("\frTexture size exceeded %dx%d: %s", 1<<12, 1<<12, file);
        return false;
    }

    return true;
}

static inline bool texturedata(ImageData &d, Slot &slot, Slot::Tex &tex, bool msg = true, int *compress = NULL, int *wrap = NULL, TextureAnim *anim = NULL)
{
    return texturedata(d, tex.name, &tex, msg, compress, wrap, anim);
}

uchar *loadalphamask(Texture *t)
{
    if(t->alphamask) return t->alphamask;
    if(!(t->type&Texture::ALPHA)) return NULL;
    ImageData s;
    if(!texturedata(s, t->name, NULL, false) || !s.data || s.compressed) return NULL;
    t->alphamask = new uchar[s.h * ((s.w+7)/8)];
    uchar *srcrow = s.data, *dst = t->alphamask-1;
    loop(y, s.h)
    {
        uchar *src = srcrow+s.bpp-1;
        loop(x, s.w)
        {
            int offset = x%8;
            if(!offset) *++dst = 0;
            if(*src) *dst |= 1<<offset;
            src += s.bpp;
        }
        srcrow += s.pitch;
    }
    return t->alphamask;
}

Texture *textureload(const char *name, int clamp, bool mipit, bool msg)
{
    string tname;
    copystring(tname, name);
    path(tname);
    Texture *t = textures.access(tname);
    if(t) return t;
    int compress = 0;
    ImageData s;
    TextureAnim anim;
    if(texturedata(s, tname, NULL, msg, &compress, &clamp, &anim))
       return newtexture(NULL, tname, s, clamp, mipit, false, false, compress, &anim);
    return notexture;
}

bool settexture(const char *name, int clamp)
{
    Texture *t = textureload(name, clamp, true, false);
    glBindTexture(GL_TEXTURE_2D, t->id);
    return t != notexture;
}

vector<VSlot *> vslots;
vector<Slot *> slots;
MatSlot materialslots[(MATF_VOLUME|MATF_INDEX)+1];
Slot dummyslot;
VSlot dummyvslot(&dummyslot);
vector<DecalSlot *> decalslots;
DecalSlot dummydecalslot;
Slot *defslot = NULL;

const char *Slot::name() const { return tempformatstring("slot %d", index); }

MatSlot::MatSlot() : Slot(int(this - materialslots)), VSlot(this) {}
const char *MatSlot::name() const { return tempformatstring("material slot %s", findmaterialname(Slot::index)); }

const char *DecalSlot::name() const { return tempformatstring("decal slot %d", Slot::index); }

void resettextures(int n)
{
    resetslotshader();
    int limit = clamp(n, 0, slots.length());
    for(int i = limit; i < slots.length(); i++)
    {
        Slot *s = slots[i];
        for(VSlot *vs = s->variants; vs; vs = vs->next) vs->slot = &dummyslot;
        delete s;
    }
    slots.setsize(limit);
    while(vslots.length())
    {
        VSlot *vs = vslots.last();
        if(vs->slot != &dummyslot || vs->changed) break;
        delete vslots.pop();
    }
}
ICOMMAND(0, texturereset, "i", (int *n), if(editmode || identflags&IDF_WORLD) resettextures(*n););

void resetmaterials()
{
    defslot = NULL;
    loopi((MATF_VOLUME|MATF_INDEX)+1) materialslots[i].reset();
}
ICOMMAND(0, materialreset, "", (void), if(editmode || identflags&IDF_WORLD) resetmaterials(););
void resetdecals(int n)
{
    defslot = NULL;
    resetslotshader();
    decalslots.deletecontents(n);
}
ICOMMAND(0, decalreset, "i", (int *n), if(editmode || identflags&IDF_WORLD) resetdecals(*n););

static int compactedvslots = 0, compactvslotsprogress = 0, clonedvslots = 0;
static bool markingvslots = false;

void clearslots()
{
    defslot = NULL;
    resetslotshader();
    slots.deletecontents();
    vslots.deletecontents();
    loopi((MATF_VOLUME|MATF_INDEX)+1) materialslots[i].reset();
    decalslots.deletecontents();
    clonedvslots = 0;
}

static void assignvslot(VSlot &vs);

static inline void assignvslotlayer(VSlot &vs)
{
    if(vs.layer && vslots.inrange(vs.layer) && vslots[vs.layer]->index < 0) assignvslot(*vslots[vs.layer]);
    if(vs.detail && vslots.inrange(vs.detail) && vslots[vs.detail]->index < 0) assignvslot(*vslots[vs.detail]);
}

static void assignvslot(VSlot &vs)
{
    vs.index = compactedvslots++;
    assignvslotlayer(vs);
}

void compactvslot(int &index)
{
    if(vslots.inrange(index))
    {
        VSlot &vs = *vslots[index];
        if(vs.index < 0) assignvslot(vs);
        if(!markingvslots) index = vs.index;
    }
}

void compactvslot(VSlot &vs)
{
    if(vs.index < 0) assignvslot(vs);
}

void compactvslots(cube *c, int n)
{
    if((compactvslotsprogress++&0xFFF)==0) progress(min(float(compactvslotsprogress)/allocnodes, 1.0f), markingvslots ? "Marking slots..." : "Compacting slots...");
    loopi(n)
    {
        if(c[i].children) compactvslots(c[i].children);
        else loopj(6) if(vslots.inrange(c[i].texture[j]))
        {
            VSlot &vs = *vslots[c[i].texture[j]];
            if(vs.index < 0) assignvslot(vs);
            if(!markingvslots) c[i].texture[j] = vs.index;
        }
    }
}

int compactvslots(bool cull)
{
    defslot = NULL;
    clonedvslots = 0;
    markingvslots = cull;
    compactedvslots = 0;
    compactvslotsprogress = 0;
    loopv(vslots) vslots[i]->index = -1;
    if(cull)
    {
        int numdefaults = min(int(NUMDEFAULTSLOTS), slots.length());
        loopi(numdefaults) slots[i]->variants->index = compactedvslots++;
        loopi(numdefaults) assignvslotlayer(*slots[i]->variants);
    }
    else
    {
        loopv(slots) slots[i]->variants->index = compactedvslots++;
        loopv(slots) assignvslotlayer(*slots[i]->variants);
        loopv(vslots)
        {
            VSlot &vs = *vslots[i];
            if(!vs.changed && vs.index < 0) { markingvslots = true; break; }
        }
    }
    compactvslots(worldroot);
    int total = compactedvslots;
    compacteditvslots();
    loopv(vslots)
    {
        VSlot *vs = vslots[i];
        if(vs->changed) continue;
        while(vs->next)
        {
            if(vs->next->index < 0) vs->next = vs->next->next;
            else vs = vs->next;
        }
    }
    if(markingvslots)
    {
        markingvslots = false;
        compactedvslots = 0;
        compactvslotsprogress = 0;
        int lastdiscard = 0;
        loopv(vslots)
        {
            VSlot &vs = *vslots[i];
            if(vs.changed || (vs.index < 0 && !vs.next)) vs.index = -1;
            else
            {
                if(!cull) while(lastdiscard < i)
                {
                    VSlot &ds = *vslots[lastdiscard++];
                    if(!ds.changed && ds.index < 0) ds.index = compactedvslots++;
                }
                vs.index = compactedvslots++;
            }
        }
        compactvslots(worldroot);
        total = compactedvslots;
        compacteditvslots();
    }
    compactmruvslots();
    loopv(vslots)
    {
        VSlot &vs = *vslots[i];
        if(vs.index >= 0)
        {
            if(vs.layer && vslots.inrange(vs.layer)) vs.layer = vslots[vs.layer]->index;
            if(vs.detail && vslots.inrange(vs.detail)) vs.detail = vslots[vs.detail]->index;
        }
    }
    if(cull)
    {
        loopvrev(slots) if(slots[i]->variants->index < 0) delete slots.remove(i);
        loopv(slots) slots[i]->index = i;
    }
    loopv(vslots)
    {
        while(vslots[i]->index >= 0 && vslots[i]->index != i)
            swap(vslots[i], vslots[vslots[i]->index]);
    }
    for(int i = compactedvslots; i < vslots.length(); i++) delete vslots[i];
    vslots.setsize(compactedvslots);
    return total;
}

ICOMMAND(0, compactvslots, "i", (int *cull),
{
    if(nompedit && multiplayer()) return;
    compactvslots(*cull!=0);
    allchanged();
});

static void clampvslotoffset(VSlot &dst, Slot *slot = NULL)
{
    if(!slot) slot = dst.slot;
    if(slot && slot->sts.inrange(0))
    {
        if(!slot->loaded) slot->load();
        Texture *t = slot->sts[0].t;
        int xs = t->xs, ys = t->ys;
        if(t->type & Texture::MIRROR) { xs *= 2; ys *= 2; }
        if(texrotations[dst.rotation].swapxy) swap(xs, ys);
        dst.offset.x %= xs; if(dst.offset.x < 0) dst.offset.x += xs;
        dst.offset.y %= ys; if(dst.offset.y < 0) dst.offset.y += ys;
    }
    else dst.offset.max(0);
}

static void propagatevslot(VSlot &dst, const VSlot &src, int diff, bool edit = false)
{
    if(diff & (1<<VSLOT_SHPARAM)) loopv(src.params) dst.params.add(src.params[i]);
    if(diff & (1<<VSLOT_SCALE)) dst.scale = src.scale;
    if(diff & (1<<VSLOT_ROTATION))
    {
        dst.rotation = src.rotation;
        if(edit && !dst.offset.iszero()) clampvslotoffset(dst);
    }
    if(diff & (1<<VSLOT_OFFSET))
    {
        dst.offset = src.offset;
        if(edit) clampvslotoffset(dst);
    }
    if(diff & (1<<VSLOT_SCROLL)) dst.scroll = src.scroll;
    if(diff & (1<<VSLOT_LAYER)) dst.layer = src.layer;
    if(diff & (1<<VSLOT_ALPHA))
    {
        dst.alphafront = src.alphafront;
        dst.alphaback = src.alphaback;
    }
    if(diff & (1<<VSLOT_COLOR)) dst.colorscale = src.colorscale;
    if(diff & (1<<VSLOT_PALETTE))
    {
        dst.palette = src.palette;
        dst.palindex = src.palindex;
    }
    if(diff & (1<<VSLOT_COAST)) dst.coastscale = src.coastscale;
    if(diff & (1<<VSLOT_REFRACT))
    {
        dst.refractscale = src.refractscale;
        dst.refractcolor = src.refractcolor;
    }
    if(diff & (1<<VSLOT_DETAIL)) dst.detail = src.detail;
}

static void propagatevslot(VSlot *root, int changed)
{
    for(VSlot *vs = root->next; vs; vs = vs->next)
    {
        int diff = changed & ~vs->changed;
        if(diff) propagatevslot(*vs, *root, diff);
    }
}

static void mergevslot(VSlot &dst, const VSlot &src, int diff, Slot *slot = NULL)
{
    if(diff & (1<<VSLOT_SHPARAM)) loopv(src.params)
    {
        const SlotShaderParam &sp = src.params[i];
        loopvj(dst.params)
        {
            SlotShaderParam &dp = dst.params[j];
            if(sp.name == dp.name)
            {
                dp.palette = sp.palette;
                dp.palindex = sp.palindex;
                memcpy(dp.val, sp.val, sizeof(dp.val));
                goto nextparam;
            }
        }
        dst.params.add(sp);
    nextparam:;
    }
    if(diff & (1<<VSLOT_SCALE))
    {
        dst.scale = clamp(dst.scale*src.scale, 1/8.0f, 8.0f);
    }
    if(diff & (1<<VSLOT_ROTATION))
    {
        dst.rotation = clamp(dst.rotation + src.rotation, 0, 7);
        if(!dst.offset.iszero()) clampvslotoffset(dst, slot);
    }
    if(diff & (1<<VSLOT_OFFSET))
    {
        dst.offset.add(src.offset);
        clampvslotoffset(dst, slot);
    }
    if(diff & (1<<VSLOT_SCROLL)) dst.scroll.add(src.scroll);
    if(diff & (1<<VSLOT_LAYER)) dst.layer = src.layer;
    if(diff & (1<<VSLOT_ALPHA))
    {
        dst.alphafront = src.alphafront;
        dst.alphaback = src.alphaback;
    }
    if(diff & (1<<VSLOT_COLOR)) dst.colorscale.mul(src.colorscale);
    if(diff & (1<<VSLOT_PALETTE))
    {
        dst.palette = src.palette;
        dst.palindex = src.palindex;
    }
    if(diff & (1<<VSLOT_COAST)) dst.coastscale = src.coastscale;
    if(diff & (1<<VSLOT_REFRACT))
    {
        dst.refractscale *= src.refractscale;
        dst.refractcolor.mul(src.refractcolor);
    }
    if(diff & (1<<VSLOT_DETAIL)) dst.detail = src.detail;
}

void mergevslot(VSlot &dst, const VSlot &src, const VSlot &delta)
{
    dst.changed = src.changed | delta.changed;
    propagatevslot(dst, src, (1<<VSLOT_NUM)-1);
    mergevslot(dst, delta, delta.changed, src.slot);
}

static VSlot *reassignvslot(Slot &owner, VSlot *vs)
{
    vs->reset();
    owner.variants = vs;
    while(vs)
    {
        vs->slot = &owner;
        vs->linked = false;
        vs = vs->next;
    }
    return owner.variants;
}

static VSlot *emptyvslot(Slot &owner)
{
    int offset = 0;
    loopvrev(slots) if(slots[i]->variants) { offset = slots[i]->variants->index + 1; break; }
    for(int i = offset; i < vslots.length(); i++) if(!vslots[i]->changed) return reassignvslot(owner, vslots[i]);
    return vslots.add(new VSlot(&owner, vslots.length()));
}

VSlot &Slot::emptyvslot()
{
    return *::emptyvslot(*this);
}

static bool comparevslot(const VSlot &dst, const VSlot &src, int diff)
{
    if(diff & (1<<VSLOT_SHPARAM))
    {
        if(src.params.length() != dst.params.length()) return false;
        loopv(src.params)
        {
            const SlotShaderParam &sp = src.params[i], &dp = dst.params[i];
            if(sp.name != dp.name || sp.palette != dp.palette || sp.palindex != dp.palindex || memcmp(sp.val, dp.val, sizeof(sp.val))) return false;
        }
    }
    if(diff & (1<<VSLOT_SCALE) && dst.scale != src.scale) return false;
    if(diff & (1<<VSLOT_ROTATION) && dst.rotation != src.rotation) return false;
    if(diff & (1<<VSLOT_OFFSET) && dst.offset != src.offset) return false;
    if(diff & (1<<VSLOT_SCROLL) && dst.scroll != src.scroll) return false;
    if(diff & (1<<VSLOT_LAYER) && dst.layer != src.layer) return false;
    if(diff & (1<<VSLOT_ALPHA) && (dst.alphafront != src.alphafront || dst.alphaback != src.alphaback)) return false;
    if(diff & (1<<VSLOT_COLOR) && dst.colorscale != src.colorscale) return false;
    if(diff & (1<<VSLOT_PALETTE) && (dst.palette != src.palette || dst.palindex != src.palindex)) return false;
    if(diff & (1<<VSLOT_COAST) && dst.coastscale != src.coastscale) return false;
    if(diff & (1<<VSLOT_REFRACT) && (dst.refractscale != src.refractscale || dst.refractcolor != src.refractcolor)) return false;
    if(diff & (1<<VSLOT_DETAIL) && dst.detail != src.detail) return false;
    return true;
}

void packvslot(vector<uchar> &buf, const VSlot &src)
{
    if(src.changed & (1<<VSLOT_SHPARAM))
    {
        loopv(src.params)
        {
            const SlotShaderParam &p = src.params[i];
            buf.put(VSLOT_SHPARAM);
            sendstring(p.name, buf);
            loopj(4) putfloat(buf, p.val[j]);
            putint(buf, p.palette);
            putint(buf, p.palindex);
        }
    }
    if(src.changed & (1<<VSLOT_SCALE))
    {
        buf.put(VSLOT_SCALE);
        putfloat(buf, src.scale);
    }
    if(src.changed & (1<<VSLOT_ROTATION))
    {
        buf.put(VSLOT_ROTATION);
        putint(buf, src.rotation);
    }
    if(src.changed & (1<<VSLOT_OFFSET))
    {
        buf.put(VSLOT_OFFSET);
        putint(buf, src.offset.x);
        putint(buf, src.offset.y);
    }
    if(src.changed & (1<<VSLOT_SCROLL))
    {
        buf.put(VSLOT_SCROLL);
        putfloat(buf, src.scroll.x);
        putfloat(buf, src.scroll.y);
    }
    if(src.changed & (1<<VSLOT_LAYER))
    {
        buf.put(VSLOT_LAYER);
        putuint(buf, vslots.inrange(src.layer) && !vslots[src.layer]->changed ? src.layer : 0);
    }
    if(src.changed & (1<<VSLOT_ALPHA))
    {
        buf.put(VSLOT_ALPHA);
        putfloat(buf, src.alphafront);
        putfloat(buf, src.alphaback);
    }
    if(src.changed & (1<<VSLOT_COLOR))
    {
        buf.put(VSLOT_COLOR);
        putfloat(buf, src.colorscale.r);
        putfloat(buf, src.colorscale.g);
        putfloat(buf, src.colorscale.b);
    }
    if(src.changed & (1<<VSLOT_PALETTE))
    {
        buf.put(VSLOT_PALETTE);
        putint(buf, src.palette);
        putint(buf, src.palindex);
    }
    if(src.changed & (1<<VSLOT_COAST))
    {
        buf.put(VSLOT_COAST);
        putfloat(buf, src.coastscale);
    }
    if(src.changed & (1<<VSLOT_REFRACT))
    {
        buf.put(VSLOT_REFRACT);
        putfloat(buf, src.refractscale);
        putfloat(buf, src.refractcolor.r);
        putfloat(buf, src.refractcolor.g);
        putfloat(buf, src.refractcolor.b);
    }
    if(src.changed & (1<<VSLOT_DETAIL))
    {
        buf.put(VSLOT_DETAIL);
        putuint(buf, vslots.inrange(src.detail) && !vslots[src.detail]->changed ? src.detail : 0);
    }
    buf.put(0xFF);
}

void packvslot(vector<uchar> &buf, int index)
{
    if(vslots.inrange(index)) packvslot(buf, *vslots[index]);
    else buf.put(0xFF);
}

void packvslot(vector<uchar> &buf, const VSlot *vs)
{
    if(vs) packvslot(buf, *vs);
    else buf.put(0xFF);
}

bool unpackvslot(ucharbuf &buf, VSlot &dst, bool delta)
{
    while(buf.remaining())
    {
        int changed = buf.get();
        if(changed >= 0x80) break;
        switch(changed)
        {
            case VSLOT_SHPARAM:
            {
                string name;
                getstring(name, buf);
                SlotShaderParam p = { name[0] ? getshaderparamname(name) : NULL, -1, 0, 0, 0, { 0, 0, 0, 0 } };
                loopi(4) p.val[i] = getfloat(buf);
                p.palette = getint(buf);
                p.palindex = getint(buf);
                if(p.name) dst.params.add(p);
                break;
            }
            case VSLOT_SCALE:
                dst.scale = getfloat(buf);
                if(dst.scale <= 0) dst.scale = 1;
                else if(!delta) dst.scale = clamp(dst.scale, 1/8.0f, 8.0f);
                break;
            case VSLOT_ROTATION:
                dst.rotation = getint(buf);
                if(!delta) dst.rotation = clamp(dst.rotation, 0, 7);
                break;
            case VSLOT_OFFSET:
                dst.offset.x = getint(buf);
                dst.offset.y = getint(buf);
                if(!delta) dst.offset.max(0);
                break;
            case VSLOT_SCROLL:
                dst.scroll.x = getfloat(buf);
                dst.scroll.y = getfloat(buf);
                break;
            case VSLOT_LAYER:
            {
                int tex = getuint(buf);
                dst.layer = vslots.inrange(tex) ? tex : 0;
                break;
            }
            case VSLOT_ALPHA:
                dst.alphafront = clamp(getfloat(buf), 0.0f, 1.0f);
                dst.alphaback = clamp(getfloat(buf), 0.0f, 1.0f);
                break;
            case VSLOT_COLOR:
                dst.colorscale.r = clamp(getfloat(buf), 0.0f, 2.0f);
                dst.colorscale.g = clamp(getfloat(buf), 0.0f, 2.0f);
                dst.colorscale.b = clamp(getfloat(buf), 0.0f, 2.0f);
                break;
            case VSLOT_PALETTE:
                dst.palette = max(getint(buf), 0);
                dst.palindex = max(getint(buf), 0);
                break;
            case VSLOT_COAST:
                dst.coastscale = clamp(getfloat(buf), 0.0f, 1000.0f);
                break;
            case VSLOT_REFRACT:
                dst.refractscale = clamp(getfloat(buf), 0.0f, 1.0f);
                dst.refractcolor.r = clamp(getfloat(buf), 0.0f, 1.0f);
                dst.refractcolor.g = clamp(getfloat(buf), 0.0f, 1.0f);
                dst.refractcolor.b = clamp(getfloat(buf), 0.0f, 1.0f);
                break;
            case VSLOT_DETAIL:
            {
                int tex = getuint(buf);
                dst.detail = vslots.inrange(tex) ? tex : 0;
                break;
            }
            default:
                return false;
        }
        dst.changed |= 1<<changed;
    }
    if(buf.overread()) return false;
    return true;
}

VSlot *findvslot(Slot &slot, const VSlot &src, const VSlot &delta)
{
    for(VSlot *dst = slot.variants; dst; dst = dst->next)
    {
        if((!dst->changed || dst->changed == (src.changed | delta.changed)) &&
           comparevslot(*dst, src, src.changed & ~delta.changed) &&
           comparevslot(*dst, delta, delta.changed))
            return dst;
    }
    return NULL;
}

static VSlot *clonevslot(const VSlot &src, const VSlot &delta)
{
    VSlot *dst = vslots.add(new VSlot(src.slot, vslots.length()));
    dst->changed = src.changed | delta.changed;
    propagatevslot(*dst, src, ((1<<VSLOT_NUM)-1) & ~delta.changed);
    propagatevslot(*dst, delta, delta.changed, true);
    return dst;
}

VAR(IDF_PERSIST, autocompactvslots, 0, 256, 0x10000);

VSlot *editvslot(const VSlot &src, const VSlot &delta)
{
    VSlot *exists = findvslot(*src.slot, src, delta);
    if(exists) return exists;
    if(vslots.length()>=0x10000)
    {
        compactvslots();
        allchanged();
        if(vslots.length()>=0x10000) return NULL;
    }
    if(autocompactvslots && ++clonedvslots >= autocompactvslots)
    {
        compactvslots();
        allchanged();
    }
    return clonevslot(src, delta);
}

static void fixinsidefaces(cube *c, const ivec &o, int size, int tex)
{
    loopi(8)
    {
        ivec co(i, o, size);
        if(c[i].children) fixinsidefaces(c[i].children, co, size>>1, tex);
        else loopj(6) if(!visibletris(c[i], j, co, size))
            c[i].texture[j] = tex;
    }
}

ICOMMAND(0, fixinsidefaces, "i", (int *tex),
{
    if(noedit(true) || (nompedit && multiplayer())) return;
    fixinsidefaces(worldroot, ivec(0, 0, 0), worldsize>>1, *tex && vslots.inrange(*tex) ? *tex : DEFAULT_GEOM);
    allchanged();
});

extern const namemap slottexs[] =
{
    {"0", TEX_DIFFUSE},
    {"1", TEX_UNKNOWN},

    {"c", TEX_DIFFUSE},
    {"u", TEX_UNKNOWN},
    {"n", TEX_NORMAL},
    {"g", TEX_GLOW},
    {"s", TEX_SPEC},
    {"z", TEX_DEPTH},
    {"e", TEX_ENVMAP}
};

int findslottex(char *name, bool tryint)
{
    loopi(sizeof(slottexs)/sizeof(slottexs[0])) if(!strcmp(slottexs[i].name, name)) { return slottexs[i].id; }
    return tryint && isnumeric(*name) ? atoi(name) : -1;
}

const char *findtexturetypename(int type)
{
    loopi(sizeof(slottexs)/sizeof(slottexs[0])) if(slottexs[i].id == type) { return slottexs[i].name; }
    return NULL;
}

void texture(char *type, char *name, int *rot, int *xoffset, int *yoffset, float *scale)
{
    int tnum = findslottex(type), matslot = -1;
    if(tnum == TEX_DIFFUSE)
    {
        if(slots.length() >= 0x10000) return;
        defslot = slots.add(new Slot(slots.length()));
    }
    else if(!strcmp(type, "decal"))
    {
        if(decalslots.length() >= 0x10000) return;
        tnum = TEX_DIFFUSE;
        defslot = decalslots.add(new DecalSlot(decalslots.length()));
    }
    else if((matslot = findmaterial(type)) >= 0)
    {
        tnum = TEX_DIFFUSE;
        defslot = &materialslots[matslot];
        defslot->reset();
    }
    else if(!defslot) { conoutf("\frNo default slot set for texture (%s)", name); return; }
    else if(tnum < 0) tnum = TEX_UNKNOWN;
    Slot &s = *defslot;
    s.loaded = false;
    s.texmask |= 1<<tnum;
    if(s.sts.length() >= TEX_MAX) conoutf("\frWarning: too many textures, [%d] %s (%d)", slots.length()-1, name, matslot);
    Slot::Tex &st = s.sts.add();
    st.type = tnum;
    copystring(st.name, name);
    path(st.name);
    if(tnum == TEX_DIFFUSE)
    {
        setslotshader(s);
        VSlot &vs = s.emptyvslot();
        vs.rotation = clamp(*rot, 0, 7);
        vs.offset = ivec2(*xoffset, *yoffset).max(0);
        vs.scale = *scale <= 0 ? 1 : *scale;
        propagatevslot(&vs, (1<<VSLOT_NUM)-1);
    }
}
COMMAND(0, texture, "ssiiif");

void texgrass(char *name)
{
    if(!defslot) return;
    Slot &s = *defslot;
    DELETEA(s.grass);
    s.grass = name[0] ? newstring(path(name)) : NULL;
}
ICOMMAND(0, texgrass, "s", (char *name), texgrass(name));
ICOMMAND(0, autograss, "s", (char *name), texgrass(name));

void texgrasscolor(float r, float g, float b)
{
    if(!defslot) return;
    Slot &s = *defslot;
    s.grasscolor = vec(max(r, 0.f), max(g, 0.f), max(b, 0.f));
}
ICOMMAND(0, texgrasscolor, "fff", (float *r, float *g, float *b), texgrasscolor(*r, *g, *b));

void texgrassblend(float blend)
{
    if(!defslot) return;
    Slot &s = *defslot;
    s.grassblend = clamp(blend, 0.f, 1.f);
}
ICOMMAND(0, texgrassblend, "f", (float *blend), texgrassblend(*blend));

void texgrassscale(int scale)
{
    if(!defslot) return;
    Slot &s = *defslot;
    s.grassscale = clamp(scale, 0, 64);
}
ICOMMAND(0, texgrassscale, "i", (int *scale), texgrassscale(*scale));

void texgrassheight(int height)
{
    if(!defslot) return;
    Slot &s = *defslot;
    s.grassheight = clamp(height, 0, 64);
}
ICOMMAND(0, texgrassheight, "i", (int *height), texgrassheight(*height));

void texscroll(float *scrollS, float *scrollT)
{
    if(!defslot) return;
    Slot &s = *defslot;
    s.variants->scroll = vec2(*scrollS/1000.0f, *scrollT/1000.0f);
    propagatevslot(s.variants, 1<<VSLOT_SCROLL);
}
COMMAND(0, texscroll, "ff");

void texoffset_(int *xoffset, int *yoffset)
{
    if(!defslot) return;
    Slot &s = *defslot;
    s.variants->offset = ivec2(*xoffset, *yoffset).max(0);
    propagatevslot(s.variants, 1<<VSLOT_OFFSET);
}
COMMANDN(0, texoffset, texoffset_, "ii");

void texrotate_(int *rot)
{
    if(!defslot) return;
    Slot &s = *defslot;
    s.variants->rotation = clamp(*rot, 0, 7);
    propagatevslot(s.variants, 1<<VSLOT_ROTATION);
}
COMMANDN(0, texrotate, texrotate_, "i");

void texscale(float *scale)
{
    if(!defslot) return;
    Slot &s = *defslot;
    s.variants->scale = *scale <= 0 ? 1 : *scale;
    propagatevslot(s.variants, 1<<VSLOT_SCALE);
}
COMMAND(0, texscale, "f");

void texlayer(int *layer)
{
    if(!defslot) return;
    Slot &s = *defslot;
    s.variants->layer = *layer < 0 ? max(slots.length()-1+*layer, 0) : *layer;
    propagatevslot(s.variants, 1<<VSLOT_LAYER);
}
COMMAND(0, texlayer, "i");

void texdetail(int *detail)
{
    if(!defslot) return;
    Slot &s = *defslot;
    s.variants->detail = *detail < 0 ? max(slots.length()-1+*detail, 0) : *detail;
    propagatevslot(s.variants, 1<<VSLOT_DETAIL);
}
COMMAND(0, texdetail, "i");

void texalpha(float *front, float *back)
{
    if(!defslot) return;
    Slot &s = *defslot;
    s.variants->alphafront = clamp(*front, 0.0f, 1.0f);
    s.variants->alphaback = clamp(*back, 0.0f, 1.0f);
    propagatevslot(s.variants, 1<<VSLOT_ALPHA);
}
COMMAND(0, texalpha, "ff");

void texcolor(float *r, float *g, float *b)
{
    if(!defslot) return;
    Slot &s = *defslot;
    s.variants->colorscale = vec(clamp(*r, 0.0f, 2.0f), clamp(*g, 0.0f, 2.0f), clamp(*b, 0.0f, 2.0f));
    propagatevslot(s.variants, 1<<VSLOT_COLOR);
}
COMMAND(0, texcolor, "fff");

void texcoastscale(float *value)
{
    if(!defslot) return;
    Slot &s = *defslot;
    s.variants->coastscale = clamp(*value, 0.f, 1000.f);
    propagatevslot(s.variants, 1<<VSLOT_COAST);
}
COMMAND(0, texcoastscale, "f");

void texpalette(int *p, int *x)
{
    if(!defslot) return;
    Slot &s = *defslot;
    s.variants->palette = max(*p, 0);
    s.variants->palindex = max(*x, 0);
    propagatevslot(s.variants, 1<<VSLOT_PALETTE);
}
COMMAND(0, texpalette, "ii");

void texrefract(float *k, float *r, float *g, float *b)
{
    if(!defslot) return;
    Slot &s = *defslot;
    s.variants->refractscale = clamp(*k, 0.0f, 1.0f);
    if(s.variants->refractscale > 0 && (*r > 0 || *g > 0 || *b > 0))
        s.variants->refractcolor = vec(clamp(*r, 0.0f, 1.0f), clamp(*g, 0.0f, 1.0f), clamp(*b, 0.0f, 1.0f));
    else
        s.variants->refractcolor = vec(1, 1, 1);
    propagatevslot(s.variants, 1<<VSLOT_REFRACT);
}
COMMAND(0, texrefract, "ffff");

void texsmooth(int *id, int *angle)
{
    if(!defslot) return;
    Slot &s = *defslot;
    s.smooth = smoothangle(*id, *angle);
}
COMMAND(0, texsmooth, "ib");

void decaldepth(float *depth, float *fade)
{
    if(!defslot || defslot->type() != Slot::DECAL) return;
    DecalSlot &s = *(DecalSlot *)defslot;
    s.depth = clamp(*depth, 1e-3f, 1e3f);
    s.fade = clamp(*fade, 0.0f, s.depth);
}
COMMAND(0, decaldepth, "ff");

static void addglow(ImageData &c, ImageData &g, const vec &glowcolor)
{
    if(g.bpp < 3)
    {
        readwritergbtex(c, g,
            loopk(3) dst[k] = clamp(int(dst[k]) + int(src[0]*glowcolor[k]), 0, 255);
        );
    }
    else
    {
        readwritergbtex(c, g,
            loopk(3) dst[k] = clamp(int(dst[k]) + int(src[k]*glowcolor[k]), 0, 255);
        );
    }
}

static void mergespec(ImageData &c, ImageData &s)
{
    if(s.bpp < 3)
    {
        readwritergbatex(c, s,
            dst[3] = src[0];
        );
    }
    else
    {
        readwritergbatex(c, s,
            dst[3] = (int(src[0]) + int(src[1]) + int(src[2]))/3;
        );
    }
}

static void mergedepth(ImageData &c, ImageData &z)
{
    readwritergbatex(c, z,
        dst[3] = src[0];
    );
}

static void collapsespec(ImageData &s)
{
    ImageData d(s.w, s.h, 1);
    if(s.bpp >= 3) readwritetex(d, s, { dst[0] = (int(src[0]) + int(src[1]) + int(src[2]))/3; });
    else readwritetex(d, s, { dst[0] = src[0]; });
    s.replace(d);
}

int Slot::findtextype(int type, int last) const
{
    for(int i = last+1; i<sts.length(); i++) if((type&(1<<sts[i].type)) && sts[i].combined<0) return i;
    return -1;
}

int Slot::cancombine(int type) const
{
    switch(type)
    {
        case TEX_DIFFUSE: return TEX_SPEC;
        case TEX_NORMAL: return TEX_DEPTH;
        default: return -1;
    }
}

int DecalSlot::cancombine(int type) const
{
    switch(type)
    {
        case TEX_GLOW: return TEX_SPEC;
        case TEX_NORMAL: return texmask&(1<<TEX_DEPTH) ? TEX_DEPTH : (texmask&(1<<TEX_GLOW) ? -1 : TEX_SPEC);
        default: return -1;
    }
}

bool DecalSlot::shouldpremul(int type) const
{
    switch(type)
    {
        case TEX_DIFFUSE: return true;
        default: return false;
    }
}

static void addname(vector<char> &key, Slot &slot, Slot::Tex &t, bool combined = false, const char *prefix = NULL)
{
    if(combined) key.add('&');
    if(prefix) { while(*prefix) key.add(*prefix++); }
    defformatstring(tname, "%s", t.name);
    for(const char *s = path(tname); *s; key.add(*s++));
}

void Slot::load(int index, Slot::Tex &t)
{
    vector<char> key;
    addname(key, *this, t, false, shouldpremul(t.type) ? "<premul>" : NULL);
    Slot::Tex *combine = NULL;
    loopv(sts)
    {
        Slot::Tex &c = sts[i];
        if(c.combined == index)
        {
            combine = &c;
            addname(key, *this, c, true);
            break;
        }
    }
    key.add('\0');
    t.t = textures.access(key.getbuf());
    if(t.t) return;
    int compress = 0, wrap = 0;
    ImageData ts;
    TextureAnim anim;
    if(!texturedata(ts, *this, t, true, &compress, &wrap, &anim)) { t.t = notexture; return; }
    if(!ts.compressed) switch(t.type)
    {
        case TEX_SPEC:
            if(ts.bpp > 1) collapsespec(ts);
            break;
        case TEX_GLOW:
        case TEX_DIFFUSE:
        case TEX_NORMAL:
            if(combine)
            {
                ImageData cs;
                if(texturedata(cs, *this, *combine))
                {
                    if(cs.w!=ts.w || cs.h!=ts.h) scaleimage(cs, ts.w, ts.h);
                    switch(combine->type)
                    {
                        case TEX_SPEC: mergespec(ts, cs); break;
                        case TEX_DEPTH: mergedepth(ts, cs); break;
                    }
                }
            }
            if(ts.bpp < 3) swizzleimage(ts);
            break;
    }
    if(!ts.compressed && shouldpremul(t.type)) texpremul(ts);
    t.t = newtexture(NULL, key.getbuf(), ts, wrap, true, true, true, compress, &anim);
}

void Slot::load()
{
    linkslotshader(*this);
    loopv(sts)
    {
        Slot::Tex &t = sts[i];
        if(t.combined >= 0) continue;
        int combine = cancombine(t.type);
        if(combine >= 0 && (combine = findtextype(1<<combine)) >= 0)
        {
            Slot::Tex &c = sts[combine];
            c.combined = i;
        }
    }
    loopv(sts)
    {
        Slot::Tex &t = sts[i];
        if(t.combined >= 0) continue;
        switch(t.type)
        {
            case TEX_ENVMAP:
                t.t = cubemapload(t.name);
                break;

            default:
                load(i, t);
                break;
        }
    }
    loaded = true;
}

MatSlot &lookupmaterialslot(int index, bool load)
{
    MatSlot &s = materialslots[index];
    if(load && !s.linked)
    {
        if(!s.loaded) s.load();
        linkvslotshader(s);
        s.linked = true;
    }
    return s;
}

Slot &lookupslot(int index, bool load)
{
    Slot &s = slots.inrange(index) ? *slots[index] : (slots.inrange(DEFAULT_GEOM) ? *slots[DEFAULT_GEOM] : dummyslot);
    if(!s.loaded && load) s.load();
    return s;
}

VSlot &lookupvslot(int index, bool load)
{
    VSlot &s = vslots.inrange(index) && vslots[index]->slot ? *vslots[index] : (slots.inrange(DEFAULT_GEOM) && slots[DEFAULT_GEOM]->variants ? *slots[DEFAULT_GEOM]->variants : dummyvslot);
    if(load && !s.linked)
    {
        if(!s.slot->loaded) s.slot->load();
        linkvslotshader(s);
        s.linked = true;
    }
    return s;
}

DecalSlot &lookupdecalslot(int index, bool load)
{
    DecalSlot &s = decalslots.inrange(index) ? *decalslots[index] : dummydecalslot;
    if(load && !s.linked)
    {
        if(!s.loaded) s.load();
        linkvslotshader(s);
        s.linked = true;
    }
    return s;
}

void linkslotshaders()
{
    loopv(slots) if(slots[i]->loaded) linkslotshader(*slots[i]);
    loopv(vslots) if(vslots[i]->linked) linkvslotshader(*vslots[i]);
    loopi((MATF_VOLUME|MATF_INDEX)+1) if(materialslots[i].loaded)
    {
        linkslotshader(materialslots[i]);
        linkvslotshader(materialslots[i]);
    }
    loopv(decalslots) if(decalslots[i]->loaded)
    {
        linkslotshader(*decalslots[i]);
        linkvslotshader(*decalslots[i]);
    }
}

static void blitthumbnail(ImageData &d, ImageData &s, int x, int y)
{
    forcergbimage(d);
    forcergbimage(s);
    uchar *dstrow = &d.data[d.pitch*y + d.bpp*x], *srcrow = s.data;
    loop(y, s.h)
    {
        for(uchar *dst = dstrow, *src = srcrow, *end = &srcrow[s.w*s.bpp]; src < end; dst += d.bpp, src += s.bpp)
        loopk(3) dst[k] = src[k];
        dstrow += d.pitch;
        srcrow += s.pitch;
    }
}

Texture *Slot::loadthumbnail()
{
    if(thumbnail) return thumbnail;
    if(!variants)
    {
        thumbnail = notexture;
        return thumbnail;
    }
    VSlot &vslot = *variants;
    linkslotshader(*this, false);
    linkvslotshader(vslot, false);
    vector<char> name;
    vec colorscale = vslot.getcolorscale();
    if(colorscale == vec(1, 1, 1)) addname(name, *this, sts[0], false, "<thumbnail>");
    else
    {
        defformatstring(prefix, "<thumbnail:%.2f/%.2f/%.2f>", colorscale.x, colorscale.y, colorscale.z);
        addname(name, *this, sts[0], false, prefix);
    }
    int glow = -1;
    vec glowcolor = vslot.getglowcolor();
    if(texmask&(1<<TEX_GLOW))
    {
        loopvj(sts) if(sts[j].type==TEX_GLOW) { glow = j; break; }
        if(glow >= 0)
        {
            defformatstring(prefix, "<glow:%.2f/%.2f/%.2f>", glowcolor.x, glowcolor.y, glowcolor.z);
            addname(name, *this, sts[glow], true, prefix);
        }
    }
    VSlot *layer = vslot.layer ? &lookupvslot(vslot.layer, false) : NULL;
    vec layerscale = layer ? layer->getcolorscale() : vec(1, 1, 1);
    if(layer)
    {
        if(layerscale == vec(1, 1, 1)) addname(name, *layer->slot, layer->slot->sts[0], true, "<layer>");
        else
        {
            defformatstring(prefix, "<layer:%.2f/%.2f/%.2f>", layerscale.x, layerscale.y, layerscale.z);
            addname(name, *layer->slot, layer->slot->sts[0], true, prefix);
        }
    }
    VSlot *detail = vslot.detail ? &lookupvslot(vslot.detail, false) : NULL;
    if(detail) addname(name, *detail->slot, detail->slot->sts[0], true, "<detail>");
    name.add('\0');
    Texture *t = textures.access(path(name.getbuf()));
    if(t) thumbnail = t;
    else
    {
        ImageData s, g, l, d;
        texturedata(s, *this, sts[0], false);
        if(glow >= 0) texturedata(g, *this, sts[glow], false);
        if(layer) texturedata(l, *layer->slot, layer->slot->sts[0], false);
        if(detail) texturedata(d, *detail->slot, detail->slot->sts[0], false);
        if(!s.data) t = thumbnail = notexture;
        else
        {
            if(colorscale != vec(1, 1, 1)) texmad(s, colorscale, vec(0, 0, 0));
            int xs = s.w, ys = s.h;
            if(s.w > 128 || s.h > 128) scaleimage(s, min(s.w, 128), min(s.h, 128));
            if(g.data)
            {
                if(g.w != s.w || g.h != s.h) scaleimage(g, s.w, s.h);
                addglow(s, g, glowcolor);
            }
            if(l.data)
            {
                if(layerscale != vec(1, 1, 1)) texmad(l, layerscale, vec(0, 0, 0));
                if(l.w != s.w/2 || l.h != s.h/2) scaleimage(l, s.w/2, s.h/2);
                blitthumbnail(s, l, s.w-l.w, s.h-l.h);
            }
            if(d.data)
            {
                if(colorscale != vec(1, 1, 1)) texmad(d, colorscale, vec(0, 0, 0));
                if(d.w != s.w/2 || d.h != s.h/2) scaleimage(d, s.w/2, s.h/2);
                blitthumbnail(s, d, 0, 0);
            }
            if(s.bpp < 3) forcergbimage(s);
            t = newtexture(NULL, name.getbuf(), s, 0, false, false, true);
            t->xs = xs;
            t->ys = ys;
            thumbnail = t;
        }
    }
    return t;
}

// environment mapped reflections
const cubemapside cubemapsides[6] =
{
    { GL_TEXTURE_CUBE_MAP_NEGATIVE_X, "lf", false, true,  true,     90,     0,      0, 1 },
    { GL_TEXTURE_CUBE_MAP_POSITIVE_X, "rt", true,  false, true,     270,    0,      0, 0 },
    { GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, "bk", false, false, false,    180,    0,      1, 1 },
    { GL_TEXTURE_CUBE_MAP_POSITIVE_Y, "ft", true,  true,  false,    0,      0,      1, 0 },
    { GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, "dn", true,  false, true,     270,    -90,    2, 1 },
    { GL_TEXTURE_CUBE_MAP_POSITIVE_Z, "up", true,  false, true,     270,    90,     2, 0 },
};

VARF(IDF_PERSIST, envmapsize, 3, 7, 12, setupmaterials());
VAR(IDF_WORLD, envmapradius, 0, 128, 10000);
VAR(IDF_WORLD, envmapbb, 0, 0, 1);
VAR(IDF_PERSIST, aaenvmap, 0, 1, 1);

Texture *cubemaploadwildcard(Texture *t, const char *name, bool mipit, bool msg, bool transient = false)
{
    string tname;
    if(!name) copystring(tname, t->name);
    else
    {
        copystring(tname, name);
        t = textures.access(path(tname));
        if(t)
        {
            if(!transient && t->type&Texture::TRANSIENT) t->type &= ~Texture::TRANSIENT;
            return t;
        }
    }
    char *wildcard = strchr(tname, '*');
    ImageData surface[6];
    string sname;
    if(!wildcard) copystring(sname, tname);
    int tsize = 0, compress = 0;
    loopi(6)
    {
        if(wildcard)
        {
            copystring(sname, stringslice(tname, wildcard));
            concatstring(sname, cubemapsides[i].name);
            concatstring(sname, wildcard+1);
        }
        ImageData &s = surface[i];
        texturedata(s, sname, NULL, msg, &compress);
        if(!s.data) return NULL;
        if(s.w != s.h)
        {
            if(msg) conoutf("\frCubemap texture %s does not have square size", sname);
            return NULL;
        }
        if(s.compressed ? s.compressed!=surface[0].compressed || s.w!=surface[0].w || s.h!=surface[0].h || s.levels!=surface[0].levels : surface[0].compressed || s.bpp!=surface[0].bpp)
        {
            if(msg) conoutf("\frCubemap texture %s doesn't match other sides' format", sname);
            return NULL;
        }
        tsize = max(tsize, max(s.w, s.h));
    }
    if(name)
    {
        char *key = newstring(tname);
        t = &textures[key];
        t->name = key;
    }
    t->type = Texture::CUBEMAP;
    if(transient) t->type |= Texture::TRANSIENT;
    GLenum format;
    if(surface[0].compressed)
    {
        format = uncompressedformat(surface[0].compressed);
        t->bpp = formatsize(format);
        t->type |= Texture::COMPRESSED;
    }
    else
    {
        format = texformat(surface[0].bpp, true);
        t->bpp = surface[0].bpp;
        if(hasTRG && !hasTSW && swizzlemask(format))
        {
            loopi(6) swizzleimage(surface[i]);
            format = texformat(surface[0].bpp, true);
            t->bpp = surface[0].bpp;
        }
    }
    if(alphaformat(format)) t->type |= Texture::ALPHA;
    t->mipmap = mipit;
    t->clamp = 3;
    t->xs = t->ys = tsize;
    t->w = t->h = min(1<<envmapsize, tsize);
    resizetexture(t->w, t->h, mipit, false, GL_TEXTURE_CUBE_MAP, compress, t->w, t->h);
    GLenum component = format;
    if(!surface[0].compressed)
    {
        component = compressedformat(format, t->w, t->h, compress);
        switch(component)
        {
            case GL_RGB: component = GL_RGB5; break;
        }
    }
    if(t->frames.empty()) t->frames.add(0);
    glGenTextures(1, &t->frames[0]);
    loopi(6)
    {
        ImageData &s = surface[i];
        const cubemapside &side = cubemapsides[i];
        texreorient(s, side.flipx, side.flipy, side.swapxy);
        if(s.compressed)
        {
            int w = s.w, h = s.h, levels = s.levels, level = 0;
            uchar *data = s.data;
            while(levels > 1 && (w > t->w || h > t->h))
            {
                data += s.calclevelsize(level++);
                levels--;
                if(w > 1) w /= 2;
                if(h > 1) h /= 2;
            }
            createcompressedtexture(!i ? t->frames[0] : 0, w, h, data, s.align, s.bpp, levels, 3, mipit ? 2 : 1, s.compressed, side.target, true);
        }
        else createtexture(!i ? t->frames[0] : 0, t->w, t->h, s.data, 3, mipit ? 2 : 1, component, side.target, s.w, s.h, s.pitch, false, format, true);
    }
    t->id = t->frames.length() ? t->frames[0] : 0;
    return t;
}

Texture *cubemapload(const char *name, bool mipit, bool msg, bool transient)
{
    Texture *t = NULL;
    if(!strchr(name, '*'))
    {
        defformatstring(pname, "%s_*", name);
        t = cubemaploadwildcard(NULL, pname, mipit, false, transient);
        if(!t && msg) conoutf("\frCould not load envmap %s", name);
    }
    else t = cubemaploadwildcard(NULL, name, mipit, msg, transient);
    return t;
}

struct envmap
{
    int radius, size, blur;
    vec o;
    GLuint tex;

    envmap() : radius(-1), size(0), blur(0), o(0, 0, 0), tex(0) {}

    void clear()
    {
        if(tex) { glDeleteTextures(1, &tex); tex = 0; }
    }
};

static vector<envmap> envmaps;

void clearenvmaps()
{
    loopv(envmaps) envmaps[i].clear();
    envmaps.shrink(0);
}

static GLuint emfbo[3] = { 0, 0, 0 }, emtex[2] = { 0, 0 };
static int emtexsize = -1;

void setupenvmap(int texsize)
{
    if(!emtex[0]) glGenTextures(2, emtex);
    if(!emfbo[0]) glGenFramebuffers_(3, emfbo);
    if(emtexsize != texsize)
    {
        emtexsize = texsize;
        loopi(2)
        {
            createtexture(emtex[i], emtexsize, emtexsize, NULL, 3, 1, GL_RGB, GL_TEXTURE_RECTANGLE);
            glBindFramebuffer_(GL_FRAMEBUFFER, emfbo[i]);
            glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, emtex[i], 0);
            if(glCheckFramebufferStatus_(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                fatal("Failed allocating envmap buffer!");
        }
    }
}

void cleanupenvmap()
{
    if(emfbo[0]) { glDeleteFramebuffers_(3, emfbo); memset(emfbo, 0, sizeof(emfbo)); }
    if(emtex[0]) { glDeleteTextures(2, emtex); memset(emtex, 0, sizeof(emtex)); }
    emtexsize = -1;
}

GLuint genenvmap(const vec &o, int esize, int aasize, int blur, bool onlysky)
{
    int rendersize = 1<<(esize+aasize), sizelimit = min(hwcubetexsize, min(gw, gh));
    if(maxtexsize) sizelimit = min(sizelimit, maxtexsize);
    while(rendersize > sizelimit) rendersize /= 2;
    int texsize = min(rendersize, 1<<esize);
    if(!aasize) rendersize = texsize;
    setupenvmap(texsize);
    GLuint tex = 0;
    glGenTextures(1, &tex);
    // workaround for Catalyst bug:
    // all texture levels must be specified before glCopyTexSubImage2D is called, otherwise it crashes
    loopi(6) createtexture(!i ? tex : 0, texsize, texsize, NULL, 3, 2, GL_RGB5, cubemapsides[i].target);
    loopi(6)
    {
        const cubemapside &side = cubemapsides[i];
        drawcubemap(rendersize, o, side.yaw, side.pitch, onlysky);
        copyhdr(rendersize, rendersize, emfbo[0], 0, 0, texsize, texsize, side.flipx, !side.flipy, side.swapxy);
        if(blur > 0)
        {
            float blurweights[MAXBLURRADIUS+1], bluroffsets[MAXBLURRADIUS+1];
            setupblurkernel(blur, blurweights, bluroffsets);
            loopj(2)
            {
                glBindFramebuffer_(GL_FRAMEBUFFER, emfbo[1]);
                glViewport(0, 0, texsize, texsize);
                setblurshader(j, 1, blur, blurweights, bluroffsets, GL_TEXTURE_RECTANGLE);
                glBindTexture(GL_TEXTURE_RECTANGLE, emtex[0]);
                screenquad(texsize, texsize);
                swap(emfbo[0], emfbo[1]);
                swap(emtex[0], emtex[1]);
            }
        }
        for(int level = 0, lsize = texsize;; level++)
        {
            if(hasFBB)
            {
                glBindFramebuffer_(GL_READ_FRAMEBUFFER, emfbo[0]);
                glBindFramebuffer_(GL_DRAW_FRAMEBUFFER, emfbo[2]);
                glFramebufferTexture2D_(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, side.target, tex, level);
                glBlitFramebuffer_(0, 0, lsize, lsize, 0, 0, lsize, lsize, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            }
            else
            {
                glBindFramebuffer_(GL_FRAMEBUFFER, emfbo[0]);
                glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
                glCopyTexSubImage2D(side.target, level, 0, 0, 0, 0, lsize, lsize);
            }
            if(lsize <= 1) break;
            int dsize = lsize/2;
            if(hasFBB)
            {
                glBindFramebuffer_(GL_READ_FRAMEBUFFER, emfbo[0]);
                glBindFramebuffer_(GL_DRAW_FRAMEBUFFER, emfbo[1]);
                glBlitFramebuffer_(0, 0, lsize, lsize, 0, 0, dsize, dsize, GL_COLOR_BUFFER_BIT, GL_LINEAR);
            }
            else
            {
                glBindFramebuffer_(GL_FRAMEBUFFER, emfbo[1]);
                glBindTexture(GL_TEXTURE_RECTANGLE, emtex[0]);
                glViewport(0, 0, dsize, dsize);
                SETSHADER(scalelinear);
                screenquad(lsize, lsize);
            }
            lsize = dsize;
            swap(emfbo[0], emfbo[1]);
            swap(emtex[0], emtex[1]);
        }
    }
    glBindFramebuffer_(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, hudw, hudh);
    clientkeepalive();
    return tex;
}

void initenvmaps()
{
    clearenvmaps();
    envmaps.add().size = hasskybox() ? 0 : 1;
    const vector<extentity *> &ents = entities::getents();
    loopv(ents)
    {
        const extentity &ent = *ents[i];
        if(ent.type != ET_ENVMAP) continue;
        envmap &em = envmaps.add();
        em.radius = clamp(int(ent.attrs[0]), 0, 10000);
        em.size = ent.attrs[1] ? clamp(int(ent.attrs[1]), 3, 12) : 0;
        em.blur = ent.attrs[2] ? clamp(int(ent.attrs[2]), 1, 2) : 0;
        em.o = ent.o;
    }
}

void genenvmaps()
{
    if(envmaps.empty()) return;
    progress(0, "Generating environment maps...");
    int lastprogress = SDL_GetTicks();
    loopv(envmaps)
    {
        envmap &em = envmaps[i];
        em.tex = genenvmap(em.o, em.size ? min(em.size, envmapsize) : envmapsize, aaenvmap, em.blur, em.radius < 0);
        if(renderedframe) continue;
        int millis = SDL_GetTicks();
        if(millis-lastprogress >= 250)
        {
            progress(float(i+1)/envmaps.length(), "Generating environment maps...");
            lastprogress = millis;
        }
    }
}

ushort closestenvmap(const vec &o)
{
    ushort minemid = EMID_SKY;
    float mindist = 1e16f;
    loopv(envmaps)
    {
        envmap &em = envmaps[i];
        float dist, radius = em.radius ? em.radius : envmapradius;
        if(envmapbb)
        {
            if(!o.insidebb(vec(em.o).sub(radius), vec(em.o).add(radius))) continue;
            dist = em.o.dist(o);
        }
        else
        {
            dist = em.o.dist(o);
            if(dist > radius) continue;
        }
        if(dist < mindist)
        {
            minemid = EMID_RESERVED + i;
            mindist = dist;
        }
    }
    return minemid;
}

ushort closestenvmap(int orient, const ivec &co, int size)
{
    vec loc(co);
    int dim = dimension(orient);
    if(dimcoord(orient)) loc[dim] += size;
    loc[R[dim]] += size/2;
    loc[C[dim]] += size/2;
    return closestenvmap(loc);
}

static inline GLuint lookupskyenvmap()
{
    return envmaps.length() && envmaps[0].radius < 0 ? envmaps[0].tex : 0;
}

GLuint lookupenvmap(Slot &slot)
{
    loopv(slot.sts) if(slot.sts[i].type==TEX_ENVMAP && slot.sts[i].t) return slot.sts[i].t->id;
    return lookupskyenvmap();
}

GLuint lookupenvmap(ushort emid)
{
    if(emid==EMID_SKY || emid==EMID_CUSTOM || drawtex) return lookupskyenvmap();
    if(emid==EMID_NONE || !envmaps.inrange(emid-EMID_RESERVED)) return 0;
    GLuint tex = envmaps[emid-EMID_RESERVED].tex;
    return tex ? tex : lookupskyenvmap();
}

VAR(0, debugenvmaps, 0, 0, 1);
bool viewenvmaps(int y, int s)
{
    if(!debugenvmaps || envmaps.empty()) return false;
    GLuint tex = lookupenvmap(closestenvmap(camera1->o));
    if(!tex) return false;
    SETSHADER(hudenvmap);
    glActiveTexture_(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
    gle::colorf(1, 1, 1);
    debugquad(0, y, s, s);
    return true;
}

VAR(IDF_PERSIST, matcapsize, 1, 5, 10);
VAR(IDF_PERSIST, matcapatlassize, 9, 12, 14);
VAR(IDF_PERSIST, matcapdist, 1, 64, 10000);
VAR(IDF_PERSIST, matcapmaxdist, 1, 128, 10000);
VAR(IDF_PERSIST, matcapblur, 0, 2, 2);
VAR(IDF_PERSIST, matcapborder, 0, 4, 4);
VAR(IDF_PERSIST, aamatcap, 0, 1, 1);

struct matcap
{
    vec o;
    int ent, tx, ty, ts, tb;
    bool sky;
    linkvector links;

    matcap() : o(0, 0, 0), ent(-1), tx(0), ty(0), ts(0), tb(0), sky(false)
    {
        links.shrink(0);
    }
};

static vector<matcap> matcaps;
static GLuint matcapatlastex = 0, matcapatlasfbo = 0;
static int matcapatlasdim = 0, matcapatlasx = 0, matcapatlasy = 0;

void setupmatcapatlas()
{
    matcapatlasdim = min((1<<matcapatlassize), hwtexsize);
    matcapatlasx = matcapatlasy = matcapborder;
    if(!matcapatlastex) glGenTextures(1, &matcapatlastex);
    if(!matcapatlasfbo) glGenFramebuffers_(1, &matcapatlasfbo);
    createtexture(matcapatlastex, matcapatlasdim, matcapatlasdim, NULL, 3, 1, GL_RGB5, GL_TEXTURE_RECTANGLE);
}

void genmatcap(const vec &o, int tx, int ty, int texsize, int border, int blur, bool onlysky)
{
    loopi(6)
    {
        const cubemapside &side = cubemapsides[i];
        int mx = tx+texsize*side.mcx+border*side.mcx, my = ty+texsize*side.mcy+border*side.mcy;
        drawcubemap(texsize, o, side.yaw, side.pitch, onlysky);
        glBindFramebuffer_(GL_FRAMEBUFFER, matcapatlasfbo);
        glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, matcapatlastex, 0);
        copyhdr(texsize, texsize, matcapatlasfbo, mx, my, texsize, texsize, side.flipx, !side.flipy, side.swapxy);
    }
    glBindFramebuffer_(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, hudw, hudh);
    clientkeepalive();
}

void cleanupmatcapatlas()
{
    if(matcapatlastex) { glDeleteTextures(1, &matcapatlastex); matcapatlastex = 0; }
    if(matcapatlasfbo) { glDeleteFramebuffers_(1, &matcapatlasfbo); matcapatlasfbo = 0; }
    matcapatlasx = matcapatlasy = matcapborder;
}

void clearmatcaps()
{
    matcaps.shrink(0);
    cleanupmatcapatlas();
}

void initmatcaps()
{
    clearmatcaps();
    matcaps.add().sky = true;
    const vector<extentity *> &ents = entities::getents();
    loopv(ents)
    {
        const extentity &ent = *ents[i];
        if(ent.type != ET_OUTLINE) continue;
        matcap &mc = matcaps.add();
        mc.o = ent.o;
        mc.ent = i;
    }
    loopv(matcaps) if(ents.inrange(matcaps[i].ent))
    {
        matcap &mc = matcaps[i];
        const extentity &ent = *ents[mc.ent];
        loopvj(ent.links) if(ents.inrange(ent.links[j]))
        {
            int link = ent.links[j], last = i;
            if(link > matcaps[i].ent) // otherwise we've already processed this link
            {
                const extentity &lent = *ents[link];
                float dist = mc.o.dist(lent.o), dvc = dist/float(matcapdist);
                if(dvc > 1)
                {
                    int num = int(ceilf(dvc));
                    float space = dist/float(num);
                    num--; // fill in between, but not the target
                    vec dir = vec(lent.o).sub(mc.o).normalize().mul(space), iter = mc.o;
                    loopk(num)
                    {
                        int lnum = matcaps.length();
                        matcap &lmc = matcaps.add();
                        lmc.o = iter.add(dir);
                        matcaps[last].links.add(lnum);
                        lmc.links.add(last);
                        last = lnum;
                    }
                }
                loopvk(matcaps) if(matcaps[k].ent == link)
                {
                    matcaps[last].links.add(k);
                    matcaps[k].links.add(last);
                }
            }
        }
    }
}

void genmatcaps()
{
    if(matcaps.empty()) return;
    progress(0, "Generating material captures...");
    int lastprogress = SDL_GetTicks(), rendersize = 1<<matcapsize, sizelimit = min(hwtexsize, min(gw, gh));
    if(maxtexsize) sizelimit = min(sizelimit, maxtexsize);
    while(rendersize > sizelimit) rendersize /= 2;
    int texsize = min(rendersize, 1<<matcapsize);
    setupmatcapatlas();
    setupenvmap(texsize);
    int xsize = texsize*3+matcapborder*3, ysize = texsize*2+matcapborder*2;
    loopv(matcaps)
    {
        matcap &mc = matcaps[i];
        mc.tx = matcapatlasx;
        mc.ty = matcapatlasy;
        mc.ts = texsize;
        mc.tb = matcapborder;
        //conoutf("Generating matcap %d at %d, %d with size %d [%d]...", i, mc.tx, mc.ty, mc.ts, mc.tb);
        genmatcap(mc.o, mc.tx, mc.ty, texsize, matcapborder, matcapblur, mc.sky);
        matcapatlasx += xsize;
        if(matcapatlasx+xsize > matcapatlasdim)
        {
            matcapatlasx = matcapborder;
            matcapatlasy += ysize;
            if(matcapatlasy+ysize > matcapatlasdim)
            {
                conoutf("Material capture atlas is full.");
                break;
            }
        }
        if(renderedframe) continue;
        int millis = SDL_GetTicks();
        if(millis-lastprogress >= 250)
        {
            progress(float(i+1)/matcaps.length(), "Generating material captures...");
            lastprogress = millis;
        }
    }
    if(matcapatlasfbo) { glDeleteFramebuffers_(1, &matcapatlasfbo); matcapatlasfbo = 0; }
}

int getmatcap(const vec &o, int len, int values[][4], float *blend, int *id1 = NULL, int *id2 = NULL)
{
    if(len < 1 || len > 2 || !values || matcaps.empty()) return 0;
    int closest = -1;
    float mindist = 1e16f;
    loopv(matcaps)
    {
        matcap &mc = matcaps[i];
        float dist = mc.o.dist(o);
        if(dist > matcapmaxdist) continue;
        if(closest < 0 || dist < mindist)
        {
            closest = i;
            mindist = dist;
        }
    }
    if(matcaps.inrange(closest))
    {
        int num = 1;
        matcap &mc = matcaps[closest];
        values[0][0] = mc.tx;
        values[0][1] = mc.ty;
        values[0][2] = mc.ts;
        values[0][3] = mc.tb;
        if(id1) *id1 = closest;
        if(len > 1 && blend && mindist > 0)
        {
            closest = -1;
            mindist = 1e16f;
            loopv(mc.links) if(matcaps.inrange(mc.links[i]))
            {
                matcap &lmc = matcaps[mc.links[i]];
                float dist = lmc.o.dist(o);
                if(dist > matcapmaxdist) continue;
                if(closest < 0 || dist < mindist)
                {
                    closest = mc.links[i];
                    mindist = dist;
                }
            }
            if(matcaps.inrange(closest))
            {
                matcap &lmc = matcaps[closest];
                vec u = vec(o).sub(mc.o), v = vec(lmc.o).sub(mc.o);
                float mdot = u.dot(v);
                if(mdot > 0)
                {
                    float mlen = v.squaredlen();
                    if(mlen > 0)
                    {
                        values[1][0] = lmc.tx;
                        values[1][1] = lmc.ty;
                        values[1][2] = lmc.ts;
                        values[1][3] = lmc.tb;
                        *blend = mdot/mlen;
                        if(id2) *id2 = closest;
                        num = 2;
                    }
                }
            }
        }
        return num;
    }
    if(!matcaps.empty())
    {
        matcap &mc = matcaps[0];
        if(mc.sky)
        {
            values[0][0] = mc.tx;
            values[0][1] = mc.ty;
            values[0][2] = mc.ts;
            values[0][3] = mc.tb;
            if(id1) *id1 = 0;
            return 1;
        }
    }
    return 0;
}

void clearenvtexs()
{
    clearenvmaps();
    clearmatcaps();
}

void initenvtexs()
{
    initenvmaps();
    initmatcaps();
}

void genenvtexs()
{
    gl_setupframe(true);
    genenvmaps();
    genmatcaps();
    cleanupenvmap();
}

VAR(0, debugmatcaps, 0, 0, 5);
bool viewmatcaps(int y, int s)
{
    if(debugmatcaps < 2 || !matcapatlastex || matcaps.empty()) return false;
    SETSHADER(hudrect);
    glActiveTexture_(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_RECTANGLE, matcapatlastex);
    gle::colorf(1, 1, 1, 1);
    debugquad(0, y, s, s, 0, 0, matcapatlasdim, matcapatlasdim);
    if(debugmatcaps < 3) return false;
    int matcapdims[2][4] = { { 0, 0, 0, 0 }, { 0, 0, 0, 0 } }, matcapid[2] = { -1, -1 };
    float blend = 1;
    int num = getmatcap(camera1->o, 2, matcapdims, &blend, &matcapid[0], &matcapid[1]);
    if(!num) return false;
    int view = min(min(debugmatcaps-2, 2), num);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    SETSHADER(hudmatcap);
    glBindTexture(GL_TEXTURE_RECTANGLE, matcapatlastex);
    loopi(view)
    {
        LOCALPARAMF(mcparams,
            float(matcapdims[i][0] + 0.5f*matcapdims[i][2]),
            float(matcapdims[i][1] + 0.5f*matcapdims[i][2]),
            float(matcapdims[i][2]),
            float(-0.5f * (matcapdims[i][2] - matcapdims[i][3]))
        );
        debugquad(s*(i+1), y, s, s, 0, 0, matcapatlasdim, matcapatlasdim);
        if(debugmatcaps > 4)
        {
            if(i) gle::colorf(1, 1, 1, blend);
            debugquad(s*3, y, s, s, 0, 0, matcapatlasdim, matcapatlasdim);
            if(i) gle::colorf(1, 1, 1, 1);
        }
    }
    float dist[2] = { -1, -1 };
    loopi(view)
    {
        dist[i] = camera1->o.dist(matcaps[matcapid[i]].o);
        draw_textf("%d @ [%d,%d:%d] (%.2f)", s*(i+1)+FONTH/4, y, 0, 0, 255, 255, 255, 255, TEXT_LEFT_JUSTIFY, -1, -1, 1, matcapid[i], matcapdims[i][0], matcapdims[i][1], matcapdims[i][2], dist[i]);
    }
    if(debugmatcaps > 4 && num > 1)
        draw_textf("Blend: %.2f (%.2f)", s*3+FONTH/4, y, 0, 0, 255, 255, 255, 255, TEXT_LEFT_JUSTIFY, -1, -1, 1, blend, dist[1]-dist[0]);
    glDisable(GL_BLEND);
    return true;
}

void rendermatcaps()
{
    if(!debugmatcaps || matcaps.empty()) return;
    vec off(0, 0, 2);
    loopv(matcaps)
    {
        matcap &mc = matcaps[i];
        vec pos = mc.o;
        part_create(PART_EDIT_ONTOP, 1, pos, 0x22FF22, 2);
        defformatstring(s, "<super>matcap (%d) [%.2f]", i, camera1->o.dist(mc.o));
        part_textcopy(pos.add(off), s, PART_TEXT);
        s[0] = 0;
        loopvk(mc.links) if(matcaps.inrange(mc.links[k]))
        {
            matcap &lmc = matcaps[mc.links[k]];
            if(mc.links[k] > i) part_trace(mc.o, lmc.o, 2, 1, 1, lmc.links.find(i) >= 0 ? 0x22FF22 : 0xFF2222);
            concformatstring(s, "%s%d", s[0] ? ", " : "links: ", mc.links[k]);
        }
        if(s[0]) part_textcopy(pos.add(off), s, PART_TEXT);
    }
}

void cleanuptexture(Texture *t)
{
    if(t->frames.length() > 1 && t->delay > 0) animtextures.removeobj(t);

    DELETEA(t->alphamask);

    loopvk(t->frames) if(t->frames[k])
    {
        if(t->frames[k])
        {
            if(t->frames[k] == t->id) t->id = 0; // using a frame directly
            glDeleteTextures(1, &t->frames[k]);
            t->frames[k] = 0;
        }
    }
    t->frames.shrink(0);
    t->id = 0;

    if(t->type&Texture::TRANSIENT) textures.remove(t->name);
}

void cleanuptextures()
{
    clearenvtexs();
    loopv(slots) slots[i]->cleanup();
    loopv(vslots) vslots[i]->cleanup();
    loopi((MATF_VOLUME|MATF_INDEX)+1) materialslots[i].cleanup();
    loopv(decalslots) decalslots[i]->cleanup();
    enumerate(textures, Texture, tex, cleanuptexture(&tex));
}

bool reloadtexture(const char *name)
{
    string tname;
    copystring(tname, name);
    path(tname);
    Texture *t = textures.access(tname);
    if(t) return reloadtexture(t);
    return true;
}

bool reloadtexture(Texture *t)
{
    loopv(t->frames) if(t->frames[i]) return true;
    switch(t->type&Texture::TYPE)
    {
        case Texture::IMAGE:
        {
            int compress = 0;
            ImageData s;
            TextureAnim anim;
            if(!texturedata(s, t->name, NULL, true, &compress, NULL, &anim) || !newtexture(t, NULL, s, t->clamp, t->mipmap, false, false, compress, &anim)) return false;
            break;
        }

        case Texture::CUBEMAP:
            if(!cubemaploadwildcard(t, NULL, t->mipmap, true)) return false;
            break;
    }
    return true;
}

void reloadtex(char *name)
{
    Texture *t = textures.access(copypath(name));
    if(!t) { conoutf("\frTexture %s is not loaded", name); return; }
    if(t->type&Texture::TRANSIENT) { conoutf("\frCan't reload transient texture %s", name); return; }
    DELETEA(t->alphamask);
    Texture oldtex = *t;
    t->frames.shrink(0);
    t->id = 0;
    if(!reloadtexture(t))
    {
        loopv(t->frames) if(t->frames[i]) glDeleteTextures(1, &t->frames[i]);
        *t = oldtex;
        conoutf("\frFailed to reload texture %s", name);
    }
}
COMMAND(0, reloadtex, "s");

void reloadtextures()
{
    int reloaded = 0;
    enumerate(textures, Texture, tex,
    {
        loadprogress = float(++reloaded)/textures.numelems;
        reloadtexture(&tex);
    });
    loadprogress = 0;
}

enum
{
    DDSD_CAPS                  = 0x00000001,
    DDSD_HEIGHT                = 0x00000002,
    DDSD_WIDTH                 = 0x00000004,
    DDSD_PITCH                 = 0x00000008,
    DDSD_PIXELFORMAT           = 0x00001000,
    DDSD_MIPMAPCOUNT           = 0x00020000,
    DDSD_LINEARSIZE            = 0x00080000,
    DDSD_BACKBUFFERCOUNT       = 0x00800000,
    DDPF_ALPHAPIXELS           = 0x00000001,
    DDPF_FOURCC                = 0x00000004,
    DDPF_INDEXED               = 0x00000020,
    DDPF_ALPHA                 = 0x00000002,
    DDPF_RGB                   = 0x00000040,
    DDPF_COMPRESSED            = 0x00000080,
    DDPF_LUMINANCE             = 0x00020000,
    DDSCAPS_COMPLEX            = 0x00000008,
    DDSCAPS_TEXTURE            = 0x00001000,
    DDSCAPS_MIPMAP             = 0x00400000,
    DDSCAPS2_CUBEMAP           = 0x00000200,
    DDSCAPS2_CUBEMAP_POSITIVEX = 0x00000400,
    DDSCAPS2_CUBEMAP_NEGATIVEX = 0x00000800,
    DDSCAPS2_CUBEMAP_POSITIVEY = 0x00001000,
    DDSCAPS2_CUBEMAP_NEGATIVEY = 0x00002000,
    DDSCAPS2_CUBEMAP_POSITIVEZ = 0x00004000,
    DDSCAPS2_CUBEMAP_NEGATIVEZ = 0x00008000,
    DDSCAPS2_VOLUME            = 0x00200000,
    FOURCC_DXT1                = 0x31545844,
    FOURCC_DXT2                = 0x32545844,
    FOURCC_DXT3                = 0x33545844,
    FOURCC_DXT4                = 0x34545844,
    FOURCC_DXT5                = 0x35545844,
    FOURCC_ATI1                = 0x31495441,
    FOURCC_ATI2                = 0x32495441
};

struct DDCOLORKEY { uint dwColorSpaceLowValue, dwColorSpaceHighValue; };
struct DDPIXELFORMAT
{
    uint dwSize, dwFlags, dwFourCC;
    union { uint dwRGBBitCount, dwYUVBitCount, dwZBufferBitDepth, dwAlphaBitDepth, dwLuminanceBitCount, dwBumpBitCount, dwPrivateFormatBitCount; };
    union { uint dwRBitMask, dwYBitMask, dwStencilBitDepth, dwLuminanceBitMask, dwBumpDuBitMask, dwOperations; };
    union { uint dwGBitMask, dwUBitMask, dwZBitMask, dwBumpDvBitMask; struct { ushort wFlipMSTypes, wBltMSTypes; } MultiSampleCaps; };
    union { uint dwBBitMask, dwVBitMask, dwStencilBitMask, dwBumpLuminanceBitMask; };
    union { uint dwRGBAlphaBitMask, dwYUVAlphaBitMask, dwLuminanceAlphaBitMask, dwRGBZBitMask, dwYUVZBitMask; };

};
struct DDSCAPS2 { uint dwCaps, dwCaps2, dwCaps3, dwCaps4; };
struct DDSURFACEDESC2
{
    uint dwSize, dwFlags, dwHeight, dwWidth;
    union { int lPitch; uint dwLinearSize; };
    uint dwBackBufferCount;
    union { uint dwMipMapCount, dwRefreshRate, dwSrcVBHandle; };
    uint dwAlphaBitDepth, dwReserved, lpSurface;
    union { DDCOLORKEY ddckCKDestOverlay; uint dwEmptyFaceColor; };
    DDCOLORKEY ddckCKDestBlt, ddckCKSrcOverlay, ddckCKSrcBlt;
    union { DDPIXELFORMAT ddpfPixelFormat; uint dwFVF; };
    DDSCAPS2 ddsCaps;
    uint dwTextureStage;
};

#define DECODEDDS(name, dbpp, initblock, writeval, nextval) \
static void name(ImageData &s) \
{ \
    ImageData d(s.w, s.h, dbpp); \
    uchar *dst = d.data; \
    const uchar *src = s.data; \
    for(int by = 0; by < s.h; by += s.align) \
    { \
        for(int bx = 0; bx < s.w; bx += s.align, src += s.bpp) \
        { \
            int maxy = min(d.h - by, s.align), maxx = min(d.w - bx, s.align); \
            initblock; \
            loop(y, maxy) \
            { \
                int x; \
                for(x = 0; x < maxx; ++x) \
                { \
                    writeval; \
                    nextval; \
                    dst += d.bpp; \
                }  \
                for(; x < s.align; ++x) { nextval; } \
                dst += d.pitch - maxx*d.bpp; \
            } \
            dst += maxx*d.bpp - maxy*d.pitch; \
        } \
        dst += (s.align-1)*d.pitch; \
    } \
    s.replace(d); \
}

DECODEDDS(decodedxt1, s.compressed == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT ? 4 : 3,
    ushort color0 = lilswap(*(const ushort *)src);
    ushort color1 = lilswap(*(const ushort *)&src[2]);
    uint bits = lilswap(*(const uint *)&src[4]);
    bvec4 rgba[4];
    rgba[0] = bvec4(bvec::from565(color0), 0xFF);
    rgba[1] = bvec4(bvec::from565(color1), 0xFF);
    if(color0 > color1)
    {
        rgba[2].lerp(rgba[0], rgba[1], 2, 1, 3);
        rgba[3].lerp(rgba[0], rgba[1], 1, 2, 3);
    }
    else
    {
        rgba[2].lerp(rgba[0], rgba[1], 1, 1, 2);
        rgba[3] = bvec4(0, 0, 0, 0);
    }
,
    memcpy(dst, rgba[bits&3].v, d.bpp);
,
    bits >>= 2;
);

DECODEDDS(decodedxt3, 4,
    ullong alpha = lilswap(*(const ullong *)src);
    ushort color0 = lilswap(*(const ushort *)&src[8]);
    ushort color1 = lilswap(*(const ushort *)&src[10]);
    uint bits = lilswap(*(const uint *)&src[12]);
    bvec rgb[4];
    rgb[0] = bvec::from565(color0);
    rgb[1] = bvec::from565(color1);
    rgb[2].lerp(rgb[0], rgb[1], 2, 1, 3);
    rgb[3].lerp(rgb[0], rgb[1], 1, 2, 3);
,
    memcpy(dst, rgb[bits&3].v, 3);
    dst[3] = ((alpha&0xF)*1088 + 32) >> 6;
,
    bits >>= 2;
    alpha >>= 4;
);

static inline void decodealpha(uchar alpha0, uchar alpha1, uchar alpha[8])
{
    alpha[0] = alpha0;
    alpha[1] = alpha1;
    if(alpha0 > alpha1)
    {
        alpha[2] = (6*alpha0 + alpha1)/7;
        alpha[3] = (5*alpha0 + 2*alpha1)/7;
        alpha[4] = (4*alpha0 + 3*alpha1)/7;
        alpha[5] = (3*alpha0 + 4*alpha1)/7;
        alpha[6] = (2*alpha0 + 5*alpha1)/7;
        alpha[7] = (alpha0 + 6*alpha1)/7;
    }
    else
    {
        alpha[2] = (4*alpha0 + alpha1)/5;
        alpha[3] = (3*alpha0 + 2*alpha1)/5;
        alpha[4] = (2*alpha0 + 3*alpha1)/5;
        alpha[5] = (alpha0 + 4*alpha1)/5;
        alpha[6] = 0;
        alpha[7] = 0xFF;
    }
}

DECODEDDS(decodedxt5, 4,
    uchar alpha[8];
    decodealpha(src[0], src[1], alpha);
    ullong alphabits = lilswap(*(const ushort *)&src[2]) + ((ullong)lilswap(*(const uint *)&src[4]) << 16);
    ushort color0 = lilswap(*(const ushort *)&src[8]);
    ushort color1 = lilswap(*(const ushort *)&src[10]);
    uint bits = lilswap(*(const uint *)&src[12]);
    bvec rgb[4];
    rgb[0] = bvec::from565(color0);
    rgb[1] = bvec::from565(color1);
    rgb[2].lerp(rgb[0], rgb[1], 2, 1, 3);
    rgb[3].lerp(rgb[0], rgb[1], 1, 2, 3);
,
    memcpy(dst, rgb[bits&3].v, 3);
    dst[3] = alpha[alphabits&7];
,
    bits >>= 2;
    alphabits >>= 3;
);

DECODEDDS(decodergtc1, 1,
    uchar red[8];
    decodealpha(src[0], src[1], red);
    ullong redbits = lilswap(*(const ushort *)&src[2]) + ((ullong)lilswap(*(const uint *)&src[4]) << 16);
,
    dst[0] = red[redbits&7];
,
    redbits >>= 3;
);

DECODEDDS(decodergtc2, 2,
    uchar red[8];
    decodealpha(src[0], src[1], red);
    ullong redbits = lilswap(*(const ushort *)&src[2]) + ((ullong)lilswap(*(const uint *)&src[4]) << 16);
    uchar green[8];
    decodealpha(src[8], src[9], green);
    ullong greenbits = lilswap(*(const ushort *)&src[10]) + ((ullong)lilswap(*(const uint *)&src[12]) << 16);
,
    dst[0] = red[redbits&7];
    dst[1] = green[greenbits&7];
,
    redbits >>= 3;
    greenbits >>= 3;
);

bool loaddds(const char *filename, ImageData &image, int force)
{
    stream *f = openfile(filename, "rb");
    if(!f) return false;
    GLenum format = GL_FALSE;
    uchar magic[4];
    if(f->read(magic, 4) != 4 || memcmp(magic, "DDS ", 4)) { delete f; return false; }
    DDSURFACEDESC2 d;
    if(f->read(&d, sizeof(d)) != sizeof(d)) { delete f; return false; }
    lilswap((uint *)&d, sizeof(d)/sizeof(uint));
    if(d.dwSize != sizeof(DDSURFACEDESC2) || d.ddpfPixelFormat.dwSize != sizeof(DDPIXELFORMAT)) { delete f; return false; }
    bool supported = false;
    if(d.ddpfPixelFormat.dwFlags & DDPF_FOURCC)
    {
        switch(d.ddpfPixelFormat.dwFourCC)
        {
            case FOURCC_DXT1:
                if((supported = hasS3TC) || force) format = d.ddpfPixelFormat.dwFlags & DDPF_ALPHAPIXELS ? GL_COMPRESSED_RGBA_S3TC_DXT1_EXT : GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
                break;
            case FOURCC_DXT2:
            case FOURCC_DXT3:
                if((supported = hasS3TC) || force) format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
                break;
            case FOURCC_DXT4:
            case FOURCC_DXT5:
                if((supported = hasS3TC) || force) format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
                break;
            case FOURCC_ATI1:
                if((supported = hasRGTC) || force) format = GL_COMPRESSED_RED_RGTC1;
                else if((supported = hasLATC)) format = GL_COMPRESSED_LUMINANCE_LATC1_EXT;
                break;
            case FOURCC_ATI2:
                if((supported = hasRGTC) || force) format = GL_COMPRESSED_RG_RGTC2;
                else if((supported = hasLATC)) format = GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT;
                break;
        }
    }
    if(!format || (!supported && !force)) { delete f; return false; }
    if(dbgdds) conoutf("%s: format 0x%X, %d x %d, %d mipmaps", filename, format, d.dwWidth, d.dwHeight, d.dwMipMapCount);
    int bpp = 0;
    switch(format)
    {
        case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT: bpp = 8; break;
        case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT: bpp = 16; break;
        case GL_COMPRESSED_LUMINANCE_LATC1_EXT:
        case GL_COMPRESSED_RED_RGTC1: bpp = 8; break;
        case GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT:
        case GL_COMPRESSED_RG_RGTC2: bpp = 16; break;
    }
    image.setdata(NULL, d.dwWidth, d.dwHeight, bpp, !supported || force > 0 ? 1 : d.dwMipMapCount, 4, format);
    size_t size = image.calcsize();
    if(f->read(image.data, size) != size) { delete f; image.cleanup(); return false; }
    delete f;
    if(!supported || force > 0) switch(format)
    {
        case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
            decodedxt1(image);
            break;
        case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
            decodedxt3(image);
            break;
        case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
            decodedxt5(image);
            break;
        case GL_COMPRESSED_LUMINANCE_LATC1_EXT:
        case GL_COMPRESSED_RED_RGTC1:
            decodergtc1(image);
            break;
        case GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT:
        case GL_COMPRESSED_RG_RGTC2:
            decodergtc2(image);
            break;
    }
    return true;
}

void gendds(char *infile, char *outfile)
{
    if(!hasS3TC || usetexcompress <= 1) { conoutf("\frOpenGL driver does not support S3TC texture compression"); return; }

    glHint(GL_TEXTURE_COMPRESSION_HINT, GL_NICEST);

    defformatstring(cfile, "<compress>%s", infile);
    extern void reloadtex(char *name);
    Texture *t = textures.access(path(cfile));
    if(t) reloadtex(cfile);
    t = textureload(cfile);
    if(t==notexture || t->frames.empty()) { conoutf("\frFailed loading %s", infile); return; }

    if(t->frames.empty()) t->frames.add(0);
    glBindTexture(GL_TEXTURE_2D, t->frames[0]);
    GLint compressed = 0, format = 0, width = 0, height = 0;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED, &compressed);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &format);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);

    if(!compressed) { conoutf("\frFailed compressing %s", infile); return; }
    int fourcc = 0;
    switch(format)
    {
        case GL_COMPRESSED_RGB_S3TC_DXT1_EXT: fourcc = FOURCC_DXT1; conoutf("Compressed as DXT1"); break;
        case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT: fourcc = FOURCC_DXT1; conoutf("Compressed as DXT1a"); break;
        case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT: fourcc = FOURCC_DXT3; conoutf("Compressed as DXT3"); break;
        case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT: fourcc = FOURCC_DXT5; conoutf("Compressed as DXT5"); break;
        case GL_COMPRESSED_LUMINANCE_LATC1_EXT:
        case GL_COMPRESSED_RED_RGTC1: fourcc = FOURCC_ATI1; conoutf("Compressed as ATI1"); break;
        case GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT:
        case GL_COMPRESSED_RG_RGTC2: fourcc = FOURCC_ATI2; conoutf("Compressed as ATI2"); break;
        default:
            conoutf("\frFailed compressing %s: unknown format: 0x%X", infile, format); break;
            return;
    }

    if(!outfile[0])
    {
        static string buf;
        copystring(buf, infile);
        int len = strlen(buf);
        if(len > 4 && buf[len-4]=='.') memcpy(&buf[len-4], ".dds", 4);
        else concatstring(buf, ".dds");
        outfile = buf;
    }

    stream *f = openfile(path(outfile), "wb");
    if(!f) { conoutf("\frFailed writing to %s", outfile); return; }

    int csize = 0;
    for(int lw = width, lh = height, level = 0;;)
    {
        GLint size = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D, level++, GL_TEXTURE_COMPRESSED_IMAGE_SIZE, &size);
        csize += size;
        if(max(lw, lh) <= 1) break;
        if(lw > 1) lw /= 2;
        if(lh > 1) lh /= 2;
    }

    DDSURFACEDESC2 d;
    memset(&d, 0, sizeof(d));
    d.dwSize = sizeof(DDSURFACEDESC2);
    d.dwWidth = width;
    d.dwHeight = height;
    d.dwLinearSize = csize;
    d.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_LINEARSIZE | DDSD_MIPMAPCOUNT;
    d.ddsCaps.dwCaps = DDSCAPS_TEXTURE | DDSCAPS_COMPLEX | DDSCAPS_MIPMAP;
    d.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
    d.ddpfPixelFormat.dwFlags = DDPF_FOURCC | (alphaformat(uncompressedformat(format)) ? DDPF_ALPHAPIXELS : 0);
    d.ddpfPixelFormat.dwFourCC = fourcc;

    uchar *data = new uchar[csize], *dst = data;
    for(int lw = width, lh = height;;)
    {
        GLint size;
        glGetTexLevelParameteriv(GL_TEXTURE_2D, d.dwMipMapCount, GL_TEXTURE_COMPRESSED_IMAGE_SIZE, &size);
        glGetCompressedTexImage_(GL_TEXTURE_2D, d.dwMipMapCount++, dst);
        dst += size;
        if(max(lw, lh) <= 1) break;
        if(lw > 1) lw /= 2;
        if(lh > 1) lh /= 2;
    }

    lilswap((uint *)&d, sizeof(d)/sizeof(uint));

    f->write("DDS ", 4);
    f->write(&d, sizeof(d));
    f->write(data, csize);
    delete f;

    delete[] data;

    conoutf("Wrote DDS file %s", outfile);

    setuptexcompress();
}
COMMAND(0, gendds, "ss");

void writepngchunk(stream *f, const char *type, uchar *data = NULL, uint len = 0)
{
    f->putbig<uint>(len);
    f->write(type, 4);
    f->write(data, len);

    uint crc = crc32(0, Z_NULL, 0);
    crc = crc32(crc, (const Bytef *)type, 4);
    if(data) crc = crc32(crc, data, len);
    f->putbig<uint>(crc);
}

void savepng(const char *filename, ImageData &image, int compress, bool flip)
{
    uchar ctype = 0;
    switch(image.bpp)
    {
        case 1: ctype = 0; break;
        case 2: ctype = 4; break;
        case 3: ctype = 2; break;
        case 4: ctype = 6; break;
        default: conoutf("\frFailed saving png to %s", filename); return;
    }
    stream *f = openfile(filename, "wb");
    if(!f) { conoutf("\frCould not write to %s", filename); return; }

    uchar signature[] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    f->write(signature, sizeof(signature));

    struct pngihdr
    {
        uint width, height;
        uchar bitdepth, colortype, compress, filter, interlace;
    } ihdr = { bigswap<uint>(image.w), bigswap<uint>(image.h), 8, ctype, 0, 0, 0 };
    writepngchunk(f, "IHDR", (uchar *)&ihdr, 13);

    stream::offset idat = f->tell();
    uint len = 0;
    f->write("\0\0\0\0IDAT", 8);
    uint crc = crc32(0, Z_NULL, 0);
    crc = crc32(crc, (const Bytef *)"IDAT", 4);

    z_stream z;
    z.zalloc = NULL;
    z.zfree = NULL;
    z.opaque = NULL;

    if(deflateInit(&z, compress) != Z_OK)
        goto error;

    uchar buf[1<<12];
    z.next_out = (Bytef *)buf;
    z.avail_out = sizeof(buf);

    loopi(image.h)
    {
        uchar filter = 0;
        loopj(2)
        {
            z.next_in = j ? (Bytef *)image.data + (flip ? image.h-i-1 : i)*image.pitch : (Bytef *)&filter;
            z.avail_in = j ? image.w*image.bpp : 1;
            while(z.avail_in > 0)
            {
                if(deflate(&z, Z_NO_FLUSH) != Z_OK) goto cleanuperror;
                #define FLUSHZ do { \
                    int flush = sizeof(buf) - z.avail_out; \
                    crc = crc32(crc, buf, flush); \
                    len += flush; \
                    f->write(buf, flush); \
                    z.next_out = (Bytef *)buf; \
                    z.avail_out = sizeof(buf); \
                } while(0)
                FLUSHZ;
            }
        }
    }

    for(;;)
    {
        int err = deflate(&z, Z_FINISH);
        if(err != Z_OK && err != Z_STREAM_END) goto cleanuperror;
        FLUSHZ;
        if(err == Z_STREAM_END) break;
    }

    deflateEnd(&z);

    f->seek(idat, SEEK_SET);
    f->putbig<uint>(len);
    f->seek(0, SEEK_END);
    f->putbig<uint>(crc);

    writepngchunk(f, "IEND");

    delete f;
    return;

cleanuperror:
    deflateEnd(&z);

error:
    delete f;

    conoutf("\frFailed saving png to %s", filename);
}

struct tgaheader
{
    uchar  identsize;
    uchar  cmaptype;
    uchar  imagetype;
    uchar  cmaporigin[2];
    uchar  cmapsize[2];
    uchar  cmapentrysize;
    uchar  xorigin[2];
    uchar  yorigin[2];
    uchar  width[2];
    uchar  height[2];
    uchar  pixelsize;
    uchar  descbyte;
};

void savetga(const char *filename, ImageData &image, int compress, bool flip)
{
    switch(image.bpp)
    {
        case 3: case 4: break;
        default: conoutf("\frFailed saving tga to %s", filename); return;
    }

    stream *f = openfile(filename, "wb");
    if(!f) { conoutf("\frCould not write to %s", filename); return; }

    tgaheader hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.pixelsize = image.bpp*8;
    hdr.width[0] = image.w&0xFF;
    hdr.width[1] = (image.w>>8)&0xFF;
    hdr.height[0] = image.h&0xFF;
    hdr.height[1] = (image.h>>8)&0xFF;
    hdr.imagetype = compress ? 10 : 2;
    f->write(&hdr, sizeof(hdr));

    uchar buf[128*4];
    loopi(image.h)
    {
        uchar *src = image.data + (flip ? i : image.h - i - 1)*image.pitch;
        for(int remaining = image.w; remaining > 0;)
        {
            int raw = 1;
            if(compress)
            {
                int run = 1;
                for(uchar *scan = src; run < min(remaining, 128); run++)
                {
                    scan += image.bpp;
                    if(src[0]!=scan[0] || src[1]!=scan[1] || src[2]!=scan[2] || (image.bpp==4 && src[3]!=scan[3])) break;
                }
                if(run > 1)
                {
                    f->putchar(0x80 | (run-1));
                    f->putchar(src[2]); f->putchar(src[1]); f->putchar(src[0]);
                    if(image.bpp==4) f->putchar(src[3]);
                    src += run*image.bpp;
                    remaining -= run;
                    if(remaining <= 0) break;
                }
                for(uchar *scan = src; raw < min(remaining, 128); raw++)
                {
                    scan += image.bpp;
                    if(src[0]==scan[0] && src[1]==scan[1] && src[2]==scan[2] && (image.bpp!=4 || src[3]==scan[3])) break;
                }
                f->putchar(raw - 1);
            }
            else raw = min(remaining, 128);
            uchar *dst = buf;
            loopj(raw)
            {
                dst[0] = src[2];
                dst[1] = src[1];
                dst[2] = src[0];
                if(image.bpp==4) dst[3] = src[3];
                dst += image.bpp;
                src += image.bpp;
            }
            f->write(buf, raw*image.bpp);
            remaining -= raw;
        }
    }

    delete f;
}

const char *ifmtexts[IFMT_MAX] = { "", ".bmp", ".png", ".tga" };
int guessimageformat(const char *filename, int format = IFMT_BMP)
{
    int len = strlen(filename);
    loopi(IFMT_MAX-1)
    {
        int extlen = strlen(ifmtexts[i+1]);
        if(len >= extlen && !strcasecmp(&filename[len-extlen], ifmtexts[i+1])) return i;
    }
    return format;
}

void saveimage(const char *fname, ImageData &image, int format, int compress, bool flip, bool skip)
{
    int f = format > IFMT_NONE && format < IFMT_MAX ? format : IFMT_PNG;
    const char *filename = makefile(fname, ifmtexts[f], 0, 1, false, skip);
    switch(f)
    {
        case IFMT_PNG: savepng(filename, image, compress, flip); break;
        case IFMT_TGA: savetga(filename, image, compress, flip); break;
        case IFMT_BMP:
        {
            ImageData flipped(image.w, image.h, image.bpp, image.data);
            if(flip) texflip(flipped);
            SDL_Surface *s = wrapsurface(flipped.data, flipped.w, flipped.h, flipped.bpp);
            if(!s) break;
            stream *f = openfile(filename, "wb");
            if(f)
            {
                SDL_SaveBMP_RW(s, f->rwops(), 1);
                delete f;
            }
            SDL_FreeSurface(s);
            break;
        }
        default: break;
    }
}

bool loadimage(const char *filename, ImageData &image)
{
    SDL_Surface *s = loadsurface(copypath(filename));
    if(!s) return false;
    image.wrap(s);
    return true;
}

SDL_Surface *loadsurface(const char *name, bool noload)
{
    const char *exts[] = { "", ".png", ".tga", ".jpg", ".bmp" }; // bmp is a last resort!
    loopi(sizeof(exts)/sizeof(exts[0]))
    {
        defformatstring(buf, "%s%s", name, exts[i]);
        if(noload)
        {
            stream *f = openfile(buf, "rb");
            if(f) { delete f; return (SDL_Surface *)-1; }
            continue;
        }
        SDL_Surface *s = NULL;
        stream *z = openzipfile(buf, "rb");
        if(z)
        {
            SDL_RWops *rw = z->rwops();
            if(rw)
            {
                char *ext = (char *)strrchr(name, '.');
                if(ext) ++ext;
                s = IMG_LoadTyped_RW(rw, 0, ext);
                SDL_FreeRW(rw);
            }
            delete z;
        }
        if(!s)
        {
            const char *fname = findfile(buf, "rb");
            if(fname && *fname) s = IMG_Load(fname);
        }
        if(s) return fixsurfaceformat(s);
    }
    return NULL;
}

void flipnormalmapy(char *destfile, char *normalfile) // jpg/png/tga -> tga
{
    ImageData ns;
    if(!loadimage(normalfile, ns)) return;
    ImageData d(ns.w, ns.h, 3);
    readwritetex(d, ns,
        dst[0] = src[0];
        dst[1] = 255 - src[1];
        dst[2] = src[2];
    );
    saveimage(destfile, d, guessimageformat(destfile, IFMT_TGA));
}

void mergenormalmaps(char *heightfile, char *normalfile) // jpg/png/tga + tga -> tga
{
    ImageData hs, ns;
    if(!loadimage(heightfile, hs) || !loadimage(normalfile, ns) || hs.w != ns.w || hs.h != ns.h) return;
    ImageData d(ns.w, ns.h, 3);
    read2writetex(d, hs, srch, ns, srcn,
        *(bvec *)dst = bvec(((bvec *)srcn)->tonormal().mul(2).add(((bvec *)srch)->tonormal()).normalize());
    );
    saveimage(normalfile, d, guessimageformat(normalfile, IFMT_TGA));
}

void normalizenormalmap(char *destfile, char *normalfile) // jpg/png/tga-> tga
{
    ImageData ns;
    if(!loadimage(normalfile, ns)) return;
    ImageData d(ns.w, ns.h, 3);
    readwritetex(d, ns,
        *(bvec *)dst = bvec(src[0], src[1], src[2]).normalize();
    );
    saveimage(destfile, d, guessimageformat(destfile, IFMT_TGA));
}

void removealphachannel(char *destfile, char *rgbafile)
{
    ImageData ns;
    if(!loadimage(rgbafile, ns)) return;
    ImageData d(ns.w, ns.h, 3);
    readwritetex(d, ns,
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
    );
    saveimage(destfile, d, guessimageformat(destfile, IFMT_TGA));
}

COMMAND(0, flipnormalmapy, "ss");
COMMAND(0, mergenormalmaps, "ss");
COMMAND(0, normalizenormalmap, "ss");
COMMAND(0, removealphachannel, "ss");

void debugtexs()
{
    int s = hud::hudwidth/4, y = 0;
    hudmatrix.ortho(0, hud::hudwidth, hud::hudheight, 0, -1, 1);
    flushhudmatrix();

    if(viewmatcaps(y, s)) y += s;
    viewenvmaps(y, s);
}

void rendertexdebug()
{
    rendermatcaps();
}
