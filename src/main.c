#include <SDL2/SDL.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>

#include "game.h"
#include "sound.h"

bool running = true;

SDL_Window* win;
SDL_Renderer* renderer;
SDL_Rect viewport;

void handle_event(SDL_Event* e) {
	switch (e->type) {
		case SDL_QUIT: {
			running = false;
		} break;
	}
}

int main(int argc, char** argv) {

	srand(time(0));

	SDL_Init(SDL_INIT_EVERYTHING);

	if (SDL_CreateWindowAndRenderer(WINDOW_WIDTH, WINDOW_HEIGHT, 0, &win, &renderer) == -1) {
		fprintf(stderr, "SDL error creating window/renderer: %s\n", SDL_GetError());
		return 1;
	}

	SDL_SetWindowTitle(win, "WIP GAME");

	SDL_RenderGetViewport(renderer, &viewport);

	uint64_t timer_freq = SDL_GetPerformanceFrequency() / (1000 * 1000);
	uint64_t timer_start;

	sound_init();
	game_init();

	timer_start = SDL_GetPerformanceCounter() / timer_freq;
	
	while (running) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			handle_event(&e);
		}

		uint64_t timer_diff = (SDL_GetPerformanceCounter() / timer_freq) - timer_start;
		timer_start = SDL_GetPerformanceCounter() / timer_freq;

		if (timer_diff < 16667) {
			SDL_Delay(17 - (timer_diff / 1000));
			timer_diff = 16667;
		}

		game_update(timer_diff / 1000);
		
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		SDL_RenderClear(renderer);
		game_draw();

		SDL_RenderPresent(renderer);
	}
}
