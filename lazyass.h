#ifndef _LAZYASS_H
#define _LAZYASS_H
/* Lazy SDL Asset "Manager" aka LazyAss */

#include <SDL.h>

#ifdef HAVE_SDLMIXER
typedef MIX_WaveChunk ASS_Sound
#else
typedef struct ASS_Sound {

    SDL_AudioSpec format;
    Uint32 len;
    Uint8 *buffer;

    Uint32 last_sample;

} ASS_Sound;
#endif

extern void ASS_Init(
SDL_Renderer *use
);
extern void ASS_Quit();

extern SDL_Texture *ASS_LoadTexture(const char *filename, SDL_Color *colorkey);
extern void ASS_FreeTexture(SDL_Texture *texture);

extern SDL_Surface *ASS_LoadSurface(const char *filename);
extern void ASS_FreeSurface(SDL_Surface *surface);

extern ASS_Sound *ASS_LoadSound(const char *filename);
extern void ASS_FreeSound(ASS_Sound *wav);

#endif