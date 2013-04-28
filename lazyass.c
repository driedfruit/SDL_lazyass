#include "lazyass.h"
#include "hash/hash.h"

static hash_t textures;
static hash_t surfaces;
static hash_t sounds;

static SDL_Renderer *use_renderer = NULL;

void ASS_Init(
SDL_Renderer *use
) {
	use_renderer = use;
	hash_init(&textures, 128);
	hash_init(&surfaces, 128);
	hash_init(&sounds, 128);
}

void hash_release_textures(hash_t *tree) {
	int i;
	if (tree->tab)
		for (i = 0; i < tree->span; i++)
			hash_release_textures((hash_t*)(tree->tab + i));
	SDL_DestroyTexture((SDL_Texture*)tree->user);
}

void hash_release_surfaces(hash_t *tree) {
	int i;
	if (tree->tab)
		for (i = 0; i < tree->span; i++)
			hash_release_surfaces((hash_t*)(tree->tab + i));
	SDL_FreeSurface((SDL_Surface*)tree->user);
}

void hash_release_sounds(hash_t *tree) {
	int i;
	if (tree->tab)
		for (i = 0; i < tree->span; i++)
			hash_release_sounds((hash_t*)(tree->tab + i));
	//TODO: cleanup something
}

void ASS_Quit() {
    hash_release_textures(&textures);
    hash_release_surfaces(&surfaces);
    hash_done(&textures);
    hash_done(&surfaces);
    hash_done(&sounds);
}

SDL_Surface *ASS_LoadSurface(const char *filename) {
	SDL_Surface *ret = NULL;

	/* see if it's already loaded */
	ret = hash_get(&surfaces, filename);
	/* nope, must load it */
	if (ret == NULL) {
#ifdef HAVE_SDLIMAGE
		ret = IMG_Load(filename);
#else
		ret = SDL_LoadBMP(filename);
#endif
		/* and cache */
		if (ret != NULL) hash_set(&surfaces, filename, ret);
	}
	return ret;
}

void ASS_FreeSurface(SDL_Surface *surface) {

}

SDL_Texture *ASS_LoadTexture(const char *filename, SDL_Color *colorkey) {
	SDL_Texture *ret = NULL;
	SDL_Surface *tmp = NULL;
	SDL_Surface *tmp32 = NULL;
	int i;

	/* see if it's already loaded */
	ret = hash_get(&textures, filename);
	/* nope, must load it */
	if (ret == NULL) {
#ifdef HAVE_SDLIMAGE
		tmp = IMG_Load(filename);
#else
		tmp = SDL_LoadBMP(filename);
#endif
	if (tmp == NULL) return NULL; /* error */

	if (colorkey != NULL) {
		Uint32 rmask, gmask, bmask, amask;
		#if SDL_BYTEORDER == SDL_BIG_ENDIAN
		rmask = 0xff000000;
		gmask = 0x00ff0000;
		bmask = 0x0000ff00;
		amask = 0x000000ff;
		#else
		rmask = 0x000000ff;
		gmask = 0x0000ff00;
		bmask = 0x00ff0000;
		amask = 0xff000000;
		#endif
		tmp32 = SDL_CreateRGBSurface(0, tmp->w, tmp->h, 32, rmask, gmask, bmask, amask);
		SDL_FillRect(tmp32, NULL, 0);
		SDL_SetColorKey(tmp, SDL_TRUE, SDL_MapRGB(tmp->format, colorkey->r, colorkey->g, colorkey->b));
		for (i = 0; i < tmp->format->palette->ncolors; i++)
			tmp->format->palette->colors[i].a = 0xFF;
			SDL_BlitSurface(tmp, NULL, tmp32, NULL);
		} else {
			tmp32 = tmp;
		}
		ret = SDL_CreateTextureFromSurface(use_renderer, tmp32);

		if (tmp32 != tmp) SDL_FreeSurface(tmp32); 
		SDL_FreeSurface(tmp);

		/* and cache */
		if (ret != NULL) hash_set(&textures, filename, ret);
	}
	return ret;
}

void ASS_FreeTexture(SDL_Texture *texture) {

}
