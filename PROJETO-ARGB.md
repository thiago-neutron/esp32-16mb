<!-- Projeto do farol ARGB da sonda — gerado por painel de 4 projetos + 3 juizes (02/08/2026).
     Propostas avaliadas: PULSO — um WS2812B, um fio, e a regra "cor parada = defeito", FAROL — 1 fio de dados, 1 WS2812B na LiPo, ritmo antes de cor, com kill-switch de honestidade, PAINEL HONESTO — barra de 4 WS2812B no GPIO27 (RMT nativo, zero biblioteca), Farol de 1 pixel — WS2812B no GPIO27, ritmo como código primário e cor como confirmação
     NADA foi editado/compilado/gravado ao produzir este documento. -->

# PROJETO FINAL — FAROL DA SONDA (LED ARGB)

## 1. VEREDITO EM UMA FRASE

Compre **um** módulo breakout WS2812B de 1 pixel (3 furos passantes), ligue **VDD direto na célula LiPo**, **DIN no GPIO27 via header 2,54 de 3 vias** (o fio que hoje vai ao die vermelho), **GND no comum do RGB antigo** — e mova a identidade de cada estado da COR para o **RITMO**, usando o `rgbLedWriteOrdered()` já compilado no core 3.3.0 (zero biblioteca, zero `#include` novo).

Base = **Proposta 1 (PULSO)**, que venceu em 2 dos 3 juízes. Enxertados: o **conector** da P4 (sem o OR de diodos), a **batida única do GPIO2** da P3, o **campo de telemetria** da P2 (sem o loopback/kill-switch), o **pulldown + `digitalWrite(LOW)` antes do `rmtInit`** da P3 e a **aceleração do batimento em bateria crítica** da P4.

---

## 2. LISTA DE COMPRAS

### OBRIGATÓRIO
| Item | Qtd | Valor / spec | Por quê |
|---|---|---|---|
| Módulo breakout **WS2812B 1 pixel**, 3 furos passantes (VCC/DIN/GND), com 100 nF a bordo | 1 | WS2812B **genuíno** (VDD 3,5–5,3 V) | Furo passante ancora o fio **antes** da solda. Foi uma perna nua do 5050 que trincou. **Não** comprar SK6812/clone: especificam 3,7–5,5 V e saem de spec já com a célula em 3,6 V |
| Resistor **10 kΩ** 1/8 W | 1 | DIN → GND, soldado nos pads do próprio módulo | Segura o DIN em nível baixo (idle correto do WS2812) enquanto o GPIO27 está hi-Z, do reset até o `setup()`. Sem ele, cor aleatória latchada a cada boot = o fantasma que custou a hora de bancada |
| **Barra de pinos macho 2,54 de 3 vias** + housing Dupont fêmea 3 vias | 1 | ou JST-PH 3 vias com trava, se tiver | Tira **todo** o esforço mecânico das juntas — a causa literal da trinca — e torna o farol trocável em 5 s sem ferro ("será que é die morto?" vira teste de 10 segundos) |
| Fio **26–28 AWG silicone TRANÇADO**, 3 cores | ~30 cm | **nunca** fio rígido | A trinca do azul é quase certamente encruamento de fio rígido no vai-e-vem, não solda fria |
| Cola quente ou epóxi + termorretrátil + abraçadeira | — | — | Alívio de tração; o esforço morre na cola, não na solda |

### OPCIONAL (só com sintoma / só no futuro)
- **330 Ω 1/8 W** em série no DIN, colado no GPIO27: **só se** aparecer cintilação ou cor errada esporádica. Amortece ringing e protege o diodo de clamp. Não muda nível lógico (isso é folclore).
- **100 nF X7R**: obrigatório **só** se acabar usando LED 5050 nu em vez do módulo.
- **10 µF cerâmico**: só se o chicote passar de 10 cm.
- **2N7002 / P-MOSFET canal P**: só no dia em que a sonda ganhar deep-sleep (ver risco 7).

### NÃO COMPRAR (dinheiro e juntas jogados fora)
74AHCT125 / 74HCT245 · 2× Schottky do OR de alimentação · NPN + 9 passivos do kill-switch · fita/anel/barra de LEDs · eletrolítico de 1000 µF · qualquer biblioteca.

---

## 3. FIAÇÃO

### Pino: **GPIO27**
Não é DAC (deixa **GPIO25=DAC1 e GPIO26=DAC2 livres**, os únicos D/A do chip), não é strapping, não emite pulso no boot, tem driver de saída completo (`SOC_GPIO_VALID_OUTPUT_GPIO_MASK` inclui 27) e **já tem fio até o rodapé do LED** — hoje é o `PIN_R=27`. Plano B: GPIO4, GPIO13, GPIO18/19/23. **Nunca** GPIO12 (strapping VDD_SDIO: pull-up de módulo = falha de boot), nem 0/5/14/15 (pulso no boot = piscada aleatória a cada reset). **Nunca** gastar GPIO32/33 (únicos ADC1 livres, reservados a sensores).

### Ordem de execução
1. **Dessoldar o RGB de 4 pinos** (4 pernas — já está meio solto).
2. **Header de 3 vias na placa**: pino 1 = **VDD** (nó da célula, o mesmo de cima do divisor 100k/100k do GPIO34 — fio curto); pino 2 = **DIN** (fio do GPIO27); pino 3 = **GND** (reaproveita o comum do RGB antigo — o firmware declara `COMMON_ANODE=false`, então o comum já vai ao GND).
3. **GPIO25 e GPIO26**: dessoldar na placa (limpo) **ou** cortar e terminar com termorretrátil. Nunca deixar ponta nua pendurada. Documentar como reserva: DAC1/DAC2 + futuro gate do MOSFET de deep-sleep.
4. **Módulo**: 3 fios em furo passante — passa o fio *pelo* furo, dobra, **depois** solda. 10 kΩ entre DIN e GND soldado nos próprios pads do módulo.
5. **Trançar DIN com GND** ao longo do chicote (reduz área de laço, melhora a borda de subida). 1 minuto, vale.
6. **Só depois de validar eletricamente** (seção 6): cola quente nas 3 juntas do módulo + trava no housing + laço de serviço de 25 mm + abraçadeira 20 mm atrás. **Desmontar epóxi é pior que soldar de novo.**

**Total: 4 juntas novas** (3 no módulo + 1 fio VDD curto), sendo GND e DIN reaproveitados.

### A pegadinha do 3,3 V × 5 V — decisão: **VDD na CÉLULA, 3,3 V direto no DIN, SEM level shifter**

O critério é de datasheet: **VIH(min) = 0,7 × VDD do LED**.

| VDD do LED | VIH(min) | ESP entrega | Veredito |
|---|---|---|---|
| **Célula 4,20 V (cheia)** | 2,94 V | ~3,3 V | ✅ margem +0,36 V |
| **Célula 3,81 V (hoje)** | 2,67 V | ~3,3 V | ✅ margem +0,63 V |
| **Célula 3,50 V** | 2,45 V | ~3,3 V | ✅ margem +0,85 V |
| 5 V (VIN/USB do DevKit) | **3,50 V** | ~3,3 V | ❌ **fora de spec por 0,2 V** |
| 3,3 V do regulador | 2,31 V (lógica sobra) | ~3,3 V | ❌ VDD abaixo do mínimo (3,5 V): verde/azul (Vf 3,0–3,2 V) passam fome |

Ressalva honesta: o VOH de catálogo do ESP32 é 0,8×VDD_IO = 2,64 V, que no papel ficaria *abaixo* de 2,94 V — mas esse número vale sob corrente de drive nominal (dezenas de mA), e o DIN é entrada CMOS que puxa microampères. A margem real se mantém.

**5 V é a armadilha que volta a assombrar:** funciona "quase sempre" porque o limiar real do CMOS fica bem abaixo de 0,7×VDD, mas é margem não especificada, varia por lote e por temperatura, e a falha típica é **intermitente** — exatamente o sintoma-fantasma que este projeto existe para banir. Escreva o aviso **no comentário do firmware, ao lado da constante do pino**, não só no diagrama.

**OR de diodos (VBAT ∥ 3V3) foi rejeitado:** compraria "LED vivo sem célula" ao preço de 3 juntas e de fabricar a mentira que se quer banir — no ramo do 3V3 o VDD cai a ~3,02 V e o farol fica pálido/descolorido em vez de honestamente apagado; e um Schottky invertido degrada **em silêncio**. A ambiguidade que ele resolvia sai de graça na **batida única do GPIO2** (seção 4).

---

## 4. MAPA DE ESTADOS

**Regra de ouro (vai no cabeçalho do arquivo):** *nenhum estado válido é cor parada.* Como o WS2812 **retém o último valor latchado** enquanto tiver VDD, um DIN rompido congela a cor — o único modo de falha desonesto do protocolo de 1 fio. Se cor parada não é estado legal nenhum, ela vira, por construção, **assinatura de defeito**. Custo: zero componente.

| Estado | Cor | Teto | Ritmo | Por que ninguém confunde |
|---|---|---|---|---|
| **Bateria CRÍTICA** (<3,20 V) | vermelho puro (255,0,0) | **160** | **strobe 4 Hz**: 125 on / 125 off, corte seco | O mais **rápido** e o mais **brilhante** — duas dimensões de urgência juntas. Vermelho é o die de menor Vf (~2,0 V): é o único que continua forte quando a célula cai a 3,2 V e o VDD já está abaixo do mínimo de datasheet. O estado mais importante servido pela cor mais robusta na tensão mais baixa |
| **Carregar** (<3,40 V) | âmbar (255,70,0) | 64 | **pisca 1 Hz**: 500/500, corte seco | Mesmo *caráter* do crítico, 4× mais lento e 2,5× mais fraco: lê-se como "o irmão calmo do alarme". A velocidade dentro da família quente **é** a escala de urgência, sem legenda. Se o die verde morrer, vira vermelho — mas continua 1 Hz, nunca vira o strobe de 4 Hz |
| **Escaneando WiFi/BLE** | ciano (0,180,255) | 64 | **rampa dente-de-serra 3 Hz**, piso 8% (nunca apaga) | Único ritmo **assimétrico** e único que nunca apaga: lê-se como varredura, não como pisca. **Zero componente vermelha** — este é o conserto de causa raiz: o roxo antigo (R=40,B=60) virava vermelho fraco quando o azul abria e se confundia com alarme. Aqui, toda degradação possível aterrissa em estado saudável |
| **Pi lendo por I2C** | verde (0,255,40) | 64 | **respiração simétrica 2,5 s**, piso 10% | Único ritmo simétrico e lento. Difere da varredura em três eixos: 7,5× mais lento, simétrico vs assimétrico, verde vs ciano. "Calmo e respirando" = saúde, leitura intuitiva. De propósito **não** copia o lub-dub do GPIO2 (batida dupla a 1,2 s vs onda contínua a 2,5 s) |
| **Viva sem o Pi** | azul (0,60,255) | 64 | **blip 180 ms a cada 3 s**, resto apagado (6% duty) | O mais escasso da tabela. Difere de "Pi lendo" em **dois** eixos (pulso isolado vs respiração contínua; 3 s vs 2,5 s) — é a distinção que mais importa no dia a dia ("o Pi está falando comigo?"), então merece margem dobrada |
| **Teste manual** (`led r|g|b|w`) | a cor pedida | 64 (branco 48) | quase fixo, com **tique de 150 ms apagado a cada 1 s** (85% duty) | O teste precisa segurar a cor pra você julgar o die — mas 100% fixo abriria a única exceção da regra, e regra com exceção não é regra. 150 ms não atrapalham o julgamento e **provam ao vivo que o enlace de dados está funcionando** enquanto você testa |

### Matriz de diagnóstico (dois LEDs, dois domínios de energia, quatro leituras sem sobreposição)

O GPIO2 (batimento, alimentado pelo **3V3 regulado**) ganha 3 modos — 4 linhas de código, zero hardware:

| ARGB (célula) | GPIO2 (3V3) | Diagnóstico |
|---|---|---|
| com ritmo | lub-dub 1200 ms | tudo certo — **agora** leia a cor |
| **congelado** (cor parada) | batendo | **fio de DADOS rompido**, ESP viva |
| apagado | **batida ÚNICA** 1200 ms | sem célula, rodando no USB — **normal** |
| apagado | lub-dub normal | **VDD ou GND do farol rompido** |
| vermelho strobe 4 Hz | **batida acelerada 400 ms** | bateria crítica (redundância cruzada: se o die vermelho morrer, o batimento acelerado denuncia sozinho) |
| qualquer | **parado** | ESP travada — ignore o ARGB |

A batida única resolve, de graça, a única ambiguidade que sobrava ("apagado" tinha duas causas) e mata a necessidade do OR de diodos.

**Teste de aceitação da tabela inteira, objetivo:** *filme em preto e branco e você ainda sabe qual é o estado.* Os seis passam — a identidade mora no ritmo, e ritmo sobrevive à morte de um die, que é literalmente a outra coisa que já aconteceu nesta bancada.

---

## 5. CÓDIGO

**API: nativa do core.** `rgbLedWriteOrdered()` está em `cores/esp32/esp32-hal-rgb-led.h:27-34`, e `Arduino.h → esp32-hal.h:101` já o inclui — **nenhum `#include` novo**. Verificado no disco: não existe FastLED nem Adafruit_NeoPixel em lugar nenhum da máquina; instalar qualquer uma seria dependência nova para fazer o que o core faz em uma linha, e os caminhos bit-bang delas desabilitam interrupção durante o frame inteiro, num chip que é **escravo I2C**. `neopixelWrite()` está deprecado e chama `log_w()` a cada invocação — a 50 Hz seriam 50 warnings/s cuspidos no terminal DBG.

**Por que não machuca o escravo I2C (verificado, não assumido):** `rmtWrite` bloqueia em `xEventGroupWaitBits` e o mutex é `xSemaphoreTake` — **nenhum `portENTER_CRITICAL`/`portDISABLE_INTERRUPTS` no caminho de escrita**; a task dorme e cede o escalonador, a onda sai do hardware RMT. `i2c_slave_task` roda em prioridade 20 contra prioridade 1 do `loopTask`. Os 24 símbolos cabem inteiros no bloco de 64 words → transmissão one-shot, sem ISR de recarga. E `rmtInit()` faz **early-return** quando o pino já está configurado com os mesmos parâmetros — chamar a 50 Hz não causa churn de heap. Custo real ~0,5–1% de CPU (o callback de fim defere pro daemon `Tmr Svc`, prio 1), não os 0,15% que a conta ingênua dá.

### O bloco novo (substitui as linhas 168–172)

```c
// ---- FAROL ARGB (WS2812B, 1 pixel, RMT nativo do core 3.3.0 — SEM biblioteca)
// VDD do LED = CELULA LiPo. NUNCA no VIN/5V do DevKit: com USB plugado o VIH sobe
// pra 3,5V > 3,3V do ESP e volta a falha intermitente. NUNCA analogWrite no PIN_ARGB.
// REGRA DE OURO: nenhum estado valido e cor PARADA. Cor parada = fio de dados rompido.
// Nao "simplificar" pra cor solida porque "a respiracao distrai" — derruba o portao inteiro.
static const int PIN_ARGB = 27;                                 // era PIN_R
static rgb_led_color_order_t ARGB_ORDEM = LED_COLOR_ORDER_GRB;  // 'led ordem rgb' troca em RAM
static const uint8_t TETO=64, TETO_ALARME=160, TETO_BRANCO=48;
uint8_t ledTeto = TETO;                       // teto do modo de teste
const char* ledEst = "boot";                  // publicado na telemetria

static inline uint8_t gama(uint8_t v){ return (uint16_t)v*v/255; }   // gama 2.0, zero tabela

// env = envelope do ritmo (0..255) ; teto = brilho maximo do estado
void setCor(uint8_t r,uint8_t g,uint8_t b,uint8_t env,uint8_t teto){
  uint16_t e = (uint16_t)gama(env)*teto/255;
  rgbLedWriteOrdered(PIN_ARGB, ARGB_ORDEM, (uint16_t)r*e/255, (uint16_t)g*e/255, (uint16_t)b*e/255);
}
// FACHADA: mantem a assinatura antiga -> 'led r|g|b|w' e qualquer chamada velha compilam
void setColor(uint8_t r,uint8_t g,uint8_t b){ setCor(r,g,b,255,TETO); }
```

### Os geradores de ritmo (depois de `pulso()`, linha 174 — que é reaproveitado)

```c
static uint8_t r_strobe(uint32_t t,uint16_t p){ return (t%p)<(p/2)?255:0; }
static uint8_t r_rampa (uint32_t t,uint16_t p){ return 20+(uint32_t)235*(t%p)/p; }
static uint8_t r_resp  (uint32_t t,uint16_t p){ uint32_t x=t%p,h=p/2; return 25+(uint32_t)230*(x<h?x:p-x)/h; }
static uint8_t r_blip  (uint32_t t,uint16_t p,uint16_t d){ uint32_t x=t%p; return x<d?pulso(x,d,255):0; }
static uint8_t r_tique (uint32_t t){ return (t%1000)<850?255:0; }   // 150 ms apagado por segundo
```

### Batimento com 3 modos (substitui `batimento()`, linhas 180–185)

```c
uint8_t batimento(uint32_t now, bool semBat, bool critica){
  if(critica){ uint16_t t=now%400; return pulso(t,150,255); }      // corrida: 1 batida a 2,5 Hz
  uint16_t t = now % HB_PERIODO;
  if(t < 200) return pulso(t,200,255);                             // "lub"
  if(!semBat && t>=280 && t<460) return pulso(t-280,180,150);      // "dub" SOME sem celula
  return 0;
}
```

### `ledStatus()` — **a máquina de estados NÃO muda**: mesmas condições, mesma ordem, só desenhando diferente

```c
void ledStatus(){
  uint32_t now=millis();
  bool semBat  = (baterioV < V_SEM_BAT);
  bool critica = (!semBat && baterioV < V_CRITICA);
  uint8_t hb = batimento(now, semBat, critica);
  analogWrite(PIN_PLACA, PLACA_INVERTIDO ? 255-hb : hb);        // GPIO2 INTOCADO (LEDC)

  if(ledTesteAte){
    if((int32_t)(now-ledTesteAte) < 0){ ledEst="teste";
      setCor(ledTR,ledTG,ledTB, r_tique(now), ledTeto); return; }
    ledTesteAte=0;
  }
  bool piVivo = (lastI2Cms && now-lastI2Cms < 3000);
  if(critica)                          { ledEst="critica"; setCor(255,  0,  0, r_strobe(now,250),    TETO_ALARME); }
  else if(!semBat && baterioV<V_BAIXA) { ledEst="carregar";setCor(255, 70,  0, r_strobe(now,1000),   TETO); }
  else if(bleBusy||WiFi.scanComplete()==WIFI_SCAN_RUNNING)
                                       { ledEst="escan";   setCor(  0,180,255, r_rampa(now,330),     TETO); }
  else if(piVivo)                      { ledEst="pi";      setCor(  0,255, 40, r_resp(now,2500),     TETO); }
  else                                 { ledEst="sozinha"; setCor(  0, 60,255, r_blip(now,3000,180), TETO); }
}
```

### `setup()` — autoteste de boot no lugar do `setColor(0,0,40)` (linhas 291–293)

```c
  pinMode(PIN_ARGB,OUTPUT); digitalWrite(PIN_ARGB,LOW);   // linha em idle ANTES do 1o rmtInit
  pinMode(PIN_PLACA,OUTPUT);
  setColor(255,0,0); delay(150); setColor(0,255,0); delay(150);
  setColor(0,0,255); delay(150); setColor(0,0,0);          // 450 ms: prova 3 dies + fio de dados
```
De brinde, **loop de brownout fica visível** como ciclo RGB se repetindo — o melhor diagnóstico de brownout que a sonda vai ter.

### Terminal (`responder()`, linhas 236–250) — 3 mudanças
- **`led ordem <grb|rgb|brg|rbg|gbr|bgr>`**: escreve só na variável `ARGB_ORDEM` em RAM (sem NVS, mantendo o loop livre de flash). Descobre a ordem em 5 s e depois você crava na constante. **Isto é o enterro da classe de bug do "soquete espelhado"**: o que custou uma hora de bancada e 10 linhas de comentário agora é um comando, sem ferro de solda.
- **`led w`** passa a usar `ledTeto=TETO_BRANCO` (48) pra segurar a corrente; os demais, `TETO`.
- **`led off` fica, mas temporizado em 10 s** e imprime `"apagado = assinatura de defeito; volta sozinho em 10s"`. Preserva o hábito de bancada sem abrir buraco na regra: falha real fica escura **pra sempre**, o comando não consegue imitar isso.

### `montarLive()` — +2 linhas (o enxerto de maior valor da P2, a custo zero)
```c
  j+=",\"led\":\""; j+=ledEst; j+="\"";     // ~14 B. So no loop, NUNCA nos callbacks I2C.
```
O Pi passa a logar **o que o firmware acredita estar desenhando**. Confrontar isso com o que o olho vê ("está vermelho" × firmware diz `escan`) transforma "uma hora diagnosticando" em dez segundos — com zero transistor, zero loopback, zero GPIO. Manter curto: `live` já encosta no teto de 3072 e cada 32 B a mais é **mais um request I2C por poll**.

### Diff total
**SAI:** bloco de comentário 46–55 (o monumento ao espelhamento) · linha 53 `PIN_R/G/B` · 55 `TEM_VERDE` (e com ela os 2 ramos duplicados de "carregar" e "Pi lendo" — o remendo do die morto perdeu a razão de ser) · 63 `AZUL_OCIOSO` · 67 `COMMON_ANODE` e a inversão `255-x` · 169–172 corpo do `setColor` · 291 os 3 `pinMode` · 293 `setColor(0,0,40)`.
**FICA INTOCADO:** todo o I2C escravo, telemetria, WiFi/BLE, bateria/`CAL_ADC`, o LEDC do GPIO2, a cadência de 20 ms da linha 328.
**Saldo:** ~−20 / ~+45 linhas em 330, **zero `#include` novo, zero lib, zero mudança de FQBN.**

---

## 6. TESTE DE ACEITE (bancada, nesta ordem, sem multímetro)

1. **Solde as 3 juntas do módulo. NÃO cole nada ainda.** Plugue o housing.
2. **Ligue.** Deve sair **vermelho → verde → azul, 150 ms cada**. Se saírem trocados: `led ordem rgb` (ou a que bater) e reinicie o teste — sem reabrir nada. Só depois crave na constante e regrave.
3. **`led r` / `led g` / `led b` / `led w`** → os 3 dies vivos, cada um com o **tique de 150 ms** visível. Tique presente = o barramento de dados está funcionando *enquanto* você segura a cor.
4. **`led auto`** → volta ao estado real.
5. **Teste do preto e branco** (o aceite de verdade): olhe de longe, apertando os olhos, ou filme em P&B. **Você tem que nomear os 6 estados só pelo ritmo.** Se dois se confundirem, o mapa falhou — não o LED.
6. **`scan`** → rampa ciano a 3 Hz por ~4 s, depois volta.
7. **Ligue o poll do Pi** → respiração verde de 2,5 s. **Pare o Pi** → em 3 s vira blip azul a cada 3 s. Confirme no JSON que o campo `"led"` acompanha.
8. **Alarme, sem descarregar a célula:** num build de bancada suba `V_CRITICA`/`V_BAIXA` acima da tensão atual (`bat` te diz qual é) — ex. 4,50/4,60 — grave e confira **strobe vermelho 4 Hz + batimento acelerado a 400 ms**. Volte os valores.
9. **Tire a célula com o USB ligado** → ARGB apagado + **batida ÚNICA** no GPIO2. Recoloque → volta o lub-dub. (Se aparecer lub-dub com o ARGB apagado e a célula presente: VDD/GND do farol rompido.)
10. **Prove a falha honesta:** com tudo ligado, retire **só o fio de DIN** do header por 5 s → a cor **congela e para**. Recoloque → o ritmo volta. Você acabou de ver, ao vivo, como a falha vai se anunciar.
11. **`reboot`** → tem que aparecer R/G/B e **nenhuma piscada aleatória antes**. Se piscar cor aleatória no reset, o pulldown de 10 k não está fazendo contato.
12. **Só agora:** cola quente nas 3 juntas do módulo + trava no housing + laço de serviço + abraçadeira.
13. **Não-regressão do I2C:** o Pi lê o JSON completo 10× seguidas sem erro com o LED animando.

---

## 7. RISCOS QUE SOBRARAM

1. **DIN rompido congela a cor** — não há readback no WS2812; nenhuma detecção elétrica. *Mitigação:* a regra "nenhum estado é cor parada" + o campo `"led"` na telemetria (o Pi sabe o que *deveria* estar aparecendo). Reduz o dano de "uma hora diagnosticando" para "dois segundos olhando". Detecção automática foi avaliada e **rejeitada**: ver seção 8.
2. **VDD abaixo do mínimo de datasheet (3,5 V) no fim da descarga** — verde e azul (Vf 3,0–3,2 V) enfraquecem. *Mitigação por arquitetura, não por compensação:* abaixo de 3,40 V só existem "carregar" (âmbar) e "crítica" (vermelho puro), servidos pelo die de Vf ~2,0 V. A física **assina embaixo** da mensagem em vez de sabotá-la. Quem calibrar a paleta com a célula cheia precisa saber que ela muda de aparência a 3,5 V.
3. **Ordem de bytes de clone** — `led ordem` conserta em RAM, mas **RAM não sobrevive ao reset**: crave na constante e regrave no mesmo dia, senão daqui a um mês o próximo boot te engana.
4. **Regressão humana (o risco de maior prazo)** — daqui a seis meses alguém acha que a respiração "distrai" e volta pra cor sólida; no instante em que isso acontece, o portão de honestidade cai inteiro. *Mitigação:* o aviso vai no cabeçalho do arquivo, no lugar exato do bloco 46–55, com a mesma energia. Aquele bloco provou que este arquivo sabe se defender de mal-entendidos futuros — é só apontar a arma pro alvo novo.
5. **Alguém "simplifica" e liga o VDD no VIN/5V** — volta a intermitência-fantasma. *Mitigação:* comentário ao lado da constante do pino **e** uma etiqueta no fio.
6. **O loop perde 1–2 frames por segundo**: `lerBateria()` (linha 91) é 16 × `analogReadMilliVolts` + `delay(2)` = **32–40 ms de bloqueio a cada 1 s**. *Mitigação:* todos os ritmos escolhidos têm feature ≥ 125 ms (6 frames). **Regra dura: nunca criar ritmo com feature < 60 ms.** Opcional: 8 amostras × `delay(1)` derruba o bloqueio pra ~10 ms sem tocar em `CAL_ADC` (média continua média).
7. **Piso novo de ~1 mA** (quiescente do controlador do WS2812, que o RGB passivo não tinha) — hoje é 0,6–1% de um consumo de 120–160 mA com rádio; **vira o item dominante** no dia em que a sonda ganhar deep-sleep. *Gatilho documentado no código:* nesse dia, GPIO25 ou 26 aciona um **2N7002/P-MOSFET** cortando o VDD do farol — **nunca um BJT**: 0,55 mA de corrente de base contínuos comeriam a economia de 1 mA que se está tentando fazer.
8. **Conector Dupont pode ficar intermitente na linha DIN** — trocaria uma falha por outra. *Mitigação:* cola quente travando o housing **depois** do teste; sintoma (piscadas/cores erradas esporádicas) → aí sim os 330 Ω.
9. **`CONFIG_RMT_ISR_IRAM_SAFE` não está setado** — durante escrita em flash a ISR pode ser suspensa. Hoje é risco nulo: o loop não escreve flash e a transmissão de 24 símbolos é one-shot. **Vira real no dia em que entrar NVS/Preferences/OTA** — anotar junto do código de persistência.
10. **Morte do die vermelho apagaria o alarme crítico** — *mitigado* (não eliminado) pela aceleração do batimento no GPIO2: LED e domínio de energia independentes.
11. **Consumo:** 1–4 mA em regime normal, 7,3 mA de pior caso sustentado (alarme), 13,5 mA de pico. ≤5% do orçamento de rádio; ~12 minutos de autonomia numa célula de 1000 mAh. **Não empata com o esquema antigo** — o ~1 mA quiescente é piso novo e permanente; o duty baixo compensa quase tudo, mas declare em vez de vender empate.

---

## 8. O QUE NÃO FAZER (armadilhas que os juízes apontaram)

**Elétrica**
- ❌ VDD do LED no **VIN/5V** do DevKit (VIH 3,5 V > 3,3 V = fantasma intermitente) ou no **3V3** (VDD fora do mínimo, verde/azul famintos) ou **por um GPIO** ("economizar um fio": 40 mA de abs max sem margem, e em brownout o pino flutua e deixa o LED meio-alimentado, parte fantasma).
- ❌ **OR de diodos VBAT∥3V3**: fabrica LED pálido em 3,02 V — a mentira plausível que o projeto existe pra banir — e um Schottky invertido degrada **sem sintoma**. A batida única do GPIO2 entrega o mesmo diagnóstico de graça.
- ❌ **Kill-switch NPN + loopback DOUT**: margem confessada de **5 mV** (2,48 V × VIH 2,475 V) num ADC que este firmware documenta como 8,8% fora, através de um divisor de 50 kΩ → **falso positivo apaga o alarme de bateria crítica**, o pior desfecho do campo inteiro. NPN em curto perde a honestidade **sem sintoma nenhum**. Pinagem BC547 (CBE) × 2N3904 (EBC) é espelhada: montado ao contrário o LED meio-funciona pela junção B-E — outra mentira plausível. E ~18 pernas de componente num aparelho cuja doença declarada é junta de solda.
- ❌ Achar que **330 Ω em série ou 100 nF "resolvem o nível lógico"** — o resistor amortece ringing, o capacitor desacopla; **nenhum dos dois muda VIH**.

**Software**
- ❌ **FastLED / Adafruit_NeoPixel** — dependência nova (confirmado: não existem na máquina) e caminhos bit-bang com interrupção desabilitada pelo frame inteiro, ao lado de um **escravo I2C** que o próprio cabeçalho do firmware (linhas 17–21) descreve como frágil.
- ❌ **`neopixelWrite()`** — deprecado, chama `log_w()` a cada invocação: 50 warnings/s no terminal DBG.
- ❌ **Deixar `analogWrite`/`pinMode` antigos no GPIO27, mesmo comentados** — `rmtInit()` chama `perimanClearPinBus()`; se alguém tocar o pino com LEDC, cada escrita seguinte vira `heap_caps_calloc` + `xEventGroupCreate` + `rmt_new_tx_channel` inteiros, **50×/s**. Apagar `PIN_R/G/B` do arquivo não é higiene, é requisito: código morto ressuscita.
- ❌ **Chamar `setCor`/`setColor` de dentro de `onReceiveI2C`/`onRequestI2C`** — rodam na `i2c_slave_task`, 4 KB de pilha, caminho de tempo real do barramento. O LED só é tocado pelo `loop()`.
- ❌ **Atrelar brilho à tensão da bateria sem histerese** — o ADC deste chip lê 8,8% fora através de 50 kΩ de impedância de fonte; parado perto de um limiar, o brilho mudaria **uma vez por segundo**. Tetos fixos (64/160) são imunes. Se um dia quiser dimerizar, chaveie por `baterioPct` com banda morta de ±3 pontos.
- ❌ **Flash por transação I2C** ("o LED conta as leituras do Pi") — cada poll são **40 a 95 requests de 32 B**: vira um borrão aceso de 10–15 frames, e a largura do borrão **cresce com o tamanho do JSON**. Hardware saudável produzindo a assinatura de fio rompido.
- ❌ **Barra de bateria por contagem de LEDs** — reintroduz exatamente o pecado: um LED morto subtrai da contagem e lê-se como carga menor; um controlador morto emudece tudo à frente e desloca a barra em 2 níveis. Falha de hardware indistinguível de estado legítimo.
- ❌ **Mais de 1–2 LEDs** — acima de 48 símbolos estoura o bloco de 64 words e liga a ISR de recarga no meio da transmissão, no mesmo nível LOWMED da ISR do escravo I2C (no Xtensa, mesmo nível **não aninha**).
- ❌ **Deixar o teste manual 100% sólido** — abre uma janela de 30 s na regra "parado = defeito". O tique de 150 ms custa nada.

**Bancada**
- ❌ **Colar epóxi/cola quente antes de validar eletricamente** — desmontar epóxi é pior que soldar de novo.
- ❌ **Fio rígido** (a trinca do azul é encruamento por vai-e-vem, não solda fria) e **solda direto em pad nu de 5050** (foi uma perna nua levando esforço que quebrou).
- ❌ **GPIO12** como plano B (strapping VDD_SDIO: pull-up de módulo = falha de boot) · **0/5/14/15** (pulso no boot = piscada aleatória a cada reset) · **GPIO32/33** (únicos ADC1 livres, guardados pra sensores).
- ❌ **Deixar os fios de GPIO25/26 pendurados com ponta nua** — dessoldar na placa ou terminar com termorretrátil.
- ❌ Se um dia for de fita: **60 LED/m, nunca 144** (pads apertados, cobertura levanta).

**Arquivo alvo:** `/mnt/c/Users/thigas/Documents/PROJETOS/esp32-16mb/sonda_i2c/sonda_i2c.ino` — remover 46–55, 53, 55, 63, 67, 169–172, 291 (3 `pinMode`), 293; reescrever 174–209; +12 no `responder()` (236–250); +2 no `montarLive()`. Nada foi editado, compilado nem aberto em porta serial.

---

# ANEXO — GROUND-TRUTH DO CORE (verificado no disco)

GROUND-TRUTH — sonda / troca do RGB de 4 pinos por LED enderecavel

Base dos caminhos:
- CORE = `/mnt/c/Users/thigas/AppData/Local/Arduino15/packages/esp32/hardware/esp32/3.3.0`
- IDFLIB = `/mnt/c/Users/thigas/AppData/Local/Arduino15/packages/esp32/tools/esp32-arduino-libs/idf-release_v5.5-b66b5448-v1/esp32`
- Firmware lido inteiro: `/mnt/c/Users/thigas/Documents/PROJETOS/esp32-16mb/sonda_i2c/sonda_i2c.ino` (330 linhas). Ele usa `analogWrite()` em 4 pinos (25,26,27 no `setColor()` linha 171 e o GPIO2 na linha 191); nao usa RMT, nem SPI, nem I2S. `Serial` (UART0 = GPIO1/3) esta em uso para DBG.

---

1) WS2812 NATIVO NO CORE 3.3.0 — SIM, existe e e RMT

Arquivos: `CORE/cores/esp32/esp32-hal-rgb-led.h` e `.c` (compilados no core, sem lib externa; `esp32-hal.h` ja e incluido por `Arduino.h`).

Assinaturas exatas (`esp32-hal-rgb-led.h:27-34`):
```c
void rgbLedWriteOrdered(uint8_t pin, rgb_led_color_order_t order, uint8_t red_val, uint8_t green_val, uint8_t blue_val);
void rgbLedWrite(uint8_t pin, uint8_t red_val, uint8_t green_val, uint8_t blue_val);
[[deprecated("Use rgbLedWrite() instead.")]] void neopixelWrite(uint8_t p, uint8_t r, uint8_t g, uint8_t b);
```
- `neopixelWrite()` esta DEPRECADO — so chama `rgbLedWrite()` e loga warning (`esp32-hal-rgb-led.c:20-23`). Usar `rgbLedWrite`/`rgbLedWriteOrdered`.
- Ordem de bytes: enum `rgb_led_color_order_t` com RGB/BGR/BRG/RBG/GBR/GRB (`.h:18-25`). Default = `LED_COLOR_ORDER_GRB` (`.h:14-16`, comentario literal "default WS2812B color order"); a montagem fica em `.c:43` (`{green, red, blue}`) com o switch em `.c:45-73`. `rgbLedWriteOrdered()` permite corrigir a ordem em software sem tocar em fiacao — relevante pro requisito de honestidade.
- Usa RMT: `rmtInit(pin, RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_1, 10000000)` em `.c:37` e `rmtWrite(pin, led_data, RMT_SYMBOLS_OF(led_data), RMT_WAIT_FOR_EVER)` em `.c:94`. Tick de 100 ns; bit 1 = 0.8us/0.4us, bit 0 = 0.4us/0.8us (`.c:80-90`).
- **1 LED SO.** `rmt_data_t led_data[24];` (`.c:31`) — 24 simbolos = exatamente um WS2812. **Nao aceita cadeia.** Para 2+ LEDs e preciso montar o array proprio e chamar `rmtWrite()` direto (padrao no exemplo do core `CORE/libraries/ESP32/examples/RMT/RMTWrite_RGB_LED/RMTWrite_RGB_LED.ino:61,97` com `NR_OF_ALL_BITS = 24*NR_OF_LEDS`).
- **Bloqueia, mas de forma SEGURA pro I2C escravo.** `RMT_WAIT_FOR_EVER` = `portMAX_DELAY` (`esp32-hal-rmt.h:65`). O bloqueio e `xEventGroupWaitBits(...)` em `esp32-hal-rmt.c:348-351` — a task dorme e cede o escalonador; **NAO desabilita interrupcoes** e nao faz bit-bang. A forma de onda sai do hardware RMT. Tempo real de barramento: 24 bits x 1.25us ~= 30us por LED. Existe `rmtWriteAsync()` (`esp32-hal-rmt.h:124`) se quiserem nem isso.
- `rgbLedWrite()` chama `rmtInit()` a CADA chamada, mas `rmtInit()` retorna cedo se o pino ja esta configurado com os mesmos parametros (`esp32-hal-rmt.c:471-479`) — nao ha realloc por frame.
- O ajuste `pin - SOC_GPIO_PIN_COUNT` de `.c:34-36` esta sob `#ifdef RGB_BUILTIN`; a variant `esp32` (`CORE/variants/esp32/pins_arduino.h`, arquivo inteiro, 48 linhas) **nao define RGB_BUILTIN nem LED_BUILTIN** — logo esse trecho nao compila e nao ha pegadinha de numeracao de pino.

2) BIBLIOTECAS DE LED ENDERECAVEL JA INSTALADAS — NENHUMA

- Sketchbook `/mnt/c/Users/thigas/Documents/Arduino` existe mas esta **vazio** (nao ha nem subpasta `libraries/`).
- `/mnt/c/Users/thigas/AppData/Local/Arduino15/libraries`: apenas Arduino_BuiltIn, Ethernet, Firmata, Keyboard, LiquidCrystal, Mouse, SD, Servo, Stepper, TFT (bundle padrao da IDE).
- `CORE/libraries/`: 42 libs do core, nenhuma de LED enderecavel.
- `find` por `FastLED*` / `*NeoPixel*` em `Documents/`, `Arduino15/` e `PROJETOS/`: **zero resultados**. Existe um `.platformio/packages/framework-arduinoespressif32/libraries` (core antigo do PIO), tambem sem nenhuma.
- Conclusao: instalar FastLED ou Adafruit_NeoPixel seria dependencia NOVA. O caminho zero-dependencia (`rgbLedWrite`) esta disponivel hoje.

3) RMT DO ESP32 CLASSICO — canais e conflitos

Verificado em `IDFLIB/include/soc/esp32/include/soc/soc_caps.h:282-289` e `IDFLIB/sdkconfig:147-154`:
- `SOC_RMT_GROUPS 1`, `SOC_RMT_CHANNELS_PER_GROUP 8`, `SOC_RMT_TX_CANDIDATES_PER_GROUP 8` → **8 canais, todos capazes de TX**. Zero em uso pelo firmware atual.
- `SOC_RMT_MEM_WORDS_PER_CHANNEL 64` → 64 simbolos por bloco. **1 LED = 24 simbolos, 2 LEDs = 48: cabem inteiros num bloco, transmissao "one-shot" sem ISR de recarga.** A partir de 3 LEDs (72 > 64) a IDF passa a recarregar o buffer no meio da transmissao por interrupcao — e so a partir dai que latencia de ISR vira risco teorico de glitch. Mitigacao existe: `rmtInit(..., RMT_MEM_NUM_BLOCKS_2..8, ...)` (`esp32-hal-rmt.h:30-42`), mas cada bloco extra consome um canal RMT vizinho.
- **Sem DMA**: `tx_cfg.flags.with_dma = 0` (`esp32-hal-rmt.c:544`) e `soc_caps.h` do esp32 nao tem `SOC_RMT_SUPPORT_DMA` (grep por DMA so retorna ADC/DAC/I2S/SPI).
- **Conflito com WiFi/BLE — o folclore nao se aplica aqui, e da pra provar:** o clock do RMT e `RMT_CLK_SRC_DEFAULT` = APB no ESP32 (`esp32-hal-rmt.c:535-540`, comentario explicito "CLK_APB for ESP32|S2|S3|C3"). O medo classico e o WiFi/PM mexer no APB. Mas `IDFLIB/sdkconfig:1473` tem **`# CONFIG_PM_ENABLE is not set`** e `sdkconfig:1581-1583` fixa a CPU em 160 MHz → **sem DFS, APB fixo em 80 MHz**, timing do RMT estavel com WiFi+BLE ligados.
- Robustez adicional a interrupcao: `CONFIG_RMT_TX_ISR_HANDLER_IN_IRAM=y` e `CONFIG_RMT_ENCODER_FUNC_IN_IRAM=y` (`sdkconfig:1166-1167`) → a ISR nao depende de cache de flash. Ressalva honesta: `# CONFIG_RMT_ISR_IRAM_SAFE is not set` (`sdkconfig:1174`) → durante escrita em flash (NVS/OTA) a ISR pode ser suspensa. Este firmware nao escreve flash no loop.
- **Conflito com o LEDC/analogWrite: NAO existe.** Sao perifericos distintos. `analogWrite()` aloca canal LEDC (`CORE/cores/esp32/esp32-hal-ledc.c:772-782`, defaults `analog_resolution=8`, `analog_frequency=1000` nas linhas 770-771). Total de canais LEDC = `SOC_LEDC_CHANNEL_NUM << 1` = **16** (`esp32-hal-ledc.c:29-33` + `soc_caps.h:248`, com HS mode em `soc_caps.h:246`). Hoje o firmware ocupa 4 de 16; largar o RGB de 4 pinos devolve 3.
- **Unica regra dura**: um pino so pode ter UM barramento. `rmtInit()` chama `perimanClearPinBus(pin)` (`esp32-hal-rmt.c:501`) — o mesmo GPIO nao pode ser LEDC e RMT ao mesmo tempo. Se o pino escolhido pro WS2812 ja tiver levado `analogWrite`, a primeira chamada RMT o desanexa (funciona, mas nao misturem as duas APIs no mesmo pino).
- Pino valido pra TX e checado por `GPIO_IS_VALID_OUTPUT_GPIO` (`esp32-hal-rmt.c:487`), definido em `IDFLIB/include/esp_driver_gpio/include/driver/gpio.h:28` sobre `SOC_GPIO_VALID_OUTPUT_GPIO_MASK` (`soc_caps.h:182-184`).

4) NIVEL LOGICO DO WS2812B EM 3,3V

FATO DE DATASHEET (Worldsemi WS2812B; nao verificavel neste core, e spec do componente):
- VIH(min) = **0,7 x VDD**; VIL(max) = 0,3 x VDD. Com VDD = 5,0 V → **VIH(min) = 3,5 V**. Um ESP32 entrega no maximo ~3,3 V (VDD do IO). Logo **3,3 V acionando WS2812B alimentado com 5 V esta FORA DE ESPECIFICACAO**, por ~0,2 V.
- Faixa de alimentacao do WS2812B em datasheet: ~3,5 V a 5,3 V.

CONSEQUENCIA DE ENGENHARIA (deducao direta do datasheet, nao folclore):
- Alimentando o LED com **3,7-4,2 V (a propria celula LiPo)**: VIH(min) = 0,7 x 4,2 = **2,94 V** < 3,3 V → **dentro da spec**, sem level shifter, com VDD ainda dentro da faixa do datasheet ate ~3,5 V de celula. Esta e a opcao mais limpa para esta placa, que ja e LiPo.
- Alimentando com 3,3 V do regulador: logica sobra (VIH = 2,31 V), mas VDD fica abaixo do minimo de datasheet (3,5 V) — funciona na pratica com verde/azul mais fracos (Vf dos dies verde/azul ~3,0-3,2 V; e fisica, nao folclore) e o comportamento perto do fim de escala fica indefinido.
- Solucao de spec com 5 V: buffer **74AHCT125 / 74HCT245** alimentado em 5 V (familia HCT tem VIH = 2,0 V, aceita 3,3 V como nivel alto por datasheet), ou baixar o VDD do LED com 1 diodo de silicio em serie (5 V - 0,7 V = 4,3 V → VIH = 3,01 V).

FOLCLORE DE FORUM (nao tratar como garantia):
- "3,3 V no data com LED em 5 V funciona sempre" — funciona MUITAS vezes porque o limiar real do CMOS de entrada e tipicamente bem abaixo de 0,7xVDD, mas e margem nao especificada, varia por lote e por temperatura; a falha tipica e intermitente e aparece so no 1o LED. Exatamente o tipo de sintoma-fantasma que este projeto esta tentando eliminar.
- "so o primeiro LED precisa de shifter" / "poe 470R em serie que resolve o nivel" — o resistor serie protege contra ringing/ESD, **nao muda nivel logico**.
- "capacitor de 100nF resolve" — desacopla, nao muda VIH.

5) GPIOs SEGUROS PRA SAIDA DE DADOS RAPIDA (excluindo 2,16,17,21,22,34)

Fora de jogo por hardware (verificado): 34-39 sao **input-only**, sem driver de saida (`soc_caps.h:184` remove BIT34..BIT39); 20, 24, 28-31 invalidos/nao bondados (`soc_caps.h:182`); 6-11 = flash SPI (datasheet, nao mexer); 1 e 3 = UART0, em uso pelo `Serial` do firmware (linhas 285-286, 326).

Candidatos, melhores primeiro:
- **GPIO25, GPIO26, GPIO27** — os que vao vagar. Nenhum e strapping, todos com driver de saida completo, ja tem trilha ate o rodape do LED. Sao ADC2/DAC, mas ADC2 ja e inutilizavel com WiFi e o firmware nao usa DAC. **Melhor escolha: reaproveitar um deles (25 ou 27) como DIN do WS2812** — mudanca fisica minima e zero risco de boot.
- **GPIO4** — livre, sem strapping, sem glitch de boot. Otimo segundo lugar.
- **GPIO13** — livre, sem strapping (e MTCK/JTAG; so importa se forem depurar por JTAG).
- **GPIO18, GPIO19, GPIO23** — livres (VSPI SCK/MISO/MOSI, e o firmware nao usa SPI), sem strapping. Boas.
- **GPIO32, GPIO33** — livres e seguros, mas sao ADC1 (os unicos que funcionam com WiFi) — reserva-los pra futuros sensores em vez de gastar num LED.

Com ressalva (usar so se necessario):
- **GPIO14, GPIO15, GPIO5** — funcionam, mas emitem sinal/PWM no boot (fato de datasheet ESP32) → piscada aleatoria a cada reset; 15 e 5 sao ainda strapping.
- **GPIO0** — strapping de boot mode, tem pull-up e botao BOOT no devkit. Evitar.
- **GPIO12 (MTDI) — NAO USAR.** Strapping do VDD_SDIO: se estiver ALTO no boot, o chip configura flash em 1,8 V e nao inicializa. Qualquer pull-up de modulo de LED nessa perna transforma o LED numa falha de boot.

---

NOTA CRITICA PRO REQUISITO "FALHA HONESTA" (fato de protocolo, nao opiniao)

O WS2812 **retem o ultimo valor latchado enquanto tiver VDD**. Se o fio de DADOS romper (a falha que acabou de acontecer com a solda do azul), o LED **congela na ultima cor valida** — que e exatamente o modo de falha desonesto que o projeto quer banir (parece um estado plausivel). Ja se o fio de **GND ou VDD** romper, ele apaga (falha honesta). Ou seja: a topologia de 1 fio de dados melhora muito (um canal so, nao ha mais "paleta que mente por canal aberto"), mas nao entrega deteccao de fio de dados rompido por si so. Nao ha readback no WS2812; quem quiser deteccao precisa de um segundo LED em cadeia com DOUT do primeiro voltando pra um GPIO de entrada, ou aceitar o gate de honestidade em outro sinal (o batimento do GPIO2 continua sendo a prova de vida independente).
