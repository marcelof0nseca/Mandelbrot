/* mandelbrot.h - tipos e prototipos compartilhados pelas quatro implementacoes.
 * Comentarios sem acento: o fonte e editado no Windows e compilado no WSL. */
#ifndef MANDELBROT_H
#define MANDELBROT_H

/* Regiao do plano complexo fixada pelo enunciado. */
#define REAL_MIN (-2.0)
#define REAL_MAX ( 1.0)
#define IMAG_MIN (-1.5)
#define IMAG_MAX ( 1.5)

/* Parametros vindos da linha de comando. Passado sempre como ponteiro const:
 * varias threads lendo a mesma struct e seguro, escrever nao seria. */
typedef struct {
    int largura;
    int altura;
    int max_iter;
    int num_threads;
} Params;

/* Iteracoes de z = z^2 + c ate |z| > 2; devolve max_iter se nao escapou. */
int mandelbrot_ponto(double cr, double ci, int max_iter);

/* Indice de pixel -> coordenada do plano. Calculadas a partir do indice
 * inteiro, nunca acumuladas, para independer da ordem de processamento. */
double coord_real(int x, const Params *p);
double coord_imag(int y, const Params *p);

/* Funcao que garante saidas byte a byte iguais: as quatro implementacoes
 * chamam este mesmo codigo e so discordam sobre qual thread pega qual linha. */
void calcular_linha(unsigned char *imagem, int y, const Params *p);

/* Devolvem 0 em sucesso, valor diferente de 0 em falha.
 *
 * Estrategia 1 de Pthreads: blocos CONTIGUOS de linhas, divisao estatica
 * decidida antes de qualquer thread comecar. Escolhida de proposito por ser a
 * que EXPOE o desbalanceamento do Mandelbrot - ver comentario em mandelbrot.c. */
int calcular_serial(unsigned char *imagem, const Params *p);
int calcular_openmp(unsigned char *imagem, const Params *p);
int calcular_pthreads1(unsigned char *imagem, const Params *p);

#endif /* MANDELBROT_H */
