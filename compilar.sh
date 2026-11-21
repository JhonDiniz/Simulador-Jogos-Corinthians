#!/bin/sh
# Compila os 3 programas. Uso: sh compilar.sh
set -e
cc -O2 -Wall -Wextra -o teste_sim teste_sim.c sim.c -lm
cc -O2 -Wall -Wextra -o coleta    coleta.c    sim.c -lcurl -lpthread -lm
cc -O2 -Wall -Wextra -o analise   analise.c   sim.c -lm
echo "ok: teste_sim, coleta e analise compilados"
