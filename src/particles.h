#ifndef PARTICLES_H_
#define PARTICLES_H_

#include <SDL2/SDL.h>

void particles_spawn(SDL_Point pos, float xv, float yv, int amount);
void particles_update(int delta);
void particles_draw(void);

#endif
