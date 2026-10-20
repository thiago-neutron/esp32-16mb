#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
calc_led_comum.py — dimensiona o LED RGB de 4 pinos da SONDA com UM resistor unico
no comum (catodo comum -> resistor -> GND), anodos direto nos GPIO 25/26/27.

Responde 3 perguntas, com modelo eletrico explicito (nada de chute redondo):

  1) O resistor UNICO no comum permite misturar cor em DC?  (resposta: NAO)
     -> prova numerica do "roubo de corrente": o die vermelho (Vf baixo) sobe a
        tensao do no comum acima do Vf do verde/azul e APAGA os dois.
  2) Quanta corrente cada die recebe em multiplexacao no tempo (1 canal por vez),
     para varios valores de R comum e varias tensoes de trilho.
  3) Que GANHO de PWM cada canal precisa para a cor sair certa (balanco de branco),
     ja considerando gamma sRGB e a eficiencia luminosa de cada die.

Modelo:
  - GPIO do ESP32 como fonte 3,3 V com resistencia de saida R_PIN (~40 ohm no
    drive padrao CAP_2). Nao e' chute: e' o que da VOH ~ 0,8*VDD na corrente nominal.
  - LED como reta V = V0 + I*Rs (linearizacao da exponencial na faixa de uso).
  - Luminosidade proporcional a corrente (valido na faixa 1..20 mA).

Rodar:  python3 calc_led_comum.py
"""

# ---------------------------------------------------------------- parametros
R_PIN = 40.0          # ohm — resistencia de saida do GPIO (drive padrao CAP_2)
R_PIN_FORTE = 20.0    # ohm — com gpio_set_drive_capability(CAP_3)
R_PIN_FRACO = 80.0    # ohm — com CAP_1 (metade da forca)

# LED RGB 5 mm / 5050 tipico: V = V0 + I*Rs  (I em A)
# valores de catalogo: Vf@20mA  R~2,0 V   G~3,0 V   B~3,0 V
DIES = {
    "R": dict(V0=1.65, Rs=14.0, mcd20=800.0,  nm=625),
    "G": dict(V0=2.60, Rs=17.0, mcd20=2000.0, nm=520),
    "B": dict(V0=2.60, Rs=17.0, mcd20=400.0,  nm=465),
}
# pessimista: dies InGaN de lote "alto" (Vf 3,2 V @20 mA) — o pior caso do verde/azul
DIES_RUINS = {
    "R": dict(V0=1.80, Rs=14.0, mcd20=800.0,  nm=625),
    "G": dict(V0=2.90, Rs=15.0, mcd20=2000.0, nm=520),
    "B": dict(V0=2.90, Rs=15.0, mcd20=400.0,  nm=465),
}

# coeficientes de luminancia sRGB (o que define "branco" para o olho)
Y = {"R": 0.2126, "G": 0.7152, "B": 0.0722}

VDD_NOM = 3.3
RCS = [22, 33, 47, 68, 100, 150, 220, 330]
I_MAX_REC = 20.0      # mA — maximo RECOMENDADO por GPIO do ESP32
I_MAX_ABS = 40.0      # mA — maximo ABSOLUTO por GPIO (datasheet)

RES_BITS = 12
PERIODO = 1 << RES_BITS          # 4096 contagens
GUARDA = 32                      # contagens de folga entre janelas (anti-sobreposicao)
JANELA = PERIODO // 3 - GUARDA   # 1333 contagens uteis por canal


def corrente(die, vdd, rc, rpin=R_PIN):
    """corrente (mA) de um die sozinho: (Vdd - V0) / (Rpin + Rs + Rc). 0 se nao acende."""
    num = vdd - die["V0"]
    if num <= 0:
        return 0.0
    return 1000.0 * num / (rpin + die["Rs"] + rc)


def luz(die, i_ma):
    """mcd, linear na corrente, ancorado no catalogo @20 mA."""
    return die["mcd20"] * i_ma / 20.0


# =========================================================== 1) prova do roubo
def prova_roubo(dies, vdd, rc):
    """
    Vermelho + verde ligados AO MESMO TEMPO, resistor unico no comum.
    Resolve o no comum Vk por bisseccao e mostra quem conduz.
    """
    def i_die(die, vk):
        num = vdd - die["V0"] - vk
        if num <= 0:
            return 0.0
        return num / (R_PIN + die["Rs"])        # A

    lo, hi = 0.0, vdd
    for _ in range(200):
        vk = (lo + hi) / 2
        itot = i_die(dies["R"], vk) + i_die(dies["G"], vk) + i_die(dies["B"], vk)
        if itot * rc > vk:
            lo = vk
        else:
            hi = vk
    vk = (lo + hi) / 2
    return vk, {c: 1000.0 * i_die(dies[c], vk) for c in "RGB"}


# ================================================== 3) ganhos de balanco branco
def ganhos(dies, vdd, rc, rpin_por_canal=None):
    """
    Fracao da JANELA que cada canal usa para BRANCO correto.
    duty_c ∝ Y_c / luz_por_contagem_c, normalizado para que o pior caso = 1,0.
    """
    rp = rpin_por_canal or {c: R_PIN for c in "RGB"}
    i = {c: corrente(dies[c], vdd, rc, rp[c]) for c in "RGB"}
    L = {c: luz(dies[c], i[c]) for c in "RGB"}
    need = {c: (Y[c] / L[c] if L[c] > 0 else float("inf")) for c in "RGB"}
    m = max(v for v in need.values() if v != float("inf"))
    g = {c: (need[c] / m if need[c] != float("inf") else 0.0) for c in "RGB"}
    return i, L, g


def linha(*cols):
    print("".join(str(c).ljust(w) for c, w in cols))


if __name__ == "__main__":
    print("=" * 78)
    print("1) POR QUE UM RESISTOR SO NAO PERMITE MISTURAR COR EM DC (sem multiplexar)")
    print("=" * 78)
    print(f"   Vdd={VDD_NOM} V, R_PIN={R_PIN} ohm, os 3 canais em 100% ao mesmo tempo:")
    print()
    linha(("Rc", 8), ("V no comum", 14), ("I_R mA", 10), ("I_G mA", 10), ("I_B mA", 10),
          ("I total mA", 12), ("veredito", 20))
    for rc in RCS:
        vk, ii = prova_roubo(DIES, VDD_NOM, rc)
        tot = sum(ii.values())
        # criterio RELATIVO: verde a 0,76 mA contra vermelho a 18,4 mA nao e "mistura",
        # e vermelho puro. Um limiar absoluto (tipo <0,05 mA) mentiria a favor de Rc pequeno.
        razao = ii["G"] / ii["R"] if ii["R"] > 0 else 0
        if ii["G"] < 0.05: vd = "G+B APAGADOS"
        elif razao < 0.25: vd = "G+B irrelevantes"
        else:              vd = "mistura possivel"
        linha((f"{rc}R", 8), (f"{vk:.2f} V", 14), (f"{ii['R']:.1f}", 10),
              (f"{ii['G']:.2f}", 10), (f"{ii['B']:.2f}", 10), (f"{tot:.1f}", 12),
              (f"{vd} (G/R={razao:.2f})", 30))
    print()
    print("   >>> O vermelho tem o menor V0 (~1,65 V). Ele conduz primeiro, empurra o no")
    print("       comum para cima, e o verde/azul (V0 ~2,6 V) ficam SEM TENSAO para acender.")
    print("       Amarelo, branco e roxo viram VERMELHO. Nenhum ganho de PWM conserta isso:")
    print("       o problema e' o no comum, nao o duty. => MULTIPLEXAR NO TEMPO e' obrigatorio.")

    print()
    print("=" * 78)
    print("2) MULTIPLEXADO (1 canal por vez): corrente de PICO por die")
    print("=" * 78)
    for vdd, nome in ((3.0, "trilho fraco 3,0 V"), (3.3, "trilho nominal 3,3 V"),
                      (4.2, "se o LED/GPIO vier da celula cheia 4,2 V")):
        print(f"\n   --- Vdd = {vdd} V ({nome}) --- [pico; a media e' 1/3 disso]")
        linha(("Rc", 8), ("I_R", 9), ("I_G", 9), ("I_B", 9),
              ("I_G pior lote", 15), ("seguranca GPIO", 18))
        for rc in RCS:
            ir = corrente(DIES["R"], vdd, rc)
            ig = corrente(DIES["G"], vdd, rc)
            ib = corrente(DIES["B"], vdd, rc)
            igr = corrente(DIES_RUINS["G"], vdd, rc)
            if ir > I_MAX_ABS:
                seg = "!! ACIMA DO ABS"
            elif ir > I_MAX_REC:
                seg = "! acima do rec."
            else:
                seg = "ok"
            linha((f"{rc}R", 8), (f"{ir:.1f} mA", 9), (f"{ig:.1f} mA", 9), (f"{ib:.1f} mA", 9),
                  (f"{igr:.1f} mA", 15), (seg, 18))

    print()
    print("=" * 78)
    print("3) GANHO DE PWM POR CANAL (fracao da janela) PARA BRANCO CORRETO")
    print("=" * 78)
    print(f"   janela util por canal = {JANELA} contagens de {PERIODO} ({100*JANELA/PERIODO:.1f}%)")
    print(f"   guarda entre janelas = {GUARDA} contagens")
    print()
    linha(("Rc", 8), ("ganho R", 10), ("ganho G", 10), ("ganho B", 10),
          ("duty R", 9), ("duty G", 9), ("duty B", 9), ("luz do branco", 14))
    for rc in RCS:
        i, L, g = ganhos(DIES, VDD_NOM, rc)
        dr, dg, db = (int(g[c] * JANELA) for c in "RGB")
        # luminancia media do branco resultante (mcd equivalente)
        lum = sum(L[c] * g[c] * JANELA / PERIODO for c in "RGB")
        linha((f"{rc}R", 8), (f"{g['R']:.3f}", 10), (f"{g['G']:.3f}", 10), (f"{g['B']:.3f}", 10),
              (str(dr), 9), (str(dg), 9), (str(db), 9), (f"{lum:.0f} mcd", 14))

    print()
    print("   --- pior lote (verde/azul com Vf 3,2 V @20 mA) ---")
    linha(("Rc", 8), ("ganho R", 10), ("ganho G", 10), ("ganho B", 10), ("luz do branco", 14))
    for rc in RCS:
        i, L, g = ganhos(DIES_RUINS, VDD_NOM, rc)
        lum = sum(L[c] * g[c] * JANELA / PERIODO for c in "RGB")
        linha((f"{rc}R", 8), (f"{g['R']:.3f}", 10), (f"{g['G']:.3f}", 10), (f"{g['B']:.3f}", 10),
              (f"{lum:.0f} mcd", 14))

    print()
    print("=" * 78)
    print("4) TRUQUE SEM COMPONENTE: forca de drive do pad por canal")
    print("=" * 78)
    print("   gpio_set_drive_capability(): CAP_3 baixa a resistencia do pad (~20 ohm),")
    print("   CAP_1 dobra (~80 ohm). Ajuda onde o resistor comum e' pequeno.")
    print()
    linha(("Rc", 8), ("I_G CAP_2", 12), ("I_G CAP_3", 12), ("ganho", 10),
          ("I_R CAP_2", 12), ("I_R CAP_1", 12))
    for rc in RCS:
        g2 = corrente(DIES["G"], VDD_NOM, rc, R_PIN)
        g3 = corrente(DIES["G"], VDD_NOM, rc, R_PIN_FORTE)
        r2 = corrente(DIES["R"], VDD_NOM, rc, R_PIN)
        r1 = corrente(DIES["R"], VDD_NOM, rc, R_PIN_FRACO)
        linha((f"{rc}R", 8), (f"{g2:.1f} mA", 12), (f"{g3:.1f} mA", 12),
              (f"+{100*(g3/g2-1):.0f}%", 10), (f"{r2:.1f} mA", 12), (f"{r1:.1f} mA", 12))

    print()
    print("=" * 78)
    print("5) LED DE SCAN (azul avulso no GPIO4, resistor SO dele)")
    print("=" * 78)
    print("   Circuito independente: anodo no GPIO4, catodo -> R -> GND. Nao entra no multiplex.")
    print("   Pisca a 2 Hz com 50% de duty, entao a media e' METADE da corrente da tabela.")
    print()
    linha(("R", 8), ("azul tipico", 14), ("azul Vf alto", 14), ("media (2 Hz)", 14), ("leitura", 22))
    for r in (150, 220, 330, 470, 680, 1000, 2200):
        it = corrente(DIES["B"], VDD_NOM, r)
        ia = corrente(DIES_RUINS["B"], VDD_NOM, r)
        if it > 4:    lt = "forte demais"
        elif it > 2:  lt = "visivel de dia"
        elif it > 1:  lt = "discreto  <-- alvo"
        else:         lt = "so no escuro"
        linha((f"{r}R", 8), (f"{it:.2f} mA", 14), (f"{ia:.2f} mA", 14),
              (f"{it/2:.2f} mA", 14), (lt, 22))
    print()
    print("   Azul tem pouco headroom (Vf ~3,0 V contra trilho de 3,3 V), entao a corrente e'")
    print("   sensivel ao lote do LED — as 2 colunas mostram a faixa. Ajuste fino sem ferro de")
    print("   solda: a constante SCAN_NIVEL do firmware (0-255) escurece por PWM.")

    print()
    print("=" * 78)
    print("6) VEREDITO")
    print("=" * 78)
    for rc in (47, 68, 100):
        i, L, g = ganhos(DIES, VDD_NOM, rc)
        ir_max = corrente(DIES["R"], 4.2, rc)
        print(f"   Rc={rc}R: pico R={i['R']:.1f} G={i['G']:.1f} B={i['B']:.1f} mA | "
              f"ganhos R={g['R']:.2f} G={g['G']:.2f} B={g['B']:.2f} | "
              f"pico R se trilho 4,2 V = {ir_max:.1f} mA")
