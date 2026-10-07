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

//-- not used just for debug
static unsigned int cube_index_cases[256] = {
    0,1,1,3,1,3,9,11,
    1,9,3,11,3,11,11,15,
    1,3,9,11,9,11,41,43,
    129,131,131,139,131,139,75,31,
    1,9,3,11,129,131,131,139,
    9,41,11,43,131,75,139,31,
    3,11,11,15,131,139,75,31,
    131,75,139,31,195,91,91,63,
    1,9,129,131,3,11,131,139,
    9,41,131,75,11,43,139,31,
    3,11,131,139,11,15,75,31,
    131,75,195,91,139,31,91,63,
    9,41,131,75,131,75,195,91,
    41,105,75,107,75,107,91,159,
    11,43,139,31,139,31,91,63,
    75,107,91,159,91,159,219,191,
    1,129,9,131,9,131,41,75,
    3,131,11,139,11,139,43,31,
    9,131,41,75,41,75,105,107,
    131,195,75,91,75,91,107,159,
    3,131,11,139,131,195,75,91,
    11,75,15,31,139,91,31,63,
    11,139,43,31,75,91,107,159,
    139,91,31,63,91,219,159,191,
    3,131,131,195,11,139,75,91,
    11,75,139,91,15,31,31,63,
    11,139,75,91,43,31,107,159,
    139,91,91,219,31,63,159,191,
    11,75,139,91,139,91,91,219,
    43,107,31,159,31,159,63,191,
    15,31,31,63,31,63,159,191,
    31,159,63,191,63,191,191,255
};
static unsigned int cube_index_base_cases[22] = {0,1,3,9,129,11,131,41,15,43,75,139,195,105,31,107,91,63,159,219,191,255};
//--not used just for debug, END

struct shape_box
{
    ivec orig, end;
    
    shape_box(ivec o, ivec e) { orig = o; end = e; }

    int collide(ivec o, ivec e)
    {
        if(o.x > end.x || e.x < orig.x || o.y > end.y || e.y < orig.y || o.z > end.z || e.z < orig.z )
            return 0;

        return 1;
    }
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

int start_mcz = 0;
double mcz = 0;

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
        mcz = 0;
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


struct march_cube
{
    iso_point corners[8];
    unsigned int cubeindex;
    cube* c;
    vec o;
    int r, st;
    iso* ig;
    uint cc[12];

    march_cube() {};
    march_cube(cube &cu,int x,int y,int z, int rgrid, iso* iso_geom, int step) { march_cube(cu,vec(x*rgrid,y*rgrid,z*rgrid), rgrid, iso_geom, step); }
    march_cube(cube &cu,vec origin, int rgrid, iso* iso_geom, int step)
    {
        c = &cu;
        ig = iso_geom;
        r = rgrid;
        st = r == 1 ? 0 : step;
        o = origin;
        init_corners();
        init_cube_index();
        //-- divide or draw
        if(divisible()) divide();
        else render();
    }
    
	//-- only one cubeindex cube
    march_cube(cube &cu, unsigned int ci)
    {
        c = &cu;
        cubeindex = ci;

        //-- render ci
        for(int i = 0; i < 24; i += 2)
        {
            if(bendTable[cubeindex][i]==8 || bendTable[cubeindex][i]==0)
                c->edges[int(i/2)] = 0x10*bendTable[cubeindex][i];
            else c->edges[int(i/2)] = 0x10*3;

            if(bendTable[cubeindex][i+1]==8 || bendTable[cubeindex][i+1]==0)
                c->edges[int(i/2)] += 0x01*bendTable[cubeindex][i+1];
            else c->edges[int(i/2)] += 0x01*3;
        }
    }
    
	//-- cube with density and inside outside corners
    march_cube(cube &cu)
    {
        c = &cu;
    }
    
    void init_corners()
    {
        corners[0] = iso_point(o);
        corners[1] = iso_point(vec(r,0,0).add(o));
        corners[2] = iso_point(vec(0,r,0).add(o));
        corners[3] = iso_point(vec(r,r,0).add(o));
        corners[4] = iso_point(vec(0,0,r).add(o));
        corners[5] = iso_point(vec(r,0,r).add(o));
        corners[6] = iso_point(vec(0,r,r).add(o));
        corners[7] = iso_point(vec(r,r,r).add(o));
        loopi(8){ corners[i].d = ig->density(corners[i].p); }
    }
    
    void init_cube_index()
    {
        cubeindex = 0;
        if(corners[0].d < 0) cubeindex |= 1;
        if(corners[1].d < 0) cubeindex |= 2;
        if(corners[2].d < 0) cubeindex |= 4;
        if(corners[3].d < 0) cubeindex |= 8;
        if(corners[4].d < 0) cubeindex |= 16;
        if(corners[5].d < 0) cubeindex |= 32;
        if(corners[6].d < 0) cubeindex |= 64;
        if(corners[7].d < 0) cubeindex |= 128;
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
    
    void divide()
    {
        int rh = r/2;
        int ns = st-1;
        c->children = newcubes();
        
        loopi(8)
        {
            ivec v(o.x + ((i&1)>>0) * rh, o.y + ((i&2)>>1) * rh, o.z + ((i&4)>>2) * rh);
            vec tempvec = vec(v.x, v.y, v.z);
            march_cube(c->children[i], tempvec, rh, ig, ns);
        }
    }
    
    void render()
    {
        if(cube_in_cut())
        {
            loopi(6) c->texture[i] = DEFAULT_GEOM;
            
            if(cube_in()) { solidfaces(*c); }
            else
            {
                solidfaces(*c);
                init_cube_cut();
                
                for(int i = 0; i < 24; i += 2)
                {
                    if(bendTable[cubeindex][i]==8 || bendTable[cubeindex][i]==0)
                        c->edges[int(i/2)] = 0x10*bendTable[cubeindex][i];
                    else c->edges[int(i/2)] = 0x10*cc[int(i/8)*4+(bendTable[cubeindex][i]-1)];

                    if(bendTable[cubeindex][i+1]==8 || bendTable[cubeindex][i+1]==0)
                        c->edges[int(i/2)] += 0x01*bendTable[cubeindex][i+1];
                    else c->edges[int(i/2)] += 0x01*cc[int(i/8)*4+(bendTable[cubeindex][i+1]-1)];
                }
            }
        }
        else { emptyfaces(*c); }
    }
    
    unsigned int calc_bendvalue(iso_point p1, iso_point p2)
    {
        if(fabs(p1.d) < 0.00001) return 0;
        if(fabs(p2.d) < 0.00001) return 8;
        if(fabs(p1.d-p2.d) < 0.00001) return 0;
        double mu = -p1.d / (p2.d-p1.d);
        return (unsigned int)((mu*8)+0.5);
    }
    
    bool divisible()
    {
        return (st>0 && !cube_out() && !cube_in());
    }

    bool cube_in_cut()
    {
        if(cubeindex==0) return false;
        return true;
    }
    
    bool cube_in()
    {
        if(cubeindex==255) return true;
        return false;
    }
    
    bool cube_out()
    {
        if(cubeindex==0) return true;
        return false;
    }
};
