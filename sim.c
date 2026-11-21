#include "sim.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ---------- gerador pseudoaleatorio (splitmix64): deterministico em qualquer maquina ---------- */
static unsigned long long estado;

static unsigned long long prox(void) {
    unsigned long long z = (estado += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

/* uniforme em (0,1) */
static double unif(void) { return ((prox() >> 11) + 0.5) / 9007199254740992.0; }

/* Poisson pelo metodo de Knuth (otimo para medias pequenas, como gols e cartoes) */
static int poisson(double lambda) {
    if (lambda <= 0) return 0;
    double L = exp(-lambda), p = 1.0;
    int k = 0;
    do { k++; p *= unif(); } while (p > L);
    return k - 1;
}

/* ---------- CSV ---------- */
int carregar_csv(const char *caminho, Jogo *out, int max) {
    FILE *f = fopen(caminho, "r");
    if (!f) { perror(caminho); return -1; }
    char linha[512];
    int n = 0;
    if (!fgets(linha, sizeof linha, f)) { fclose(f); return -1; } /* descarta cabecalho */
    while (n < max && fgets(linha, sizeof linha, f)) {
        linha[strcspn(linha, "\r\n")] = 0;
        if (!linha[0]) continue;
        Jogo j; memset(&j, 0, sizeof j);
        int campos = sscanf(linha, "%15[^,],%47[^,],%7[^,],%d,%d,%d,%d,%d,%lf",
                            j.data, j.adversario, j.mando,
                            &j.gols_pro, &j.gols_contra, &j.amarelos, &j.vermelhos,
                            &j.chutes, &j.posse);
        if (campos < 7) { fprintf(stderr, "linha ignorada: %s\n", linha); continue; }
        out[n++] = j;
    }
    fclose(f);
    return n;
}

/* ---------- Monte Carlo ---------- */

/* TODO (Jhon): ESTA E A SUA DECISAO DE MODELAGEM.
   Hoje: media simples dos 50 jogos da base.
   Alternativas: (a) media ponderada com mais peso nos jogos recentes;
                 (b) medias separadas para casa e fora (usa o campo "mando").
   Se mudar, justifique na ficha: muda o jogo sorteado, nao o custo medido. */
static void estimar_medias(const Jogo *b, int n, double *pro, double *contra,
                           double *amarelo, double *vermelho) {
    double sp = 0, sc = 0, sa = 0, sv = 0;
    for (int i = 0; i < n; i++) {
        sp += b[i].gols_pro; sc += b[i].gols_contra;
        sa += b[i].amarelos; sv += b[i].vermelhos;
    }
    *pro = sp / n; *contra = sc / n; *amarelo = sa / n; *vermelho = sv / n;
}

static int cmp_evento(const void *a, const void *b) {
    return ((const Evento *)a)->min - ((const Evento *)b)->min;
}

static int add_eventos(Evento *ev, int n, int max, int qtd, const char *tipo) {
    for (int i = 0; i < qtd && n < max; i++) {
        ev[n].min = 1 + (int)(prox() % 90);
        snprintf(ev[n].tipo, sizeof ev[n].tipo, "%s", tipo);
        n++;
    }
    return n;
}

int simular_jogo(const Jogo *base, int n_base, unsigned long long semente,
                 Evento *ev, int max_ev) {
    estado = semente * 0x2545F4914F6CDD1DULL + 12345ULL;
    prox(); /* descarta a primeira saida: sementes 1,2,3 ficam bem diferentes */
    double pro, contra, am, vm;
    estimar_medias(base, n_base, &pro, &contra, &am, &vm);
    int n = 0;
    n = add_eventos(ev, n, max_ev, poisson(pro),    "gol do Corinthians");
    n = add_eventos(ev, n, max_ev, poisson(contra), "gol do adversario");
    n = add_eventos(ev, n, max_ev, poisson(am),     "cartao amarelo");
    n = add_eventos(ev, n, max_ev, poisson(vm),     "cartao vermelho");
    qsort(ev, n, sizeof *ev, cmp_evento);
    return n;
}

/* ---------- prompt ---------- */
#define CHECA() do { if (off >= cap) return -1; } while (0)

int montar_prompt(const Jogo *jogos, int total, int n_hist,
                  const Evento *ev, int n_ev, char *buf, size_t cap) {
    if (n_hist > total) n_hist = total;
    size_t off = 0;
    off += snprintf(buf + off, cap - off,
        "Voce e um narrador esportivo brasileiro. Historico recente do Corinthians "
        "(do mais antigo ao mais recente):\n"); CHECA();
    for (int i = total - n_hist; i < total; i++) {
        const Jogo *j = &jogos[i];
        off += snprintf(buf + off, cap - off, "%s Corinthians %dx%d %s (%s)",
                        j->data, j->gols_pro, j->gols_contra, j->adversario, j->mando); CHECA();
        if (j->chutes > 0) {
            off += snprintf(buf + off, cap - off, " chutes %d posse %.0f%%", j->chutes, j->posse); CHECA();
        }
        off += snprintf(buf + off, cap - off, "\n"); CHECA();
    }
    off += snprintf(buf + off, cap - off, "\nEventos do jogo simulado (minuto: lance):\n"); CHECA();
    for (int i = 0; i < n_ev; i++) {
        off += snprintf(buf + off, cap - off, "%d': %s\n", ev[i].min, ev[i].tipo); CHECA();
    }
    off += snprintf(buf + off, cap - off,
        "\nNarre apenas esses momentos, em portugues, um paragrafo curto por evento."); CHECA();
    return (int)off;
}

/* ---------- JSON minimo ---------- */
double json_num(const char *json, const char *chave, double padrao) {
    char busca[80];
    snprintf(busca, sizeof busca, "\"%s\":", chave);
    const char *p = strstr(json, busca);
    return p ? strtod(p + strlen(busca), NULL) : padrao;
}

char *ler_arquivo(const char *caminho) {
    FILE *f = fopen(caminho, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long t = ftell(f); fseek(f, 0, SEEK_SET);
    char *b = malloc(t + 1);
    if (b && fread(b, 1, t, f) == (size_t)t) b[t] = 0; else { free(b); b = NULL; }
    fclose(f);
    return b;
}
