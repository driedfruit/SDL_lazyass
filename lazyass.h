#ifndef _LAZYASS_H
#define _LAZYASS_H
/* Lazy SDL Asset "Manager" aka LazyAss */

#include <SDL.h>

#ifdef HAVE_SDLMIXER
#include <SDL2/SDL_mixer.h>
typedef Mix_Chunk ASS_Sound;
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

extern Uint32 ASS_GetProgress(Uint64 *left, Uint64 *total);

extern SDL_Texture *ASS_LoadTexture(const char *filename, SDL_Color *colorkey);
extern int ASS_LoadTextureTO(SDL_Texture **dst, const char *filename, SDL_Color *colorkey);
extern void ASS_FreeTexture(SDL_Texture *texture);

extern SDL_Surface *ASS_LoadSurface(const char *filename);
extern int ASS_LoadSurfaceTO(SDL_Surface **dst, const char *filename);
extern void ASS_FreeSurface(SDL_Surface *surface);

extern ASS_Sound *ASS_LoadSound(const char *filename);
extern int ASS_LoadSoundTO(ASS_Sound **dst, const char *filename);
extern void ASS_FreeSound(ASS_Sound *wav);

#endif
