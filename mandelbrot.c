/* mandelbrot.c - nucleo de calculo. Nao conhece argumentos, arquivo nem relogio. */
#include <stddef.h>
#include "mandelbrot.h"

int mandelbrot_ponto(double cr, double ci, int max_iter)
{
    double zr = 0.0, zi = 0.0;
    int i;

    for (i = 0; i < max_iter; i++) {
        double zr2 = zr * zr;
        double zi2 = zi * zi;

        /* |z| > 2 escrito como zr^2 + zi^2 > 4: evita a raiz quadrada, que
         * seria a operacao mais cara deste laco. */
        if (zr2 + zi2 > 4.0) break;

        /* zi antes de zr: zi usa o zr antigo. Inverter as duas linhas gera
         * uma imagem errada que ainda parece um fractal. */
        zi = 2.0 * zr * zi + ci;
        zr = zr2 - zi2 + cr;
    }

    return i;
}

/* (x + 0.5) e o centro do pixel; dividir por largura, e nao por (largura - 1),
 * mantem o calculo valido quando largura vale 1. */
double coord_real(int x, const Params *p)
{
    return REAL_MIN + ((double)x + 0.5) * (REAL_MAX - REAL_MIN) / (double)p->largura;
}

/* Subtrai de IMAG_MAX porque a linha 0 e o topo da imagem. */
double coord_imag(int y, const Params *p)
{
    return IMAG_MAX - ((double)y + 0.5) * (IMAG_MAX - IMAG_MIN) / (double)p->altura;
}

void calcular_linha(unsigned char *imagem, int y, const Params *p)
{
    const double ci = coord_imag(y, p);          /* constante ao longo da linha */
    const size_t base = (size_t)y * (size_t)p->largura;
    int x;

    for (x = 0; x < p->largura; x++) {
        int iter = mandelbrot_ponto(coord_real(x, p), ci, p->max_iter);

        /* Normalizacao inteira, nao em double: o criterio de correcao e
         * igualdade byte a byte. O long evita estouro de iter * 255. */
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

    /* O que cada clausula faz:
     *
     * parallel for      - a libgomp acorda num_threads threads, reparte as
     *                     iteracoes de y entre elas e poe uma barreira no fim.
     *                     y vira privado de cada thread automaticamente.
     *
     * schedule(dynamic, 1) - cada thread pega UMA linha, termina e volta para
     *                     pegar a proxima livre. Com static (o padrao) a
     *                     divisao seria em blocos contiguos, e as threads que
     *                     pegassem o miolo do conjunto - onde todo ponto roda
     *                     as max_iter completas - terminariam muito depois das
     *                     que pegaram as bordas.
     *
     * num_threads(...)  - respeita o argumento da linha de comando sem mexer
     *                     no estado global do runtime, ao contrario de
     *                     omp_set_num_threads().
     *
     * Nao ha condicao de corrida: as threads escrevem no mesmo buffer, mas em
     * linhas diferentes, ou seja, em faixas de memoria que nao se sobrepoem.
     * Tudo o que calcular_linha usa e declarado dentro dela, na pilha de cada
     * thread; p e const e so lido. */
    #pragma omp parallel for schedule(dynamic, 1) num_threads(p->num_threads)
    for (y = 0; y < p->altura; y++) {
        calcular_linha(imagem, y, p);
    }

    return 0;
}
