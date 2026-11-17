#include <math.h>
#include "SDL.h"
#include "SDL_image.h"

static int bendTable[256][24] = {
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{1,0,1,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,1,0,1,0,1,0},{8,1,8,1,8,8,8,8,0,0,0,0,3,0,0,0,2,0,2,0,2,0,2,0},{8,0,8,0,8,0,8,0,1,0,0,0,3,0,0,0,1,0,2,0,1,0,2,0},{2,0,2,0,0,0,0,0,8,1,8,8,8,8,8,8,3,0,3,0,3,0,3,0},{1,0,2,0,0,0,0,0,8,0,8,0,8,0,8,0,1,0,1,0,3,0,3,0},{8,1,2,0,8,8,0,0,1,0,8,0,8,3,8,0,2,0,2,0,3,0,3,0},{8,0,8,2,8,0,8,0,8,0,8,0,3,0,0,0,1,0,2,0,3,0,2,0},
    {8,2,8,2,8,8,8,8,8,8,8,8,8,3,8,8,4,0,4,0,4,0,4,0},{8,1,2,0,8,0,8,0,1,0,0,0,8,3,8,8,1,0,4,0,1,0,4,0},{8,1,8,2,8,8,8,8,8,0,8,0,8,0,8,0,2,0,2,0,4,0,4,0},{8,0,8,2,8,0,8,8,8,1,8,0,8,0,8,0,1,0,2,0,4,0,4,0},{8,0,8,0,8,0,8,0,8,1,8,8,8,3,8,8,3,0,4,0,3,0,4,0},{1,0,8,0,0,0,8,0,8,0,8,0,3,0,8,0,1,0,1,0,3,0,4,0},{1,0,8,0,8,0,8,0,8,1,8,8,8,0,8,0,3,0,2,0,3,0,4,0},{8,0,8,0,8,0,8,0,8,0,8,0,8,0,8,0,1,0,2,0,3,0,4,0},
    {0,0,0,0,3,0,3,0,0,0,2,0,0,0,0,0,8,1,8,1,8,1,8,1},{1,0,1,0,3,0,3,0,1,0,2,0,0,0,0,0,8,0,8,0,8,0,8,0},{1,0,8,0,8,3,8,0,2,0,2,0,3,0,3,0,8,1,2,0,8,8,0,0},{8,0,8,0,3,0,0,0,1,0,2,0,3,0,2,0,8,0,8,2,8,0,8,0},{2,0,2,0,3,0,3,0,8,1,2,0,8,8,0,0,1,0,8,0,8,3,8,0},{1,0,2,0,3,0,2,0,8,0,8,2,8,0,8,0,8,0,8,0,3,0,0,0},{0,0,0,0,0,0,0,0,8,8,8,8,8,8,8,8,0,0,0,0,0,0,0,0},{8,0,2,0,8,3,0,0,8,0,2,0,8,3,2,0,8,0,2,0,8,3,8,0},
    {8,0,8,2,3,0,8,0,8,0,2,0,8,3,8,0,8,1,8,0,8,0,4,0},{8,1,2,0,8,3,8,0,1,0,2,0,8,3,8,8,8,0,4,0,8,0,4,0},{8,1,8,2,3,0,0,0,8,0,2,0,8,0,2,0,1,0,8,2,8,0,8,4},{8,0,2,0,8,3,8,0,1,0,8,2,8,0,8,0,8,0,2,0,8,0,4,0},{8,0,8,0,3,0,3,0,8,1,2,0,8,3,0,0,1,0,8,0,8,3,8,4},{1,0,8,0,3,0,8,0,8,0,8,2,3,0,8,0,8,0,8,0,3,0,8,4},{8,8,8,8,8,8,8,8,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{8,0,8,0,3,0,0,0,8,0,2,0,8,0,2,0,8,0,8,2,8,3,8,4},
    {8,8,8,8,8,3,8,8,0,0,4,0,0,0,4,0,8,2,8,2,8,2,8,2},{8,1,8,0,3,0,8,0,1,0,1,0,4,0,4,0,1,0,8,2,0,0,8,8},{8,1,8,1,8,3,8,3,0,0,0,0,3,0,4,0,8,0,8,0,8,0,8,0},{8,0,8,0,8,3,8,8,1,0,4,0,3,0,4,0,8,1,8,0,8,0,8,0},{8,0,2,0,8,3,8,0,8,1,8,0,8,0,4,0,8,0,8,2,3,0,8,0},{1,0,2,0,8,3,8,8,8,0,4,0,8,0,4,0,8,1,2,0,8,3,8,0},{8,1,2,0,8,3,0,0,1,0,8,0,8,3,8,4,8,0,8,0,3,0,3,0},{8,0,8,2,3,0,8,0,8,0,8,0,3,0,8,4,1,0,8,0,3,0,8,0},
    {8,2,8,2,8,3,8,3,8,8,0,0,8,3,4,0,8,0,2,0,8,0,8,4},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{8,1,8,2,8,3,8,2,8,0,8,0,8,0,8,4,8,0,8,0,0,0,4,0},{8,0,2,0,8,3,8,3,1,0,0,0,8,0,8,4,8,1,8,0,8,0,4,0},{8,0,8,0,8,3,8,3,8,1,0,0,8,3,4,0,8,0,2,0,8,3,8,4},{8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,0,0,0,0,0,0,0,0},{8,1,8,0,8,3,8,0,1,0,8,0,8,0,8,4,8,0,8,0,8,3,4,0},{8,0,8,0,8,3,8,8,8,0,4,0,8,0,4,0,8,1,8,0,8,3,8,4},
    {8,0,8,0,8,0,8,0,0,0,2,0,0,0,4,0,8,1,8,2,8,1,8,2},{1,0,0,0,8,0,8,0,1,0,2,0,1,0,4,0,8,0,2,0,8,0,8,0},{8,1,8,8,8,0,8,0,3,0,2,0,3,0,4,0,1,0,8,0,8,0,8,0},{8,0,8,0,8,0,8,0,1,0,2,0,3,0,4,0,8,0,8,0,8,0,8,0},{2,0,2,0,8,0,8,0,8,1,2,0,8,8,4,0,1,0,2,0,8,3,8,0},{1,0,2,0,8,0,8,0,8,0,2,0,8,0,8,4,8,0,2,0,8,3,8,0},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,8,8,8,8,8,8,8},{8,0,2,0,8,0,0,0,8,0,8,2,8,3,8,4,8,0,8,0,3,0,3,0},
    {8,2,8,2,8,0,8,0,8,8,2,0,8,3,4,0,1,0,2,0,8,0,8,4},{8,8,8,8,8,8,8,8,0,0,0,0,0,0,0,0,8,8,8,8,8,8,8,8},{8,1,8,2,8,0,8,0,8,0,8,2,8,0,4,0,1,0,8,0,8,0,8,4},{8,0,8,2,8,0,8,8,8,1,8,2,8,0,8,4,8,0,8,0,4,0,4,0},{8,0,8,0,8,0,8,0,1,0,8,2,3,0,8,4,8,1,8,2,3,0,4,0},{8,1,8,0,8,0,8,0,8,0,8,2,3,0,8,4,8,0,8,2,3,0,4,0},{1,0,8,0,8,0,8,0,8,1,2,0,8,0,4,0,1,0,8,0,8,3,8,4},{8,0,8,0,8,0,8,0,8,0,8,2,8,0,8,4,8,0,8,0,3,0,4,0},
    {0,0,0,0,0,0,4,0,8,8,8,2,8,8,8,2,8,3,8,3,8,3,8,3},{1,0,1,0,4,0,4,0,1,0,8,2,0,0,8,8,8,1,8,0,3,0,8,0},{8,1,8,0,8,0,4,0,8,0,8,2,3,0,8,0,8,0,2,0,8,3,8,0},{8,0,8,0,4,0,4,0,1,0,8,2,3,0,8,8,8,1,8,2,3,0,8,0},{2,0,2,0,4,0,4,0,8,1,8,2,8,8,8,8,8,0,8,0,8,0,8,0},{1,0,2,0,1,0,4,0,8,0,2,0,8,0,8,0,1,0,0,0,8,0,8,0},{8,1,2,0,8,8,4,0,1,0,2,0,8,3,8,0,2,0,2,0,8,0,8,0},{8,0,2,0,8,0,4,0,8,0,2,0,8,3,8,0,1,0,8,2,8,0,8,0},
    {8,0,2,0,8,0,8,4,8,2,8,2,8,3,8,3,8,8,0,0,8,3,4,0},{8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,0,0,0,0,0,0,0,0},{8,1,8,2,0,0,4,0,8,0,8,2,8,0,8,2,8,0,8,2,3,0,8,4},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{8,0,8,0,0,0,4,0,8,1,8,2,8,3,8,2,8,0,8,0,8,0,8,4},{8,1,8,0,4,0,4,0,8,0,2,0,8,3,8,8,1,0,8,0,8,0,8,4},{1,0,8,0,8,0,8,4,8,1,8,2,8,0,8,0,8,0,8,2,8,0,4,0},{8,0,8,0,4,0,4,0,8,0,8,2,8,0,8,8,8,1,8,2,8,0,8,4},
    {0,0,0,0,3,0,4,0,8,0,8,0,8,0,8,0,8,1,8,1,8,3,8,3},{1,0,4,0,3,0,4,0,8,1,8,0,8,0,8,0,8,0,8,0,8,3,8,8},{8,1,8,8,3,0,4,0,3,0,8,0,3,0,8,0,1,0,8,2,3,0,8,0},{8,0,8,0,3,0,8,4,1,0,8,0,3,0,8,0,8,0,8,2,3,0,8,0},{3,0,2,0,3,0,4,0,1,0,8,0,8,0,8,0,8,1,8,8,8,0,8,0},{1,0,2,0,3,0,4,0,8,0,8,0,8,0,8,0,8,0,8,0,8,0,8,0},{0,0,0,0,0,0,0,0,8,8,8,8,8,8,8,8,0,0,0,0,0,0,0,0},{8,0,8,2,8,3,8,4,8,0,8,0,3,0,3,0,8,0,2,0,8,0,0,0},
    {8,8,8,2,3,0,4,0,8,3,8,0,8,3,8,0,1,0,8,0,3,0,8,4},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{1,0,2,0,8,3,8,4,8,0,8,0,8,0,8,0,8,1,2,0,8,3,4,0},{8,0,8,2,3,0,4,0,8,1,8,0,8,0,8,0,8,0,8,2,3,0,8,4},{8,0,8,0,3,0,4,0,8,1,8,0,3,0,8,0,1,0,8,0,8,0,8,4},{8,1,8,0,8,3,8,4,8,0,8,0,8,3,8,3,8,0,0,0,8,0,4,0},{8,1,8,0,3,0,4,0,1,0,8,0,8,0,8,0,1,0,8,2,8,0,8,4},{8,0,8,0,8,3,8,4,8,0,8,0,8,0,8,0,8,0,2,0,8,0,4,0},
    {8,0,8,0,3,0,8,4,8,8,8,2,0,0,4,0,8,3,8,2,8,3,8,2},{0,0,0,0,0,0,0,0,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8},{8,1,0,0,8,3,4,0,8,0,2,0,8,3,8,4,8,0,8,0,8,3,8,3},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{8,8,2,0,8,3,4,0,1,0,2,0,8,0,8,4,8,2,8,2,8,0,8,0},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{1,0,8,2,3,0,8,4,8,1,8,2,3,0,4,0,8,0,8,0,8,0,8,0},{8,0,8,2,3,0,8,4,8,0,8,2,3,0,4,0,8,1,8,0,8,0,8,0},
    {8,8,8,8,8,8,8,8,0,0,0,0,0,0,0,0,8,8,8,8,8,8,8,8},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{8,8,8,8,8,8,8,8,0,0,0,0,0,0,0,0,8,8,8,8,8,8,8,8},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,0,0,0,0,0,0,0,0},{8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,0,0,0,0,0,0,0,0},{1,0,8,0,3,0,8,4,8,1,8,2,8,0,4,0,8,0,8,0,8,0,8,4},{8,0,8,0,8,3,8,4,8,0,2,0,8,0,8,4,8,1,8,0,8,0,8,4},
    {8,0,8,0,8,0,8,4,8,0,8,0,0,0,4,0,8,1,8,2,8,3,8,2},{8,1,0,0,8,0,4,0,1,0,8,0,1,0,8,4,8,0,8,2,3,0,8,0},{1,0,8,0,8,0,8,4,8,0,8,0,3,0,4,0,8,1,8,0,3,0,8,0},{8,0,0,0,8,0,4,0,8,1,8,0,8,3,8,4,8,0,8,0,8,3,8,3},{8,0,2,0,8,0,4,0,1,0,8,0,8,0,8,4,8,1,2,0,8,0,8,0},{8,1,8,2,8,0,8,4,8,0,8,0,4,0,4,0,8,0,8,2,8,0,8,8},{8,1,2,0,8,0,4,0,1,0,8,0,8,3,8,4,1,0,8,0,8,0,8,0},{8,0,2,0,8,0,4,0,8,0,8,0,8,3,8,4,8,0,8,0,8,0,8,0},
    {0,0,0,0,0,0,0,0,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8},{0,0,0,0,0,0,0,0,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8},{8,1,8,2,8,0,4,0,8,0,8,0,8,0,8,4,1,0,8,0,3,0,8,4},{8,0,2,0,8,0,8,4,8,1,8,0,8,0,8,4,8,0,8,0,3,0,4,0},{8,0,8,0,8,0,8,4,8,1,8,0,8,3,4,0,1,0,2,0,8,0,8,4},{8,1,8,0,8,0,8,4,8,0,8,0,8,3,8,4,8,0,2,0,8,0,8,4},{1,0,8,0,8,0,8,4,1,0,8,0,8,0,8,4,8,1,8,0,8,0,4,0},{8,0,8,0,8,0,8,4,8,0,8,0,8,0,8,4,8,0,8,0,8,0,4,0},
    {8,8,8,8,8,4,8,4,8,8,8,8,8,8,8,4,8,4,8,4,8,4,8,4},{1,0,8,0,8,0,8,4,1,0,8,0,8,0,8,4,1,0,8,0,8,0,8,4},{8,1,8,1,8,4,8,4,0,0,8,8,3,0,8,4,8,0,8,2,8,0,4,0},{8,0,8,0,8,4,8,4,1,0,8,8,3,0,8,4,8,1,8,2,8,0,4,0},{8,0,8,2,8,0,4,0,8,1,8,1,8,4,8,4,0,0,8,8,3,0,8,4},{1,0,2,0,8,8,8,4,8,0,8,4,8,0,8,4,8,1,8,0,8,3,4,0},{8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8},{0,0,0,0,0,0,0,0,8,8,8,8,8,8,8,8,0,0,0,0,0,0,0,0},
    {8,8,8,2,8,8,8,4,8,3,8,4,8,3,8,4,8,0,8,0,8,0,8,0},{8,1,2,0,8,0,4,0,1,0,0,0,8,3,8,4,1,0,8,0,1,0,8,0},{8,1,8,2,8,1,8,4,8,0,8,0,8,0,4,0,0,0,2,0,8,0,8,0},{8,0,8,2,8,0,8,4,8,1,8,0,8,0,4,0,8,1,2,0,8,0,8,0},{8,0,8,0,8,8,8,4,8,1,8,4,8,3,8,4,8,0,8,0,8,3,8,0},{8,1,8,0,8,0,4,0,8,0,8,0,8,3,8,4,8,1,8,0,3,0,8,0},{8,1,8,0,8,0,4,0,1,0,8,8,8,0,8,4,3,0,8,2,3,0,8,0},{8,0,8,0,8,8,8,4,8,0,8,4,8,0,8,4,8,1,8,2,8,3,8,0},
    {8,0,8,0,8,3,4,0,0,0,2,0,8,8,8,4,8,1,8,4,8,1,8,4},{1,0,8,8,3,0,8,4,8,1,8,2,8,0,4,0,8,0,8,0,8,4,8,4},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,8,8,8,8,8,8,8},{8,8,8,8,8,8,8,8,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,8,8,8,8,8,8,8},{8,8,8,8,8,8,8,8,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,8,8,8,8,8,8,8,8,0,0,0,0,0,0,0,0},
    {0,0,8,2,3,0,8,4,8,0,8,2,3,0,4,0,8,1,8,1,8,0,8,0},{1,0,8,2,3,0,8,4,8,1,8,2,3,0,4,0,8,0,8,0,8,0,8,0},{8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8},{8,0,2,0,8,3,4,0,1,0,2,0,8,0,8,4,8,0,8,2,8,0,8,0},{0,0,0,0,0,0,0,0,8,8,8,8,8,8,8,8,0,0,0,0,0,0,0,0},{1,0,8,0,3,0,8,4,8,0,8,2,3,0,4,0,8,0,8,0,8,3,8,0},{8,8,8,8,8,8,8,8,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{8,0,8,0,8,3,4,0,8,0,8,2,8,0,8,4,8,0,8,2,8,3,8,0},
    {8,8,8,8,8,3,8,4,8,0,8,0,8,0,8,0,8,2,8,2,8,4,8,4},{1,0,0,0,8,3,8,4,1,0,8,0,1,0,8,0,8,1,2,0,8,0,4,0},{8,1,8,4,8,3,8,4,8,0,8,0,8,3,8,0,8,0,8,0,8,8,8,4},{8,0,8,0,8,3,8,4,8,1,8,0,3,0,8,0,8,1,8,0,8,0,4,0},{0,0,2,0,8,3,8,4,8,1,8,0,8,1,8,0,8,0,2,0,8,3,4,0},{8,1,8,2,3,0,4,0,8,0,8,0,8,0,8,0,1,0,8,2,3,0,8,4},{8,8,8,8,8,8,8,8,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{8,0,2,0,8,3,8,4,8,0,8,0,8,3,8,0,8,1,8,0,8,3,4,0},
    {8,3,8,2,8,3,8,4,8,0,8,0,3,0,8,0,8,8,8,2,8,0,8,0},{8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,0,0,0,0,0,0,0,0},{8,1,8,2,8,3,8,4,8,0,8,0,8,0,8,0,8,0,8,0,8,0,8,0},{8,0,2,0,3,0,4,0,1,0,1,0,8,0,8,0,1,0,8,0,0,0,8,0},{8,0,8,0,8,3,8,4,1,0,8,0,8,3,8,0,8,0,2,0,8,3,8,0},{1,0,8,0,8,3,8,4,8,0,8,0,3,0,8,0,8,1,2,0,8,3,8,0},{1,0,8,0,3,0,4,0,8,1,8,1,8,0,8,0,0,0,8,0,3,0,8,0},{8,0,8,0,3,0,4,0,8,0,8,0,8,0,8,0,1,0,8,0,3,0,8,0},
    {8,0,8,8,8,0,8,4,8,0,8,2,8,0,8,0,8,1,8,2,8,4,8,4},{8,1,8,0,8,0,4,0,8,1,2,0,8,0,8,0,8,0,8,2,8,0,8,4},{8,1,8,1,8,0,4,0,0,0,2,0,8,3,8,0,1,0,8,0,8,0,8,4},{8,0,8,8,8,0,8,4,8,1,8,2,8,3,8,0,8,0,8,0,8,4,8,4},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,8,8,8,8,8,8,8},{8,1,8,2,8,0,4,0,8,0,8,2,8,0,8,0,8,0,8,2,3,0,8,4},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,8,8,8,8,8,8,8},{8,0,8,2,8,0,4,0,8,0,8,2,8,3,8,0,8,0,8,0,8,3,8,4},
    {8,0,8,2,8,0,8,4,8,0,8,2,3,0,8,0,1,0,8,2,8,0,8,0},{8,1,2,0,8,0,4,0,1,0,2,0,8,3,8,0,8,0,2,0,8,0,8,0},{1,0,2,0,8,0,4,0,2,0,2,0,8,0,8,0,8,1,8,0,8,8,8,0},{8,0,8,2,8,0,8,4,8,1,8,2,8,0,8,0,8,0,8,0,8,0,8,0},{8,0,8,0,8,0,4,0,8,1,2,0,8,3,8,0,1,0,2,0,8,3,8,0},{8,1,8,0,8,0,4,0,8,0,2,0,8,3,8,0,8,0,2,0,8,3,8,0},{1,0,8,0,8,0,4,0,1,0,2,0,8,0,8,0,1,0,8,0,8,3,8,0},{8,0,8,0,8,0,4,0,8,0,8,2,8,0,8,0,8,0,8,0,3,0,8,0},
    {8,0,8,0,8,0,8,0,8,8,8,2,8,8,8,4,8,3,8,4,8,3,8,4},{1,0,1,0,8,0,8,0,1,0,8,2,0,0,8,4,8,1,8,0,3,0,4,0},{8,1,8,1,8,0,8,0,0,0,8,2,3,0,8,4,8,0,8,2,3,0,4,0},{8,0,8,0,8,0,8,0,8,1,2,0,8,3,4,0,1,0,2,0,8,3,8,4},{0,0,2,0,8,0,8,0,8,1,8,2,8,1,8,4,8,0,8,0,8,0,4,0},{8,1,2,0,8,0,8,0,8,0,8,2,8,0,8,4,8,1,8,0,8,0,4,0},{0,0,0,0,0,0,0,0,8,8,8,8,8,8,8,8,0,0,0,0,0,0,0,0},{8,0,8,2,8,0,8,0,8,0,8,2,3,0,8,4,8,1,8,2,8,0,4,0},
    {8,8,8,2,8,0,8,0,8,3,8,2,8,3,8,4,8,0,8,0,3,0,8,0},{0,0,0,0,0,0,0,0,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8},{1,0,8,2,8,0,8,0,8,0,8,2,8,0,8,4,8,0,8,2,3,0,8,0},{8,0,2,0,8,0,8,0,1,0,8,2,8,0,8,4,8,1,8,2,3,0,8,0},{8,0,8,0,8,0,8,0,8,1,8,2,8,3,8,4,8,0,8,0,8,0,8,0},{1,0,8,0,0,0,8,0,8,0,2,0,3,0,4,0,1,0,1,0,8,0,8,0},{8,1,8,0,8,8,8,0,1,0,2,0,8,0,4,0,2,0,2,0,8,0,8,0},{8,0,8,0,8,0,8,0,8,0,2,0,8,0,4,0,1,0,2,0,8,0,8,0},
    {0,0,8,0,3,0,8,0,8,0,8,0,8,0,4,0,8,1,8,1,8,3,8,4},{1,0,8,0,3,0,8,0,8,1,8,0,8,0,4,0,8,0,8,0,8,3,4,0},{8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8},{8,0,8,0,8,3,8,0,1,0,8,0,3,0,8,4,8,0,8,2,3,0,4,0},{8,0,8,2,3,0,8,0,8,1,8,0,8,8,4,0,1,0,8,4,8,0,8,4},{8,1,8,2,8,3,8,0,8,0,8,0,8,4,8,4,8,0,8,8,8,0,8,4},{8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8},{8,0,8,2,8,3,8,0,8,0,8,0,8,3,4,0,8,0,8,2,8,0,8,4},
    {8,0,2,0,8,3,8,0,8,0,8,0,3,0,8,4,8,1,8,0,8,3,8,0},{8,1,2,0,8,3,8,0,1,0,8,0,8,3,8,4,8,0,8,0,3,0,8,0},{8,1,8,2,3,0,8,0,8,0,8,0,8,0,4,0,1,0,8,2,3,0,8,0},{8,0,2,0,8,3,8,0,8,1,8,0,8,0,4,0,8,0,2,0,8,3,8,0},{0,0,8,0,3,0,8,0,1,0,8,0,3,0,4,0,8,1,8,1,8,0,8,0},{8,1,8,0,8,3,8,0,8,0,8,0,8,3,8,4,8,0,8,0,8,0,8,0},{1,0,8,0,8,3,8,0,1,0,8,0,8,0,4,0,8,1,8,2,8,0,8,0},{8,0,8,0,8,3,8,0,8,0,8,0,8,0,4,0,8,0,2,0,8,0,8,0},
    {8,0,8,0,3,0,8,0,8,8,8,2,8,0,8,0,8,3,8,2,8,3,8,4},{8,8,8,8,8,8,8,8,0,0,0,0,0,0,0,0,8,8,8,8,8,8,8,8},{1,0,8,0,8,3,8,0,8,0,2,0,8,3,8,0,8,0,8,0,8,3,8,4},{8,0,8,0,3,0,8,0,1,0,8,2,3,0,8,0,8,1,8,0,3,0,4,0},{8,0,8,2,3,0,8,0,8,1,8,2,8,0,8,0,8,0,2,0,8,0,8,4},{1,0,2,0,8,3,8,0,8,0,2,0,8,0,8,0,8,1,2,0,8,0,4,0},{8,1,2,0,8,3,8,0,1,0,2,0,8,3,8,0,8,0,8,0,8,0,4,0},{8,0,8,2,3,0,8,0,8,0,2,0,8,3,8,0,1,0,8,0,8,0,8,4},
    {8,0,2,0,8,3,8,0,8,8,2,0,8,3,8,0,8,3,2,0,8,3,8,0},{0,0,0,0,0,0,0,0,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8},{1,0,2,0,3,0,8,0,8,2,8,2,8,0,8,0,8,8,8,0,8,3,8,0},{8,0,2,0,3,0,8,0,8,1,8,2,8,0,8,0,8,1,8,0,3,0,8,0},{8,8,8,0,8,3,8,0,1,0,2,0,3,0,8,0,8,2,8,2,8,0,8,0},{8,1,8,0,3,0,8,0,8,0,2,0,3,0,8,0,8,1,8,2,8,0,8,0},{1,0,8,0,3,0,8,0,8,1,8,2,8,0,8,0,8,0,8,0,8,0,8,0},{8,0,8,0,3,0,8,0,8,0,2,0,8,0,8,0,1,0,8,0,8,0,8,0},
    {8,0,8,0,8,0,8,0,8,0,8,0,8,0,8,0,8,1,8,2,8,3,8,4},{1,0,0,0,8,0,8,0,1,0,8,0,1,0,8,0,8,0,2,0,3,0,4,0},{8,1,8,1,8,0,8,0,0,0,8,0,3,0,8,0,1,0,8,0,3,0,4,0},{8,0,8,0,8,0,8,0,8,1,8,0,8,3,8,0,8,0,8,0,8,3,8,4},{2,0,2,0,8,0,8,0,8,1,8,0,8,8,8,0,1,0,2,0,8,0,4,0},{8,1,8,2,8,0,8,0,8,0,8,0,8,0,8,0,8,0,8,2,8,0,8,4},{1,0,2,0,8,0,8,0,1,0,8,0,8,3,8,0,1,0,8,0,8,0,4,0},{8,0,8,2,8,0,8,0,8,0,8,0,8,3,8,0,8,0,8,0,8,0,8,4},
    {8,8,8,2,8,0,8,0,8,3,8,0,8,3,8,0,1,0,2,0,3,0,8,0},{8,1,2,0,8,0,8,0,8,1,8,0,8,3,8,0,8,0,2,0,3,0,8,0},{1,0,2,0,8,0,8,0,8,0,8,0,8,0,8,0,8,1,8,0,8,3,8,0},{8,0,2,0,8,0,8,0,8,1,8,0,8,0,8,0,8,0,8,0,8,3,8,0},{8,0,8,0,8,0,8,0,1,0,8,0,3,0,8,0,8,1,8,2,8,0,8,0},{8,1,8,0,8,0,8,0,8,0,8,0,3,0,8,0,8,0,8,2,8,0,8,0},{1,0,8,0,8,0,8,0,1,0,8,0,8,0,8,0,8,1,8,0,8,0,8,0},{8,0,8,0,8,0,8,0,8,0,8,0,8,0,8,0,8,0,8,0,8,0,8,0}
};

static int edgeTable[256] = {
    0x0, 0x111, 0x241, 0x350, 0x412, 0x503, 0x653, 0x742, 
    0x842, 0x953, 0xa03, 0xb12, 0xc50, 0xd41, 0xe11, 0xf00, 
    0x124, 0x35, 0x365, 0x274, 0x536, 0x427, 0x777, 0x666, 
    0x966, 0x877, 0xb27, 0xa36, 0xd74, 0xc65, 0xf35, 0xe24, 
    0x284, 0x395, 0xc5, 0x1d4, 0x696, 0x787, 0x4d7, 0x5c6, 
    0xac6, 0xbd7, 0x887, 0x996, 0xed4, 0xfc5, 0xc95, 0xd84, 
    0x3a0, 0x2b1, 0x1e1, 0xf0, 0x7b2, 0x6a3, 0x5f3, 0x4e2, 
    0xbe2, 0xaf3, 0x9a3, 0x8b2, 0xff0, 0xee1, 0xdb1, 0xca0, 
    0x428, 0x539, 0x669, 0x778, 0x3a, 0x12b, 0x27b, 0x36a, 
    0xc6a, 0xd7b, 0xe2b, 0xf3a, 0x878, 0x969, 0xa39, 0xb28, 
    0x50c, 0x41d, 0x74d, 0x65c, 0x11e, 0xf, 0x35f, 0x24e, 
    0xd4e, 0xc5f, 0xf0f, 0xe1e, 0x95c, 0x84d, 0xb1d, 0xa0c, 
    0x6ac, 0x7bd, 0x4ed, 0x5fc, 0x2be, 0x3af, 0xff, 0x1ee, 
    0xeee, 0xfff, 0xcaf, 0xdbe, 0xafc, 0xbed, 0x8bd, 0x9ac, 
    0x788, 0x699, 0x5c9, 0x4d8, 0x39a, 0x28b, 0x1db, 0xca, 
    0xfca, 0xedb, 0xd8b, 0xc9a, 0xbd8, 0xac9, 0x999, 0x888, 
    0x888, 0x999, 0xac9, 0xbd8, 0xc9a, 0xd8b, 0xedb, 0xfca, 
    0xca, 0x1db, 0x28b, 0x39a, 0x4d8, 0x5c9, 0x699, 0x788, 
    0x9ac, 0x8bd, 0xbed, 0xafc, 0xdbe, 0xcaf, 0xfff, 0xeee, 
    0x1ee, 0xff, 0x3af, 0x2be, 0x5fc, 0x4ed, 0x7bd, 0x6ac, 
    0xa0c, 0xb1d, 0x84d, 0x95c, 0xe1e, 0xf0f, 0xc5f, 0xd4e, 
    0x24e, 0x35f, 0xf, 0x11e, 0x65c, 0x74d, 0x41d, 0x50c, 
    0xb28, 0xa39, 0x969, 0x878, 0xf3a, 0xe2b, 0xd7b, 0xc6a, 
    0x36a, 0x27b, 0x12b, 0x3a, 0x778, 0x669, 0x539, 0x428, 
    0xca0, 0xdb1, 0xee1, 0xff0, 0x8b2, 0x9a3, 0xaf3, 0xbe2, 
    0x4e2, 0x5f3, 0x6a3, 0x7b2, 0xf0, 0x1e1, 0x2b1, 0x3a0, 
    0xd84, 0xc95, 0xfc5, 0xed4, 0x996, 0x887, 0xbd7, 0xac6, 
    0x5c6, 0x4d7, 0x787, 0x696, 0x1d4, 0xc5, 0x395, 0x284, 
    0xe24, 0xf35, 0xc65, 0xd74, 0xa36, 0xb27, 0x877, 0x966, 
    0x666, 0x777, 0x427, 0x536, 0x274, 0x365, 0x35, 0x124, 
    0xf00, 0xe11, 0xd41, 0xc50, 0xb12, 0xa03, 0x953, 0x842, 
    0x742, 0x653, 0x503, 0x412, 0x350, 0x241, 0x111, 0x0
};

struct iso
{
    block3 aabb;
    vec scale;
    virtual double density(vec p) { return 0; };
};

struct sphere_iso : iso
{
    float r, rr;
    vec o;

    //-- radius in cubes, off in cubes (1,1,1)
    sphere_iso(selinfo &sel, float rad, vec off, vec scl)
    {
        r = rad;
        rr = r*sel.grid;
        scale = scl;
        
        sel.s.x = ceilf(r*2) + ceilf(off.x);
        sel.s.y = ceilf(r*2) + ceilf(off.y);
        sel.s.z = ceilf(r*2) + ceilf(off.z);

        o = vec(rr).add(off.mul(vec(sel.grid)));
        aabb = block3(sel);
    }

    double density(vec p)
    {
        vec rp = p.sub(o); //-- relative to origin
        return double((rp.x/scale.x)*(rp.x/scale.x) + (rp.y/scale.y)*(rp.y/scale.y) + (rp.z/scale.z)*(rp.z/scale.z)) - double(rr*rr);
    }
};

struct helix_iso : iso
{
    float r, rr, br, rbr;
    vec o;

    //-- radius in cubes, off in cubes (1,1,1)
    helix_iso(selinfo &sel, float or2, float ir)
    {
        r = ir;
        br = or2;
        rr = r*sel.grid;
        rbr = br*sel.grid;

        sel.s.x = ceilf((r+br)*2);
        sel.s.y = ceilf((r+br)*2);
        sel.s.z = ceilf((r+br)*2);

		o = vec(rr+rbr);
        aabb = block3(sel);
    }

    double density(vec p)
    {
        vec rp = p.sub(o); //-- relative to origin

        double z;
        if(rp.z>=rbr-rr && rp.y<0 && rp.x<0) z = rp.z - rbr*2 - (atan2(rp.y,rp.x)/PI)*rbr;
        else if(rp.z<=-(rbr-rr) && rp.y>=0 && rp.x<=0) z = rp.z + rbr*2 - (atan2(rp.y,rp.x)/PI)*rbr;
        else z = rp.z - (atan2(rp.y,rp.x)/PI)*rbr;
        
        double d = sqrt(rp.x*rp.x+rp.y*rp.y)-rbr;
        return double(d*d + z*z) - double(rr*rr);
    }
};

struct cylinder_iso : iso
{
    float r, rr, h, rh;
    vec o;

    //-- radius in cubes, off in cubes (1,1,1)
    cylinder_iso(selinfo &sel, float rad, float height, vec off)
    {
        r = rad;
        rr = r*sel.grid;
        h = height;
        rh = h*sel.grid;
        
        sel.s.x = ceilf(r*2) + ceilf(off.x);
        sel.s.y = ceilf(r*2) + ceilf(off.y);
        sel.s.z = ceilf(h) + ceilf(off.z);

		o = vec(rr).add(off.mul(vec(sel.grid))); // todo height (not must)
        aabb = block3(sel);
    }

    double density(vec p)
    {
        vec rp = p.sub(o); //-- relative to origin
        return double(rp.x*rp.x + rp.y*rp.y) - double(rr*rr);
    }
};


struct tunnel_iso : iso
{
    float r, rr, h, rh, ribr, ribl;
    vec o;

    //-- radius in cubes, off in cubes (1,1,1)
    tunnel_iso(selinfo &sel, float rad, float length, vec off)
    {
        r = rad;
        rr = r*sel.grid;
        h = length;
        rh = h*sel.grid;
        ribr = (r-1)*sel.grid;
        ribl = sel.grid;
        
        sel.s.x = ceilf(r*2) + ceilf(off.x) + 1;
        sel.s.y = ceilf(h) + ceilf(off.y);
        sel.s.z = ceilf(r*2) + ceilf(off.z);

		o = vec(rr).add(off.mul(vec(sel.grid))); // todo height (not must)
        aabb = block3(sel);
    }
    
    double density(vec p)
    {
        vec rp = p.sub(o); //-- relative to origin
        //return double(rp.x*rp.x + rp.y*rp.y) - double(rr*rr);
        //if (rp.z < (rh / 4) && rp.z > -(rh / 4))
        if(rp.y < ribl && rp.y > -ribl)
            return double(ribr*ribr) - double(rp.x*rp.x + (rp.z*2)*(rp.z*2));
        else return double(rr*rr) - double(rp.x*rp.x + (rp.z*2)*(rp.z*2));
    }
};


struct torus_iso : iso
{
    float a, b, c;  // a = major radius, b = minor radius
    vec o;

    //-- radius in cubes, off in cubes (1,1,1)
    torus_iso(selinfo &sel, float rada, float radb, vec off)
    {
        a = rada*sel.grid;
        b = radb*sel.grid;
        c = a - b;
        //scale = scl;
        
        sel.s.x = ceilf(rada*2) + ceilf(off.x);
        sel.s.y = ceilf(rada*2) + ceilf(off.y);
        sel.s.z = ceilf(radb*2) + ceilf(off.z);

        o = vec(a,a,b).add(off.mul(vec(sel.grid)));
        aabb = block3(sel);
    }

    double density(vec p)
    {
        vec rp = p.sub(o); //-- relative to origin
        double x = rp.x; double y = rp.y; double z = rp.z;
        double d = c - sqrt(x*x + y*y);
        return sqrt( d*d + z*z ) - b;
    }
};

struct pipebend_iso : iso
{
    float a, b, c;  // a = major radius, b = minor radius
    vec o;

    //-- radius in cubes, off in cubes (1,1,1)
    pipebend_iso(selinfo &sel, float rada, float radb, vec off)
    {
        a = rada*sel.grid;
        b = radb*sel.grid;
        c = a - b;
        //scale = scl;
        
        sel.s.x = ceilf(rada*2) + ceilf(off.x);
        sel.s.y = ceilf(rada*2) + ceilf(off.y);
        sel.s.z = ceilf(radb*2) + ceilf(off.z);

        o = vec(a,a,b).add(off.mul(vec(sel.grid)));
        aabb = block3(sel);
    }

    double density(vec p)
    {
        vec rp = p.sub(o); //-- relative to origin
        double ox = rp.x + c;
        double oy = rp.y + c;
        double x = rp.x; double y = rp.y; double z = rp.z;
        double d = c - sqrt(x*x + y*y);
        if(x<0 && y<0) return sqrt( d*d + z*z ) - b; // the bend
        else if(x>=0 && y<0) return double(oy*oy + rp.z*rp.z) - double(b*b); // positive x pipe
        else if(x<0 && y>=0) return double(ox*ox + rp.z*rp.z) - double(b*b); // positive y pipe
        else return 10;
    }
};


struct paraboloid_iso : iso
{
    float r, rr;
    vec o;

    //-- radius in cubes, off in cubes (1,1,1)
    paraboloid_iso(selinfo &sel, float rad, vec off)
    {
        r = rad;
        rr = r*sel.grid;
        
        sel.s.x = ceilf(r*2) + ceilf(off.x);
        sel.s.y = ceilf(r*2) + ceilf(off.y);
        sel.s.z = ceilf(r*2) + ceilf(off.z);
        
        o = vec(rr).add(off.mul(vec(sel.grid)));
        aabb = block3(sel);
    }

    double density(vec p)
    {
        vec rp = p.sub(o); //-- relative to origin
        return double(rp.x*rp.x + rp.z + rp.y*rp.y - rr);
    }
};

//maybe later to rotate too
struct octahedron_iso : iso
{
    float r, rr;
    vec o;

    //-- radius in cubes, off in cubes (1,1,1)
    octahedron_iso(selinfo &sel, float rad, vec off)
    {
        r = rad;
        rr = r*sel.grid;

		sel.s.x = ceilf(r*2) + ceilf(off.x);
        sel.s.y = ceilf(r*2) + ceilf(off.y);
        sel.s.z = ceilf(r*2) + ceilf(off.z);

		o = vec(rr).add(off.mul(vec(sel.grid)));
        aabb = block3(sel);
    }

    double density(vec p)
    {
        vec rp = p.sub(o); //-- relative to origin
        return double(fabs(rp.x)+fabs(rp.y)+fabs(rp.z)-rr);
    }
};



struct terrain_iso : iso
{
    float h, g, scale;
    Uint8 *pixels;
    int xsize, ysize, pitch, bpp;

    terrain_iso(selinfo &sel, SDL_Surface *surface, float height)
    {
        h = height*sel.grid; // Adjust actual height based on the selection granualarity.
        xsize = surface->w;  // Get the graphic dimensions
        ysize = surface->h;
        g = sel.grid;
        scale = h / 256;
        pixels = (Uint8 *) surface->pixels;
        pitch = surface->pitch;
        bpp = surface->format->BytesPerPixel;

		// Set the selection area. The x,y size will be one less than the size
        // of our provided graphic because each pixel represents a column corner, not
        // an entire cube column.
        sel.s.x = ceilf(xsize-1);
        sel.s.y = ceilf(ysize-1);

		// The Z height is whatever we provide. This allows us to scale the height map.
        sel.s.z = ceilf(height);
        aabb = block3(sel);
    }

    double density(vec p)
    {
        // locate the pixel coordinates in the heightmap graphic
        int xp = (int) (p.x / g);
        int yp = (int) (p.y / g);

        // get the x/y coordinates of the point we are checking, relative to the cube
        // column it is in (i.e. somewhere from 0 to 1).
        float x = (p.x / g) - ((float) xp);
        float y = (p.y / g) - ((float) yp);

        // get the height of the terrain at each corner of the cube column containing
        // our point, adusted for our provided height scale.
        float p1 = (float) pixels[yp * pitch + xp * bpp] * scale;
        float p2 = (float) pixels[yp * pitch + (xp+1) * bpp] * scale;
        float p3 = (float) pixels[(yp+1) * pitch + xp * bpp] * scale;
        float p4 = (float) pixels[(yp+1) * pitch + (xp+1) * bpp] * scale;

        // determin the terrain height at each column wall for the x/y coordiates.
        // Imagine a pair of perpendicular vertical planes positioned at coordinates
        // we are checking. We are locating the points where they intersect the top
        // of the terrain on each cube column wall by taking the distance weighted
        // average of the height at that wall's corners.
        float a = p1 * (1-x) + p2 * x;
        float b = p1 * (1-y) + p3 * y;
        float c = p2 * (1-y) + p4 * y;
        float d = p3 * (1-x) + p4 * x;

        // Now we find the terrain height at our x/y coordinates by taking a distance
        // weighted average of the side heights we just found. Finally, we subract that
        // value from the z depth of the point we are checking. If the result is
        // negative, then the terrain top is heigher, and the point we are checking is
        // within the geometry.
        return p.z - ((a * (1-y) + d * y) + (b * (1-x) + c * x)) / 2;
    }
};


struct iso_point
{
    double d;
    vec p;
    
    iso_point() {};
    iso_point(vec point) { p = point; }
    iso_point(double dens) { d = dens; }
    iso_point(double dens,vec point) { d = dens; p = point; }
};


// the corners of one cube and the surface the Shape makes through it
struct march_cube
{
    iso_point corners[8];
    unsigned int cubeindex;
    uint cc[12];

    march_cube() : cubeindex(0) { memset(cc, 0, sizeof(cc)); }

    // reads the corners of the cube of size r at o from the Shape, bit 0 of a corner's index being x, bit 1 y and bit 2 z;
    // inverted, the solid side is outside the Shape, which is what is left of geometry it carves
    void sample(iso &ig, const vec &o, int r, bool invert = false)
    {
        loopi(8)
        {
            corners[i].p = vec(i&1 ? r : 0, i&2 ? r : 0, i&4 ? r : 0).add(o);
            corners[i].d = ig.density(corners[i].p);
            if(invert) corners[i].d = -corners[i].d;
        }
        init_cube_index();
    }

    void init_cube_index()
    {
        cubeindex = 0;
        loopi(8) if(corners[i].d < 0) cubeindex |= 1<<i;
    }

    void init_cube_cut()
    {
        if(edgeTable[cubeindex] & 1) cc[0] = calc_bendvalue(corners[0], corners[1]);
        if(edgeTable[cubeindex] & 2) cc[1] = calc_bendvalue(corners[2], corners[3]);
        if(edgeTable[cubeindex] & 4) cc[2] = calc_bendvalue(corners[4], corners[5]);
        if(edgeTable[cubeindex] & 8) cc[3] = calc_bendvalue(corners[6], corners[7]);
        if(edgeTable[cubeindex] & 16) cc[4] = calc_bendvalue(corners[0], corners[2]);
        if(edgeTable[cubeindex] & 32) cc[5] = calc_bendvalue(corners[4], corners[6]);
        if(edgeTable[cubeindex] & 64) cc[6] = calc_bendvalue(corners[1], corners[3]);
        if(edgeTable[cubeindex] & 128) cc[7] = calc_bendvalue(corners[5], corners[7]);
        if(edgeTable[cubeindex] & 256) cc[8] = calc_bendvalue(corners[0], corners[4]);
        if(edgeTable[cubeindex] & 512) cc[9] = calc_bendvalue(corners[1], corners[5]);
        if(edgeTable[cubeindex] & 1024) cc[10] = calc_bendvalue(corners[2], corners[6]);
        if(edgeTable[cubeindex] & 2048) cc[11] = calc_bendvalue(corners[3], corners[7]);
    }

    // sets c's faces to the surface through the corners, textured with tex, or keeping c's textures when tex is negative
    void render(cube &c, int tex = DEFAULT_GEOM)
    {
        if(!cubeindex) { emptyfaces(c); return; }
        if(tex >= 0) loopi(6) c.texture[i] = tex;
        solidfaces(c);
        if(cubeindex == 255) return;
        init_cube_cut();
        for(int i = 0; i < 24; i += 2)
        {
            if(bendTable[cubeindex][i]==8 || bendTable[cubeindex][i]==0)
                c.edges[int(i/2)] = 0x10*bendTable[cubeindex][i];
            else c.edges[int(i/2)] = 0x10*cc[int(i/8)*4+(bendTable[cubeindex][i]-1)];

            if(bendTable[cubeindex][i+1]==8 || bendTable[cubeindex][i+1]==0)
                c.edges[int(i/2)] += 0x01*bendTable[cubeindex][i+1];
            else c.edges[int(i/2)] += 0x01*cc[int(i/8)*4+(bendTable[cubeindex][i+1]-1)];
        }
    }

    static unsigned int calc_bendvalue(const iso_point &p1, const iso_point &p2)
    {
        if(fabs(p1.d) < 0.00001) return 0;
        if(fabs(p2.d) < 0.00001) return 8;
        if(fabs(p1.d-p2.d) < 0.00001) return 0;
        double mu = -p1.d / (p2.d-p1.d);
        return (unsigned int)((mu*8)+0.5);
    }
};

// whether c and every cube under it are empty
static bool allempty(const cube &c)
{
    if(!c.children) return isempty(c);
    loopi(8) if(!allempty(c.children[i])) return false;
    return true;
}

// empties c and every cube under it, keeping its material
static void clearcube(cube &c)
{
    ushort mat = c.material;
    discardchildren(c);
    c.material = mat;
    emptyfaces(c);
}

// adds the Shape to cube c of size r at o, or carves it out, subdividing c up to step more times where the surface crosses it;
// what is already there outside the Shape is kept, and faces the Shape fills from nothing get texture tex
static void marchshape(cube &c, const vec &o, int r, iso &ig, int step, bool carve, int tex)
{
    march_cube mc;
    mc.sample(ig, o, r, carve);
    if(carve)
    {
        if(mc.cubeindex == 255 || allempty(c)) return;
        if(!mc.cubeindex) { clearcube(c); return; }
    }
    else
    {
        if(!mc.cubeindex) return;
        if(mc.cubeindex == 255)
        {
            bool wasempty = allempty(c);
            ushort mat = c.material;
            discardchildren(c, true);
            c.material = mat;
            solidfaces(c);
            if(wasempty) loopi(6) c.texture[i] = tex;
            return;
        }
    }
    // the surface crosses this cube: go down to the detail asked for, or into geometry that is already finer
    if(c.children || (r > 1 && step > 0))
    {
        bool made = !c.children;
        if(made && !subdividecube(c, false, false)) return;
        int h = r/2;
        loopi(8) marchshape(c.children[i], vec(i&1 ? h : 0, i&2 ? h : 0, i&4 ? h : 0).add(o), h, ig, max(step-1, 0), carve, tex);
        // children made here that the Shape left empty are dropped again
        if(made && allempty(c)) clearcube(c);
        return;
    }
    // the leaf the surface crosses: unite (add) or intersect (carve) each edge's solid span with the Shape's, which is exact for
    // solid and empty cubes and close for partial ones
    cube t;
    memset(&t, 0, sizeof(t));
    mc.render(t);
    bool wasempty = isempty(c);
    uint bak[3] = { c.faces[0], c.faces[1], c.faces[2] };
    loopi(12)
    {
        int as = edgeget(c.edges[i], 0), ae = edgeget(c.edges[i], 1), bs = edgeget(t.edges[i], 0), be = edgeget(t.edges[i], 1), s, e;
        // a collapsed span (start at or past its end) still places its corners, so it is kept where it is rather than moved
        bool aempty = as >= ae, bempty = bs >= be;
        if(carve)
        {
            if(aempty) { s = as; e = ae; }
            else if(bempty) { s = bs; e = be; }
            else { s = max(as, bs); e = min(ae, be); if(s > e) s = e; }
        }
        else if(aempty) { s = bs; e = be; }
        else if(bempty) { s = as; e = ae; }
        else { s = min(as, bs); e = max(ae, be); }
        c.edges[i] = edgemake(s, e);
    }
    // a dimension whose spans have all collapsed has nothing between its faces
    loopi(3)
    {
        bool solid = false;
        loopj(4) if(edgeget(c.edges[i*4+j], 0) < edgeget(c.edges[i*4+j], 1)) solid = true;
        if(!solid) { emptyfaces(c); break; }
    }
    if(!isempty(c) && !isvalidcube(c))
    {
        // the spans don't make a convex cube: adding keeps what was there, carving keeps it only when it is mostly outside the Shape
        loopi(3) c.faces[i] = bak[i];
        if(carve && POPCOUNT(mc.cubeindex) < 4) emptyfaces(c);
    }
    if(wasempty && !isempty(c)) loopi(6) c.texture[i] = tex;
}
