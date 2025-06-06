#include <stdio.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>

// Screen dimensions
#define SCREEN_WIDTH 1024
#define SCREEN_HEIGHT 768

// Forward declarations for structs
typedef struct Graphics Graphics;
typedef struct GameState GameState;

// Struct to hold all SDL-related components
struct Graphics {
    SDL_Window* window;
    SDL_Renderer* renderer;
    // You can add fonts and textures here later
    // TTF_Font* font;
};

// Struct to hold the player's state
typedef struct {
    int x;
    int y;
} Player;

// Struct to hold the overall game state
struct GameState {
    bool is_running;
    Player player;
};

// --- Function Prototypes ---

bool init_sdl(Graphics* graphics);
void cleanup(Graphics* graphics);
void handle_input(GameState* game_state);
void update_game(GameState* game_state);
void render(const Graphics* graphics, const GameState* game_state);


// --- Main Function ---

int main(void) {
    Graphics graphics = {0};
    GameState game_state = {
        .is_running = true,
        .player = { .x = SCREEN_WIDTH / 2, .y = SCREEN_HEIGHT / 2 }
    };

    if (!init_sdl(&graphics)) {
        return 1;
    }

    // Main game loop
    while (game_state.is_running) {
        handle_input(&game_state);
        update_game(&game_state);
        render(&graphics, &game_state);
    }

    cleanup(&graphics);
    return 0;
}


// --- Function Definitions ---

/**
 * @brief Initializes SDL, its subsystems, and creates a window and renderer.
 * @param graphics A pointer to the Graphics struct to be initialized.
 * @return true on success, false on failure.
 */
bool init_sdl(Graphics* graphics) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "Could not initialize SDL: %s\n", SDL_GetError());
        return false;
    }

    if (TTF_Init() == -1) {
        fprintf(stderr, "Could not initialize SDL_ttf: %s\n", TTF_GetError());
        SDL_Quit();
        return false;
    }

    int img_flags = IMG_INIT_PNG;
    if (!(IMG_Init(img_flags) & img_flags)) {
        fprintf(stderr, "Could not initialize SDL_image: %s\n", IMG_GetError());
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    graphics->window = SDL_CreateWindow(
        "C Roguelike",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        SDL_WINDOW_SHOWN
    );
    if (!graphics->window) {
        fprintf(stderr, "Could not create window: %s\n", SDL_GetError());
        IMG_Quit();
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    graphics->renderer = SDL_CreateRenderer(graphics->window, -1, SDL_RENDERER_ACCELERATED);
    if (!graphics->renderer) {
        fprintf(stderr, "Could not create renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(graphics->window);
        IMG_Quit();
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    return true;
}

/**
 * @brief Cleans up SDL resources.
 * @param graphics A pointer to the Graphics struct containing the resources to be freed.
 */
void cleanup(Graphics* graphics) {
    if (graphics->renderer) {
        SDL_DestroyRenderer(graphics->renderer);
    }
    if (graphics->window) {
        SDL_DestroyWindow(graphics->window);
    }
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
}

/**
 * @brief Processes all pending SDL events.
 * @param game_state A pointer to the GameState to be modified based on input.
 */
void handle_input(GameState* game_state) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            game_state->is_running = false;
        } else if (event.type == SDL_KEYDOWN) {
            switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                    game_state->is_running = false;
                    break;
                case SDLK_UP:
                    game_state->player.y -= 10;
                    break;
                case SDLK_DOWN:
                    game_state->player.y += 10;
                    break;
                case SDLK_LEFT:
                    game_state->player.x -= 10;
                    break;
                case SDLK_RIGHT:
                    game_state->player.x += 10;
                    break;
            }
        }
    }
}

/**
 * @brief Updates the game state (e.g., enemy movement, physics).
 * @param game_state A pointer to the GameState to be updated.
 */
void update_game(GameState* game_state) {
    // This is where you'll put game logic that happens every frame.
    // For now, it's empty.
    (void)game_state; // Mark as unused for now
}

/**
 * @brief Renders the game to the screen.
 * @param graphics A pointer to the Graphics struct.
 * @param game_state A pointer to the GameState struct containing what to render.
 */
void render(const Graphics* graphics, const GameState* game_state) {
    // Set background color to dark gray
    SDL_SetRenderDrawColor(graphics->renderer, 34, 34, 34, 255);
    SDL_RenderClear(graphics->renderer);

    // --- Draw game objects here ---
    
    // Draw the player (as a white rectangle for now)
    SDL_Rect player_rect = {
        .x = game_state->player.x,
        .y = game_state->player.y,
        .w = 20,
        .h = 20
    };
    SDL_SetRenderDrawColor(graphics->renderer, 255, 255, 255, 255);
    SDL_RenderFillRect(graphics->renderer, &player_rect);

    // Present the frame
    SDL_RenderPresent(graphics->renderer);
}
