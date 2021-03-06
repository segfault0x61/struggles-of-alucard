#include "room.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game.h"
#include "particles.h"
#include "sound.h"
#include "sprite.h"

#define ROOM_WIDTH 26
#define ROOM_HEIGHT 15

enum {
	TILE_AIR,
	TILE_BLOOD,
	TILE_SOLID = 0x100,
	TILE_WALL  = TILE_SOLID,
	TILE_SPIKE = TILE_SOLID + 1,
};

enum {
	TILE_TEX_MODE_NONE,
	TILE_TEX_ADJACENCY,
	TILE_TEX_ANIMATE,
	TILE_TEX_STATIC = 0x1000,
};

typedef struct {
    unsigned int color;
    const char* texture;
    int collision_type;
    int collision_response;
    int tex_mode;
} Tile;

Tile tile_desc[] = {
	[TILE_AIR]   = {
		.color          = 0xff,
		.collision_type = COLLISION_NONE
	},

	[TILE_WALL]  = {
		.texture            = "res/sprites/walls.png",
		.collision_type     = COLLISION_BOX,
		.collision_response = CRESP_BLOCK,
		.tex_mode           = TILE_TEX_ADJACENCY,
	},

	[TILE_SPIKE] = {
		.texture            = "res/sprites/spikes.png",
		.collision_type     = COLLISION_BOX,
		.collision_response = CRESP_KILL,
		.tex_mode           = TILE_TEX_ADJACENCY,
	},

	[TILE_BLOOD] = {
		.texture            = "res/sprites/blood.png",
		.collision_type     = COLLISION_BOX,
		.collision_response = CRESP_POWERUP,
		.tex_mode           = TILE_TEX_ANIMATE,
	}
};

static int current_room;
static int tile_types[ROOM_WIDTH * ROOM_HEIGHT];
static int sprite_offset;

static SDL_Point current_room_spawn;

typedef struct {
    int neighbours[4];
} RoomMap;

static RoomMap room_map[256];

void room_init(void) {
    FILE* f = fopen("res/maps/map.txt", "r");

    // XXX: first line is ignored...
    char line[256];
    fgets(line, sizeof(line), f);

    int id;
    int n[4];
    while (fscanf(f, "%d: %d %d %d %d\n", &id, n, n + 1, n + 2, n + 3) == 5) {
        SDL_assert(id < array_count(room_map));
        memcpy(room_map[id].neighbours, n, sizeof(n));
    }
}

void room_load(int number) {
    printf("Loading room %d\n", number);

    // TODO: error message if map is not the correct format

    // already a room loaded
    if (sprite_offset) {
        current_room = 0;
        memset(tile_types, 0, sizeof(tile_types));
        sprite_pop(sprites + sprite_offset, ROOM_WIDTH * ROOM_HEIGHT);
        sprite_offset = num_sprites;
    }

    current_room_spawn.x = (WIN_WIDTH / 2);
    current_room_spawn.y = (WIN_HEIGHT / 2);

    char* filename;
    asprintf(&filename, "res/maps/room%d.txt", number);
    FILE* f = fopen(filename, "r");

    SDL_assert(f);

    current_room = number;

    free(filename);

    char line[256];

    int x = 0, y = 0;
    int i = 0;

    int tile_type = 0;

    sprite_offset = num_sprites;

    while (fgets(line, sizeof(line), f)) {
        for (const char* c = line; *c; ++c) {
            if (*c == '\n') continue;

            switch (*c) {
                case '#': {
                    tile_type = TILE_WALL;
                } break;

                case 'x': {
                    tile_type = TILE_SPIKE;
                } break;

                case 'b': {
                    tile_type = TILE_BLOOD;
                } break;

                case 'c': {
                    tile_type = TILE_AIR;
                    current_room_spawn.x = x;
                    current_room_spawn.y = y;
                } break;

                case '.':
                default: {
                    tile_type = TILE_AIR;
                } break;
            }

            tile_types[i] = tile_type;
            Tile* t = tile_desc + tile_type;
            Sprite* s;

            if (t->texture) {
                s = sprite_push_tex(x, y, 32, 32, t->texture);
                if (t->tex_mode & TILE_TEX_STATIC) {
                    s->cur_frame = t->tex_mode - TILE_TEX_STATIC;
                }
            } else {
                s = sprite_push_col(x, y, 32, 32, t->color);
            }

            s->collision_type = t->collision_type;
            s->collision_response = t->collision_response;

            ++i;
            x += 32;
        }

        x = 0;
        y += 32;
    }

    for (int j = 0; j < (ROOM_WIDTH * ROOM_HEIGHT); ++j) {
        Tile* t = tile_desc + tile_types[j];

        if (t->tex_mode == TILE_TEX_ADJACENCY) {
            int tex_choice = 0;

            if ((j % ROOM_WIDTH) == 0 || (tile_types[j - 1] & TILE_SOLID)) {
                tex_choice |= 1;
            }
            if ((j % ROOM_WIDTH) == (ROOM_WIDTH - 1) ||
                (tile_types[j + 1] & TILE_SOLID)) {
                tex_choice |= 2;
            }
            if (j <= ROOM_WIDTH || (tile_types[j - ROOM_WIDTH] & TILE_SOLID)) {
                tex_choice |= 4;
            }
            if (j >= (ROOM_WIDTH * (ROOM_HEIGHT - 1)) ||
                (tile_types[j + ROOM_WIDTH] & TILE_SOLID)) {
                tex_choice |= 8;
            }

            struct {
                int frame;
                int rotation;
            } tex_lookup[] = {
                [0] = {3, 0},   [1] = {2, 90},  [2] = {2, -90},
                [3] = {5, 0},   [4] = {2, 180}, [5] = {1, 180},
                [6] = {1, -90}, [7] = {0, 180}, [8] = {2, 0},
                [9] = {1, 90},  [10] = {1, 0},  [11] = {0, 0},
                [12] = {5, 90}, [13] = {0, 90}, [14] = {0, -90},
                [15] = {4, 0}, 
            };

            sprites[sprite_offset + j].cur_frame = tex_lookup[tex_choice].frame;
            sprites[sprite_offset + j].rotation =
                tex_lookup[tex_choice].rotation;
        }
    }

    fclose(f);
}

static int anim_timer;

void room_update(double delta) {
    anim_timer += delta;

    if (anim_timer > 150) {
        for (int i = 0; i < (ROOM_WIDTH * ROOM_HEIGHT); ++i) {
            Tile* t = tile_desc + tile_types[i];
            Sprite* s = sprites + sprite_offset + i;

            if (t->tex_mode == TILE_TEX_ANIMATE && s->num_frames) {
                s->cur_frame = (s->cur_frame + 1) % s->num_frames;
            }
        }
        anim_timer = 0;
    }

    for (int i = 0; i < (ROOM_WIDTH * ROOM_HEIGHT); ++i) {
        Sprite* s = sprites + sprite_offset + i;

        if (s->respawn_timer > 0) {
            s->respawn_timer -= delta;

            if (s->respawn_timer <= 0) {
                sprite_set_tex(s, "res/sprites/blood.png", 0);
                s->collision_type = COLLISION_BOX;
            }
        }
    }
}

void room_switch(int which) {
    int id = room_map[current_room].neighbours[which];
    room_load(id);
}

void room_get_powerup(int index) {
    Sprite* s = sprites + index;

    s->collision_type = COLLISION_NONE;
    s->tex = 0;
    s->respawn_timer = 2000;

    sound_play("res/sfx/powerup.wav", 0);

    SDL_Point p = sprite_get_center(s);

    particles_spawn(p, 0, 0, 10);
}

SDL_Point room_get_spawn(void) { return current_room_spawn; }
