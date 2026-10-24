// records movies as MP4 files with AV1 or VP9 video and Opus sound, or OpenDML AVI files with MJPEG or uncompressed I420
// video and 16 bit PCM sound
// while recording, time advances a fixed step every frame and the sound is rendered a frame's worth at a time through
// a loopback device, so the movie runs smoothly and stays in sync however long each frame takes to render and encode

#include "engine.h"
#include "SDL_image.h"
#include "opus/opus.h"
#include "vpx/vpx_encoder.h"
#include "vpx/vp8cx.h"
#include "svt-av1/EbSvtAv1Enc.h"

VAR(0, dbgmovie, 0, 0, 1);
SVAR(IDF_PERSIST, moviedir, "movies");
VAR(IDF_PERSIST, moview, 0, 0, 10000); // 0 = size of the screen
VAR(IDF_PERSIST, movieh, 0, 0, 10000);
VAR(IDF_PERSIST, moviefps, 1, 60, 1000);
VAR(IDF_PERSIST, moviesound, 0, 1, 1);
VAR(IDF_PERSIST, moviesoundrate, 8000, 48000, 192000);
VAR(IDF_PERSIST, moviemonitor, 0, 1, 1); // play the sound being recorded through the speakers too
VAR(IDF_PERSIST, moviepace, 0, 1, 1); // hold recording to real time, 0 = as fast as frames render, only alone in a local game
VAR(IDF_PERSIST, moviecodec, 0, 2, 3); // 0 = uncompressed I420 AVI, 1 = MJPEG AVI, 2 = AV1 MP4, 3 = VP9 MP4
VAR(IDF_PERSIST, moviequality, 1, 90, 100); // MJPEG quality
VAR(IDF_PERSIST, moviecrf, 0, 30, 63); // AV1 and VP9 constant quality, lower is better
VAR(IDF_PERSIST, movieav1preset, 0, 8, 13); // AV1 speed, lower is slower and smaller
VAR(IDF_PERSIST, movievp9speed, 0, 4, 8); // VP9 speed, lower is slower and smaller
VAR(IDF_PERSIST, moviekeyint, 1, 2, 10); // seconds between key frames in AV1 and VP9
VAR(IDF_PERSIST, movieaudiobitrate, 32, 192, 512); // Opus kilobits per second
VAR(0, dbgmovieskip, 0, 0, 7); // treat codec libraries as missing: 1 = Opus, 2 = libvpx, 4 = SVT-AV1
VAR(IDF_PERSIST, movieencoders, 1, 4, 64); // threads encoding frames

// what every movie file has, whatever the container
struct moviewriter
{
    enum { CODEC_I420 = 0, CODEC_MJPG, CODEC_AV1, CODEC_VP9, CODEC_MAX };

    stream *f;
    uint videoframes;
    const uint videow, videoh, videofps;
    const int codec, soundfrequency, soundchannels;
    const int *yuvcoeffs;
    string filename;

    moviewriter(const char *name, const char *ext, uint w, uint h, uint fps, int codec, int soundfrequency, int soundchannels) :
        f(NULL), videoframes(0), videow(w&~1), videoh(h&~1), videofps(fps), codec(codec), soundfrequency(soundfrequency), soundchannels(soundchannels)
    {
        // Y, Cb and Cr from R, G and B in studio range, Y scaled by 1<<12 and Cb/Cr by 1<<10 as they're from four pixels
        // BT.601 for the AVI's I420 as players expect, BT.709 for the compressed video as it's signalled as that
        static const int bt601[9] = { 1052, 2065, 401, -152, -298, 450, 450, -377, -73 },
                         bt709[9] = { 748, 2516, 254, -103, -347, 450, 450, -409, -41 };
        yuvcoeffs = codec >= CODEC_AV1 ? bt709 : bt601;

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
        int len = strlen(filename), extlen = strlen(ext);
        if(len < extlen || strcasecmp(&filename[len-extlen], ext)) concatstring(filename, ext);
    }
    virtual ~moviewriter() {}

    virtual bool open() = 0;
    virtual void close() = 0;
    virtual uint framesize() const { return videow*videoh*3/2; } // the most an encoded frame handed to writevideo takes
    virtual bool writevideo(const uchar *data, uint size, uint frame) = 0;
    virtual bool writesound(uchar *data, uint size, uint frame) = 0;

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

    // BGRA pixels read bottom up, scaled with a box filter into I420 with the writer's coefficients
    void scaleyuv(const uchar *pixels, uint srcw, uint srch, uchar *yuv) const
    {
        const int flip = -1, *c = yuvcoeffs;
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

                *ydst++ = ((16<<12) + c[0]*r1 + c[1]*g1 + c[2]*b1)>>12;
                *ydst++ = ((16<<12) + c[0]*r2 + c[1]*g2 + c[2]*b2)>>12;
                *ydst2++ = ((16<<12) + c[0]*r3 + c[1]*g3 + c[2]*b3)>>12;
                *ydst2++ = ((16<<12) + c[0]*r4 + c[1]*g4 + c[2]*b4)>>12;

                const uint b = b1 + b2 + b3 + b4,
                           g = g1 + g2 + g3 + g4,
                           r = r1 + r2 + r3 + r4;
                // note: weights here are scaled by 1<<10, as opposed to 1<<12, since r/g/b are already *4
                *udst++ = ((128<<12) + c[3]*r + c[4]*g + c[5]*b)>>12;
                *vdst++ = ((128<<12) + c[6]*r + c[7]*g + c[8]*b)>>12;
            }

            yplane += 2*ystride;
            uplane += uvstride;
            vplane += uvstride;
        }
    }

    // BGRA pixels read bottom up, the same size as the video, into I420
    void encodeyuv(const uchar *pixels, uchar *yuv) const
    {
        const int flip = -1, *c = yuvcoeffs;
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

                *ydst++ = ((16<<12) + c[0]*r1 + c[1]*g1 + c[2]*b1)>>12;
                *ydst++ = ((16<<12) + c[0]*r2 + c[1]*g2 + c[2]*b2)>>12;
                *ydst2++ = ((16<<12) + c[0]*r3 + c[1]*g3 + c[2]*b3)>>12;
                *ydst2++ = ((16<<12) + c[0]*r4 + c[1]*g4 + c[2]*b4)>>12;

                const uint b = b1 + b2 + b3 + b4,
                           g = g1 + g2 + g3 + g4,
                           r = r1 + r2 + r3 + r4;
                // note: weights here are scaled by 1<<10, as opposed to 1<<12, since r/g/b are already *4
                *udst++ = ((128<<12) + c[3]*r + c[4]*g + c[5]*b)>>12;
                *vdst++ = ((128<<12) + c[6]*r + c[7]*g + c[8]*b)>>12;

                src += 8;
                src2 += 8;
            }
            src = src2;
            yplane += 2*ystride;
            uplane += uvstride;
            vplane += uvstride;
        }
    }

};

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

struct aviwriter : moviewriter
{
    stream::offset totalsize;

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

    void close() override
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
        moviewriter(name, ".avi", w, h, fps, codec, soundfrequency, soundchannels), totalsize(0) {}

    ~aviwriter()
    {
        close();
    }

    uint framesize() const override { return codec == CODEC_MJPG ? videow*videoh*3 : videow*videoh*3/2; }

    bool open() override
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

    // 16 bit native endian samples, converted in place to little endian
    bool writesound(uchar *data, uint size, uint frame) override
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
    bool writevideo(const uchar *data, uint size, uint frame) override
    {
        if(totalsize - segments.last().offset + size > 1000*1000*1000 && !nextsegment()) return false;

        while(videoframes <= frame) addindex(videoframes++, 0, size);

        writechunk("00dc", data, size);

        return true;
    }
};

// the codecs for compressed movies are loaded when they're first wanted rather than linked, so the game runs without
// them, recording AVI instead, and isn't tied to the library versions of whichever system built it
namespace moviecodecs
{
    #define OPUSFUNCS(f) f(opus_encoder_create) f(opus_encoder_ctl) f(opus_encode) f(opus_encoder_destroy) f(opus_strerror)
    #define VPXFUNCS(f) f(vpx_codec_vp9_cx) f(vpx_codec_enc_config_default) f(vpx_codec_enc_init_ver) f(vpx_codec_control_) \
        f(vpx_codec_encode) f(vpx_codec_get_cx_data) f(vpx_codec_destroy) f(vpx_codec_error) f(vpx_img_wrap)
    #define SVTFUNCS(f) f(svt_av1_get_version) f(svt_av1_enc_init_handle) f(svt_av1_enc_set_parameter) f(svt_av1_enc_init) \
        f(svt_av1_enc_stream_header) f(svt_av1_enc_stream_header_release) f(svt_av1_enc_send_picture) f(svt_av1_enc_get_packet) \
        f(svt_av1_enc_release_out_buffer) f(svt_av1_enc_deinit) f(svt_av1_enc_deinit_handle)

    #define CODECFUNC(name) static decltype(&name) p_##name = NULL;
    OPUSFUNCS(CODECFUNC)
    VPXFUNCS(CODECFUNC)
    SVTFUNCS(CODECFUNC)
    #undef CODECFUNC

#if defined(WIN32)
    static const char * const opusnames[] = { "libopus-0.dll", "opus.dll", NULL };
    static const char * const vpxnames[] = { "libvpx-1.dll", "vpx.dll", NULL };
    static const char * const svtnames[] = { "libSvtAv1Enc-4.dll", "SvtAv1Enc.dll", NULL };
#elif defined(__APPLE__)
    static const char * const opusnames[] = { "libopus.0.dylib", NULL };
    static const char * const vpxnames[] = { "libvpx.12.dylib", "libvpx.11.dylib", "libvpx.9.dylib", "libvpx.8.dylib", "libvpx.7.dylib", "libvpx.6.dylib", NULL };
    static const char * const svtnames[] = { "libSvtAv1Enc.4.dylib", NULL };
#else
    static const char * const opusnames[] = { "libopus.so.0", NULL };
    static const char * const vpxnames[] = { "libvpx.so.12", "libvpx.so.11", "libvpx.so.9", "libvpx.so.8", "libvpx.so.7", "libvpx.so.6", NULL };
    static const char * const svtnames[] = { "libSvtAv1Enc.so.4", NULL };
#endif

    enum { LIB_OPUS = 0, LIB_VPX, LIB_SVT, LIB_MAX };
    static const char * const libdescs[LIB_MAX] = { "Opus", "VP9 (libvpx)", "AV1 (SVT-AV1)" };
    static void *libhandles[LIB_MAX] = { NULL, NULL, NULL };
    static int libstates[LIB_MAX] = { -1, -1, -1 }; // -1 = not tried yet, 0 = unavailable, 1 = loaded

    static void *openlib(const char * const *names)
    {
        for(const char * const *name = names; *name; name++)
        {
            if(void *handle = SDL_LoadObject(*name)) return handle;
#ifdef __APPLE__
            static const char * const prefixes[] = { "/opt/homebrew/lib/", "/usr/local/lib/" };
            loopi(sizeof(prefixes)/sizeof(prefixes[0]))
            {
                defformatstring(full, "%s%s", prefixes[i], *name);
                if(void *handle = SDL_LoadObject(full)) return handle;
            }
#endif
        }
        return NULL;
    }

    #define CODECRESOLVE(name) if(!missing && !(p_##name = (decltype(p_##name))SDL_LoadFunction(handle, #name))) missing = #name;

    static bool load(int lib)
    {
        if(dbgmovieskip&(1<<lib)) return false; // pretend it's missing, to test falling back
        if(libstates[lib] >= 0) return libstates[lib] > 0;
        libstates[lib] = 0;

        static const char * const * const names[LIB_MAX] = { opusnames, vpxnames, svtnames };
        void *handle = openlib(names[lib]);
        if(!handle)
        {
            conoutf(colouryellow, "Movie codec %s is unavailable: %s", libdescs[lib], SDL_GetError());
            return false;
        }

        const char *missing = NULL;
        switch(lib)
        {
            case LIB_OPUS: OPUSFUNCS(CODECRESOLVE) break;
            case LIB_VPX: VPXFUNCS(CODECRESOLVE) break;
            case LIB_SVT: SVTFUNCS(CODECRESOLVE) break;
        }
        if(missing)
        {
            conoutf(colouryellow, "Movie codec %s is unavailable, it has no %s", libdescs[lib], missing);
            SDL_UnloadObject(handle);
            return false;
        }

        if(lib == LIB_SVT) // the configuration it fills in must be laid out as the headers it was built with say
        {
            const char *version = p_svt_av1_get_version();
            while(*version && !isdigit(*version)) version++;
            if(atoi(version) != SVT_AV1_VERSION_MAJOR)
            {
                conoutf(colouryellow, "Movie codec %s is unavailable, version %s isn't %d.x", libdescs[lib], p_svt_av1_get_version(), SVT_AV1_VERSION_MAJOR);
                SDL_UnloadObject(handle);
                return false;
            }
        }

        libhandles[lib] = handle;
        libstates[lib] = 1;
        return true;
    }

    #undef CODECRESOLVE
}
using namespace moviecodecs;

// an ISO BMFF box being built in memory, nested boxes have their sizes filled in as they end
struct mp4buf
{
    vector<uchar> data;
    vector<int> boxes;

    int pos() const { return data.length(); }
    void u8(uint n) { data.add(uchar(n)); }
    void u16(uint n) { u8(n>>8); u8(n); }
    void u24(uint n) { u8(n>>16); u16(n); }
    void u32(uint n) { u16(n>>16); u16(n); }
    void u64(ullong n) { u32(uint(n>>32)); u32(uint(n)); }
    void fcc(const char *s) { data.put((const uchar *)s, 4); }
    void put(const void *p, int n) { data.put((const uchar *)p, n); }
    void zeros(int n) { loopi(n) u8(0); }
    void box(const char *s) { boxes.add(pos()); u32(0); fcc(s); }
    void fullbox(const char *s, int version, uint flags) { box(s); u8(version); u24(flags); }
    void end() { int start = boxes.pop(); patch32(start, pos()-start); }
    void patch32(int at, uint n) { loopi(4) data[at+i] = uchar(n>>(24-i*8)); }
    void matrix() { u32(0x10000); u32(0); u32(0); u32(0); u32(0x10000); u32(0); u32(0); u32(0); u32(0x40000000); }
};

// AV1 is stored in MP4 as a series of OBUs, each with its size
static int av1obu(const uchar *p, int len, int &type, int &payload, int &size)
{
    if(len < 1) return 0;
    type = (p[0]>>3)&15;
    int at = 1 + (p[0]&4 ? 1 : 0);
    ullong n = 0;
    if(p[0]&2)
    {
        for(int shift = 0;; shift += 7)
        {
            if(at >= len || shift >= 56) return 0;
            uchar c = p[at++];
            n |= ullong(c&0x7F)<<shift;
            if(!(c&0x80)) break;
        }
    }
    else n = len - at;
    if(at + n > ullong(len)) return 0;
    payload = at;
    size = int(n);
    return at + size;
}

struct av1bits
{
    const uchar *p;
    int len, at;

    uint bits(int n)
    {
        uint v = 0;
        while(n-- > 0) { v = (v<<1) | (at < len*8 ? (p[at>>3]>>(7-(at&7)))&1 : 0); at++; }
        return v;
    }
    uint uvlc()
    {
        int zeros = 0;
        while(!bits(1)) if(++zeros >= 32) return ~0U;
        return bits(zeros) + (1U<<zeros) - 1;
    }
};

// just enough of the sequence header for the av1C box, up to the first operating point's level and tier
static void av1seqinfo(const uchar *p, int len, int &profile, int &level, int &tier)
{
    av1bits b = { p, len, 0 };
    profile = b.bits(3);
    b.bits(1); // still_picture
    tier = 0;
    if(b.bits(1)) { level = b.bits(5); return; } // reduced_still_picture_header
    if(b.bits(1)) // timing_info_present_flag
    {
        b.bits(32); // num_units_in_display_tick
        b.bits(32); // time_scale
        if(b.bits(1)) b.uvlc(); // num_ticks_per_picture_minus_1
        if(b.bits(1)) { b.bits(5); b.bits(32); b.bits(5); b.bits(5); } // decoder_model_info
    }
    b.bits(1); // initial_display_delay_present_flag
    b.bits(5); // operating_points_cnt_minus_1
    b.bits(12); // operating_point_idc[0]
    level = b.bits(5);
    if(level > 7) tier = b.bits(1);
}

// fragmented MP4, with AV1 or VP9 video and Opus sound: a fragment is written at each key frame, so the file plays up to
// the last one even if it's cut short, and the lengths are filled in at the start when it's closed
struct mp4writer : moviewriter
{
    enum { SOUNDRATE = 48000, OPUSFRAME = 960, TRACK_VIDEO = 1, TRACK_SOUND = 2 };

    // the fragment being gathered
    vector<uchar> videodata, sounddata;
    vector<uint> videosizes, soundsizes;
    vector<bool> videosync;
    ullong videobase, soundbase; // decode times of the first samples being gathered, which end up as the totals

    struct fragentry { ullong time, offset; };
    vector<fragentry> fragments;
    uint sequence;
    stream::offset mvhdpos, tkhdpos[2], mdhdpos[2], mehdpos, elstpos;

    EbComponentType *svt;
    vpx_codec_ctx_t vpx;
    bool vpxinit, failed;
    vector<uchar> av1config;
    int av1profile, av1level, av1tier;

    OpusEncoder *opus;
    int preskip;
    vector<short> pcm;
    ullong pcmsamples;

    mp4writer(const char *name, uint w, uint h, uint fps, int codec, int soundfrequency, int soundchannels) :
        moviewriter(name, ".mp4", w, h, fps, codec, soundfrequency ? SOUNDRATE : 0, soundfrequency ? 2 : 0),
        videobase(0), soundbase(0), sequence(0), mvhdpos(0), mehdpos(0), elstpos(0), svt(NULL), vpxinit(false), failed(false),
        av1profile(0), av1level(0), av1tier(0), opus(NULL), preskip(0), pcmsamples(0)
    {
        tkhdpos[0] = tkhdpos[1] = mdhdpos[0] = mdhdpos[1] = 0;
    }

    ~mp4writer()
    {
        close();
    }

    int keyint() const { return max(int(videofps)*moviekeyint, 1); }

    bool initav1()
    {
        EbSvtAv1EncConfiguration cfg;
        memset(&cfg, 0, sizeof(cfg));
        if(p_svt_av1_enc_init_handle(&svt, &cfg) != EB_ErrorNone) { svt = NULL; return false; }
        cfg.enc_mode = movieav1preset;
        cfg.source_width = videow;
        cfg.source_height = videoh;
        cfg.frame_rate_numerator = videofps;
        cfg.frame_rate_denominator = 1;
        cfg.encoder_bit_depth = 8;
        cfg.rate_control_mode = SVT_AV1_RC_MODE_CQP_OR_CRF;
        cfg.aq_mode = 2; // CRF rather than a fixed QP
        cfg.qp = moviecrf;
        cfg.intra_refresh_type = SVT_AV1_KF_REFRESH; // closed GOPs, so every key frame can start a fragment
        cfg.intra_period_length = max((keyint()+8)/16*16, 16); // a multiple of the mini-GOP as it recommends
        cfg.color_primaries = EB_CICP_CP_BT_709;
        cfg.transfer_characteristics = EB_CICP_TC_BT_709;
        cfg.matrix_coefficients = EB_CICP_MC_BT_709;
        cfg.color_range = EB_CR_STUDIO_RANGE;
        if(p_svt_av1_enc_set_parameter(svt, &cfg) != EB_ErrorNone || p_svt_av1_enc_init(svt) != EB_ErrorNone) return false;

        EbBufferHeaderType *header = NULL;
        if(p_svt_av1_enc_stream_header(svt, &header) != EB_ErrorNone || !header) return false;
        // av1C carries the sequence header, which also gives the profile and level it has to state
        const uchar *p = header->p_buffer;
        int len = header->n_filled_len, type = 0, payload = 0, size = 0;
        for(int n; (n = av1obu(p, len, type, payload, size)) > 0; p += n, len -= n) if(type == 1) // OBU_SEQUENCE_HEADER
        {
            av1seqinfo(&p[payload], size, av1profile, av1level, av1tier);
            av1config.put(p, n);
        }
        p_svt_av1_enc_stream_header_release(header);
        return !av1config.empty();
    }

    bool initvp9()
    {
        vpx_codec_iface_t *iface = p_vpx_codec_vp9_cx();
        vpx_codec_enc_cfg_t cfg;
        if(p_vpx_codec_enc_config_default(iface, &cfg, 0) != VPX_CODEC_OK) return false;
        cfg.g_w = videow;
        cfg.g_h = videoh;
        cfg.g_timebase.num = 1;
        cfg.g_timebase.den = videofps;
        cfg.g_threads = clamp(SDL_GetCPUCount(), 1, 16);
        cfg.g_pass = VPX_RC_ONE_PASS;
        cfg.g_lag_in_frames = 25;
        cfg.rc_end_usage = VPX_Q;
        cfg.rc_min_quantizer = 0;
        cfg.rc_max_quantizer = 63;
        cfg.kf_mode = VPX_KF_AUTO;
        cfg.kf_max_dist = keyint();
        if(p_vpx_codec_enc_init_ver(&vpx, iface, &cfg, 0, VPX_ENCODER_ABI_VERSION) != VPX_CODEC_OK) return false;
        vpxinit = true;

        int tiles = 0;
        while(tiles < 4 && (512<<tiles) <= int(videow)) tiles++; // as many as fit at least 256 wide
        p_vpx_codec_control_(&vpx, VP8E_SET_CPUUSED, movievp9speed);
        p_vpx_codec_control_(&vpx, VP8E_SET_CQ_LEVEL, (unsigned int)moviecrf);
        p_vpx_codec_control_(&vpx, VP8E_SET_ENABLEAUTOALTREF, 1);
        p_vpx_codec_control_(&vpx, VP9E_SET_ROW_MT, 1);
        p_vpx_codec_control_(&vpx, VP9E_SET_TILE_COLUMNS, tiles);
        p_vpx_codec_control_(&vpx, VP9E_SET_COLOR_SPACE, (int)VPX_CS_BT_709);
        p_vpx_codec_control_(&vpx, VP9E_SET_COLOR_RANGE, (int)VPX_CR_STUDIO_RANGE);
        return true;
    }

    bool initopus()
    {
        int err = OPUS_OK;
        opus = p_opus_encoder_create(SOUNDRATE, soundchannels, OPUS_APPLICATION_AUDIO, &err);
        if(!opus || err != OPUS_OK)
        {
            conoutf(colourred, "Unable to start Opus encoding: %s", p_opus_strerror(err));
            opus = NULL;
            return false;
        }
        opus_int32 lookahead = 0;
        p_opus_encoder_ctl(opus, OPUS_SET_BITRATE(movieaudiobitrate*1000));
        p_opus_encoder_ctl(opus, OPUS_SET_COMPLEXITY(10));
        p_opus_encoder_ctl(opus, OPUS_GET_LOOKAHEAD(&lookahead));
        preskip = lookahead;
        return true;
    }

    static void hdlr(mp4buf &m, const char *type, const char *name)
    {
        m.fullbox("hdlr", 0, 0);
        m.u32(0); // pre_defined
        m.fcc(type);
        m.zeros(12);
        m.put(name, strlen(name)+1);
        m.end();
    }

    static void dinf(mp4buf &m)
    {
        m.box("dinf");
        m.fullbox("dref", 0, 0);
        m.u32(1);
        m.fullbox("url ", 0, 1); // the samples are in this file
        m.end();
        m.end();
        m.end();
    }

    static void emptytables(mp4buf &m) // the samples are all described by the fragments
    {
        m.fullbox("stts", 0, 0); m.u32(0); m.end();
        m.fullbox("stsc", 0, 0); m.u32(0); m.end();
        m.fullbox("stsz", 0, 0); m.u32(0); m.u32(0); m.end();
        m.fullbox("stco", 0, 0); m.u32(0); m.end();
    }

    void tkhd(mp4buf &m, int track, int volume, uint w, uint h)
    {
        m.fullbox("tkhd", 0, 3); // enabled, in the movie
        m.u32(0); // creation_time
        m.u32(0); // modification_time
        m.u32(track);
        m.u32(0); // reserved
        tkhdpos[track-1] = m.pos();
        m.u32(0); // duration
        m.zeros(8);
        m.u16(0); // layer
        m.u16(0); // alternate_group
        m.u16(volume);
        m.u16(0); // reserved
        m.matrix();
        m.u32(w<<16);
        m.u32(h<<16);
        m.end();
    }

    void mdhd(mp4buf &m, int track, uint timescale)
    {
        m.fullbox("mdhd", 0, 0);
        m.u32(0); // creation_time
        m.u32(0); // modification_time
        m.u32(timescale);
        mdhdpos[track-1] = m.pos();
        m.u32(0); // duration
        m.u16(0x55C4); // "und"
        m.u16(0);
        m.end();
    }

    int vp9level() const
    {
        // the lowest level whose sample rate and picture size the video fits in
        static const struct { int level; double rate; double size; } levels[] = {
            { 10, 829440, 36864 }, { 11, 2764800, 73728 }, { 20, 4608000, 122880 }, { 21, 9216000, 245760 },
            { 30, 20736000, 552960 }, { 31, 36864000, 983040 }, { 40, 83558400, 2228224 }, { 41, 160432128, 2228224 },
            { 50, 311951360, 8912896 }, { 51, 588251136, 8912896 }, { 52, 1176502272, 8912896 }, { 60, 1176502272, 35651584 },
            { 61, 2353004544.0, 35651584 }, { 62, 4706009088.0, 35651584 }
        };
        double size = double(videow)*videoh, rate = size*videofps;
        loopi(sizeof(levels)/sizeof(levels[0])) if(rate <= levels[i].rate && size <= levels[i].size) return levels[i].level;
        return 62;
    }

    bool writeheader()
    {
        mp4buf m;
        m.box("ftyp");
        m.fcc("isom");
        m.u32(0x200);
        m.fcc("isom");
        m.fcc("iso6");
        m.fcc("iso2");
        m.fcc("mp41");
        if(codec == CODEC_AV1) m.fcc("av01");
        m.end();

        m.box("moov");

        m.fullbox("mvhd", 0, 0);
        m.u32(0); // creation_time
        m.u32(0); // modification_time
        m.u32(1000); // timescale
        mvhdpos = m.pos();
        m.u32(0); // duration
        m.u32(0x10000); // rate
        m.u16(0x100); // volume
        m.zeros(10);
        m.matrix();
        m.zeros(24);
        m.u32(soundfrequency ? 3 : 2); // next_track_ID
        m.end();

        m.box("trak");
        tkhd(m, TRACK_VIDEO, 0, videow, videoh);
        m.box("mdia");
        mdhd(m, TRACK_VIDEO, videofps);
        hdlr(m, "vide", "VideoHandler");
        m.box("minf");
        m.fullbox("vmhd", 0, 1); m.u16(0); m.zeros(6); m.end();
        dinf(m);
        m.box("stbl");
        m.fullbox("stsd", 0, 0);
        m.u32(1);
        m.box(codec == CODEC_AV1 ? "av01" : "vp09");
        m.zeros(6);
        m.u16(1); // data_reference_index
        m.zeros(16);
        m.u16(videow);
        m.u16(videoh);
        m.u32(0x480000); // 72dpi
        m.u32(0x480000);
        m.u32(0);
        m.u16(1); // frame_count
        const char *compressor = codec == CODEC_AV1 ? "AOM Coding" : "VPC Coding";
        m.u8(strlen(compressor));
        m.put(compressor, strlen(compressor));
        m.zeros(31-strlen(compressor));
        m.u16(0x18); // depth
        m.u16(0xFFFF);
        if(codec == CODEC_AV1)
        {
            m.box("av1C");
            m.u8(0x81); // marker, version 1
            m.u8((av1profile<<5)|av1level);
            m.u8((av1tier<<7)|(1<<3)|(1<<2)); // 8 bit 4:2:0
            m.u8(0);
            m.put(av1config.getbuf(), av1config.length());
            m.end();
        }
        else
        {
            m.fullbox("vpcC", 1, 0);
            m.u8(0); // profile
            m.u8(vp9level());
            m.u8((8<<4)|(1<<1)); // 8 bit, 4:2:0, studio range
            m.u8(1); // BT.709 primaries
            m.u8(1); // transfer
            m.u8(1); // matrix
            m.u16(0);
            m.end();
        }
        m.box("colr");
        m.fcc("nclx");
        m.u16(1); // BT.709 primaries
        m.u16(1); // transfer
        m.u16(1); // matrix
        m.u8(0); // studio range
        m.end();
        m.end(); // av01/vp09
        m.end(); // stsd
        emptytables(m);
        m.end(); // stbl
        m.end(); // minf
        m.end(); // mdia
        m.end(); // trak

        if(soundfrequency)
        {
            m.box("trak");
            tkhd(m, TRACK_SOUND, 0x100, 0, 0);
            m.box("edts"); // skip what the encoder primes itself with
            m.fullbox("elst", 0, 0);
            m.u32(1);
            elstpos = m.pos();
            m.u32(0); // segment_duration
            m.u32(preskip); // media_time
            m.u16(1); // media_rate
            m.u16(0);
            m.end();
            m.end();
            m.box("mdia");
            mdhd(m, TRACK_SOUND, SOUNDRATE);
            hdlr(m, "soun", "SoundHandler");
            m.box("minf");
            m.fullbox("smhd", 0, 0); m.u16(0); m.u16(0); m.end();
            dinf(m);
            m.box("stbl");
            m.fullbox("stsd", 0, 0);
            m.u32(1);
            m.box("Opus");
            m.zeros(6);
            m.u16(1); // data_reference_index
            m.zeros(8);
            m.u16(soundchannels);
            m.u16(16); // samplesize
            m.u16(0);
            m.u16(0);
            m.u32(uint(SOUNDRATE)<<16);
            m.box("dOps");
            m.u8(0); // version
            m.u8(soundchannels);
            m.u16(preskip);
            m.u32(SOUNDRATE); // input sample rate
            m.u16(0); // output gain
            m.u8(0); // channel mapping family
            m.end();
            m.end(); // Opus
            m.end(); // stsd
            emptytables(m);
            m.end(); // stbl
            m.end(); // minf
            m.end(); // mdia
            m.end(); // trak
        }

        m.box("mvex");
        m.fullbox("mehd", 0, 0);
        mehdpos = m.pos();
        m.u32(0); // fragment_duration
        m.end();
        m.fullbox("trex", 0, 0);
        m.u32(TRACK_VIDEO);
        m.u32(1); // sample description
        m.u32(1); // duration
        m.u32(0); // size
        m.u32(0x01010000); // depends on others, not a sync sample
        m.end();
        if(soundfrequency)
        {
            m.fullbox("trex", 0, 0);
            m.u32(TRACK_SOUND);
            m.u32(1);
            m.u32(OPUSFRAME);
            m.u32(0);
            m.u32(0x02000000); // depends on nothing
            m.end();
        }
        m.end(); // mvex

        m.end(); // moov

        // the buffer starts at the start of the file, so its positions are the file's
        return f->write(m.data.getbuf(), m.data.length()) == size_t(m.data.length());
    }

    bool open() override
    {
        if(codec == CODEC_AV1 ? !initav1() : !initvp9())
        {
            conoutf(colourred, "Unable to start %s encoding", codec == CODEC_AV1 ? "AV1" : "VP9");
            return false;
        }
        if(soundfrequency && !initopus()) return false;
        f = openfile(filename, "wb");
        if(!f) return false;
        return writeheader();
    }

    bool flushfragment()
    {
        if(videosizes.empty() && soundsizes.empty()) return true;

        mp4buf m;
        m.box("moof");
        m.fullbox("mfhd", 0, 0);
        m.u32(++sequence);
        m.end();
        int videooffset = -1, soundoffset = -1;
        if(videosizes.length())
        {
            m.box("traf");
            m.fullbox("tfhd", 0, 0x020000); m.u32(TRACK_VIDEO); m.end(); // offsets are from the moof
            m.fullbox("tfdt", 1, 0); m.u64(videobase); m.end();
            m.fullbox("trun", 0, 0x000001|0x000100|0x000200|0x000400); // offset, durations, sizes, flags
            m.u32(videosizes.length());
            videooffset = m.pos();
            m.u32(0);
            loopv(videosizes)
            {
                m.u32(1);
                m.u32(videosizes[i]);
                m.u32(videosync[i] ? 0x02000000 : 0x01010000);
            }
            m.end();
            m.end();
        }
        if(soundsizes.length())
        {
            m.box("traf");
            m.fullbox("tfhd", 0, 0x020000); m.u32(TRACK_SOUND); m.end();
            m.fullbox("tfdt", 1, 0); m.u64(soundbase); m.end();
            m.fullbox("trun", 0, 0x000001|0x000100|0x000200); // offset, durations, sizes
            m.u32(soundsizes.length());
            soundoffset = m.pos();
            m.u32(0);
            loopv(soundsizes)
            {
                m.u32(OPUSFRAME);
                m.u32(soundsizes[i]);
            }
            m.end();
            m.end();
        }
        m.end(); // moof

        int moofsize = m.pos();
        if(videooffset >= 0) m.patch32(videooffset, moofsize + 8);
        if(soundoffset >= 0) m.patch32(soundoffset, moofsize + 8 + videodata.length());

        if(videosizes.length() && videosync[0])
        {
            fragentry &e = fragments.add();
            e.time = videobase;
            e.offset = f->tell();
        }
        uint mdatsize = 8 + videodata.length() + sounddata.length();
        bool ok = f->write(m.data.getbuf(), moofsize) == size_t(moofsize) && f->putbig<uint>(mdatsize) && f->write("mdat", 4) == 4 &&
            f->write(videodata.getbuf(), videodata.length()) == size_t(videodata.length()) &&
            f->write(sounddata.getbuf(), sounddata.length()) == size_t(sounddata.length());

        videobase += videosizes.length();
        soundbase += ullong(soundsizes.length())*OPUSFRAME;
        videodata.setsize(0);
        videosizes.setsize(0);
        videosync.setsize(0);
        sounddata.setsize(0);
        soundsizes.setsize(0);
        return ok;
    }

    bool addvideo(const uchar *p, int len, bool sync)
    {
        // start a fragment at each key frame, or if one has gone on far too long without
        if(videosizes.length() && (sync || videosizes.length() >= keyint()*4) && !flushfragment()) return false;
        int start = videodata.length();
        if(codec == CODEC_AV1)
        {
            // temporal delimiters are left out of AV1 samples
            int type = 0, payload = 0, size = 0;
            for(int n; (n = av1obu(p, len, type, payload, size)) > 0; p += n, len -= n) if(type != 2) videodata.put(p, n);
        }
        else videodata.put(p, len);
        videosizes.add(videodata.length() - start);
        videosync.add(sync);
        return true;
    }

    bool drainav1(bool done)
    {
        for(;;)
        {
            EbBufferHeaderType *out = NULL;
            EbErrorType err = p_svt_av1_enc_get_packet(svt, &out, done ? 1 : 0);
            if(err == EB_NoErrorEmptyQueue) return true;
            if(err != EB_ErrorNone || !out) return false;
            bool eos = (out->flags&EB_BUFFERFLAG_EOS) != 0,
                 ok = !out->n_filled_len || addvideo(out->p_buffer, out->n_filled_len, out->pic_type == EB_AV1_KEY_PICTURE);
            p_svt_av1_enc_release_out_buffer(&out);
            if(!ok) return false;
            if(eos) return true;
        }
    }

    bool drainvp9()
    {
        vpx_codec_iter_t iter = NULL;
        while(const vpx_codec_cx_pkt_t *pkt = p_vpx_codec_get_cx_data(&vpx, &iter))
        {
            if(pkt->kind != VPX_CODEC_CX_FRAME_PKT) continue;
            if(!addvideo((const uchar *)pkt->data.frame.buf, int(pkt->data.frame.sz), (pkt->data.frame.flags&VPX_FRAME_IS_KEY) != 0)) return false;
        }
        return true;
    }

    // an I420 frame, in order
    bool writevideo(const uchar *data, uint size, uint frame) override
    {
        if(failed) return false;
        videoframes++;
        uchar *yuv = (uchar *)data;
        const uint planesize = videow*videoh;
        if(codec == CODEC_AV1)
        {
            EbSvtIOFormat io;
            memset(&io, 0, sizeof(io));
            io.luma = yuv;
            io.cb = yuv + planesize;
            io.cr = yuv + planesize + planesize/4;
            io.y_stride = videow;
            io.cb_stride = io.cr_stride = videow/2;
            EbBufferHeaderType in;
            memset(&in, 0, sizeof(in));
            in.size = sizeof(in);
            in.p_buffer = (uint8_t *)&io;
            in.n_filled_len = in.n_alloc_len = planesize*3/2;
            in.pts = frame;
            in.pic_type = EB_AV1_INVALID_PICTURE;
            if(p_svt_av1_enc_send_picture(svt, &in) != EB_ErrorNone || !drainav1(false)) failed = true;
        }
        else
        {
            vpx_image_t img;
            if(!p_vpx_img_wrap(&img, VPX_IMG_FMT_I420, videow, videoh, 1, yuv) ||
               p_vpx_codec_encode(&vpx, &img, frame, 1, 0, VPX_DL_GOOD_QUALITY) != VPX_CODEC_OK || !drainvp9())
            {
                conoutf(colourred, "VP9 encoding failed: %s", p_vpx_codec_error(&vpx));
                failed = true;
            }
        }
        return !failed;
    }

    bool encodesound(bool done)
    {
        const int stride = OPUSFRAME*soundchannels;
        if(done)
        {
            // prime the end with as much again as the start skips, then pad it out to whole packets
            int pad = preskip*soundchannels;
            pad += (stride - (pcm.length() + pad)%stride)%stride;
            loopi(pad) pcm.add(0);
        }
        int used = 0;
        uchar packet[4000];
        for(; used + stride <= pcm.length(); used += stride)
        {
            opus_int32 n = p_opus_encode(opus, &pcm[used], OPUSFRAME, packet, sizeof(packet));
            if(n < 0)
            {
                conoutf(colourred, "Opus encoding failed: %s", p_opus_strerror(n));
                return false;
            }
            sounddata.put(packet, n);
            soundsizes.add(n);
        }
        int left = pcm.length() - used;
        if(used && left) memmove(pcm.getbuf(), &pcm[used], left*sizeof(short));
        pcm.setsize(left);
        return true;
    }

    // 16 bit native endian samples
    bool writesound(uchar *data, uint size, uint frame) override
    {
        if(failed || !opus) return !failed;
        int samples = size/sizeof(short);
        pcm.put((const short *)data, samples);
        pcmsamples += samples/soundchannels;
        if(!encodesound(false)) failed = true;
        return !failed;
    }

    void patch(stream::offset at, uint n)
    {
        if(!at) return;
        f->seek(at, SEEK_SET);
        f->putbig<uint>(n);
    }

    void close() override
    {
        if(f)
        {
            // flush everything the encoders still hold
            if(!failed && svt)
            {
                EbBufferHeaderType eos;
                memset(&eos, 0, sizeof(eos));
                eos.size = sizeof(eos);
                eos.flags = EB_BUFFERFLAG_EOS;
                eos.pic_type = EB_AV1_INVALID_PICTURE;
                if(p_svt_av1_enc_send_picture(svt, &eos) != EB_ErrorNone || !drainav1(true)) failed = true;
            }
            if(!failed && vpxinit)
            {
                for(;;)
                {
                    int before = videosizes.length() + videobase;
                    if(p_vpx_codec_encode(&vpx, NULL, 0, 1, 0, VPX_DL_GOOD_QUALITY) != VPX_CODEC_OK || !drainvp9()) { failed = true; break; }
                    if(int(videosizes.length() + videobase) == before) break;
                }
            }
            if(opus && !encodesound(true)) failed = true;
            flushfragment();

            mp4buf m; // a random access index of where each key frame starts
            m.box("mfra");
            m.fullbox("tfra", 1, 0);
            m.u32(TRACK_VIDEO);
            m.u32(0); // traf, trun and sample numbers are a byte each
            m.u32(fragments.length());
            loopv(fragments)
            {
                m.u64(fragments[i].time);
                m.u64(fragments[i].offset);
                m.u8(1);
                m.u8(1);
                m.u8(1);
            }
            m.end();
            m.fullbox("mfro", 0, 0);
            m.u32(m.pos() + 4);
            m.end();
            m.end();
            f->write(m.data.getbuf(), m.data.length());

            uint videoms = uint(videobase*1000/videofps),
                 soundms = soundfrequency ? uint(pcmsamples*1000/SOUNDRATE) : 0,
                 moviems = max(videoms, soundms);
            patch(mvhdpos, moviems);
            patch(mehdpos, moviems);
            patch(tkhdpos[0], videoms);
            patch(mdhdpos[0], uint(videobase));
            if(soundfrequency)
            {
                patch(tkhdpos[1], soundms);
                patch(mdhdpos[1], uint(soundbase));
                patch(elstpos, soundms);
            }
            f->seek(0, SEEK_END);
            DELETEP(f);
        }

        if(svt)
        {
            p_svt_av1_enc_deinit(svt);
            p_svt_av1_enc_deinit_handle(svt);
            svt = NULL;
        }
        if(vpxinit)
        {
            p_vpx_codec_destroy(&vpx);
            vpxinit = false;
        }
        if(opus)
        {
            p_opus_encoder_destroy(opus);
            opus = NULL;
        }
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

    static moviewriter *file = NULL;
    static moviejob *jobs = NULL;
    static int numjobs = 0, state = REC_OK, quality = 90;
    static bool stopping = false;
    static uint capturedframes = 0, writtenframes = 0, stepframes = 0, soundsamples = 0;
    static vector<SDL_Thread *> encoders;
    static SDL_Thread *writer = NULL;
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
#if SDL_VERSION_ATLEAST(2, 0, 16)
            if(!out || SDL_SoftStretchLinear(src, NULL, out, NULL))
#else
            if(!out || SDL_SoftStretch(src, NULL, out, NULL)) // linear filtering needs SDL 2.0.16
#endif
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
        if(file->codec == moviewriter::CODEC_MJPG) return encodejpeg(j);

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

        soundmonitorstop();
        soundloopback = 0;
        if(hadsound && restore) resetsound();
        clockreset(); // carry on from where the movie's time got to
        state = REC_OK;
        stopping = false;
    }

    void start(const char *name)
    {
        if(file) return;

        // compressed movies fall back from AV1 to VP9, then to MJPEG AVI, as far as the codecs are there
        int codec = clamp(moviecodec, int(moviewriter::CODEC_I420), int(moviewriter::CODEC_MAX)-1);
        bool wantsound = moviesound && !nosound;
        if(codec >= moviewriter::CODEC_AV1 && wantsound && !load(LIB_OPUS))
        {
            conoutf(colouryellow, "Movie sound can't be compressed without Opus, recording MJPEG instead");
            codec = moviewriter::CODEC_MJPG;
        }
        if(codec == moviewriter::CODEC_AV1 && !load(LIB_SVT))
        {
            conoutf(colouryellow, "Movie can't be recorded as AV1, trying VP9 instead");
            codec = moviewriter::CODEC_VP9;
        }
        if(codec == moviewriter::CODEC_VP9 && !load(LIB_VPX))
        {
            conoutf(colouryellow, "Movie can't be recorded as VP9, recording MJPEG instead");
            codec = moviewriter::CODEC_MJPG;
        }
        if(codec == moviewriter::CODEC_MJPG && !(IMG_Init(IMG_INIT_JPG)&IMG_INIT_JPG)) // load it here, as the encoders would race to
        {
            conoutf(colouryellow, "JPEG encoding is unavailable, recording uncompressed video instead: %s", IMG_GetError());
            codec = moviewriter::CODEC_I420;
        }

        int soundrate = 0, rate = codec >= moviewriter::CODEC_AV1 ? int(mp4writer::SOUNDRATE) : moviesoundrate; // Opus only takes certain rates
        if(wantsound)
        {
            soundloopback = rate;
            resetsound();
            if(soundcapturing) soundrate = rate;
            else
            {
                soundloopback = 0;
                resetsound();
                conoutf(colouryellow, "Movie will be recorded without sound");
            }
        }

        uint w = moview ? moview : renderw, h = movieh ? movieh : renderh;
        w = max(w, 2U);
        h = max(h, 2U);
        if(codec >= moviewriter::CODEC_AV1) file = new mp4writer(name, w, h, moviefps, codec, soundrate, 2);
        else file = new aviwriter(name, w, h, moviefps, codec, soundrate, 2);
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
        clockreset(); // so the movie's frames come due in real time from here
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
        if(soundrate && moviemonitor && !soundmonitorstart(soundrate)) conoutf(colouryellow, "Unable to play the movie's sound while recording");

        static const char * const codecnames[moviewriter::CODEC_MAX] = { "I420", "MJPEG", "AV1", "VP9" };
        conoutf(colourwhite, "Movie recording to: %s %dx%d @ %dfps %s%s", findfile(file->filename, "wb"), file->videow, file->videoh, file->videofps,
            codecnames[codec], soundrate ? (codec >= moviewriter::CODEC_AV1 ? " + Opus sound" : " + sound") : "");
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
            soundmonitor(buf, samples);
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
    if(recorder::isrecording()) recorder::stop();
    else if(*name) recorder::start(name);
    else recorder::start(*filetimeformat ? gettime(filetimelocal ? currenttime : clocktime, filetimeformat) : "movie");
});
ICOMMANDV(0, movierecording, recorder::isrecording() ? 1 : 0);
