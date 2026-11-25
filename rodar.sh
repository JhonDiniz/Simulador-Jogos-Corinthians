#!/bin/sh
# Compila, coleta e analisa. Uso: sh rodar.sh [jogos.csv] [modelo]
set -e

CSV="${1:-jogos.csv}"
MODELO="${2:-llama3.1:8b}"

echo "==> Compilando..."
cc -O2 -Wall -Wextra -o coleta  coleta.c  sim.c -lcurl -lpthread -lm
cc -O2 -Wall -Wextra -o analise analise.c sim.c -lm
mkdir -p dados
echo "ok: coleta e analise compilados"

echo ""
echo "==> Coletando (30 chamadas ao Ollama, pode demorar varios minutos)..."
./coleta "$CSV" "$MODELO"

echo ""
echo "==> Analisando..."
./analise | tee dados/resumo.txt

echo ""
echo "Pronto. Resultados em dados/resumo.txt"
