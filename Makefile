# Raycaster - Makefile

CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=c11
LDFLAGS = -lm

SRC     = src/main.c
TARGET  = raycaster

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
