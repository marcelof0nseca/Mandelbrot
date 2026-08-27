# Makefile do gerador do conjunto de Mandelbrot.
# ATENCAO: linhas de receita comecam com TAB, nunca com espacos.

CC = gcc

# -O2       sem otimizacao os tempos ficam varias vezes maiores e achatam a
#           diferenca entre as implementacoes
# -fopenmp  habilita os #pragma omp; SEM ela o gcc apenas avisa "ignoring
#           #pragma omp" e compila mesmo assim - o binario sai correto porem
#           serial, e o unico sintoma e o speedup ficar em 1,0x
# -pthread  linka a libpthread e define _REENTRANT; nao e sinonimo de -lpthread
CFLAGS  = -std=c11 -Wall -Wextra -O2 -fopenmp -pthread
LDFLAGS = -fopenmp -pthread

ALVO = mandelbrot
OBJS = main.o mandelbrot.o

all: $(ALVO)

$(ALVO): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

# Dependencia do header explicita: sem ela o make nao recompila quando
# mandelbrot.h muda, e voce acaba depurando um binario desatualizado.
main.o: main.c mandelbrot.h
	$(CC) $(CFLAGS) -c main.c

mandelbrot.o: mandelbrot.c mandelbrot.h
	$(CC) $(CFLAGS) -c mandelbrot.c

clean:
	rm -f $(ALVO) $(OBJS) *.pgm times.txt

.PHONY: all clean
