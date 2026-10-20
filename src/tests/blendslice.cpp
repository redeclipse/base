// blendslice.cpp: checks the face read rule in engine/octa.h, which picks the blendmap slice a face reads. Runs at
// startup in _DEBUG builds; to run it alone, build from src with:
//   g++ -std=c++17 -D_DEBUG -DSTANDALONE -DBLENDSLICE_MAIN -I. -Ishared -Iengine -Igame -Ienet/include -Isupport tests/blendslice.cpp -o blendslice
//   ./blendslice
#include "cube.h"
#ifdef BLENDSLICE_MAIN
typedef unsigned int GLuint;
#include "world.h"
#endif
#include "octa.h"

// the slice spans world units [lo, lo+BLENDSLICE_SIZE) along the face's axis
static float sliceorigin(int orient, float pos) { return blendslice(orient, pos)*float(BLENDSLICE_SIZE); }

void testblendslice()
{
    // examples from the design: a 2-unit wall from x=0 to x=2, and a floor at z=0 meeting it
    ASSERT(blendslice(O_LEFT, 0) == -1);
    ASSERT(blendslice(O_RIGHT, 2) == 1);
    ASSERT(blendslice(O_TOP, 0) == 0);
    ASSERT(blendslice(O_BOTTOM, 4) == 0);

    // geometry sits on a 1/8 unit grid
    for(int i = -64*8; i <= 64*8; i++)
    {
        float p = i/8.0f;
        // the slice lies wholly in front of the face, within the tolerance, and is the nearest such slice
        loopk(3)
        {
            float lo = sliceorigin(2*k + 1, p);
            ASSERT(lo >= p - BLENDSLICE_TOLERANCE && lo - BLENDSLICE_SIZE < p - BLENDSLICE_TOLERANCE);
            float hi = sliceorigin(2*k, p) + BLENDSLICE_SIZE;
            ASSERT(hi <= p + BLENDSLICE_TOLERANCE && hi + BLENDSLICE_SIZE > p + BLENDSLICE_TOLERANCE);
        }

        // interpolated fragment positions jitter around the face, which must not change the slice
        loopk(6)
        {
            int slice = blendslice(k, p);
            ASSERT(blendslice(k, p - 0.002f) == slice);
            ASSERT(blendslice(k, p + 0.002f) == slice);
        }

        // the two sides of a wall from p to q, however thin, read different slices
        for(int j = i; j <= i + 4*8; j++)
        {
            float q = j/8.0f;
            loopk(3) ASSERT(blendslice(2*k + 1, q) > blendslice(2*k, p));
        }
    }
}

#ifdef BLENDSLICE_MAIN
int main()
{
    testblendslice();
    printf("blendslice: ok\n");
    return 0;
}
#endif
