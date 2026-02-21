CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=c11

SRC = src/main.c src/data.c src/data_skills.c src/data_enemies.c src/data_items.c \
      src/hero.c src/combat.c src/ui.c src/save.c
OBJ = $(SRC:.c=.o)

ifeq ($(OS),Windows_NT)
    TARGET  = dungeon-grind.exe
    LDFLAGS = -lpdcurses
    RM      = del /Q 2>nul
else
    TARGET  = dungeon-grind
    LDFLAGS = -lncurses
    RM      = rm -f
endif

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

src/%.o: src/%.c src/game.h
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	$(RM) $(OBJ) $(TARGET)
