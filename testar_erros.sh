TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

falhas=0
total=0

verificar() {
    local id="$1" desc="$2" espera="$3" chave="$4"
    shift 4
    local cod erro=0

    total=$((total + 1))

    timeout 120 ./mandelbrot "$@" >"$TMP/out.txt" 2>"$TMP/err.txt"
    cod=$?

    echo ""
    echo "--------------------------------------------------------------"
    echo "$id  $desc"
    echo "    comando : ./mandelbrot $*"
    echo "    saida   : $cod   (esperado: $espera)"
    echo "    stdout  : $(wc -c <"$TMP/out.txt") bytes"
    echo "    stderr  : $(head -2 "$TMP/err.txt")"

    if [ "$cod" -eq 124 ]; then
        echo "    FALHOU: estourou o tempo limite de 120s"
        erro=1
    elif [ "$espera" = "ok" ] && [ "$cod" -ne 0 ]; then
        echo "    FALHOU: esperava sucesso, veio $cod"
        erro=1
    elif [ "$espera" = "erro" ] && [ "$cod" -eq 0 ]; then
        echo "    FALHOU: esperava erro, veio 0"
        erro=1
    fi

    if [ -s "$TMP/out.txt" ]; then
        echo "    FALHOU: escreveu em stdout"
        erro=1
    fi

    if [ "$espera" = "erro" ]; then
        if [ ! -s "$TMP/err.txt" ]; then
            echo "    FALHOU: stderr vazio"
            erro=1
        elif [ -n "$chave" ] && ! grep -qi -- "$chave" "$TMP/err.txt"; then
            echo "    FALHOU: stderr nao menciona '$chave'"
            erro=1
        fi
    else
        local n
        n=$(md5sum mandelbrot_maf_*.pgm 2>/dev/null | awk '{print $1}' | sort -u | wc -l)
        echo "    hashes  : $n distinto(s)"
        if [ "$n" -ne 1 ]; then
            echo "    FALHOU: as quatro imagens divergiram"
            erro=1
        fi
    fi

    if [ "$erro" -eq 0 ]; then
        echo "    OK"
    else
        falhas=$((falhas + 1))
    fi
}

echo "=============================================================="
echo "VARREDURA DE ERROS E CASOS-LIMITE"
date
echo "=============================================================="

echo ""
echo "### 1. NUMERO DE ARGUMENTOS"
verificar E1 "Sem argumentos"        erro "argumento"
verificar E2 "Argumentos de menos"   erro "argumento"  100 100 50
verificar E3 "Argumentos de mais"    erro "argumento"  100 100 50 4 9

echo ""
echo "### 2. LARGURA"
verificar E4 "Largura zero"          erro "largura"  0 100 50 4
verificar E5 "Largura negativa"      erro "largura"  -10 100 50 4
verificar E6 "Largura nao numerica"  erro "inteiro"  abc 100 50 4
verificar E7 "Lixo depois do numero" erro "invalido" 100xyz 100 50 4
verificar E8 "Valor fracionario"     erro "invalido" 100.5 100 50 4
verificar E9 "Estouro de long"       erro "faixa"    99999999999999999999 100 50 4

echo ""
echo "### 3. DEMAIS PARAMETROS"
verificar E10 "Altura invalida"      erro "altura"        100 0 50 4
verificar E11 "Iteracoes invalidas"  erro "max_iteracoes" 100 100 0 4
verificar E12 "Threads zero"         erro "num_threads"   100 100 50 0
verificar E13 "Threads negativas"    erro "num_threads"   100 100 50 -4
verificar E14 "Threads absurdas"     erro "num_threads"   100 100 50 999999
verificar E15 "Argumento vazio"      erro "vazio"         "" 100 50 4

echo ""
echo "### 4. FALHA DE ALOCACAO"
verificar E16 "Imagem gigante"       erro "alocar"  100000 100000 10 4

echo ""
echo "### 5. CASOS-LIMITE (devem funcionar)"
verificar L1 "Imagem 1x1"            ok "" 1 1 1 1
verificar L2 "Coluna, largura 1"     ok "" 1 1000 100 6
verificar L3 "Linha unica"           ok "" 1000 1 100 6
verificar L4 "Mais threads que linhas" ok "" 100 3 100 6
verificar L5 "Threads acima dos nucleos" ok "" 1024 1024 1000 12
verificar L6 "max_iter = 1"          ok "" 100 100 1 4

echo ""
echo "=============================================================="
date
if [ "$falhas" -eq 0 ]; then
    echo "RESULTADO: $total de $total casos passaram"
else
    echo "RESULTADO: $falhas de $total casos FALHARAM"
fi
echo "=============================================================="
