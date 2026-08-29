#ifndef MANDELBROT_H
#define MANDELBROT_H

#define REAL_MIN (-2.0)
#define REAL_MAX ( 1.0)
#define IMAG_MIN (-1.5)
#define IMAG_MAX ( 1.5)

typedef struct {
    int largura;
    int altura;
    int max_iter;
    int num_threads;
} Params;

int mandelbrot_ponto(double cr, double ci, int max_iter);

double coord_real(int x, const Params *p);
double coord_imag(int y, const Params *p);

void calcular_linha(unsigned char *imagem, int y, const Params *p);

int calcular_serial(unsigned char *imagem, const Params *p);
int calcular_openmp(unsigned char *imagem, const Params *p);
int calcular_pthreads1(unsigned char *imagem, const Params *p);
int calcular_pthreads2(unsigned char *imagem, const Params *p);

#endif
