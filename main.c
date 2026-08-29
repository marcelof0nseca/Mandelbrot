#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>

#include "mandelbrot.h"

#define ARQ_SERIAL    "mandelbrot_maf_serial.pgm"
#define ARQ_OPENMP    "mandelbrot_maf_openmp.pgm"
#define ARQ_PTHREADS1 "mandelbrot_maf_pthreads1.pgm"
#define ARQ_PTHREADS2 "mandelbrot_maf_pthreads2.pgm"
#define ARQ_TIMES     "times.txt"

#define MIN_DIMENSAO 1
#define MAX_DIMENSAO 100000
#define MIN_ITER     1
#define MAX_ITER     1000000
#define MIN_THREADS  1
#define MAX_THREADS  4096

typedef int (*FuncCalculo)(unsigned char *, const Params *);

typedef struct {
    const char *rotulo;
    const char *arquivo;
    FuncCalculo funcao;
} Implementacao;

static const Implementacao IMPLEMENTACOES[] = {
    { "Serial",    ARQ_SERIAL,    calcular_serial },
    { "OpenMP",    ARQ_OPENMP,    calcular_openmp },
    { "Pthreads1", ARQ_PTHREADS1, calcular_pthreads1 },
    { "Pthreads2", ARQ_PTHREADS2, calcular_pthreads2 },
};

#define NUM_IMPL (sizeof(IMPLEMENTACOES) / sizeof(IMPLEMENTACOES[0]))

static void imprimir_uso(const char *prog)
{
    fprintf(stderr, "Uso: %s [largura] [altura] [max_iteracoes] [num_threads]\n", prog);
}

static int ler_inteiro(const char *texto, const char *nome,
                       int minimo, int maximo, int *saida)
{
    char *fim = NULL;
    long valor;

    if (texto == NULL || texto[0] == '\0') {
        fprintf(stderr, "Erro: %s nao pode ser vazio.\n", nome);
        return -1;
    }

    errno = 0;
    valor = strtol(texto, &fim, 10);

    if (fim == texto) {
        fprintf(stderr, "Erro: %s ('%s') nao e um numero inteiro.\n", nome, texto);
        return -1;
    }
    if (*fim != '\0') {
        fprintf(stderr, "Erro: %s ('%s') contem caractere invalido em '%s'.\n",
                nome, texto, fim);
        return -1;
    }
    if (errno == ERANGE) {
        fprintf(stderr, "Erro: %s ('%s') esta fora da faixa representavel.\n", nome, texto);
        return -1;
    }
    if (valor < (long)minimo || valor > (long)maximo) {
        fprintf(stderr, "Erro: %s deve estar entre %d e %d (recebido %ld).\n",
                nome, minimo, maximo, valor);
        return -1;
    }

    *saida = (int)valor;
    return 0;
}

static int escrever_pgm(const char *caminho, const unsigned char *imagem,
                        const Params *p)
{
    FILE *f;
    int y, x;

    f = fopen(caminho, "w");
    if (f == NULL) {
        fprintf(stderr, "Erro: nao foi possivel criar '%s': %s\n", caminho, strerror(errno));
        return -1;
    }
    setvbuf(f, NULL, _IOFBF, 1024 * 1024);

    for (y = 0; y < p->altura; y++) {
        const size_t base = (size_t)y * (size_t)p->largura;

        for (x = 0; x < p->largura; x++) {
            if (x > 0) fputc(' ', f);
            fprintf(f, "%u", (unsigned)imagem[base + (size_t)x]);
        }
        fputc('\n', f);
    }

    if (ferror(f)) {
        fprintf(stderr, "Erro: falha ao escrever em '%s': %s\n", caminho, strerror(errno));
        fclose(f);
        return -1;
    }
    if (fclose(f) != 0) {
        fprintf(stderr, "Erro: falha ao fechar '%s': %s\n", caminho, strerror(errno));
        return -1;
    }

    return 0;
}

static int executar_e_medir(FuncCalculo funcao, unsigned char *imagem,
                            const Params *p, double *tempo)
{
    struct timespec inicio, fim;

    if (clock_gettime(CLOCK_MONOTONIC, &inicio) != 0) {
        fprintf(stderr, "Erro: falha ao ler o relogio: %s\n", strerror(errno));
        return -1;
    }

    if (funcao(imagem, p) != 0) return -1;

    if (clock_gettime(CLOCK_MONOTONIC, &fim) != 0) {
        fprintf(stderr, "Erro: falha ao ler o relogio: %s\n", strerror(errno));
        return -1;
    }

    *tempo = (double)(fim.tv_sec - inicio.tv_sec)
           + (double)(fim.tv_nsec - inicio.tv_nsec) / 1e9;
    return 0;
}

int main(int argc, char *argv[])
{
    const char *prog = (argc > 0 && argv[0] != NULL) ? argv[0] : "mandelbrot";
    Params p;
    size_t total, i;
    unsigned char *buffers[NUM_IMPL] = { NULL };
    double tempos[NUM_IMPL] = { 0.0 };
    FILE *ft = NULL;
    int status = EXIT_FAILURE;

    if (argc != 5) {
        fprintf(stderr, "Erro: numero incorreto de argumentos (esperado 4, recebido %d).\n",
                argc > 0 ? argc - 1 : 0);
        imprimir_uso(prog);
        return EXIT_FAILURE;
    }

    if (ler_inteiro(argv[1], "largura", MIN_DIMENSAO, MAX_DIMENSAO, &p.largura) != 0 ||
        ler_inteiro(argv[2], "altura", MIN_DIMENSAO, MAX_DIMENSAO, &p.altura) != 0 ||
        ler_inteiro(argv[3], "max_iteracoes", MIN_ITER, MAX_ITER, &p.max_iter) != 0 ||
        ler_inteiro(argv[4], "num_threads", MIN_THREADS, MAX_THREADS, &p.num_threads) != 0) {
        imprimir_uso(prog);
        return EXIT_FAILURE;
    }

    if ((size_t)p.altura > SIZE_MAX / (size_t)p.largura) {
        fprintf(stderr, "Erro: largura x altura excede o tamanho representavel.\n");
        return EXIT_FAILURE;
    }
    total = (size_t)p.largura * (size_t)p.altura;

    for (i = 0; i < NUM_IMPL; i++) {
        buffers[i] = malloc(total);
        if (buffers[i] == NULL) {
            fprintf(stderr, "Erro: falha ao alocar %zu bytes para a imagem %s.\n",
                    total, IMPLEMENTACOES[i].rotulo);
            goto limpeza;
        }
    }

    for (i = 0; i < NUM_IMPL; i++) {
        if (executar_e_medir(IMPLEMENTACOES[i].funcao, buffers[i], &p, &tempos[i]) != 0) {
            fprintf(stderr, "Erro: falha na execucao da implementacao %s.\n",
                    IMPLEMENTACOES[i].rotulo);
            goto limpeza;
        }
    }

    for (i = 0; i < NUM_IMPL; i++) {
        if (escrever_pgm(IMPLEMENTACOES[i].arquivo, buffers[i], &p) != 0) goto limpeza;
    }

    ft = fopen(ARQ_TIMES, "w");
    if (ft == NULL) {
        fprintf(stderr, "Erro: nao foi possivel criar '%s': %s\n", ARQ_TIMES, strerror(errno));
        goto limpeza;
    }
    for (i = 0; i < NUM_IMPL; i++) {
        fprintf(ft, "%s: %.6fs\n", IMPLEMENTACOES[i].rotulo, tempos[i]);
    }
    if (ferror(ft) || fclose(ft) != 0) {
        fprintf(stderr, "Erro: falha ao escrever '%s': %s\n", ARQ_TIMES, strerror(errno));
        ft = NULL;
        goto limpeza;
    }
    ft = NULL;

    status = EXIT_SUCCESS;

limpeza:
    if (ft != NULL) fclose(ft);
    for (i = 0; i < NUM_IMPL; i++) free(buffers[i]);
    return status;
}
