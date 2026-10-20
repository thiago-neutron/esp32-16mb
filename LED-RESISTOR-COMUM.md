# LED RGB com UM resistor no comum — cálculo e firmware (02/08/2026)

> Ponte até o ARGB chegar. Decisão do Thiago: chega de resistor por canal — **os 3 anodos
> vão direto aos GPIO** e o **catodo comum vai ao GND por um único resistor**.
> Contas em `calc_led_comum.py` (rode: `python3 calc_led_comum.py`).
> Firmware: `sonda_i2c/sonda_i2c.ino` (compilado limpo, 1.680.715 B / RAM 20%).
> Versão anterior preservada em `_versoes/sonda_i2c-2026-08-02-pre-multiplex.ino.txt`.

---

## 1. A descoberta que muda o projeto

Com resistor único no comum, **misturar cor em DC é impossível** — e não é questão de
ajustar duty. O die vermelho tem V0 ≈ 1,65 V contra ≈ 2,6 V do verde e do azul. Ele conduz
primeiro, a corrente dele levanta a tensão do nó comum, e aí **verde e azul ficam sem
headroom para acender**.

Os 3 canais em 100% ao mesmo tempo, Vdd 3,3 V (modelo do `calc_led_comum.py`):

| R comum | V no nó comum | I vermelho | I verde | I azul | veredito |
|---|---|---|---|---|---|
| 22 Ω | 0,56 V | 20,3 mA | 2,5 mA | 2,5 mA | verde/azul 8× mais fracos |
| 33 Ω | 0,66 V | 18,4 mA | 0,8 mA | 0,8 mA | verde/azul quase mortos |
| **47 Ω** | 0,77 V | 16,3 mA | **0,00 mA** | **0,00 mA** | **APAGADOS** |
| 100 Ω | 1,07 V | 10,7 mA | 0,00 mA | 0,00 mA | APAGADOS |
| 220 Ω | 1,32 V | 6,0 mA | 0,00 mA | 0,00 mA | APAGADOS |

Ou seja: **amarelo, branco e roxo virariam vermelho puro.** Nenhum ganho de PWM conserta —
o problema é o nó comum, não o duty. Era exatamente esse o mecanismo que já tinha mordido
antes (o "roxo que virava vermelho fraco" documentado no `PROJETO-ARGB.md`), só que agora,
com resistor único, ele fica **total** em vez de parcial.

## 2. A solução: multiplexar no tempo (custo zero, tudo em hardware)

Se nunca houver **dois dies acesos ao mesmo tempo**, o resistor comum passa a se comportar
como um resistor por canal — e a mistura de cor vira mistura **temporal** (o olho integra).

O LEDC do ESP32 faz isso sozinho, sem CPU, via **hpoint** (a contagem em que o canal sobe).
Um timer único de 4 kHz / 12 bits (4096 contagens) alimenta os 3 canais:

```
contagem:  0        1365        2730        4096
           |  R on  |   G on    |   B on    |
           |<-1333->|<--1333--->|<--1333--->|    32 contagens de guarda no fim de cada
```

Por que a fase não escorrega: os 3 canais **compartilham o mesmo contador**. O core 3.3.0
os coloca no mesmo timer porque `ledcAttachChannel()` com a **mesma frequência e resolução**
cai no `find_matching_timer()` (`esp32-hal-ledc.c`). E o `hpoint` programado uma vez no
`setup()` sobrevive ao loop inteiro porque `ledcWrite()` → `ledc_set_duty()`, que a IDF
documenta: *"This function do not change the hpoint value of this channel"* (`driver/ledc.h:277`).

Custo: 4 chamadas a `ledcWrite` por quadro, a 50 Hz. Zero interrupção, zero risco pro
escravo I2C. Preço óptico: cada die acende no máximo 1/3 do tempo → o LED fica ~3× mais
fraco. Como o pedido foi *"não precisa de brilho, só entender a cor"*, é troca boa.

**Por que 4 kHz e não 1 kHz:** a 1 kHz cada die acende num pulso de 333 µs. Numa sacada
(a olhada rápida com que você *vai* olhar pro LED) os campos R e B se separam na retina e
sai rastro arco-íris; e o rolling shutter do celular lê uma linha em 8–30 µs, então uma
janela de 333 µs cobre 11 a 42 linhas — um close do LED sai **em faixas R/G/B**, o que
arruinaria qualquer teste por vídeo. A 4 kHz o divisor continua exato
(80 MHz × 256 / (4000 × 4096) = 1250, erro 0 ppm) e a guarda ainda é 1,95 µs, três ordens
de grandeza acima do transitório do nó comum (τ = Rc·C ≈ 2–9 ns).

**Três chamadas ficam proibidas** nos GPIO 2/25/26/27, porque quebram a fase em silêncio
(verificado no fonte do core, não deduzido): `ledcChangeFrequency` /
`analogWriteFrequency` / `analogWriteResolution` / `ledcWriteTone` / `tone()` reconfiguram
o **timer compartilhado** (`esp32-hal-ledc.c:483` usa `bus->timer_num`) e derrubam os 4
canais juntos; e `ledcFade*` chama `ledc_set_duty_and_update(..., 0)` com **hpoint zero
hard-coded** (`esp32-hal-ledc.c:585`), apagando a fase daquele canal. Para "respirar" o RGB,
escreva duty no loop — que é o que o `ledStatus()` faz. Está anotado no fonte.

⚠️ **Isto exige CATODO comum.** Anodo comum não funciona: inverter a saída do LEDC inverte
o *pulso inteiro*, e os canais voltariam a se sobrepor. O firmware trava isso num
`static_assert` — se alguém virar o flag, o build falha em vez de mentir.

## 3. Qual resistor comprar/soldar

Corrente de **pico** por die, multiplexado (a média é 1/3 disso), Vdd 3,3 V:

| R comum | pico R | pico G | pico B | luz do branco | se um GPIO cair no pino COMUM |
|---|---|---|---|---|---|
| 33 Ω | 19,0 mA | 7,8 mA | 7,8 mA | 354 mcd | **50,0 mA — queima o pad** |
| 47 Ω | 16,3 mA | 6,7 mA | 6,7 mA | 306 mcd | **41,2 mA — acima do máx. absoluto** |
| 68 Ω | 13,5 mA | 5,6 mA | 5,6 mA | 255 mcd | 32,7 mA |
| 100 Ω | 10,7 mA | 4,5 mA | 4,5 mA | 203 mcd | 24,8 mA |
| **150 Ω** ✅ | **8,1 mA** | **3,4 mA** | **3,4 mA** | 154 mcd | **18,0 mA — folgado** |
| 220 Ω | 6,0 mA | 2,5 mA | 2,5 mA | 115 mcd | 13,0 mA |

**Recomendado: 150 Ω** (ou qualquer coisa entre 100 e 220 Ω). Eu tinha escrito 47 Ω aqui
primeiro, mirando no máximo de luz para o verde/azul, que são os famintos. Dois argumentos
independentes derrubaram isso, e nenhum deles custa cor:

1. **Proteção do pad.** Este LED **já entrou espelhado no soquete uma vez** (é a razão do
   `PIN_R=27` estar trocado no fonte). Se numa inserção errada um GPIO cair no pino
   **comum**, não há die nenhum no caminho e a corrente é só `3,3 / (R_pad + Rc)`: com
   47 Ω dá **41 mA, acima do máximo absoluto de 40 mA** do pad. Com 150 Ω dá 18 mA.
2. **Legibilidade da cor.** 306 mcd saindo de um die de ~5 mm² é da ordem de 17.000 cd/m²,
   ~50× uma tela clara. Um ponto assim satura os cones e o centro do LED aparece **branco
   seja qual for a cor** — exatamente o oposto de "entender a cor". Menos luz aqui é
   *melhor* leitura de matiz.

E o custo em cor é **zero**: o ganho do vermelho vai de 0,306 (47 Ω) a 0,312 (220 Ω) — 2 %.
Se quiser ainda mais matiz: lixe de leve a cúpula ou pingue silicone difusor. Espalhar a
área aparente corta a luminância 5–20× e é o que mais faz a cor aparecer.

> **Vale para o trilho de 3,3 V do regulador.** Se algum dia o 3V3 vier direto da célula
> LiPo (3,2–4,2 V), 150 Ω continua seguro na faixa inteira (pico do vermelho 12,5 mA a
> 4,2 V) — outra razão para não descer para 47 Ω.

## 4. Por que os ganhos de PWM são o que são

O firmware não manda o duty cru: ele faz **sRGB → gamma 2.2 → ganho do canal → duty**.

O ganho por canal sai de `duty_c ∝ Y_c / (luz por contagem do die c)`, onde `Y` são os
coeficientes de luminância sRGB (R 0,2126 / G 0,7152 / B 0,0722) — o que define "branco"
pro olho. Dois efeitos se somam e ambos empurram o vermelho pra baixo:

1. o vermelho recebe **~2,4× mais corrente** que verde/azul (Vf menor, mesmo resistor);
2. o olho pesa o verde **3,4× mais** que o vermelho.

Resultado: **R = 31 % · G = 100 % · B = 50,5 %** da janela.

**O achado prático:** esses ganhos são **quase independentes do resistor** — 0,303 com 22 Ω
e 0,313 com 330 Ω. Algebricamente, quando Rc cresce ele domina o denominador dos dois canais
e a razão converge para `(Vdd−V0_R)/(Vdd−V0_G)`. **Trocar o resistor muda o brilho, não a
cor** → não precisa recalcular nada se você mudar de ideia sobre o valor.

A maior incerteza que sobra é a razão de **mcd entre os dies** (varia por lote, chutei
R 800 / G 2000 / B 400 mcd @20 mA). Por isso existe o `led ganho` — calibração a olho.

### O que eu deliberadamente NÃO fiz: correção colorimétrica

Uma auditoria refez essa conta com as CMFs CIE 1931 e apontou, com razão, que usar os `Y`
do sRGB com primárias que **não são** as do sRGB põe o branco em ~11.500 K (Δuv 0,037 de
D65) — ele vai puxar levemente pro ciano. A correção completa seria trocar os ganhos por
0,4695/1,00/0,4172 **e** aplicar uma matriz 3×3 no domínio linear.

Não apliquei, por dois motivos:

- **Meia correção é pior que nenhuma.** Os dois erros (Y do sRGB + gamut errado) hoje se
  cancelam em parte; trocar só os ganhos piora 4 das 6 cores da paleta (o amarelo vai de
  +7° para −11° de erro de matiz). Ou os dois juntos, ou nenhum.
- **A matriz otimiza a coisa errada para um indicador.** Ela mapeia as primárias do sRGB
  para dentro do gamut do LED, então "verde puro" (0,255,0) sairia como 37 % vermelho +
  93 % verde — um verde-amarelado. Isso é fidelidade sRGB, mas **aproxima** o verde do
  "Pi lendo" do âmbar do "carregar", que é justamente o par que precisa ser inconfundível.
  Num farol de status, o que vale é separação máxima entre estados, não Δ E baixo.

Ou seja: fica com as primárias nativas saturadas e o branco levemente frio. Se o branco
incomodar, `led ganho` resolve em 30 segundos — e aí é ajuste medido no LED real, não num
modelo de comprimento de onda chutado.

## 5. Bancada (5 minutos, pela USB, sem o Pi)

✅ **Resolvido:** o scan ocupava ~60 % do tempo e sequestrava a cor do RGB (o `coletarWiFi()`
rearma a varredura a cada chamada, e um scan 2,4 GHz async leva ~4 s de cada 8 s; somado ao
BLE, a condição ficava verdadeira quase sempre e vencia "Pi lendo" e "ocioso" no `else if`).
Agora ele mora no LED azul do GPIO4 (seção 7) e o RGB diz o estado o tempo inteiro.

Serial 115200, digite:

| comando | o que provar |
|---|---|
| `led cal` | **primeiro de todos**: lê a fase no registrador do LEDC e diz `OK` ou `QUEBRADA` por canal. Se não vier `0 / 1365 / 2730`, pare — o resto não vale nada |
| `led ciclo` | R, G, B, **amarelo, ciano, magenta**, branco — 1,2 s cada, por 60 s |
| `led y` | **o teste que importa**: amarelo tem que ser amarelo. Se sair vermelho, a multiplexação não está valendo |
| `led c` / `led m` | ciano e magenta limpos |
| `led w` | branco. Puxando pro vermelho → baixe o ganho R; pro verde → suba o R |
| `led ganho 250 1000 505` | ajusta ao vivo (milésimos da janela; só na RAM). Cada `led ganho` **renova** os 60 s da cor travada, então dá pra iterar sem o alvo sumir |
| `scan` | o **LED azul do GPIO4** tem que piscar a 2 Hz por ~4 s (o RGB não muda mais) |
| `led auto` | volta ao status real |

Calibre **com uma referência branca no campo de visão** (folha sob luz do dia, ou a tela do
celular em `#FFFFFF` encostada no LED). Um LED sozinho no escuro vira "branco" por adaptação
cromática em ~30 s e a calibração nunca converge.

Quando o branco/amarelo ficarem bons, **crave os números em `GANHO[]`** no fonte e regrave —
o comando não persiste. Enquanto isso o LED da placa (GPIO2) continua batendo: se ele bate
e o RGB está estranho, o problema é do farol; se ele parou, é a ESP. E se o GPIO2 estiver
**piscando rápido e liso** (não o lub-dub) com o RGB apagado, é o fail-safe: o
`farolSetup()` falhou e o firmware preferiu não acender a mentir uma cor (sem fase, os 3
canais caem no hpoint 0 e o LED ficaria vermelho fixo — idêntico ao alarme de bateria).

## 6. Paleta nova

Como agora a mistura é honesta, as cores viraram **matizes de saturação cheia** e quem faz
o balanço é o `GANHO[]` (não dá mais pra escurecer baixando os números — use `setCorN()`,
senão o matiz anda):

| estado | antes | agora | motivo |
|---|---|---|---|
| bateria crítica | (255,0,0) piscando | igual | — |
| carregar | (255,40,0) | **(255,120,0)** | com gamma correto, 40/255 vira 1,6 % de luz e some; 120 dá âmbar de verdade |
| escaneando | roxo (40,0,60) | **saiu do RGB** | virou LED próprio (seção 7) — rodava ~60 % do tempo e abafava todo o resto |
| Pi lendo | verde (0,120,0) | **(0,255,40)** | saturação cheia; o brilho quem controla é o ganho |
| ocioso | azul 40 | **(0,60,255) nível 90** | mesmo matiz, escurecido do jeito certo |

## 7. LED de scan separado (GPIO4) — três LEDs, três perguntas

O estado "escaneando" saiu do RGB. Ele respondia a uma pergunta diferente das outras
(*atividade* em vez de *estado*) e, por rodar ~60 % do tempo, ganhava do resto na cadeia de
`else if` — o verde do "Pi lendo" quase nunca aparecia. Agora cada sinal tem um lugar só:

| LED | pergunta | como se lê |
|---|---|---|
| RGB (25/26/27) | **qual é o estado** | cor estável |
| onboard GPIO2 | **está viva** | batimento lub-dub, 1,2 s |
| **azul avulso GPIO4** | **tem varredura rodando** | pisca a 2 Hz |

**Fiação:** anodo (perna longa) no **GPIO4**, catodo → resistor → GND. Circuito totalmente
independente do RGB — não entra no multiplex e pode acender junto com qualquer cor.
GPIO4 é seguro: não é strapping, não pulsa no boot, tem pulldown fraco no reset (LED apagado
até o `setup()`) e não gasta ADC1. Alternativas: 13, 18, 19, 23. **Nunca 0/2/5/12/15.**

**Resistor** (azul sozinho, sem multiplex, mas piscando a 50 % → média é metade):

| R | azul típico | azul de Vf alto | leitura |
|---|---|---|---|
| 220 Ω | 2,53 mA | 1,45 mA | visível de dia |
| **330 Ω** ✅ | **1,81 mA** | 1,04 mA | discreto |
| **470 Ω** ✅ | **1,33 mA** | 0,76 mA | discreto |
| 1 kΩ | 0,66 mA | 0,38 mA | só no escuro |

Comece com **470 Ω**; se ficar apagado demais, 330 ou 220. O azul tem pouco headroom
(Vf ~3,0 V contra trilho de 3,3 V), então a corrente varia com o lote — as duas colunas
mostram a faixa. **Para escurecer sem ferro de solda**, baixe `SCAN_NIVEL` (0–255) no
firmware: ele passa pelo mesmo gamma e tem canal de PWM próprio.

**Teste:** digite `scan` no terminal → o azul tem que piscar por ~4 s. E `led cal` agora
imprime o pino, o nível, a frequência e se está varrendo **neste instante**.

## 7. Quando o ARGB chegar

Nada disto se perde e nada disto atrapalha: o `PROJETO-ARGB.md` já prevê **reaproveitar o
GPIO27** (hoje o vermelho) como DIN e o **GND comum** como terra do módulo. Quando trocar,
o bloco "FAROL RGB" inteiro sai e entra o `rgbLedWriteOrdered()`. O que sobrevive é a lição:
**cor parada não é estado válido**, e o mapa de estados por *ritmo* do doc do ARGB.
