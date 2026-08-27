#!/usr/bin/env bash
# testar_oficial.sh - compara as saidas com os casos de teste do professor.
#
# Uso: bash testar_oficial.sh [diretorio]   (padrao: testes_professor)
#
# Cada arquivo de teste traz o comando, o .pgm a verificar e o conteudo
# esperado. O script roda o comando e compara o bloco esperado com TODOS os
# .pgm gerados - as implementacoes precisam produzir a mesma imagem, entao o
# gabarito de uma vale para todas.
#
# A normalizacao com tr/sed remove \r (os arquivos vieram de um Mac) e espaco
# no fim de linha, para a comparacao nao falhar por caractere invisivel.

DIR=${1:-testes_professor}
falhas=0

comparar() {
    local teste="$1" largura="$2" altura="$3" iter="$4" threads="$5"

    echo ""
    echo "=============================================================="
    echo "TESTE:   $teste"
    echo "COMANDO: ./mandelbrot $largura $altura $iter $threads"
    echo "=============================================================="

    if [ ! -f "$DIR/$teste" ]; then
        echo "PULADO: $DIR/$teste nao encontrado"
        return
    fi

    if ! ./mandelbrot "$largura" "$altura" "$iter" "$threads"; then
        echo "FALHOU: erro na execucao do programa"
        falhas=$((falhas + 1))
        return
    fi

    sed -n '/Conteudo esperado:/,$p' "$DIR/$teste" | tail -n +2 \
        | tr -d '\r' | sed 's/[[:space:]]*$//' | sed '/^$/d' > /tmp/esperado.txt

    for pgm in mandelbrot_maf_*.pgm; do
        tr -d '\r' < "$pgm" | sed 's/[[:space:]]*$//' | sed '/^$/d' > /tmp/obtido.txt
        if diff -u /tmp/esperado.txt /tmp/obtido.txt > /tmp/diferenca.txt; then
            echo "  OK      $pgm"
        else
            echo "  FALHOU  $pgm"
            echo "  --- esperado (-) vs obtido (+) ---"
            sed 's/^/  /' /tmp/diferenca.txt | head -30
            falhas=$((falhas + 1))
        fi
    done
}

comparar teste1_serial.txt     4  4 50 1
comparar teste2_openmp.txt     6  6 30 3
comparar teste3_pthreads1.txt 10  6 40 4

echo ""
echo "=============================================================="
echo "times.txt gerado agora:"
cat times.txt
echo "formato de referencia do professor:"
cat "$DIR/times.txt"
echo "=============================================================="

if [ "$falhas" -eq 0 ]; then
    echo "RESULTADO: todas as comparacoes bateram com o gabarito"
else
    echo "RESULTADO: $falhas comparacao(oes) falharam"
fi
