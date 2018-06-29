#include "SDL.h"   /* All SDL App's need this */

#define VDP_CLOCK 53200000

/* Globals */

extern SDL_Surface *sdl_screen;
extern Uint32 sdl_colours[16];

int sound_init(void);
void sdl_setpalette(void);
int init_sdl(void);
Uint32 sdl_getticks(void);

