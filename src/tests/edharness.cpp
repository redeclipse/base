#include "engine.h"

// Map editor test harness helpers, see tools/harness/.
static bool nearenough(float a, float b) { return fabs(a - b) < 0.001f; }

static bool vecnear(const vec &v, float x, float y, float z)
{
    return nearenough(v.x, x) && nearenough(v.y, y) && nearenough(v.z, z);
}

void testedharness()
{
    // vec(yaw, pitch) points +Y at yaw 0, so an orbit camera at yaw 0 sits on
    // the -Y side of its target and looks back along +Y.
    ASSERT(vecnear(orbitpos(vec(0, 0, 0), 10, 0, 0), 0, -10, 0));

    // Yaw 90 puts the direction on -X, so the camera sits on +X.
    ASSERT(vecnear(orbitpos(vec(0, 0, 0), 10, 90, 0), 10, 0, 0));

    // Looking straight down means sitting straight above.
    ASSERT(vecnear(orbitpos(vec(0, 0, 0), 10, 0, 90), 0, 0, -10));
    ASSERT(vecnear(orbitpos(vec(0, 0, 0), 10, 0, -90), 0, 0, 10));

    // The target is an offset, not an origin.
    ASSERT(vecnear(orbitpos(vec(5, 5, 5), 10, 0, 0), 5, -5, 5));

    // Zero distance degenerates to the target itself.
    ASSERT(vecnear(orbitpos(vec(3, 4, 5), 0, 45, 30), 3, 4, 5));

    conoutf(colourwhite, "testedharness: ok");
}
