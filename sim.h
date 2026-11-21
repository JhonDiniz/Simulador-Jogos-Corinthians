#ifndef SIM_H
#define SIM_H
#include <stddef.h>

#define MAX_JOGOS   512
#define MAX_EVENTOS 64
#define BASE_JOGOS  50   /* base FIXA do Monte Carlo: sempre os ultimos 50 jogos */

typedef struct {
    char   data[16], adversario[48], mando[8];
    int    gols_pro, gols_contra, amarelos, vermelhos, chutes;
    double posse;
} Jogo;

typedef struct { int min; char tipo[40]; } Evento;

/* CSV: data,adversario,mando,gols_pro,gols_contra,amarelos,vermelhos[,chutes,posse]
   Retorna quantos jogos leu (ordem cronologica), ou -1 em erro. */
int carregar_csv(const char *caminho, Jogo *out, int max);

/* Sorteia os eventos de 1 jogo. Mesma semente + mesma base => mesmo jogo. */
int simular_jogo(const Jogo *base, int n_base, unsigned long long semente,
                 Evento *ev, int max_ev);

/* Monta o prompt: ultimos n_hist jogos + eventos sorteados. Retorna tamanho ou -1. */
int montar_prompt(const Jogo *jogos, int total, int n_hist,
                  const Evento *ev, int n_ev, char *buf, size_t cap);

/* Utilitarios de JSON (so o necessario para ler os campos numericos do Ollama) */
double json_num(const char *json, const char *chave, double padrao);
char  *ler_arquivo(const char *caminho);

#endif
