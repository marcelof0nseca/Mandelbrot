#!/usr/bin/env bash
# medir.sh - varredura do numero de threads para montar a tabela de tempos.
#
# Uso: bash medir.sh [largura] [altura] [max_iteracoes]
# Padrao: 1600 1600 1000
#
# Tres repeticoes por configuracao: a variacao entre corridas identicas medida
# no dia 1 foi de 2,4%, e uma medicao unica nao distingue ganho real de ruido.
#
# Comparar tempos de execucoes com parametros diferentes nao significa nada,
# por isso o cabecalho imprime os parametros usados em toda a varredura.

LARGURA=${1:-1600}
ALTURA=${2:-1600}
ITER=${3:-1000}

echo "=============================================================="
echo "parametros fixos: ${LARGURA}x${ALTURA}, max_iter=${ITER}"
echo "maquina: $(nproc) CPUs logicas"
date
echo "=============================================================="

for T in 1 2 3 4 5 6; do
    echo ""
    echo "--- num_threads = ${T} ---"
    for R in 1 2 3; do
        if ! ./mandelbrot "$LARGURA" "$ALTURA" "$ITER" "$T"; then
            echo "FALHOU em threads=${T} repeticao=${R}"
            exit 1
        fi
        printf "  rep%d | " "$R"
        tr '\n' '|' < times.txt
        echo ""
    done
done

echo ""
echo "=============================================================="
date
echo "hashes distintos (tem que ser 1):"
md5sum mandelbrot_maf_*.pgm | awk '{print $1}' | sort -u | wc -l
echo "=============================================================="
