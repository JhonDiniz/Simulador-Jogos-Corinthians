/* teste_sim.c: confere Monte Carlo e prompt SEM o Ollama.
 * Uso: ./teste_sim jogos.csv
 * Deve mostrar: mesma semente => mesmos eventos; o tamanho do prompt cresce com N.
 */
#include "sim.h"
#include <stdio.h>

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "uso: %s jogos.csv\n", argv[0]); return 1; }
    static Jogo jogos[MAX_JOGOS];
    int total = carregar_csv(argv[1], jogos, MAX_JOGOS);
    if (total < BASE_JOGOS) { fprintf(stderr, "preciso de >= %d jogos, li %d\n", BASE_JOGOS, total); return 1; }
    const Jogo *base = jogos + (total - BASE_JOGOS);
    printf("jogos lidos: %d (base do Monte Carlo: ultimos %d)\n\n", total, BASE_JOGOS);

    static char prompt[65536];
    for (int sem = 1; sem <= 3; sem++) {
        Evento ev[MAX_EVENTOS];
        int n_ev = simular_jogo(base, BASE_JOGOS, (unsigned long long)sem, ev, MAX_EVENTOS);
        printf("semente %d: %d eventos ->", sem, n_ev);
        for (int i = 0; i < n_ev; i++) printf(" %d'%s;", ev[i].min, ev[i].tipo);
        printf("\n");
        const int ns[] = {5, 20, 50};
        for (int k = 0; k < 3; k++) {
            int tam = montar_prompt(jogos, total, ns[k], ev, n_ev, prompt, sizeof prompt);
            printf("   N=%2d: prompt com %5d caracteres (~%d tokens)\n", ns[k], tam, tam / 4);
        }
    }
    return 0;
}
