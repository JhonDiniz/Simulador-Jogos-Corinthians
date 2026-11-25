# Le os JSON da coleta (dados/n<N>_s<S>.json, sementes 1..5; a semente 0 e aquecimento) e gera os graficos do relatorio.
# Uso: python3 gera_graficos.py <pasta_dados> <pasta_saida>
import json, sys, os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

dados, saida = sys.argv[1], sys.argv[2]
CONDS = [5, 20, 50, 100, 150]
SEMS = [1, 2, 3, 4, 5]
R = {}
for n in CONDS:
    for s in SEMS:
        d = json.load(open(os.path.join(dados, f"n{n}_s{s}.json")))
        r = d["resp"]
        p = r["prompt_eval_duration"] / 1e9
        g = r["eval_duration"] / 1e9
        R[(n, s)] = dict(p=p, g=g, t=p + g, ptok=r["prompt_eval_count"], gtok=r["eval_count"],
                         vel=r["eval_count"] / g, mst=1000 * p / r["prompt_eval_count"], ps=d["mem_ps_mb"], num_ctx=d["num_ctx"])
def vals(n, k): return [R[(n, s)][k] for s in SEMS]
def media(l): return sum(l) / len(l)

json.dump({str(n): {k: vals(n, k) for k in ("p", "g", "t", "ptok", "gtok", "vel", "mst", "ps", "num_ctx")} for n in CONDS},
          open(os.path.join(saida, "numeros.json"), "w"), indent=1)

AZ, LR, VD, CZ = "#1F5FA8", "#C0562B", "#2E7D4F", "#6B6B6B"
plt.rcParams.update({"font.size": 12, "axes.spines.top": False, "axes.spines.right": False})
XT = dict(xticks=CONDS)

def pontos(ax, k, cor):
    ax.scatter([n for n in CONDS for _ in SEMS], [v for n in CONDS for v in vals(n, k)], s=24, color=cor, alpha=0.4, zorder=3)
    mm = [media(vals(n, k)) for n in CONDS]
    ax.plot(CONDS, mm, "-o", color=cor, lw=2.4, ms=7, zorder=4)
    return mm

# ---- grafico 1: tempo ----
fig, ax = plt.subplots(figsize=(8.2, 4.7), dpi=200)
ax.axvspan(100, 150, color="#EEE8D5", alpha=0.7, zorder=0)
ax.text(125, 3.2, "o prompt passa a narração\nentre 100 e 150 jogos", ha="center", fontsize=10.5, color="#6B5B1F")
mp = pontos(ax, "p", AZ); mg = pontos(ax, "g", LR)
ax.annotate("processar o prompt", (150, mp[-1]), xytext=(-8, 14), textcoords="offset points", ha="right", color=AZ, fontweight="bold")
ax.annotate("gerar a narração", (150, mg[-1]), xytext=(-8, -22), textcoords="offset points", ha="right", color=LR, fontweight="bold")
ax.set(xticks=CONDS, xlim=(0, 160), ylim=(0, 38))
ax.set_xlabel("jogos de histórico no prompt (quantidade)"); ax.set_ylabel("tempo (s)")
ax.set_title("O tempo de processar o prompt cresce mais que o de gerar a narração\ne passa dele entre 100 e 150 jogos", fontsize=13, loc="left")
fig.tight_layout(); fig.savefig(os.path.join(saida, "grafico_tempo.png")); plt.close(fig)

# ---- grafico 2: memoria (/api/ps) ----
fig, ax = plt.subplots(figsize=(8.2, 4.4), dpi=200)
mm = pontos(ax, "ps", AZ)
ctx = [vals(n, "num_ctx")[0] for n in CONDS]
desl = {5: (-6, -46), 20: (0, -34), 50: (0, 14), 100: (0, 14), 150: (0, 14)}
for n, m, c in zip(CONDS, mm, ctx):
    ax.annotate(f"{m:,.0f} MiB (num_ctx {c})".replace(",", "."), (n, m), xytext=desl[n], textcoords="offset points",
                ha="left" if n == 5 else "center", color=AZ, fontsize=10, fontweight="bold")
base = mm[0]
ref = [base, base + 256, base + 768, base + 768, base + 768]
ax.plot(CONDS, ref, "--", color=CZ, lw=1.6)
ax.text(155, 4300, "tracejado: aumento esperado só pelo cache\nde contexto (cálculo a partir de 5 jogos)", ha="right", color=CZ, fontsize=10)
ax.set(xticks=CONDS, xlim=(0, 165), ylim=(0, 7000))
ax.set_xlabel("jogos de histórico no prompt (quantidade)"); ax.set_ylabel("memória do modelo no Ollama (MiB)")
ax.set_title("A memória acompanha o num_ctx: sobe até 50 jogos e fica parada com 100 e 150", fontsize=13, loc="left")
fig.tight_layout(); fig.savefig(os.path.join(saida, "grafico_memoria.png")); plt.close(fig)

# ---- grafico 3: velocidade de geracao ----
fig, ax = plt.subplots(figsize=(8.2, 4.0), dpi=200)
mv = pontos(ax, "vel", VD)
for n, m in zip(CONDS, mv):
    ax.annotate(f"{m:.1f}".replace(".", ","), (n, m), xytext=(0, 10), textcoords="offset points", ha="center", color=VD, fontweight="bold")
ax.axhline(mv[1], color=CZ, ls=":", lw=1.6)
ax.text(152, mv[1] + 0.5, "velocidade com 20 jogos", ha="right", color=CZ, fontsize=10.5)
ax.set(xticks=CONDS, xlim=(0, 160), ylim=(0, 24))
ax.set_xlabel("jogos de histórico no prompt (quantidade)"); ax.set_ylabel("velocidade de gerar (tokens/s)")
ax.set_title("A velocidade de gerar cai de cerca de 19 para 13 tokens/s com 150 jogos", fontsize=13, loc="left")
fig.tight_layout(); fig.savefig(os.path.join(saida, "grafico_velocidade.png")); plt.close(fig)

# ---- numeros de apoio ----
for n in CONDS:
    print(n, "p", round(media(vals(n,"p")),2), "g", round(media(vals(n,"g")),2), "t", round(media(vals(n,"t")),2),
          "ptok", round(media(vals(n,"ptok"))), "gtok", round(media(vals(n,"gtok")),1),
          "vel", round(media(vals(n,"vel")),2), "ms/tok", round(media(vals(n,"mst")),2), "ps", vals(n,"ps")[0])
print("resposta por semente:")
for s in SEMS: print(s, [round(R[(n,s)]["t"],2) for n in CONDS])
print("prompt>ger por rodada:")
for n in CONDS: print(n, [R[(n,s)]["p"] > R[(n,s)]["g"] for s in SEMS])
d100 = media(vals(100,"p")) - media(vals(100,"g")); d150 = media(vals(150,"p")) - media(vals(150,"g"))
print("interp cruzamento", 100 + 50 * (-d100) / (d150 - d100), d100, d150)
