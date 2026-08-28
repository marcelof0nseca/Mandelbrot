/* mandelbrot.c - nucleo de calculo. Nao conhece a linha de comando, os nomes dos
 * arquivos de saida nem o relogio: isso tudo e do main.c.
 *
 * A partir do dia 3 o arquivo passa a escrever em stderr, e so nesse caso:
 * quando uma chamada de SISTEMA falha (malloc das estruturas de controle,
 * pthread_create). O motivo e que so aqui se sabe QUAL thread falhou e com que
 * codigo; o main so saberia dizer "a implementacao X falhou", que e menos
 * coerente do que o enunciado pede. Nada vai para stdout em nenhuma hipotese. */
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

/* Mapeamento pixel -> plano complexo, fixado pelos casos de teste oficiais.
 *
 * Usa o CANTO do pixel (x, e nao x + 0.5) e comeca pelo MINIMO do eixo
 * imaginario (linha 0 = -1,5, subindo). Amostrar pelo centro seria mais
 * correto do ponto de vista de imagem, e comecar por cima seria a convencao
 * usual, mas as duas escolhas quebram o gabarito: com altura par, este
 * mapeamento coloca uma linha exatamente sobre ci = 0, onde todo cr em
 * [-2 ; 0,25] pertence ao conjunto e devolve 255. E o que os tres arquivos de
 * teste do professor mostram, e foi verificado ponto a ponto no teste 4x4.
 *
 * Dividir por largura, e nao por (largura - 1), tambem mantem o calculo
 * valido quando largura vale 1. */
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

/* ------------------------------------------------------------------------
 * PTHREADS - ESTRATEGIA 1: blocos contiguos de linhas
 * ------------------------------------------------------------------------
 *
 * A thread i recebe a faixa fechada-aberta [i*altura/T ; (i+1)*altura/T).
 * Divisao ESTATICA: quem calcula o que fica decidido antes de a primeira
 * thread comecar, e nao muda mais.
 *
 * Por que esta estrategia foi escolhida sabendo que ela e a pior:
 * o Mandelbrot e desbalanceado. Ponto dentro do conjunto roda as max_iter
 * completas; ponto fora escapa em 1 a 5 iteracoes. Como o eixo imaginario vai
 * de -1,5 a +1,5, as linhas caras ficam agrupadas no MEIO da imagem, em torno
 * de ci = 0. Blocos contiguos entregam esse miolo inteiro para uma ou duas
 * threads enquanto as das pontas terminam cedo e ficam ociosas - e o tempo
 * total e o da thread mais lenta, porque o pthread_join espera todas. Expor
 * esse efeito e o objetivo; a estrategia 2 (dia 4) e a que o corrige.
 *
 * Nao ha mutex nesta implementacao, e nao ha corrida: as threads escrevem no
 * mesmo buffer, mas em faixas de linhas disjuntas, ou seja, em enderecos que
 * nao se sobrepoem. Corrida exigiria duas threads podendo tocar o MESMO
 * endereco, o que a divisao impede por construcao. */

/* Uma struct por thread. O pthread_create so aceita um unico void*, entao os
 * quatro dados vao empacotados aqui e a thread recebe o endereco da SUA copia.
 *
 * Nao reutilizar uma unica struct para todas as threads: se o laco de criacao
 * sobrescrevesse os campos a cada volta, a thread 0 poderia ainda nao ter lido
 * y_inicio quando os valores da thread 1 ja estivessem no lugar. Seria corrida
 * de dados, nao deterministica, com linha calculada duas vezes e linha nunca
 * calculada. */
typedef struct {
    unsigned char *imagem;
    const Params  *p;
    int            y_inicio;   /* primeira linha da faixa, inclusive */
    int            y_fim;      /* primeira linha FORA da faixa */
} FaixaP1;

static void *trabalhador_p1(void *arg)
{
    const FaixaP1 *f = (const FaixaP1 *)arg;
    int y;

    /* y e local: cada thread tem o seu na propria pilha, como a clausula
     * private do OpenMP fazia automaticamente. */
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
    int criadas = 0;      /* quantas realmente existem: so essas podem ser joinadas */
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

        /* O cast para long antes da multiplicacao evita estouro: com o teto de
         * 100000 linhas e 4096 threads, i * altura chega a 4,1e8 - cabe em int,
         * mas por pouco, e o long tira a duvida sem custo nenhum.
         *
         * y_fim(i) e y_inicio(i+1) sao a MESMA expressao, entao o fim de uma
         * faixa e exatamente o comeco da proxima: sem buraco e sem sobreposicao,
         * mesmo quando altura nao e divisivel por T (algumas threads ficam com
         * uma linha a mais, em vez de a ultima ficar com todo o resto).
         *
         * Quando T > altura, sobram threads com y_inicio == y_fim: a faixa e
         * vazia, o laco do trabalhador nao executa nenhuma vez e a thread
         * termina de imediato. Correto, so inutil - e o caso L4 do roteiro. */
        faixas[i].y_inicio = (int)(((long)i       * (long)p->altura) / (long)T);
        faixas[i].y_fim    = (int)((((long)i + 1) * (long)p->altura) / (long)T);

        /* pthread_create DEVOLVE o codigo de erro e nao mexe em errno. Usar
         * strerror(errno) aqui imprimiria a mensagem de algum erro anterior,
         * sem relacao com a falha real. */
        rc = pthread_create(&threads[i], NULL, trabalhador_p1, &faixas[i]);
        if (rc != 0) {
            fprintf(stderr, "Erro: falha ao criar a thread %d de %d: %s\n",
                    i + 1, T, strerror(rc));
            status = -1;
            break;
        }
        criadas++;
    }

    /* O join acontece mesmo no caminho de erro, e ANTES do free. As threads ja
     * criadas continuam escrevendo em imagem e lendo faixas[i]; sair daqui sem
     * esperar por elas liberaria memoria que ainda esta em uso. */
    for (i = 0; i < criadas; i++) {
        pthread_join(threads[i], NULL);
    }

    free(threads);
    free(faixas);

    /* Se alguma thread nao chegou a ser criada, a faixa dela ficou sem calcular
     * e a imagem esta incompleta. Devolver erro faz o main abortar sem escrever
     * arquivo nenhum - melhor do que gravar um .pgm com lixo. */
    return status;
}
