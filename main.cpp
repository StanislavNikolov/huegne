#include <iostream>
#include <ctime>
#include <random>
#include <thread>
#include <SDL2/SDL.h>

int sizeX = 600, sizeY = 600;
int targetFPS = 30;
int targetITS = 1000;
int renderScale = 1;
int animalDieThreshold = 30;
int currStep = animalDieThreshold;

int redrawQueueSize = 0, *redrawQueue;
int MAX_REDRAW_QUEUE_SIZE = sizeX * sizeY;
bool drawOnlyAnimals = false;

int mulTime[] = {30, 26};

enum creatureType {PLANT, ANIMAL, EMPTY};

struct Creature {
	creatureType type = EMPTY;
	unsigned char r = 128, g = 128, b = 128;
	int lastMul = 0;
	bool inQueue = false;
} *map;

SDL_Window* window;
SDL_Renderer* renderer;

std::random_device seeder;
std::default_random_engine eng(seeder());
std::uniform_int_distribution<int> directionDist(-1, 1);
std::uniform_int_distribution<int> colorMutDist(-3, 3);
std::uniform_int_distribution<int> chaceDist(0, 99);

inline void drawSquare(int idx) {
	if(map[idx].type == ANIMAL and renderScale <= 2) return; // can't draw it anyways
	if(map[idx].type == PLANT) {
		SDL_SetRenderDrawColor(renderer, map[idx].r, map[idx].g, map[idx].b, 255);
	} else {
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	}

	const int x = idx % sizeX;
	const int y = idx / sizeX;
	const SDL_Rect rect1 = {x * renderScale,
		y * renderScale,
		renderScale,
		renderScale};

	SDL_RenderFillRect(renderer, &rect1);

	if(map[idx].type == ANIMAL) {
		SDL_SetRenderDrawColor(renderer, map[idx].r, map[idx].g, map[idx].b, 255);

		const int border = 2;
		const SDL_Rect rect2 = {x * renderScale + border,
			y * renderScale + border,
			renderScale - 2*border,
			renderScale - 2*border};

		SDL_RenderFillRect(renderer, &rect2);
	}
}

inline float diff(int idx1, int idx2) {
	float sum = std::abs(map[idx1].r - map[idx2].r)
		+ std::abs(map[idx1].g - map[idx2].g)
		+ std::abs(map[idx1].b - map[idx2].b);

	return (sum / 33) + 1;
}

int getNewIdx(int idx) {
	int nx = directionDist(eng);
	int ny = directionDist(eng);

	// TODO 2/3 is 0, 1/2 is 1, fix it
	// right on the border checks
	if(idx % sizeX == 0 and nx == -1) { nx ++; }
	if(idx % sizeX == sizeX - 1) { nx --; }
	if(idx < sizeX and ny == -1) { ny ++; }
	if(idx >= (sizeY - 1) * sizeX) { ny --; }

	return idx + nx + ny * sizeX;
}

void addToQueue(int idx) {
	if(!map[idx].inQueue and redrawQueueSize < MAX_REDRAW_QUEUE_SIZE) {
		redrawQueue[redrawQueueSize ++] = idx;
		map[idx].inQueue = true;
	}
}

void iterate() {
	for(unsigned int idx = 0;idx < sizeX * sizeY;idx ++) {
		if(map[idx].type == EMPTY) continue;
		if(map[idx].lastMul + mulTime[map[idx].type] < currStep) {
			int newIdx = getNewIdx(idx);

			map[idx].lastMul = currStep;
			if(map[newIdx].type == EMPTY) {
				map[newIdx].type = map[idx].type;
				map[newIdx].lastMul = currStep;

				map[newIdx].r = map[idx].r + colorMutDist(eng);
				map[newIdx].g = map[idx].g + colorMutDist(eng);
				map[newIdx].b = map[idx].b + colorMutDist(eng);

				addToQueue(newIdx);
			}
		}
		if(map[idx].type == ANIMAL) {
			map[idx].lastMul += 3; // take away energy
			if(map[idx].lastMul - animalDieThreshold > currStep) {
				map[idx].type = EMPTY; // the animal dies
				addToQueue(idx);
			} else {
				int newIdx = getNewIdx(idx);

				if(map[newIdx].type != ANIMAL) {
					if(map[newIdx].type == PLANT) { // eat the plant
						map[newIdx].type = EMPTY;
						map[idx].lastMul -= (int)(20.0 / diff(idx, newIdx)); // give energy
					}
					addToQueue(idx);
					addToQueue(newIdx);

					std::swap(map[newIdx], map[idx]); // move there
				}
			}
		}
	}
}

void sdlForceRedraw() {
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderClear(renderer);

	for(int idx = 0;idx < sizeX * sizeY;idx ++) {
		map[idx].inQueue = false;
		if(drawOnlyAnimals and map[idx].type != ANIMAL) continue;
		drawSquare(idx);
	}
	redrawQueueSize = 0;

	SDL_RenderPresent(renderer);
}

void sdlDraw() {
	for(int i = 0;i < redrawQueueSize;i ++) {
		const int idx = redrawQueue[i]; // shortcut
		map[idx].inQueue = false;
		//if(drawOnlyAnimals and map[idx].type != ANIMAL) continue;
		drawSquare(idx);
	}
	redrawQueueSize = 0;
	SDL_RenderPresent(renderer);
}

void init() {
	map = new Creature[sizeX * sizeY];
	redrawQueue = new int[MAX_REDRAW_QUEUE_SIZE];
	SDL_CreateWindowAndRenderer(sizeX * renderScale, sizeY * renderScale, 0, &window, &renderer);

	// the initial seed
	const int idx = sizeX * (1 + sizeY) / 2;
	map[idx].type = PLANT;
}

int main(int argc, char** argv) {
	for(int i = 0;i < argc;++ i)
	{
		if(strcmp(argv[i], "--scale") == 0)
			renderScale = atoi(argv[++ i]);
		if(strcmp(argv[i], "--x") == 0)
			sizeX = atoi(argv[++ i]);
		if(strcmp(argv[i], "--y") == 0)
			sizeY = atoi(argv[++ i]);
		if(strcmp(argv[i], "--fps") == 0)
			targetFPS = atoi(argv[++ i]);
		//if(strcmp(argv[i], "--threads") == 0)
			//threadCount = atoi(argv[++ i]);
	}

	init();
	sdlForceRedraw();
	int frames = 0, iterations = 0;

	clock_t lastIteration = clock();
	clock_t lastFrame = clock();
	clock_t lastFPSMeasure = lastIteration;

	bool firstAnimal = false;
	bool paused = false;

	while(true)
	{
		if(lastFrame + CLOCKS_PER_SEC / targetFPS < clock())
		{
			if(redrawQueueSize < MAX_REDRAW_QUEUE_SIZE) sdlDraw();
			else sdlForceRedraw();

			lastFrame = clock();
			frames ++;
		}

		if(lastIteration + CLOCKS_PER_SEC / targetITS < clock())
		{
			if(!paused) iterate();

			lastIteration = clock();

			currStep ++;
			iterations ++;

			if(lastFPSMeasure + CLOCKS_PER_SEC < lastIteration) {
				std::cout << "Frames: " << frames << " | Iterations: " << iterations << std::endl;
				frames = 0;
				iterations = 0;
				lastFPSMeasure = lastIteration;
			}
		}
		/*
		const unsigned char *keys = SDL_GetKeyboardState(NULL); // no need to free it later
		if(keys[SDL_SCANCODE_A]) {
			drawOnlyAnimals = !drawOnlyAnimals;
			sdlForceRedraw();
		}
		*/

		if(!firstAnimal and currStep > mulTime[PLANT] * 120) {
			const int idx = sizeX * (1 + sizeY) / 2;
			map[idx].type = ANIMAL;
			map[idx].r = 128;
			map[idx].g = 128;
			map[idx].b = 128;
			targetITS = 20;
			addToQueue(idx);
			firstAnimal = true;
		}

		SDL_Event event;
		while(SDL_PollEvent(&event) != 0) {
			if(event.type == SDL_QUIT) {
				SDL_Quit();
				return 0;
			}
			if(event.type == SDL_KEYDOWN) {
				bool change = false;

				if(event.key.keysym.sym == SDLK_1) { targetITS -= 10; change = true; }
				if(event.key.keysym.sym == SDLK_2) { targetITS += 10; change = true; }
				if(event.key.keysym.sym == SDLK_3) { targetFPS -= 10; change = true; }
				if(event.key.keysym.sym == SDLK_4) { targetFPS += 10; change = true; }
				if(event.key.keysym.sym == SDLK_p) paused = !paused;

				if(change) {
					if(targetFPS <= 0) targetFPS = 1;
					if(targetITS <= 0) targetITS = 1;
					std::cout << "Targets: fps: " << targetFPS << " its: " << targetITS << std::endl;
				}
			}
		}
	}
	return 0;
}
