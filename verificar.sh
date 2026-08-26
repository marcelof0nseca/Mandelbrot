#!/usr/bin/env bash
# verificar.sh - roda o programa e confere as saidas geradas.
#
# Uso:  bash verificar.sh [largura] [altura] [max_iteracoes] [num_threads]
# Padrao: 800 800 500 6
#
# O 'set -x' faz o bash ecoar cada comando (prefixado por '+') antes de
# executa-lo. E o que garante que o evidencias.log registre comando E saida,
# em vez de so a saida solta.

LARGURA=${1:-800}
ALTURA=${2:-800}
ITER=${3:-500}
THREADS=${4:-6}

set -x

# 1) execucao normal, com stdout desviado para arquivo
./mandelbrot "$LARGURA" "$ALTURA" "$ITER" "$THREADS" > saida_stdout.txt
echo "codigo de saida: $?"

# 2) stdout tem que estar vazio: o enunciado proibe saida padrao
wc -c saida_stdout.txt

# 3) arquivos gerados e tempos
ls -la mandelbrot_maf_*.pgm times.txt
cat times.txt

# 4) formato: sem cabecalho, so numeros e espacos
head -c 200 mandelbrot_maf_serial.pgm
echo

# 5) dimensoes: numero de linhas = altura, valores na 1a linha = largura
wc -l < mandelbrot_maf_serial.pgm
head -1 mandelbrot_maf_serial.pgm | wc -w

# 6) normalizacao: menor e maior valor presentes no arquivo
tr ' ' '\n' < mandelbrot_maf_serial.pgm | sort -n | sed -n '1p;$p'

# 7) igualdade entre as implementacoes (com uma so, imprime um hash)
md5sum mandelbrot_maf_*.pgm

set +x

rm -f saida_stdout.txt
echo "--------------------------------------------------------------"
echo "ESPERADO: stdout 0 bytes | ${ALTURA} linhas | ${LARGURA} valores"
echo "          menor valor 0 | maior valor 255 | hashes iguais"
echo "--------------------------------------------------------------"
