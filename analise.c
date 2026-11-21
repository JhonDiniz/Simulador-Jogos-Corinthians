/* analise.c: le dados/n<N>_s<S>.json (sementes 1..3; a 0 e descartada) e imprime o resumo.
 * Uso: ./analise > dados/resumo.txt
 */
#include "sim.h"
#include <stdio.h>
#include <stdlib.h>

static const int CONDICOES[] = {5, 20, 50};
#define N_COND 3
#define N_REP  3

int main(void) {
    double x[N_COND], prompt_medio[N_COND], gen_medio[N_COND];

    printf("%-4s | %-26s | %-26s | %-18s | %-12s | %-14s | %-22s\n",
           "N", "tempo resposta s (med/max)", "prompt s / gerar s (med)", "tokens prompt/gerado",
           "tokens/s ger", "RAM /api/ps MB", "RAM sistema delta MB");
    printf("-----+----------------------------+----------------------------+--------------------+--------------+----------------+-----------------------\n");

    for (int c = 0; c < N_COND; c++) {
        double soma_t = 0, max_t = 0, soma_p = 0, soma_g = 0, soma_pt = 0, soma_gt = 0;
        double soma_ps = 0, max_ps = 0, soma_ds = 0, max_ds = 0;
        for (int r = 1; r <= N_REP; r++) {
            char nome[64]; snprintf(nome, sizeof nome, "dados/n%d_s%d.json", CONDICOES[c], r);
            char *j = ler_arquivo(nome);
            if (!j) { fprintf(stderr, "falta %s\n", nome); return 1; }

            double p  = json_num(j, "prompt_eval_duration", 0) / 1e9;
            double g  = json_num(j, "eval_duration", 0) / 1e9;
            double pt = json_num(j, "prompt_eval_count", 0);
            double gt = json_num(j, "eval_count", 0);
            double ps = json_num(j, "mem_ps_mb", 0);
            double ds = json_num(j, "mem_sistema_mb", 0) - json_num(j, "mem_sistema_antes_mb", 0);
            double t  = p + g;                      /* tempo de resposta, sem carregar o modelo */

            soma_t += t;  if (t > max_t) max_t = t;
            soma_p += p;  soma_g += g;  soma_pt += pt;  soma_gt += gt;
            soma_ps += ps; if (ps > max_ps) max_ps = ps;
            soma_ds += ds; if (ds > max_ds) max_ds = ds;
            free(j);
        }
        x[c] = CONDICOES[c];
        prompt_medio[c] = soma_p / N_REP;
        gen_medio[c]    = soma_g / N_REP;
        printf("%-4d | %9.2f / %-14.2f | %9.2f / %-14.2f | %7.0f / %-9.0f | %12.1f | %6.0f (max %5.0f) | %8.0f (max %6.0f)\n",
               CONDICOES[c], soma_t / N_REP, max_t, prompt_medio[c], gen_medio[c],
               soma_pt / N_REP, soma_gt / N_REP, soma_gt / soma_g, soma_ps / N_REP, max_ps,
               soma_ds / N_REP, max_ds);
    }

    /* ponto de equilibrio: ajuste linear prompt_s = a*N + b contra o tempo medio de geracao */
    double sx = 0, sy = 0, sxx = 0, sxy = 0, g = 0;
    for (int c = 0; c < N_COND; c++) {
        sx += x[c]; sy += prompt_medio[c]; sxx += x[c] * x[c]; sxy += x[c] * prompt_medio[c]; g += gen_medio[c];
    }
    g /= N_COND;
    double a = (N_COND * sxy - sx * sy) / (N_COND * sxx - sx * sx);
    double b = (sy - a * sx) / N_COND;
    printf("\nAjuste linear: tempo_prompt(N) = %.4f * N + %.3f   (s)\n", a, b);
    printf("Tempo medio de gerar a narracao: %.2f s\n", g);
    if (a > 1e-9) {
        double n_eq = (g - b) / a;
        printf("Ponto de equilibrio estimado: %.0f jogos de historico %s\n", n_eq,
               (n_eq > CONDICOES[N_COND - 1]) ? "(EXTRAPOLADO: alem dos 50 jogos medidos)" :
               (n_eq < CONDICOES[0])          ? "(EXTRAPOLADO: antes dos 5 jogos medidos)" : "(dentro do intervalo medido)");
    } else {
        printf("O tempo de processar o prompt nao cresce com o historico: sem ponto de equilibrio.\n");
    }
    return 0;
}
