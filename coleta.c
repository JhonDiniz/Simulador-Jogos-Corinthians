/* coleta.c: roda as condicoes (5/20/50 jogos de historico) no Ollama e salva a saida bruta.
 *
 * Uso:   ./coleta jogos.csv [modelo]
 * Saida: dados/n<N>_s<SEMENTE>.json  (semente 0 = aquecimento, descartada na analise)
 */
#include "sim.h"
#include <curl/curl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#ifdef __APPLE__
#include <mach/mach.h>
#endif

#define URL "http://localhost:11434"

static const int CONDICOES[] = {5, 20, 50};
static const int NUM_CTX[]   = {2048, 4096, 8192};   /* cresce junto com o historico */
static const int SEMENTES[]  = {0, 1, 2, 3};          /* 0 = aquecimento */
#define N_COND 3
#define N_SEM  4

typedef struct { char *dados; size_t tam; } Buf;

static size_t escreve(char *p, size_t s, size_t n, void *u) {
    Buf *b = u; size_t t = s * n;
    char *novo = realloc(b->dados, b->tam + t + 1);
    if (!novo) return 0;
    b->dados = novo;
    memcpy(b->dados + b->tam, p, t);
    b->tam += t; b->dados[b->tam] = 0;
    return t;
}

static CURL *novo_curl(Buf *b) {
    CURL *c = curl_easy_init();
    curl_easy_setopt(c, CURLOPT_NOPROXY, "localhost,127.0.0.1");
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, escreve);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, b);
    return c;
}

/* memoria usada no sistema, em MB (macOS: ativa + wired + comprimida). Aproximacao. */
static double ram_sistema_mb(void) {
#ifdef __APPLE__
    mach_port_t host = mach_host_self();
    vm_statistics64_data_t vm;
    mach_msg_type_number_t cnt = HOST_VM_INFO64_COUNT;
    if (host_statistics64(host, HOST_VM_INFO64, (host_info64_t)&vm, &cnt) != KERN_SUCCESS) return 0;
    vm_size_t pagina; host_page_size(host, &pagina);
    return ((double)vm.active_count + vm.wire_count + vm.compressor_page_count) * pagina / 1048576.0;
#else
    return 0;
#endif
}

/* ---------- thread que amostra a memoria enquanto o Ollama gera ---------- */
typedef struct { volatile int parar; double ps_max_mb, sis_max_mb; } Amostra;

static void *amostrar(void *arg) {
    Amostra *a = arg;
    while (!a->parar) {
        Buf b = {0};
        CURL *c = novo_curl(&b);
        curl_easy_setopt(c, CURLOPT_URL, URL "/api/ps");
        if (curl_easy_perform(c) == CURLE_OK && b.dados) {
            double mb = json_num(b.dados, "size", 0) / 1048576.0;   /* modelo + cache de contexto */
            if (mb > a->ps_max_mb) a->ps_max_mb = mb;
        }
        curl_easy_cleanup(c); free(b.dados);
        double r = ram_sistema_mb();
        if (r > a->sis_max_mb) a->sis_max_mb = r;
        usleep(300000);
    }
    return NULL;
}

/* ---------- JSON ---------- */
static char *json_escape(const char *s) {
    char *o = malloc(strlen(s) * 2 + 1), *q = o;
    for (; *s; s++) {
        if (*s == '"' || *s == '\\') { *q++ = '\\'; *q++ = *s; }
        else if (*s == '\n')         { *q++ = '\\'; *q++ = 'n'; }
        else if ((unsigned char)*s < 0x20) { /* ignora controle */ }
        else *q++ = *s;
    }
    *q = 0;
    return o;
}

static double agora_s(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "uso: %s jogos.csv [modelo]\n", argv[0]); return 1; }
    const char *modelo = argc > 2 ? argv[2] : "llama3.1:8b";

    static Jogo jogos[MAX_JOGOS];
    int total = carregar_csv(argv[1], jogos, MAX_JOGOS);
    if (total < BASE_JOGOS) { fprintf(stderr, "preciso de >= %d jogos, li %d\n", BASE_JOGOS, total); return 1; }
    const Jogo *base = jogos + (total - BASE_JOGOS);   /* base fixa do Monte Carlo */

    mkdir("dados", 0755);
    curl_global_init(CURL_GLOBAL_DEFAULT);

    for (int c = 0; c < N_COND; c++) {
        for (int s = 0; s < N_SEM; s++) {
            int n = CONDICOES[c], sem = SEMENTES[s];
            Evento ev[MAX_EVENTOS];
            int n_ev = simular_jogo(base, BASE_JOGOS, (unsigned long long)sem, ev, MAX_EVENTOS);

            static char prompt[65536];
            if (montar_prompt(jogos, total, n, ev, n_ev, prompt, sizeof prompt) < 0) {
                fprintf(stderr, "prompt grande demais\n"); return 1;
            }
            char *esc = json_escape(prompt);
            size_t cap = strlen(esc) + 512;
            char *corpo = malloc(cap);
            snprintf(corpo, cap,
                "{\"model\":\"%s\",\"prompt\":\"%s\",\"stream\":false,\"keep_alive\":0,"
                "\"options\":{\"num_ctx\":%d,\"temperature\":0,\"seed\":42,\"num_predict\":800}}",
                modelo, esc, NUM_CTX[c]);

            double ram_antes = ram_sistema_mb();
            Amostra am = {0};
            pthread_t th; pthread_create(&th, NULL, amostrar, &am);

            Buf resp = {0};
            CURL *h = novo_curl(&resp);
            struct curl_slist *hdr = curl_slist_append(NULL, "Content-Type: application/json");
            curl_easy_setopt(h, CURLOPT_URL, URL "/api/generate");
            curl_easy_setopt(h, CURLOPT_HTTPHEADER, hdr);
            curl_easy_setopt(h, CURLOPT_POSTFIELDS, corpo);
            curl_easy_setopt(h, CURLOPT_TIMEOUT, 900L);
            double t0 = agora_s();
            CURLcode rc = curl_easy_perform(h);
            double wall = agora_s() - t0;

            am.parar = 1; pthread_join(th, NULL);
            curl_slist_free_all(hdr); curl_easy_cleanup(h);

            if (rc != CURLE_OK || !resp.dados) {
                fprintf(stderr, "falha n=%d semente=%d: %s\n", n, sem, curl_easy_strerror(rc));
                return 1;
            }

            char nome[64]; snprintf(nome, sizeof nome, "dados/n%d_s%d.json", n, sem);
            FILE *f = fopen(nome, "w");
            fprintf(f, "{\"n\":%d,\"semente\":%d,\"num_ctx\":%d,\"eventos\":%d,\"wall_s\":%.3f,"
                       "\"mem_ps_mb\":%.1f,\"mem_sistema_mb\":%.1f,\"mem_sistema_antes_mb\":%.1f,\"resp\":%s}\n",
                    n, sem, NUM_CTX[c], n_ev, wall, am.ps_max_mb, am.sis_max_mb, ram_antes, resp.dados);
            fclose(f);

            double pc = json_num(resp.dados, "prompt_eval_count", 0);
            double gc = json_num(resp.dados, "eval_count", 0);
            printf("n=%2d semente=%d eventos=%2d prompt=%4.0f tok gerou=%4.0f tok | cabe em num_ctx=%d? %s | %.1fs\n",
                   n, sem, n_ev, pc, gc, NUM_CTX[c], (pc + gc < NUM_CTX[c]) ? "sim" : "NAO (cortado!)", wall);
            fflush(stdout);

            free(esc); free(corpo); free(resp.dados);
        }
    }
    curl_global_cleanup();
    return 0;
}
