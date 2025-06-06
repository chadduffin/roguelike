# Makefile for a single-file C roguelike using SDL2

# Compiler and flags
CC = clang
CFLAGS = -Wall -Wextra -std=c11 -O2 -I/opt/homebrew/opt/sdl2/include -I/opt/homebrew/Cellar/sdl2_ttf/2.24.0/include -I/opt/homebrew/Cellar/sdl2_image/2.8.8/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/opt/sdl2/lib -L/opt/homebrew/Cellar/sdl2_ttf/2.24.0/lib/ -L/opt/homebrew/Cellar/sdl2_image/2.8.8/lib/
LDFLAGS = -lSDL2 -lSDL2_ttf -lSDL2_image

# Source and executable names
SRC = rogue.c
EXECUTABLE = rogue

# Default target
all: $(EXECUTABLE)

# Linking
$(EXECUTABLE): $(SRC)
	$(CC) $(CFLAGS) -o $(EXECUTABLE) $(SRC) $(LDFLAGS)

# Clean up build files
clean:
	rm -f $(EXECUTABLE)

# Phony targets
.PHONY: all clean
