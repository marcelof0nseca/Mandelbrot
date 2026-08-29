#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "mandelbrot.h"

int mandelbrot_ponto(double cr, double ci, int max_iter)
{
    double zr = 0.0, zi = 0.0;
    int i;

    for (i = 0; i < max_iter; i++) {
        double zr2 = zr * zr;
        double zi2 = zi * zi;

        if (zr2 + zi2 > 4.0) break;

        zi = 2.0 * zr * zi + ci;
        zr = zr2 - zi2 + cr;
    }

    return i;
}

double coord_real(int x, const Params *p)
{
    return REAL_MIN + (double)x * (REAL_MAX - REAL_MIN) / (double)p->largura;
}

double coord_imag(int y, const Params *p)
{
    return IMAG_MIN + (double)y * (IMAG_MAX - IMAG_MIN) / (double)p->altura;
}

void calcular_linha(unsigned char *imagem, int y, const Params *p)
{
    const double ci = coord_imag(y, p);
    const size_t base = (size_t)y * (size_t)p->largura;
    int x;

    for (x = 0; x < p->largura; x++) {
        int iter = mandelbrot_ponto(coord_real(x, p), ci, p->max_iter);

        imagem[base + (size_t)x] =
            (unsigned char)(((long)iter * 255L) / (long)p->max_iter);
    }
}

int calcular_serial(unsigned char *imagem, const Params *p)
{
    int y;

    for (y = 0; y < p->altura; y++) {
        calcular_linha(imagem, y, p);
    }

    return 0;
}

int calcular_openmp(unsigned char *imagem, const Params *p)
{
    int y;

    #pragma omp parallel for schedule(dynamic, 1) num_threads(p->num_threads)
    for (y = 0; y < p->altura; y++) {
        calcular_linha(imagem, y, p);
    }

    return 0;
}

typedef struct {
    unsigned char *imagem;
    const Params  *p;
    int            y_inicio;
    int            y_fim;
} FaixaP1;

static void *trabalhador_p1(void *arg)
{
    const FaixaP1 *f = (const FaixaP1 *)arg;
    int y;

    for (y = f->y_inicio; y < f->y_fim; y++) {
        calcular_linha(f->imagem, y, f->p);
    }

    return NULL;
}

int calcular_pthreads1(unsigned char *imagem, const Params *p)
{
    const int T = p->num_threads;
    pthread_t *threads = NULL;
    FaixaP1   *faixas  = NULL;
    int criadas = 0;
    int status  = 0;
    int i;

    threads = malloc((size_t)T * sizeof *threads);
    faixas  = malloc((size_t)T * sizeof *faixas);
    if (threads == NULL || faixas == NULL) {
        fprintf(stderr, "Erro: falha ao alocar as estruturas de controle de %d threads.\n", T);
        free(threads);
        free(faixas);
        return -1;
    }

    for (i = 0; i < T; i++) {
        int rc;

        faixas[i].imagem = imagem;
        faixas[i].p      = p;

        faixas[i].y_inicio = (int)(((long)i       * (long)p->altura) / (long)T);
        faixas[i].y_fim    = (int)((((long)i + 1) * (long)p->altura) / (long)T);

        rc = pthread_create(&threads[i], NULL, trabalhador_p1, &faixas[i]);
        if (rc != 0) {
            fprintf(stderr, "Erro: falha ao criar a thread %d de %d: %s\n",
                    i + 1, T, strerror(rc));
            status = -1;
            break;
        }
        criadas++;
    }

    for (i = 0; i < criadas; i++) {
        pthread_join(threads[i], NULL);
    }

    free(threads);
    free(faixas);

    return status;
}

typedef struct {
    pthread_mutex_t mutex;
    int             proxima;
    unsigned char  *imagem;
    const Params   *p;
} FilaP2;

static void *trabalhador_p2(void *arg)
{
    FilaP2 *f = (FilaP2 *)arg;

    for (;;) {
        int y;

        pthread_mutex_lock(&f->mutex);
        y = f->proxima;
        f->proxima++;
        pthread_mutex_unlock(&f->mutex);

        if (y >= f->p->altura) break;

        calcular_linha(f->imagem, y, f->p);
    }

    return NULL;
}

int calcular_pthreads2(unsigned char *imagem, const Params *p)
{
    const int T = p->num_threads;
    FilaP2 fila;
    pthread_t *threads = NULL;
    int criadas = 0;
    int status  = 0;
    int rc, i;

    fila.proxima = 0;
    fila.imagem  = imagem;
    fila.p       = p;

    rc = pthread_mutex_init(&fila.mutex, NULL);
    if (rc != 0) {
        fprintf(stderr, "Erro: falha ao inicializar o mutex da fila: %s\n",
                strerror(rc));
        return -1;
    }

    threads = malloc((size_t)T * sizeof *threads);
    if (threads == NULL) {
        fprintf(stderr, "Erro: falha ao alocar as estruturas de controle de %d threads.\n", T);
        pthread_mutex_destroy(&fila.mutex);
        return -1;
    }

    for (i = 0; i < T; i++) {
        rc = pthread_create(&threads[i], NULL, trabalhador_p2, &fila);
        if (rc != 0) {
            fprintf(stderr, "Erro: falha ao criar a thread %d de %d: %s\n",
                    i + 1, T, strerror(rc));
            status = -1;
            break;
        }
        criadas++;
    }

    for (i = 0; i < criadas; i++) {
        pthread_join(threads[i], NULL);
    }

    pthread_mutex_destroy(&fila.mutex);
    free(threads);

    return status;
}
