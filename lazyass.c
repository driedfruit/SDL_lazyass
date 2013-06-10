#include "lazyass.h"
#include "hash/hash.h"

static hash_t textures;
static hash_t surfaces;
static hash_t sounds;

static SDL_Renderer *use_renderer = NULL;
static SDL_Thread *loader_thread = NULL;
static SDL_mutex *queue_mutex = NULL;

ASS_Sound *ASS_LoadSound_RW(SDL_RWops *ops);
SDL_Surface *ASS_LoadSurface_RW(SDL_RWops *ops);
SDL_Texture *ASS_LoadTexture_RW(SDL_RWops *ops, SDL_Color *colorkey);

#define ASStype_SOUND	0
#define ASStype_SURFACE	1
#define ASStype_TEXTURE	2

typedef struct queue_item_t queue_item_t;

struct queue_item_t {
	int type;
	union {
		SDL_Texture *texture;
		SDL_Surface *surface;
		ASS_Sound *sound;
	} r;
	void **setto;
	char *filename;
	SDL_RWops *ops;
	void *userdata;
	Uint64 bytes_total;
	Uint64 bytes_read;
	queue_item_t *next;
};

typedef struct queue_t {
	queue_item_t *root;
	Uint32 files_left;
	Uint64 bytes_left;
	Uint64 bytes_total;
	int work;
} queue_t;

queue_t queue = { NULL, 0, 0, 0, 1 };

queue_item_t* queue_peek() {
	queue_item_t *ret;

	if (SDL_mutexP(queue_mutex) == -1) {
		fprintf(stderr, "Couldn't lock mutex\n");
		exit(-1);
	}

	ret = queue.root;

	if (SDL_mutexV(queue_mutex) == -1) {
		fprintf(stderr, "Couldn't unlock mutex\n");
		exit(-1);
	}

	return ret;
}

queue_item_t* queue_pop() {
	queue_item_t *ret;

	if (SDL_mutexP(queue_mutex) == -1) {
		fprintf(stderr, "Couldn't lock mutex\n");
		exit(-1);
	}

	ret = queue.root;
	
	if (ret != NULL) {
		queue.root = queue.root->next; 
	}

	if (SDL_mutexV(queue_mutex) == -1) {
		fprintf(stderr, "Couldn't unlock mutex\n");
		exit(-1);
	}

	return ret;
}

void queue_free_item(queue_item_t* item) {
	if (item->ops != NULL) SDL_RWclose(item->ops);
	if (item->filename != NULL) free(item->filename);
	free(item);
}

queue_item_t* queue_item(const char *filename, int type, void* param, void** dst) {
	queue_item_t *item;
	item = malloc(sizeof(queue_item_t));
	if (item != NULL) {
		item->ops = NULL;
		item->filename = NULL;
		item->ops = SDL_RWFromFile(filename, "rb");
		item->filename = strdup(filename);
		if (item->ops == NULL || item->filename == NULL) {
			fprintf(stderr, "Unable to open file `%s`: %s\n", filename, SDL_GetError());
			queue_free_item(item);
			item = NULL;
		} else {
			item->userdata = param;
			item->type = type;
			item->r.texture = NULL;
			item->r.surface = NULL;
			item->r.sound = NULL;
			item->bytes_total = SDL_RWsize(item->ops);
			item->bytes_read = 0;
			item->next = NULL;
			item->setto = dst;
		}

	}
	return item;
}

queue_item_t* queue_push(const char *filename, int type, void* param, void** dst) {
	queue_item_t *item;

	if (SDL_mutexP(queue_mutex) == -1) {
		fprintf(stderr, "Couldn't lock mutex\n");
		exit(-1);
	}

	item = queue_item(filename, type, param, dst);
	if (queue.root == NULL) queue.root = item;
	else {
		queue_item_t *last = queue.root;
		while (last->next) last = last->next;
		last->next = item;
	}
	queue.files_left += 1;
	queue.bytes_total += item->bytes_total;
	queue.bytes_left += item->bytes_total; 

	if (SDL_mutexV(queue_mutex) == -1) {
		fprintf(stderr, "Couldn't unlock mutex\n");
		exit(-1);
	}

	return item;
}

Uint32 ASS_GetProgress(Uint64 *write_left, Uint64 *write_total) {
	queue_item_t *item;

	if (SDL_mutexP(queue_mutex) == -1) {
		fprintf(stderr, "Couldn't lock mutex\n");
		exit(-1);
	}

	item = queue.root;
	queue.bytes_left = 0;
	queue.bytes_total = 0;
	queue.files_left = 0;

	while (item) {
		if (item->ops) item->bytes_read = SDL_RWtell(item->ops);
		queue.files_left += 1;
		queue.bytes_total += item->bytes_total;
		queue.bytes_left += (item->bytes_total - item->bytes_read);
		item = item->next;
	}

	if (write_total != NULL) {
		*write_total = queue.bytes_total;
	}
	if (write_left != NULL) {
		*write_left = queue.bytes_left;
	}

	if (SDL_mutexV(queue_mutex) == -1) {
		fprintf(stderr, "Couldn't unlock mutex\n");
		exit(-1);
	}

	return queue.files_left;
}

void ASS_LoadOne(queue_item_t *item) {
	switch (item->type) {
		case ASStype_SOUND:
			item->r.sound = hash_get(&sounds, item->filename);
			if (item->r.sound == NULL) {
				item->r.sound = ASS_LoadSound_RW(item->ops);
				if (item->r.sound != NULL) {
					hash_set(&sounds, item->filename, item->r.sound);
				} else fprintf(stderr, "Failed to load `%s`\n", item->filename);
			}
			*item->setto = (void*)item->r.sound;
		break;
		case ASStype_SURFACE:
			item->r.surface = hash_get(&surfaces, item->filename);
			if (item->r.surface == NULL) {
				item->r.surface = ASS_LoadSurface_RW(item->ops);
				if (item->r.surface != NULL) {
					hash_set(&surfaces, item->filename, item->r.surface);
				} else fprintf(stderr, "Failed to load `%s`\n", item->filename);
			}
			*item->setto = (void*)item->r.surface;
		break;
		case ASStype_TEXTURE:
		default:
			item->r.texture = hash_get(&textures, item->filename);
			if (item->r.surface == NULL) {
				item->r.texture = ASS_LoadTexture_RW(item->ops, item->userdata);
				if (item->r.texture != NULL) {
					hash_set(&textures, item->filename, item->r.texture);
				} else fprintf(stderr, "Failed to load `%s`\n", item->filename);
			}
			*item->setto = (void*)item->r.texture;
		break;
	}
}

int ASS_Thread_Load(void *ptr) {

	while (queue.work) {

		queue_item_t *item = queue_peek();
		if (item == NULL) {
			SDL_Delay(10);
			continue;
		}
		ASS_LoadOne(item);
		queue_free_item( queue_pop() );
	}

	return 0;
}

void ASS_Init(
SDL_Renderer *use
) {
	queue_mutex = SDL_CreateMutex();
	loader_thread = SDL_CreateThread(ASS_Thread_Load, "assload", NULL);
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

hash_t *hash_find_val(hash_t *tree, void* val) {
	int i;
	hash_t *found = NULL;
	if (tree->user == val) return tree;
	if (tree->tab) {
		for (i = 0; i < tree->span; i++) {
			found = hash_find_val((hash_t*)(tree->tab + i), val);
			if (found) break;
		}
	}
	return found;
}

void ASS_Quit() {
	queue.work = 0;
	SDL_WaitThread(loader_thread, NULL);
	SDL_DestroyMutex(queue_mutex);
	hash_release_textures(&textures);
	hash_release_surfaces(&surfaces);
	hash_done(&textures);
	hash_done(&surfaces);
	hash_done(&sounds);
}

ASS_Sound *ASS_LoadSound(const char *filename) {
	ASS_Sound *ret = NULL;
	queue_item_t *item = NULL;
	/* see if it's already loaded */
	ret = hash_get(&sounds, filename);
	/* nope, must load it */
	if (ret == NULL) {
		item = queue_item(filename, ASStype_SOUND, NULL, (void**)&ret);
		if (item != NULL) {
			ASS_LoadOne(item);
			queue_free_item(item);
		}
	}
	return ret;
}
int ASS_LoadSoundTO(ASS_Sound **dst, const char *filename) {
	queue_item_t *item = NULL;
	/* queue it */
	item = queue_push(filename, ASStype_SOUND, NULL, (void**)dst);
	if (item == NULL) return -1;
	return (item->bytes_total - item->bytes_read);
}
ASS_Sound *ASS_LoadSound_RW(SDL_RWops *ops) {
	ASS_Sound *ret;
#ifdef HAVE_SDLMIXER
	ret = Mix_LoadWAV_RW(ops, 0);
	if (ret == NULL) {
		fprintf(stderr, "Could not read: %s\n", Mix_GetError());
	}
#else
	ret = malloc(sizeof(ASS_Sound));
	if (ret == NULL) return NULL;
	ret->last_sample = 0;

	if (SDL_LoadWAV_RW(ops, 0, &ret->format, &ret->buffer, &ret->len) == NULL) {
		fprintf(stderr, "Could not read: %s\n", SDL_GetError());
		free(ret);
		ret = NULL;
	}

#endif
	return ret;
}
void ASS_FreeSound(ASS_Sound *sound) {
	hash_t *entry = hash_find_val(&textures, sound);
	if (entry != NULL) {
		entry->key = 0;
		entry->user = NULL;
		//todo: something that frees sound;
	}
}

SDL_Surface *ASS_LoadSurface(const char *filename) {
	SDL_Surface *ret = NULL;
	queue_item_t *item = NULL;
	/* see if it's already loaded */
	ret = hash_get(&surfaces, filename);
	/* nope, must load it */
	if (ret == NULL) {
		item = queue_item(filename, ASStype_SURFACE, NULL, (void**)&ret);
		if (item != NULL) {
			ASS_LoadOne(item);
			queue_free_item(item);
		}
	}
	return ret;
}
int ASS_LoadSurfaceTO(SDL_Surface **dst, const char *filename) {
	queue_item_t *item = NULL;
	/* queue it */
	item = queue_push(filename, ASStype_SURFACE, NULL, (void**)dst);
	if (item == NULL) return -1;
	return (item->bytes_total - item->bytes_read);
}
SDL_Surface *ASS_LoadSurface_RW(SDL_RWops *ops) {
	SDL_Surface *ret = NULL;

#ifdef HAVE_SDLIMAGE
	ret = IMG_Load_RW(ops, 0);
#else
	ret = SDL_LoadBMP_RW(ops, 0);
#endif

	return ret;
}
void ASS_FreeSurface(SDL_Surface *surface) {
	hash_t *entry = hash_find_val(&surfaces, surface);
	if (entry != NULL) {
		entry->key = 0;
		entry->user = NULL;
		SDL_FreeSurface(surface);
	}
}

SDL_Texture *ASS_LoadTexture(const char *filename, SDL_Color *colorkey) {
	SDL_Texture *ret = NULL;
	queue_item_t *item = NULL;
	/* see if it's already loaded */
	ret = hash_get(&textures, filename);
	/* nope, must load it */
	if (ret == NULL) {
		item = queue_item(filename, ASStype_TEXTURE, (void*)colorkey, (void**)&ret);
		if (item != NULL) {
			ASS_LoadOne(item);
			queue_free_item(item);
		}
	}
	return ret;
}
int ASS_LoadTextureTO(SDL_Texture **dst, const char *filename, SDL_Color *colorkey) {
	queue_item_t *item = NULL;
	/* queue it */
	item = queue_push(filename, ASStype_TEXTURE, (void*)colorkey, (void**)dst);
	if (item == NULL) return -1;
	return (item->bytes_total - item->bytes_read);
}
SDL_Texture *ASS_LoadTexture_RW(SDL_RWops *ops, SDL_Color *colorkey) {
	SDL_Texture *ret = NULL;
	SDL_Surface *tmp = NULL;
	SDL_Surface *tmp32 = NULL;
	int i;

	if (ret == NULL) {
#ifdef HAVE_SDLIMAGE
		tmp = IMG_Load_RW(ops, 0);
#else
		tmp = SDL_LoadBMP_RW(ops, 0);
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
	}
	return ret;
}
void ASS_FreeTexture(SDL_Texture *texture) {
	hash_t *entry = hash_find_val(&textures, texture);
	if (entry != NULL) {
		entry->key = 0;
		entry->user = NULL;
		SDL_DestroyTexture(texture);
	}
}
