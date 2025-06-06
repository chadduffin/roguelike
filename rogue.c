#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h> // For rand(), srand(), malloc(), free()
#include <time.h>   // For time()
#include <math.h>   // For roundf()
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>

// --- Game Constants ---
#define DUNGEON_FLOOR_COUNT 5

// The dimensions of the tile grid
#define GRID_COLS 80
#define GRID_ROWS 50

// The pixel dimensions of a single tile
#define TILE_WIDTH 12
#define TILE_HEIGHT 12

// The calculated screen dimensions
#define SCREEN_WIDTH (GRID_COLS * TILE_WIDTH)
#define SCREEN_HEIGHT (GRID_ROWS * TILE_HEIGHT)

// Procedural Generation Parameters
#define MAX_ROOMS 15
#define MIN_ROOM_W 6
#define MAX_ROOM_W 12
#define MIN_ROOM_H 6
#define MAX_ROOM_H 12


// --- Enum and Struct Definitions ---

typedef enum {
    TILE_WALL,
    TILE_GROUND,
    TILE_STAIRS_UP,
    TILE_STAIRS_DOWN
} TileType;

typedef struct {
    TileType type;
    bool is_visible;
    bool is_explored; // Has this tile been seen at least once?
} Tile;

typedef struct {
    Tile tiles[GRID_ROWS][GRID_COLS];
    SDL_Point stairs_up;
    SDL_Point stairs_down;
} Floor;

typedef struct {
    Floor* floors;
    int floor_count;
} Dungeon;

typedef struct {
    SDL_Window* window;
    SDL_Renderer* renderer;
} Graphics;

typedef struct {
    int x; // Column
    int y; // Row
} Player;

typedef struct {
    bool is_running;
    int current_floor_index;
    Player player;
    Dungeon dungeon;
} GameState;


// --- Function Prototypes ---

// Game Loop Functions
bool init_systems(Graphics* graphics, GameState* game_state);
void cleanup(Graphics* graphics, GameState* game_state);
void handle_input(GameState* game_state);
void update_game(GameState* game_state);
void render(const Graphics* graphics, const GameState* game_state);

// Dungeon Generation
void generate_floor(Floor* floor);
void carve_room(Floor* floor, SDL_Rect room);
void carve_h_corridor(Floor* floor, int x1, int x2, int y);
void carve_v_corridor(Floor* floor, int y1, int y2, int x);

// Field of View
void update_fov(GameState* game_state);
void cast_light(GameState* game_state, int octant, int row, float start_slope, float end_slope);


// --- Main Function ---

int main(void) {
    srand(time(NULL)); // Seed the random number generator

    Graphics graphics = {0};
    GameState game_state = { .is_running = true };

    if (!init_systems(&graphics, &game_state)) {
        return 1;
    }

    // Main game loop
    while (game_state.is_running) {
        handle_input(&game_state);
        update_game(&game_state);
        render(&graphics, &game_state);
        SDL_Delay(16); // Cap framerate roughly
    }

    cleanup(&graphics, &game_state);
    return 0;
}


// --- Game System Functions ---

bool init_systems(Graphics* graphics, GameState* game_state) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0 || TTF_Init() == -1 || !(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        fprintf(stderr, "Could not initialize SDL libraries: %s\n", SDL_GetError());
        return false;
    }

    graphics->window = SDL_CreateWindow("C Roguelike", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (!graphics->window) {
        fprintf(stderr, "Could not create window: %s\n", SDL_GetError());
        return false;
    }

    graphics->renderer = SDL_CreateRenderer(graphics->window, -1, SDL_RENDERER_ACCELERATED);
    if (!graphics->renderer) {
        fprintf(stderr, "Could not create renderer: %s\n", SDL_GetError());
        return false;
    }

    // Initialize Dungeon
    game_state->dungeon.floor_count = DUNGEON_FLOOR_COUNT;
    game_state->dungeon.floors = malloc(sizeof(Floor) * game_state->dungeon.floor_count);
    if (!game_state->dungeon.floors) {
        fprintf(stderr, "Failed to allocate memory for dungeon floors.\n");
        return false;
    }

    for (int i = 0; i < game_state->dungeon.floor_count; ++i) {
        generate_floor(&game_state->dungeon.floors[i]);
    }
    
    // Adjust stairs for top and bottom floors
    game_state->dungeon.floors[0].tiles[game_state->dungeon.floors[0].stairs_up.y][game_state->dungeon.floors[0].stairs_up.x].type = TILE_GROUND;
    int last_floor = game_state->dungeon.floor_count - 1;
    game_state->dungeon.floors[last_floor].tiles[game_state->dungeon.floors[last_floor].stairs_down.y][game_state->dungeon.floors[last_floor].stairs_down.x].type = TILE_GROUND;


    // Set initial game state
    game_state->current_floor_index = 0;
    game_state->player.x = game_state->dungeon.floors[0].stairs_up.x;
    game_state->player.y = game_state->dungeon.floors[0].stairs_up.y;
    
    // Initial FOV calculation
    update_fov(game_state);

    return true;
}

void cleanup(Graphics* graphics, GameState* game_state) {
    if (game_state->dungeon.floors) {
        free(game_state->dungeon.floors);
    }
    if (graphics->renderer) SDL_DestroyRenderer(graphics->renderer);
    if (graphics->window) SDL_DestroyWindow(graphics->window);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
}


// --- Core Game Loop Functions ---

void handle_input(GameState* game_state) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            game_state->is_running = false;
        } else if (event.type == SDL_KEYDOWN) {
            int next_x = game_state->player.x;
            int next_y = game_state->player.y;
            bool key_pressed = false;

            switch (event.key.keysym.sym) {
                case SDLK_ESCAPE: game_state->is_running = false; break;
                case SDLK_UP: case SDLK_k: next_y--; key_pressed = true; break;
                case SDLK_DOWN: case SDLK_j: next_y++; key_pressed = true; break;
                case SDLK_LEFT: case SDLK_h: next_x--; key_pressed = true; break;
                case SDLK_RIGHT: case SDLK_l: next_x++; key_pressed = true; break;
            }

            if (key_pressed) {
                if (next_x < 0 || next_x >= GRID_COLS || next_y < 0 || next_y >= GRID_ROWS) {
                    continue;
                }

                Floor* current_floor = &game_state->dungeon.floors[game_state->current_floor_index];
                TileType next_tile_type = current_floor->tiles[next_y][next_x].type;
                bool moved = false;

                switch (next_tile_type) {
                    case TILE_WALL:
                        break; // Cannot move
                    
                    case TILE_STAIRS_DOWN:
                        if (game_state->current_floor_index < game_state->dungeon.floor_count - 1) {
                            game_state->current_floor_index++;
                            Floor* new_floor = &game_state->dungeon.floors[game_state->current_floor_index];
                            game_state->player.x = new_floor->stairs_up.x;
                            game_state->player.y = new_floor->stairs_up.y;
                            moved = true;
                        }
                        break;

                    case TILE_STAIRS_UP:
                        if (game_state->current_floor_index > 0) {
                            game_state->current_floor_index--;
                            Floor* new_floor = &game_state->dungeon.floors[game_state->current_floor_index];
                            game_state->player.x = new_floor->stairs_down.x;
                            game_state->player.y = new_floor->stairs_down.y;
                            moved = true;
                        }
                        break;
                    
                    default: // Includes TILE_GROUND
                        game_state->player.x = next_x;
                        game_state->player.y = next_y;
                        moved = true;
                        break;
                }
                if (moved) {
                    update_fov(game_state);
                }
            }
        }
    }
}

void update_game(GameState* game_state) {
    (void)game_state;
}

void render(const Graphics* graphics, const GameState* game_state) {
    SDL_SetRenderDrawColor(graphics->renderer, 0, 0, 0, 255);
    SDL_RenderClear(graphics->renderer);

    const Floor* current_floor = &game_state->dungeon.floors[game_state->current_floor_index];

    // Draw the map
    for (int y = 0; y < GRID_ROWS; ++y) {
        for (int x = 0; x < GRID_COLS; ++x) {
            SDL_Rect tile_rect = { x * TILE_WIDTH, y * TILE_HEIGHT, TILE_WIDTH, TILE_HEIGHT };
            const Tile* tile = &current_floor->tiles[y][x];

            if (tile->is_visible) {
                // Draw visible tiles with bright colors
                switch (tile->type) {
                    case TILE_WALL:       SDL_SetRenderDrawColor(graphics->renderer, 80, 80, 80, 255); break;
                    case TILE_GROUND:     SDL_SetRenderDrawColor(graphics->renderer, 180, 180, 180, 255); break;
                    case TILE_STAIRS_DOWN: SDL_SetRenderDrawColor(graphics->renderer, 60, 120, 220, 255); break;
                    case TILE_STAIRS_UP:   SDL_SetRenderDrawColor(graphics->renderer, 220, 120, 60, 255); break;
                }
                SDL_RenderFillRect(graphics->renderer, &tile_rect);
            } else if (tile->is_explored) {
                // Draw explored but not visible tiles with dark colors
                switch (tile->type) {
                    case TILE_WALL:       SDL_SetRenderDrawColor(graphics->renderer, 20, 20, 20, 255); break;
                    case TILE_GROUND:     SDL_SetRenderDrawColor(graphics->renderer, 60, 60, 60, 255); break;
                    case TILE_STAIRS_DOWN: SDL_SetRenderDrawColor(graphics->renderer, 20, 40, 80, 255); break;
                    case TILE_STAIRS_UP:   SDL_SetRenderDrawColor(graphics->renderer, 80, 40, 20, 255); break;
                }
                 SDL_RenderFillRect(graphics->renderer, &tile_rect);
            } else {
                 // Unexplored tiles are pure black (already cleared)
            }
        }
    }

    // Draw the player only if their tile is visible (it always should be)
    if (current_floor->tiles[game_state->player.y][game_state->player.x].is_visible) {
        SDL_Rect player_rect = { game_state->player.x * TILE_WIDTH, game_state->player.y * TILE_HEIGHT, TILE_WIDTH, TILE_HEIGHT };
        SDL_SetRenderDrawColor(graphics->renderer, 255, 255, 0, 255); // Yellow
        SDL_RenderFillRect(graphics->renderer, &player_rect);
    }

    SDL_RenderPresent(graphics->renderer);
}


// --- Dungeon Generation Functions ---

void generate_floor(Floor* floor) {
    for (int y = 0; y < GRID_ROWS; ++y) {
        for (int x = 0; x < GRID_COLS; ++x) {
            floor->tiles[y][x].type = TILE_WALL;
            floor->tiles[y][x].is_visible = false;
            floor->tiles[y][x].is_explored = false; // Initialize as unexplored
        }
    }

    SDL_Rect rooms[MAX_ROOMS];
    int room_count = 0;

    for (int i = 0; i < MAX_ROOMS; ++i) {
        int w = MIN_ROOM_W + rand() % (MAX_ROOM_W - MIN_ROOM_W + 1);
        int h = MIN_ROOM_H + rand() % (MAX_ROOM_H - MIN_ROOM_H + 1);
        int x = rand() % (GRID_COLS - w - 1) + 1;
        int y = rand() % (GRID_ROWS - h - 1) + 1;

        SDL_Rect new_room = {x, y, w, h};
        bool failed = false;
        for (int j = 0; j < room_count; ++j) {
            if (SDL_HasIntersection(&new_room, &rooms[j])) {
                failed = true;
                break;
            }
        }

        if (!failed) {
            carve_room(floor, new_room);
            if (room_count > 0) {
                SDL_Point new_center = {x + w / 2, y + h / 2};
                SDL_Point prev_center = {rooms[room_count - 1].x + rooms[room_count - 1].w / 2, rooms[room_count - 1].y + rooms[room_count - 1].h / 2};
                if (rand() % 2 == 0) {
                    carve_h_corridor(floor, prev_center.x, new_center.x, prev_center.y);
                    carve_v_corridor(floor, prev_center.y, new_center.y, new_center.x);
                } else {
                    carve_v_corridor(floor, prev_center.y, new_center.y, prev_center.x);
                    carve_h_corridor(floor, prev_center.x, new_center.x, new_center.y);
                }
            }
            rooms[room_count++] = new_room;
        }
    }
    
    floor->stairs_up = (SDL_Point){rooms[0].x + rooms[0].w / 2, rooms[0].y + rooms[0].h / 2};
    floor->tiles[floor->stairs_up.y][floor->stairs_up.x].type = TILE_STAIRS_UP;
    
    floor->stairs_down = (SDL_Point){rooms[room_count - 1].x + rooms[room_count - 1].w / 2, rooms[room_count - 1].y + rooms[room_count - 1].h / 2};
    floor->tiles[floor->stairs_down.y][floor->stairs_down.x].type = TILE_STAIRS_DOWN;
}

void carve_room(Floor* floor, SDL_Rect room) {
    for (int y = room.y; y < room.y + room.h; ++y) {
        for (int x = room.x; x < room.x + room.w; ++x) {
            floor->tiles[y][x].type = TILE_GROUND;
        }
    }
}

void carve_h_corridor(Floor* floor, int x1, int x2, int y) {
    for (int x = (x1 < x2 ? x1 : x2); x <= (x1 > x2 ? x1 : x2); ++x) {
        floor->tiles[y][x].type = TILE_GROUND;
    }
}

void carve_v_corridor(Floor* floor, int y1, int y2, int x) {
    for (int y = (y1 < y2 ? y1 : y2); y <= (y1 > y2 ? y1 : y2); ++y) {
        floor->tiles[y][x].type = TILE_GROUND;
    }
}

// --- Field of View Functions ---

void update_fov(GameState* game_state) {
    Floor* floor = &game_state->dungeon.floors[game_state->current_floor_index];
    
    for (int y = 0; y < GRID_ROWS; y++) {
        for (int x = 0; x < GRID_COLS; x++) {
            floor->tiles[y][x].is_visible = false;
        }
    }

    // Player's tile is always visible and explored
    floor->tiles[game_state->player.y][game_state->player.x].is_visible = true;
    floor->tiles[game_state->player.y][game_state->player.x].is_explored = true;

    for (int i = 0; i < 8; i++) {
        cast_light(game_state, i, 1, 1.0f, 0.0f);
    }
}

void cast_light(GameState* game_state, int octant, int row, float start_slope, float end_slope) {
    Floor* floor = &game_state->dungeon.floors[game_state->current_floor_index];
    int px = game_state->player.x;
    int py = game_state->player.y;
    
    if (start_slope < end_slope) {
        return;
    }

    for (int i = row; i < GRID_ROWS + GRID_COLS; i++) {
        int dx = i;
        int dy = 0;
        bool blocked = false;
        
        for (dy = roundf(dx * start_slope); dy >= roundf(dx * end_slope); dy--) {
            int x = px, y = py;
            switch(octant) {
                case 0: x += dy; y -= dx; break;
                case 1: x += dx; y -= dy; break;
                case 2: x += dx; y += dy; break;
                case 3: x += dy; y += dx; break;
                case 4: x -= dy; y += dx; break;
                case 5: x -= dx; y += dy; break;
                case 6: x -= dx; y -= dy; break;
                case 7: x -= dy; y -= dx; break;
            }

            if (x < 0 || x >= GRID_COLS || y < 0 || y >= GRID_ROWS) {
                continue;
            }

            floor->tiles[y][x].is_visible = true;
            floor->tiles[y][x].is_explored = true; // Once visible, it's always explored

            if (floor->tiles[y][x].type == TILE_WALL) {
                if (blocked) {
                    cast_light(game_state, octant, i + 1, start_slope, (dy + 0.5f) / (dx - 0.5f));
                }
                start_slope = (dy - 0.5f) / (dx + 0.5f);
                blocked = true;
            } else {
                blocked = false;
            }
        }

        if (blocked) {
            break;
        }
    }
}
