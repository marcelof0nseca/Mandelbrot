CC = gcc

CFLAGS  = -std=c11 -Wall -Wextra -O2 -fopenmp -pthread
LDFLAGS = -fopenmp -pthread

ALVO  = mandelbrot
BUILD = build
OBJS  = $(BUILD)/main.o $(BUILD)/mandelbrot.o

all: $(ALVO)

$(ALVO): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/main.o: main.c mandelbrot.h | $(BUILD)
	$(CC) $(CFLAGS) -c main.c -o $@

$(BUILD)/mandelbrot.o: mandelbrot.c mandelbrot.h | $(BUILD)
	$(CC) $(CFLAGS) -c mandelbrot.c -o $@

clean:
	rm -rf $(BUILD) $(ALVO) *.pgm times.txt

.PHONY: all clean
