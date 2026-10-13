// records movies to OpenDML AVI files, as MJPEG or uncompressed I420 video with 16 bit PCM sound
// while recording, time advances a fixed step every frame and the sound is rendered a frame's worth at a time through
// a loopback device, so the movie runs smoothly and stays in sync however long each frame takes to render and encode

#include "engine.h"
#include "SDL_image.h"

VAR(0, dbgmovie, 0, 0, 1);
SVAR(IDF_PERSIST, moviedir, "movies");
VAR(IDF_PERSIST, moview, 0, 0, 10000); // 0 = size of the screen
VAR(IDF_PERSIST, movieh, 0, 0, 10000);
VAR(IDF_PERSIST, moviefps, 1, 60, 1000);
VAR(IDF_PERSIST, moviesound, 0, 1, 1);
VAR(IDF_PERSIST, moviesoundrate, 8000, 48000, 192000);
VAR(IDF_PERSIST, moviemonitor, 0, 1, 1); // play the sound being recorded through the speakers too
VAR(IDF_PERSIST, moviecodec, 0, 1, 1); // 0 = uncompressed I420, 1 = MJPEG
VAR(IDF_PERSIST, moviequality, 1, 90, 100); // MJPEG quality
VAR(IDF_PERSIST, movieencoders, 1, 4, 64); // threads encoding frames

struct aviindexentry
{
    int frame, type, size;
    uint offset;

    aviindexentry() {}
    aviindexentry(int frame, int type, int size, uint offset) : frame(frame), type(type), size(size), offset(offset) {}
};

struct avisegmentinfo
{
    stream::offset offset, videoindexoffset, soundindexoffset;
    int firstindex;
    uint videoindexsize, soundindexsize, indexframes, videoframes, soundframes, soundsamples;

    avisegmentinfo() {}
    avisegmentinfo(stream::offset offset, int firstindex) : offset(offset), videoindexoffset(0), soundindexoffset(0), firstindex(firstindex), videoindexsize(0), soundindexsize(0), indexframes(0), videoframes(0), soundframes(0), soundsamples(0) {}
};

struct aviwriter
{
    enum { CODEC_I420 = 0, CODEC_MJPG };

    stream *f;
    uint videoframes;
    stream::offset totalsize;
    const uint videow, videoh, videofps;
    const int codec, soundfrequency, soundchannels;
    string filename;

    vector<aviindexentry> index;
    vector<avisegmentinfo> segments;

    stream::offset fileframesoffset, fileextframesoffset, filevideooffset, filesoundoffset, superindexvideooffset, superindexsoundoffset;

    enum { MAX_CHUNK_DEPTH = 16, MAX_SUPER_INDEX = 1024 };
    stream::offset chunkoffsets[MAX_CHUNK_DEPTH];
    int chunkdepth;

    aviindexentry &addindex(int frame, int type, int size)
    {
        avisegmentinfo &seg = segments.last();
        int i = index.length();
        while(--i >= seg.firstindex)
        {
            aviindexentry &e = index[i];
            if(frame > e.frame || (frame == e.frame && type <= e.type)) break;
        }
        return index.insert(i + 1, aviindexentry(frame, type, size, uint(totalsize - chunkoffsets[chunkdepth])));
    }

    void startchunk(const char *fcc, uint size = 0)
    {
        f->write(fcc, 4);
        f->putlil<uint>(size);
        totalsize += 4 + 4;
        chunkoffsets[++chunkdepth] = totalsize;
        totalsize += size;
    }

    void listchunk(const char *fcc, const char *lfcc)
    {
        startchunk(fcc);
        f->write(lfcc, 4);
        totalsize += 4;
    }

    void endchunk()
    {
        ASSERT(chunkdepth >= 0);
        --chunkdepth;
    }

    void endlistchunk()
    {
        ASSERT(chunkdepth >= 0);
        int size = int(totalsize - chunkoffsets[chunkdepth]);
        f->seek(-4 - size, SEEK_CUR);
        f->putlil(size);
        f->seek(0, SEEK_END);
        if(size & 1) { f->putchar(0x00); totalsize++; }
        endchunk();
    }

    void writechunk(const char *fcc, const void *data, uint len) // simplify startchunk()/endchunk() to avoid f->seek()
    {
        f->write(fcc, 4);
        f->putlil(len);
        f->write(data, len);
        totalsize += 4 + 4 + len;
        if(len & 1) { f->putchar(0x00); totalsize++; }
    }

    void close()
    {
        if(!f) return;
        flushsegment();

        uint soundindexes = 0, videoindexes = 0, soundframes = 0, soundsamples = 0, videoframes = 0, indexframes = 0;
        loopv(segments)
        {
            avisegmentinfo &seg = segments[i];
            if(seg.soundindexsize) soundindexes++;
            videoindexes++;
            soundframes += seg.soundframes;
            soundsamples += seg.soundsamples;
            videoframes += seg.videoframes;
            indexframes += seg.indexframes;
        }
        if(dbgmovie) conoutf(colourwhite, "Movie frames: sound=%d, video=%d+%d(dups)", soundframes, videoframes, indexframes-videoframes);
        f->seek(fileframesoffset, SEEK_SET);
        f->putlil<uint>(segments[0].indexframes);
        // the streams' lengths cover every segment, in frames for the video and samples for the sound
        f->seek(filevideooffset, SEEK_SET);
        f->putlil<uint>(indexframes);
        if(soundsamples > 0)
        {
            f->seek(filesoundoffset, SEEK_SET);
            f->putlil<uint>(soundsamples);
        }
        f->seek(fileextframesoffset, SEEK_SET);
        f->putlil<uint>(indexframes); // total video frames

        f->seek(superindexvideooffset + 2 + 2, SEEK_SET);
        f->putlil<uint>(videoindexes);
        f->seek(superindexvideooffset + 2 + 2 + 4 + 4 + 4 + 4 + 4, SEEK_SET);
        loopv(segments)
        {
            avisegmentinfo &seg = segments[i];
            f->putlil<uint>(seg.videoindexoffset&stream::offset(0xFFFFFFFFU));
            f->putlil<uint>(seg.videoindexoffset>>32);
            f->putlil<uint>(seg.videoindexsize);
            f->putlil<uint>(seg.indexframes);
        }

        if(soundindexes > 0)
        {
            f->seek(superindexsoundoffset + 2 + 2, SEEK_SET);
            f->putlil<uint>(soundindexes);
            f->seek(superindexsoundoffset + 2 + 2 + 4 + 4 + 4 + 4 + 4, SEEK_SET);
            loopv(segments)
            {
                avisegmentinfo &seg = segments[i];
                if(!seg.soundindexsize) continue;
                f->putlil<uint>(seg.soundindexoffset&stream::offset(0xFFFFFFFFU));
                f->putlil<uint>(seg.soundindexoffset>>32);
                f->putlil<uint>(seg.soundindexsize);
                f->putlil<uint>(seg.soundsamples);
            }
        }

        f->seek(0, SEEK_END);

        DELETEP(f);
    }

    aviwriter(const char *name, uint w, uint h, uint fps, int codec, int soundfrequency, int soundchannels) :
        f(NULL), videoframes(0), totalsize(0), videow(w&~1), videoh(h&~1), videofps(fps), codec(codec), soundfrequency(soundfrequency), soundchannels(soundchannels)
    {
        copystring(filename, moviedir);
        if(moviedir[0])
        {
            int dirlen = strlen(filename);
            if(filename[dirlen-1] != '/' && filename[dirlen-1] != '\\' && dirlen+1 < (int)sizeof(filename)) { filename[dirlen++] = '/'; filename[dirlen] = '\0'; }
            const char *dir = findfile(filename, "w");
            if(!fileexists(dir, "w")) createdir(dir);
        }

        concatstring(filename, name);
        path(filename);
        if(!strrchr(filename, '.')) concatstring(filename, ".avi");
    }

    ~aviwriter()
    {
        close();
    }

    uint framesize() const { return codec == CODEC_MJPG ? videow*videoh*3 : videow*videoh*3/2; }

    bool open()
    {
        f = openfile(filename, "wb");
        if(!f) return false;

        chunkdepth = -1;
        const char *fourcc = codec == CODEC_MJPG ? "MJPG" : "I420";

        listchunk("RIFF", "AVI ");

        listchunk("LIST", "hdrl");

        startchunk("avih", 56);
        f->putlil<uint>(1000000 / videofps); // microsecsperframe
        f->putlil<uint>(0); // maxbytespersec
        f->putlil<uint>(0); // reserved
        f->putlil<uint>(0x10 | 0x20); // flags - hasindex|mustuseindex
        fileframesoffset = f->tell();
        f->putlil<uint>(0); // totalvideoframes
        f->putlil<uint>(0); // initialframes
        f->putlil<uint>(soundfrequency > 0 ? 2 : 1); // streams
        f->putlil<uint>(0); // buffersize
        f->putlil<uint>(videow); // video width
        f->putlil<uint>(videoh); // video height
        loopi(4) f->putlil<uint>(0); // reserved
        endchunk(); // avih

        listchunk("LIST", "strl");

        startchunk("strh", 56);
        f->write("vids", 4); // fcctype
        f->write(fourcc, 4); // fcchandler
        f->putlil<uint>(0); // flags
        f->putlil<uint>(0); // priority
        f->putlil<uint>(0); // initialframes
        f->putlil<uint>(1); // scale
        f->putlil<uint>(videofps); // rate
        f->putlil<uint>(0); // start
        filevideooffset = f->tell();
        f->putlil<uint>(0); // length
        f->putlil<uint>(framesize()); // suggested buffersize
        f->putlil<uint>(0); // quality
        f->putlil<uint>(0); // samplesize
        f->putlil<ushort>(0); // frame left
        f->putlil<ushort>(0); // frame top
        f->putlil<ushort>(videow); // frame right
        f->putlil<ushort>(videoh); // frame bottom
        endchunk(); // strh

        startchunk("strf", 40);
        f->putlil<uint>(40); // headersize
        f->putlil<uint>(videow); // width
        f->putlil<uint>(videoh); // height
        f->putlil<ushort>(codec == CODEC_MJPG ? 1 : 3); // planes
        f->putlil<ushort>(codec == CODEC_MJPG ? 24 : 12); // bitcount
        f->write(fourcc, 4); // compression
        f->putlil<uint>(framesize()); // imagesize
        f->putlil<uint>(0); // xres
        f->putlil<uint>(0); // yres;
        f->putlil<uint>(0); // colorsused
        f->putlil<uint>(0); // colorsrequired
        endchunk(); // strf

        startchunk("indx", 24 + 16*MAX_SUPER_INDEX);
        superindexvideooffset = f->tell();
        f->putlil<ushort>(4); // longs per entry
        f->putlil<ushort>(0); // index of indexes
        f->putlil<uint>(0); // entries in use
        f->write("00dc", 4); // chunk id
        f->putlil<uint>(0); // reserved 1
        f->putlil<uint>(0); // reserved 2
        f->putlil<uint>(0); // reserved 3
        loopi(MAX_SUPER_INDEX)
        {
            f->putlil<uint>(0); // offset low
            f->putlil<uint>(0); // offset high
            f->putlil<uint>(0); // size
            f->putlil<uint>(0); // duration
        }
        endchunk(); // indx

        startchunk("vprp", 68);
        f->putlil<uint>(0); // video format token
        f->putlil<uint>(0); // video standard
        f->putlil<uint>(videofps); // vertical refresh rate
        f->putlil<uint>(videow); // horizontal total
        f->putlil<uint>(videoh); // vertical total
        int gcd = videow, rem = videoh;
        while(rem > 0) { gcd %= rem; swap(gcd, rem); }
        f->putlil<ushort>(videoh/gcd); // aspect denominator
        f->putlil<ushort>(videow/gcd); // aspect numerator
        f->putlil<uint>(videow); // frame width
        f->putlil<uint>(videoh); // frame height
        f->putlil<uint>(1); // fields per frame
        f->putlil<uint>(videoh); // compressed bitmap height
        f->putlil<uint>(videow); // compressed bitmap width
        f->putlil<uint>(videoh); // valid bitmap height
        f->putlil<uint>(videow); // valid bitmap width
        f->putlil<uint>(0); // valid bitmap x offset
        f->putlil<uint>(0); // valid bitmap y offset
        f->putlil<uint>(0); // video x offset
        f->putlil<uint>(0); // video y start
        endchunk(); // vprp

        endlistchunk(); // LIST strl

        if(soundfrequency > 0)
        {
            const int bps = 2;

            listchunk("LIST", "strl");

            startchunk("strh", 56);
            f->write("auds", 4); // fcctype
            f->putlil<uint>(1); // fcchandler - normally 4cc, but audio is a special case
            f->putlil<uint>(0); // flags
            f->putlil<uint>(0); // priority
            f->putlil<uint>(0); // initialframes
            f->putlil<uint>(1); // scale
            f->putlil<uint>(soundfrequency); // rate
            f->putlil<uint>(0); // start
            filesoundoffset = f->tell();
            f->putlil<uint>(0); // length
            f->putlil<uint>(soundfrequency*bps*soundchannels/2); // suggested buffer size (this is a half second)
            f->putlil<uint>(0); // quality
            f->putlil<uint>(bps*soundchannels); // samplesize
            f->putlil<ushort>(0); // frame left
            f->putlil<ushort>(0); // frame top
            f->putlil<ushort>(0); // frame right
            f->putlil<ushort>(0); // frame bottom
            endchunk(); // strh

            startchunk("strf", 18);
            f->putlil<ushort>(1); // format (uncompressed PCM)
            f->putlil<ushort>(soundchannels); // channels
            f->putlil<uint>(soundfrequency); // sampleframes per second
            f->putlil<uint>(soundfrequency*bps*soundchannels); // average bytes per second
            f->putlil<ushort>(bps*soundchannels); // block align
            f->putlil<ushort>(bps*8); // bits per sample
            f->putlil<ushort>(0); // size
            endchunk(); // strf

            startchunk("indx", 24 + 16*MAX_SUPER_INDEX);
            superindexsoundoffset = f->tell();
            f->putlil<ushort>(4); // longs per entry
            f->putlil<ushort>(0); // index of indexes
            f->putlil<uint>(0); // entries in use
            f->write("01wb", 4); // chunk id
            f->putlil<uint>(0); // reserved 1
            f->putlil<uint>(0); // reserved 2
            f->putlil<uint>(0); // reserved 3
            loopi(MAX_SUPER_INDEX)
            {
                f->putlil<uint>(0); // offset low
                f->putlil<uint>(0); // offset high
                f->putlil<uint>(0); // size
                f->putlil<uint>(0); // duration
            }
            endchunk(); // indx

            endlistchunk(); // LIST strl
        }

        listchunk("LIST", "odml");
        startchunk("dmlh", 4);
        fileextframesoffset = f->tell();
        f->putlil<uint>(0);
        endchunk(); // dmlh
        endlistchunk(); // LIST odml

        listchunk("LIST", "INFO");
        const char *software = versionfname;
        writechunk("ISFT", software, strlen(software)+1);
        endlistchunk(); // LIST INFO

        endlistchunk(); // LIST hdrl

        nextsegment();

        return true;
    }

    static inline void boxsample(const uchar *src, const uint stride,
                                 const uint area, const uint w, uint h,
                                 const uint xlow, const uint xhigh, const uint ylow, const uint yhigh,
                                 uint &bdst, uint &gdst, uint &rdst)
    {
        const uchar *end = &src[w<<2];
        uint bt = 0, gt = 0, rt = 0;
        for(const uchar *cur = &src[4]; cur < end; cur += 4)
        {
            bt += cur[0];
            gt += cur[1];
            rt += cur[2];
        }
        bt = ylow*(bt + ((src[0]*xlow + end[0]*xhigh)>>12));
        gt = ylow*(gt + ((src[1]*xlow + end[1]*xhigh)>>12));
        rt = ylow*(rt + ((src[2]*xlow + end[2]*xhigh)>>12));
        if(h)
        {
            for(src += stride, end += stride; --h; src += stride, end += stride)
            {
                uint b = 0, g = 0, r = 0;
                for(const uchar *cur = &src[4]; cur < end; cur += 4)
                {
                    b += cur[0];
                    g += cur[1];
                    r += cur[2];
                }
                bt += (b<<12) + src[0]*xlow + end[0]*xhigh;
                gt += (g<<12) + src[1]*xlow + end[1]*xhigh;
                rt += (r<<12) + src[2]*xlow + end[2]*xhigh;
            }
            uint b = 0, g = 0, r = 0;
            for(const uchar *cur = &src[4]; cur < end; cur += 4)
            {
                b += cur[0];
                g += cur[1];
                r += cur[2];
            }
            bt += yhigh*(b + ((src[0]*xlow + end[0]*xhigh)>>12));
            gt += yhigh*(g + ((src[1]*xlow + end[1]*xhigh)>>12));
            rt += yhigh*(r + ((src[2]*xlow + end[2]*xhigh)>>12));
        }
        bdst = (bt*area)>>24;
        gdst = (gt*area)>>24;
        rdst = (rt*area)>>24;
    }

    // BGRA pixels read bottom up, scaled with a box filter into I420
    void scaleyuv(const uchar *pixels, uint srcw, uint srch, uchar *yuv) const
    {
        const int flip = -1;
        const uint planesize = videow * videoh;
        uchar *yplane = yuv, *uplane = yuv + planesize, *vplane = yuv + planesize + planesize/4;
        const int ystride = flip*int(videow), uvstride = flip*int(videow)/2;
        if(flip < 0) { yplane -= int(videoh-1)*ystride; uplane -= int(videoh/2-1)*uvstride; vplane -= int(videoh/2-1)*uvstride; }

        const uint stride = srcw<<2;
        srcw &= ~1;
        srch &= ~1;
        const uint wfrac = (srcw<<12)/videow, hfrac = (srch<<12)/videoh,
                   area = ((ullong)planesize<<12)/(srcw*srch + 1),
                   dw = videow*wfrac, dh = videoh*hfrac;

        for(uint y = 0; y < dh;)
        {
            uint yn = y + hfrac - 1, yi = y>>12, h = (yn>>12) - yi, ylow = ((yn|(-int(h)>>24))&0xFFFU) + 1 - (y&0xFFFU), yhigh = (yn&0xFFFU) + 1;
            y += hfrac;
            uint y2n = y + hfrac - 1, y2i = y>>12, h2 = (y2n>>12) - y2i, y2low = ((y2n|(-int(h2)>>24))&0xFFFU) + 1 - (y&0xFFFU), y2high = (y2n&0xFFFU) + 1;
            y += hfrac;

            const uchar *src = &pixels[yi*stride], *src2 = &pixels[y2i*stride];
            uchar *ydst = yplane, *ydst2 = yplane + ystride, *udst = uplane, *vdst = vplane;
            for(uint x = 0; x < dw;)
            {
                uint xn = x + wfrac - 1, xi = x>>12, w = (xn>>12) - xi, xlow = ((w+0xFFFU)&0x1000U) - (x&0xFFFU), xhigh = (xn&0xFFFU) + 1;
                x += wfrac;
                uint x2n = x + wfrac - 1, x2i = x>>12, w2 = (x2n>>12) - x2i, x2low = ((w2+0xFFFU)&0x1000U) - (x&0xFFFU), x2high = (x2n&0xFFFU) + 1;
                x += wfrac;

                uint b1, g1, r1, b2, g2, r2, b3, g3, r3, b4, g4, r4;
                boxsample(&src[xi<<2], stride, area, w, h, xlow, xhigh, ylow, yhigh, b1, g1, r1);
                boxsample(&src[x2i<<2], stride, area, w2, h, x2low, x2high, ylow, yhigh, b2, g2, r2);
                boxsample(&src2[xi<<2], stride, area, w, h2, xlow, xhigh, y2low, y2high, b3, g3, r3);
                boxsample(&src2[x2i<<2], stride, area, w2, h2, x2low, x2high, y2low, y2high, b4, g4, r4);

                // Y  = 16 + 65.481*R + 128.553*G + 24.966*B
                // Cb = 128 - 37.797*R - 74.203*G + 112.0*B
                // Cr = 128 + 112.0*R - 93.786*G - 18.214*B
                *ydst++ = ((16<<12) + 1052*r1 + 2065*g1 + 401*b1)>>12;
                *ydst++ = ((16<<12) + 1052*r2 + 2065*g2 + 401*b2)>>12;
                *ydst2++ = ((16<<12) + 1052*r3 + 2065*g3 + 401*b3)>>12;
                *ydst2++ = ((16<<12) + 1052*r4 + 2065*g4 + 401*b4)>>12;

                const uint b = b1 + b2 + b3 + b4,
                           g = g1 + g2 + g3 + g4,
                           r = r1 + r2 + r3 + r4;
                // note: weights here are scaled by 1<<10, as opposed to 1<<12, since r/g/b are already *4
                *udst++ = ((128<<12) - 152*r - 298*g + 450*b)>>12;
                *vdst++ = ((128<<12) + 450*r - 377*g - 73*b)>>12;
            }

            yplane += 2*ystride;
            uplane += uvstride;
            vplane += uvstride;
        }
    }

    // BGRA pixels read bottom up, the same size as the video, into I420
    void encodeyuv(const uchar *pixels, uchar *yuv) const
    {
        const int flip = -1;
        const uint planesize = videow * videoh;
        uchar *yplane = yuv, *uplane = yuv + planesize, *vplane = yuv + planesize + planesize/4;
        const int ystride = flip*int(videow), uvstride = flip*int(videow)/2;
        if(flip < 0) { yplane -= int(videoh-1)*ystride; uplane -= int(videoh/2-1)*uvstride; vplane -= int(videoh/2-1)*uvstride; }

        const uint stride = videow<<2;
        const uchar *src = pixels, *yend = src + videoh*stride;
        while(src < yend)
        {
            const uchar *src2 = src + stride, *xend = src2;
            uchar *ydst = yplane, *ydst2 = yplane + ystride, *udst = uplane, *vdst = vplane;
            while(src < xend)
            {
                const uint b1 = src[0], g1 = src[1], r1 = src[2],
                           b2 = src[4], g2 = src[5], r2 = src[6],
                           b3 = src2[0], g3 = src2[1], r3 = src2[2],
                           b4 = src2[4], g4 = src2[5], r4 = src2[6];

                // Y  = 16 + 65.481*R + 128.553*G + 24.966*B
                // Cb = 128 - 37.797*R - 74.203*G + 112.0*B
                // Cr = 128 + 112.0*R - 93.786*G - 18.214*B
                *ydst++ = ((16<<12) + 1052*r1 + 2065*g1 + 401*b1)>>12;
                *ydst++ = ((16<<12) + 1052*r2 + 2065*g2 + 401*b2)>>12;
                *ydst2++ = ((16<<12) + 1052*r3 + 2065*g3 + 401*b3)>>12;
                *ydst2++ = ((16<<12) + 1052*r4 + 2065*g4 + 401*b4)>>12;

                const uint b = b1 + b2 + b3 + b4,
                           g = g1 + g2 + g3 + g4,
                           r = r1 + r2 + r3 + r4;
                // note: weights here are scaled by 1<<10, as opposed to 1<<12, since r/g/b are already *4
                *udst++ = ((128<<12) - 152*r - 298*g + 450*b)>>12;
                *vdst++ = ((128<<12) + 450*r - 377*g - 73*b)>>12;

                src += 8;
                src2 += 8;
            }
            src = src2;
            yplane += 2*ystride;
            uplane += uvstride;
            vplane += uvstride;
        }
    }

    // 16 bit native endian samples, converted in place to little endian
    bool writesound(uchar *data, uint size, uint frame)
    {
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
        endianswap((short *)data, size/2);
#endif
        if(totalsize - segments.last().offset + size > 1000*1000*1000 && !nextsegment()) return false;

        addindex(frame, 1, size);

        writechunk("01wb", data, size);

        return true;
    }

    void flushsegment()
    {
        endlistchunk(); // LIST movi

        avisegmentinfo &seg = segments.last();

        uint indexframes = 0, videoframes = 0, soundframes = 0, soundbytes = 0;
        for(int i = seg.firstindex; i < index.length(); i++)
        {
            aviindexentry &e = index[i];
            if(e.type) { soundframes++; soundbytes += e.size; }
            else
            {
                if(i == seg.firstindex || e.offset != index[i-1].offset)
                    videoframes++;
                indexframes++;
            }
        }

        if(segments.length() == 1)
        {
            startchunk("idx1", index.length()*16);
            loopv(index)
            {
                aviindexentry &entry = index[i];
                f->write(entry.type ? "01wb" : "00dc", 4); // chunkid
                f->putlil<uint>(0x10); // flags - KEYFRAME
                f->putlil<uint>(entry.offset); // offset (relative to movi)
                f->putlil<uint>(entry.size); // size
            }
            endchunk();
        }

        seg.videoframes = videoframes;
        seg.indexframes = indexframes;
        seg.videoindexoffset = totalsize;
        startchunk("ix00", 24 + indexframes*8);
        f->putlil<ushort>(2); // longs per entry
        f->putlil<ushort>(0x0100); // index of chunks
        f->putlil<uint>(indexframes); // entries in use
        f->write("00dc", 4); // chunk id
        f->putlil<uint>(seg.offset&stream::offset(0xFFFFFFFFU)); // offset low
        f->putlil<uint>(seg.offset>>32); // offset high
        f->putlil<uint>(0); // reserved 3
        for(int i = seg.firstindex; i < index.length(); i++)
        {
            aviindexentry &e = index[i];
            if(e.type) continue;
            f->putlil<uint>(e.offset + 4 + 4);
            f->putlil<uint>(e.size);
        }
        endchunk(); // ix00
        seg.videoindexsize = uint(totalsize - seg.videoindexoffset);

        if(soundframes)
        {
            seg.soundframes = soundframes;
            seg.soundsamples = soundbytes/(2*soundchannels);
            seg.soundindexoffset = totalsize;
            startchunk("ix01", 24 + soundframes*8);
            f->putlil<ushort>(2); // longs per entry
            f->putlil<ushort>(0x0100); // index of chunks
            f->putlil<uint>(soundframes); // entries in use
            f->write("01wb", 4); // chunk id
            f->putlil<uint>(seg.offset&stream::offset(0xFFFFFFFFU)); // offset low
            f->putlil<uint>(seg.offset>>32); // offset high
            f->putlil<uint>(0); // reserved 3
            for(int i = seg.firstindex; i < index.length(); i++)
            {
                aviindexentry &e = index[i];
                if(!e.type) continue;
                f->putlil<uint>(e.offset + 4 + 4);
                f->putlil<uint>(e.size);
            }
            endchunk(); // ix01
            seg.soundindexsize = uint(totalsize - seg.soundindexoffset);
        }

        endlistchunk(); // RIFF AVI/AVIX
    }

    bool nextsegment()
    {
        if(segments.length())
        {
            if(segments.length() >= MAX_SUPER_INDEX) return false;
            flushsegment();
            listchunk("RIFF", "AVIX");
        }
        listchunk("LIST", "movi");
        segments.add(avisegmentinfo(chunkoffsets[chunkdepth], index.length()));
        return true;
    }

    // an encoded frame, in the stream's codec
    bool writevideo(const uchar *data, uint size, uint frame)
    {
        if(totalsize - segments.last().offset + size > 1000*1000*1000 && !nextsegment()) return false;

        while(videoframes <= frame) addindex(videoframes++, 0, size);

        writechunk("00dc", data, size);

        return true;
    }
};

namespace recorder
{
    enum { REC_OK = 0, REC_USERHALT, REC_FILEERROR, REC_ENCODEERROR };

    // a captured frame going through the encoders to the file, in a ring as long as a few frames per encoder
    struct moviejob
    {
        enum { FREE = 0, CAPTURED, ENCODING, ENCODED };

        int status;
        uint frame, w, h;
        vector<uchar> pixels, data, sound;

        moviejob() : status(FREE), frame(0), w(0), h(0) {}
    };

    static aviwriter *file = NULL;
    static moviejob *jobs = NULL;
    static int numjobs = 0, state = REC_OK, quality = 90;
    static bool stopping = false;
    static uint capturedframes = 0, writtenframes = 0, stepframes = 0, soundsamples = 0;
    static vector<SDL_Thread *> encoders;
    static SDL_Thread *writer = NULL;
    static SDL_AudioDeviceID monitor = 0;
    static SDL_mutex *joblock = NULL;
    static SDL_cond *jobcaptured = NULL, *jobencoded = NULL, *jobfreed = NULL;

    bool isrecording() { return file != NULL; }

    int step()
    {
        if(!file) return 0;
        // whole frames of time so far, rounded down, so the steps land on each frame's time exactly
        int prev = int((ullong)stepframes*1000/file->videofps);
        stepframes++;
        return int((ullong)stepframes*1000/file->videofps) - prev;
    }

    static Sint64 SDLCALL rwsize(SDL_RWops *rw) { return ((vector<uchar> *)rw->hidden.unknown.data1)->length(); }
    static Sint64 SDLCALL rwseek(SDL_RWops *rw, Sint64 offset, int whence) { return whence == RW_SEEK_CUR && !offset ? ((vector<uchar> *)rw->hidden.unknown.data1)->length() : -1; }
    static size_t SDLCALL rwread(SDL_RWops *rw, void *ptr, size_t size, size_t maxnum) { return 0; }
    static size_t SDLCALL rwwrite(SDL_RWops *rw, const void *ptr, size_t size, size_t num)
    {
        ((vector<uchar> *)rw->hidden.unknown.data1)->put((const uchar *)ptr, int(size*num));
        return num;
    }
    static int SDLCALL rwclose(SDL_RWops *rw) { SDL_FreeRW(rw); return 0; }

    static bool encodejpeg(moviejob &j)
    {
        // read bottom up, so flip it the right way up first
        const uint stride = j.w*4;
        uchar *top = j.pixels.getbuf(), *bottom = top + (j.h-1)*stride;
        for(; top < bottom; top += stride, bottom -= stride) loopi(stride) swap(top[i], bottom[i]);

        SDL_Surface *src = SDL_CreateRGBSurfaceWithFormatFrom(j.pixels.getbuf(), j.w, j.h, 32, stride, SDL_PIXELFORMAT_BGRA32);
        if(!src) return false;
        SDL_SetSurfaceBlendMode(src, SDL_BLENDMODE_NONE); // ignore the alpha left in the frame
        SDL_Surface *out = src;
        if(j.w != file->videow || j.h != file->videoh)
        {
            out = SDL_CreateRGBSurfaceWithFormat(0, file->videow, file->videoh, 32, SDL_PIXELFORMAT_BGRA32);
            if(!out || SDL_SoftStretchLinear(src, NULL, out, NULL))
            {
                if(out) SDL_FreeSurface(out);
                SDL_FreeSurface(src);
                return false;
            }
            SDL_SetSurfaceBlendMode(out, SDL_BLENDMODE_NONE);
        }

        SDL_RWops *rw = SDL_AllocRW();
        bool ok = false;
        if(rw)
        {
            rw->size = rwsize;
            rw->seek = rwseek;
            rw->read = rwread;
            rw->write = rwwrite;
            rw->close = rwclose;
            rw->type = SDL_RWOPS_UNKNOWN;
            rw->hidden.unknown.data1 = &j.data;
            ok = !IMG_SaveJPG_RW(out, rw, 1, quality) && !j.data.empty();
        }

        if(out != src) SDL_FreeSurface(out);
        SDL_FreeSurface(src);
        return ok;
    }

    static bool encode(moviejob &j)
    {
        j.data.setsize(0);
        if(file->codec == aviwriter::CODEC_MJPG) return encodejpeg(j);

        uchar *yuv = j.data.pad(file->framesize());
        if(j.w != file->videow || j.h != file->videoh) file->scaleyuv(j.pixels.getbuf(), j.w, j.h, yuv);
        else file->encodeyuv(j.pixels.getbuf(), yuv);
        return true;
    }

    static int encodeloop(void *data) // runs on each encoder thread
    {
        SDL_LockMutex(joblock);
        for(;;)
        {
            moviejob *j = NULL;
            loopi(numjobs) if(jobs[i].status == moviejob::CAPTURED && (!j || jobs[i].frame < j->frame)) j = &jobs[i];
            if(!j)
            {
                if(stopping) break;
                SDL_CondWait(jobcaptured, joblock);
                continue;
            }
            j->status = moviejob::ENCODING;
            SDL_UnlockMutex(joblock);

            bool ok = encode(*j);

            SDL_LockMutex(joblock);
            if(!ok && state == REC_OK) state = REC_ENCODEERROR;
            j->status = moviejob::ENCODED;
            SDL_CondBroadcast(jobencoded);
        }
        SDL_UnlockMutex(joblock);
        return 0;
    }

    static int writeloop(void *data) // runs on the writer thread, putting the frames in the file in order
    {
        SDL_LockMutex(joblock);
        for(;;)
        {
            moviejob &j = jobs[writtenframes%numjobs];
            if(j.status != moviejob::ENCODED || j.frame != writtenframes)
            {
                if(stopping && writtenframes >= capturedframes) break;
                SDL_CondWait(jobencoded, joblock);
                continue;
            }
            SDL_UnlockMutex(joblock);

            bool ok = state != REC_OK || (file->writevideo(j.data.getbuf(), j.data.length(), j.frame) &&
                (j.sound.empty() || file->writesound(j.sound.getbuf(), j.sound.length(), j.frame)));

            SDL_LockMutex(joblock);
            if(!ok && state == REC_OK) state = REC_FILEERROR;
            j.status = moviejob::FREE;
            writtenframes++;
            SDL_CondBroadcast(jobfreed);
        }
        SDL_UnlockMutex(joblock);
        return 0;
    }

    void stop(bool restore)
    {
        if(!file) return;

        SDL_LockMutex(joblock);
        stopping = true;
        SDL_CondBroadcast(jobcaptured);
        SDL_CondBroadcast(jobencoded);
        SDL_UnlockMutex(joblock);

        loopv(encoders) SDL_WaitThread(encoders[i], NULL); // they finish every frame captured before leaving
        encoders.setsize(0);
        SDL_WaitThread(writer, NULL);
        writer = NULL;

        static const char * const mesgs[] = { "ok", "stopped", "file error", "encoding error" };
        if(state == REC_OK) state = REC_USERHALT;
        conoutf(state == REC_USERHALT ? colourwhite : colourred, "Movie recording halted: %s, %d frames (%s)", mesgs[state], file->videoframes, timestr(int((ullong)file->videoframes*1000/file->videofps), 4));
        bool hadsound = file->soundfrequency > 0;
        DELETEP(file);

        DELETEA(jobs);
        numjobs = 0;
        SDL_DestroyMutex(joblock);
        SDL_DestroyCond(jobcaptured);
        SDL_DestroyCond(jobencoded);
        SDL_DestroyCond(jobfreed);
        joblock = NULL;
        jobcaptured = jobencoded = jobfreed = NULL;

        if(monitor)
        {
            SDL_CloseAudioDevice(monitor);
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            monitor = 0;
        }

        soundloopback = 0;
        if(hadsound && restore) resetsound();
        clockreset(); // carry on from where the movie's time got to
        state = REC_OK;
        stopping = false;
    }

    void start(const char *name)
    {
        if(file) return;

        int soundrate = 0;
        if(moviesound && !nosound)
        {
            soundloopback = moviesoundrate;
            resetsound();
            if(soundcapturing) soundrate = moviesoundrate;
            else
            {
                soundloopback = 0;
                resetsound();
                conoutf(colouryellow, "Movie will be recorded without sound");
            }
        }

        int codec = clamp(moviecodec, int(aviwriter::CODEC_I420), int(aviwriter::CODEC_MJPG));
        if(codec == aviwriter::CODEC_MJPG && !(IMG_Init(IMG_INIT_JPG)&IMG_INIT_JPG)) // load it here, as the encoders would race to
        {
            conoutf(colouryellow, "JPEG encoding is unavailable, recording uncompressed video instead: %s", IMG_GetError());
            codec = aviwriter::CODEC_I420;
        }

        uint w = moview ? moview : renderw, h = movieh ? movieh : renderh;
        file = new aviwriter(name, max(w, 2U), max(h, 2U), moviefps, codec, soundrate, 2);
        if(!file->open())
        {
            conoutf(colourred, "Unable to create movie file: %s", file->filename);
            DELETEP(file);
            if(soundrate)
            {
                soundloopback = 0;
                resetsound();
            }
            return;
        }

        quality = moviequality;
        state = REC_OK;
        stopping = false;
        capturedframes = writtenframes = stepframes = soundsamples = 0;
        int numencoders = movieencoders;
        numjobs = numencoders*2 + 2;
        jobs = new moviejob[numjobs];
        joblock = SDL_CreateMutex();
        jobcaptured = SDL_CreateCond();
        jobencoded = SDL_CreateCond();
        jobfreed = SDL_CreateCond();
        loopi(numencoders) encoders.add(SDL_CreateThread(encodeloop, "movie encoder", NULL));
        writer = SDL_CreateThread(writeloop, "movie writer", NULL);

        // the loopback device replaces the speakers, so pass what it renders on to them
        if(soundrate && moviemonitor && !SDL_InitSubSystem(SDL_INIT_AUDIO))
        {
            SDL_AudioSpec want;
            memset(&want, 0, sizeof(want));
            want.freq = soundrate;
            want.format = AUDIO_S16SYS;
            want.channels = 2;
            want.samples = 1024;
            monitor = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
            if(monitor) SDL_PauseAudioDevice(monitor, 0);
            else
            {
                conoutf(colouryellow, "Unable to play the movie's sound while recording: %s", SDL_GetError());
                SDL_QuitSubSystem(SDL_INIT_AUDIO);
            }
        }

        conoutf(colourwhite, "Movie recording to: %s %dx%d @ %dfps %s%s", findfile(file->filename, "wb"), file->videow, file->videoh, file->videofps,
            codec == aviwriter::CODEC_MJPG ? "MJPEG" : "I420", soundrate ? " + sound" : "");
    }

    void capture(bool overlay)
    {
        if(!file || !overlay) return;

        SDL_LockMutex(joblock);
        int curstate = state;
        SDL_UnlockMutex(joblock);
        if(curstate != REC_OK)
        {
            stop();
            return;
        }

        moviejob &j = jobs[capturedframes%numjobs];
        SDL_LockMutex(joblock);
        while(j.status != moviejob::FREE) SDL_CondWait(jobfreed, joblock); // wait for the encoders to catch up
        SDL_UnlockMutex(joblock);

        j.frame = capturedframes;
        j.w = renderw;
        j.h = renderh;
        j.pixels.setsize(0);
        glBindFramebuffer_(GL_FRAMEBUFFER, 0);
        glPixelStorei(GL_PACK_ALIGNMENT, 4);
        glReadPixels(0, 0, j.w, j.h, GL_BGRA, GL_UNSIGNED_BYTE, j.pixels.pad(j.w*j.h*4));

        j.sound.setsize(0);
        if(file->soundfrequency > 0)
        {
            // the sound up to the end of this frame, so it never drifts from the video
            uint total = uint((ullong)(capturedframes+1)*file->soundfrequency/file->videofps), samples = total - soundsamples;
            uchar *buf = j.sound.pad(samples*2*file->soundchannels);
            if(!soundcapture(buf, samples)) memset(buf, 0, samples*2*file->soundchannels);
            soundsamples = total;
            // only keep a quarter second queued, so it doesn't lag behind when recording runs faster than real time
            if(monitor && SDL_GetQueuedAudioSize(monitor) < uint(file->soundfrequency*2*file->soundchannels/4)) SDL_QueueAudio(monitor, buf, j.sound.length());
        }

        SDL_LockMutex(joblock);
        j.status = moviejob::CAPTURED;
        capturedframes++;
        SDL_CondSignal(jobcaptured);
        SDL_UnlockMutex(joblock);
    }
}

ICOMMAND(0, movie, "s", (char *name),
{
    if(identflags&IDF_MAP) return;
    if(recorder::isrecording()) recorder::stop();
    else if(*name) recorder::start(name);
    else recorder::start(*filetimeformat ? gettime(filetimelocal ? currenttime : clocktime, filetimeformat) : "movie");
});
ICOMMANDV(0, movierecording, recorder::isrecording() ? 1 : 0);
