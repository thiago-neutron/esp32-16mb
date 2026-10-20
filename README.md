# esp32-16mb

Firmwares e ferramentas de bancada para um **ESP32 com flash de 16 MB** trabalhando como
**sonda** ao lado de um Raspberry Pi: mede uma célula LiPo, escaneia WiFi e BLE de 2,4 GHz
(recon passivo) e conversa com o Pi por I2C, UART ou WiFi.

## Sketches (Arduino IDE / arduino-cli)

| Pasta | O que faz |
|---|---|
| `sonda_i2c/` | **Versão principal.** ESP como escravo I2C `0x08` (GPIO21/22): o Pi puxa o JSON de telemetria (bateria + scans). UART2 (GPIO16/17) vira terminal interativo (`scan`, `bat`, `wifi`, `ble`, `reboot`, `help`) e ponte para o console do Pi. |
| `sonda/` | Sonda com AP próprio + página web (testável só com o celular) e STA opcional que empurra o JSON para o Pi a cada 1 s. |
| `sonda_serial/` | Sonda sem WiFi de dados: o rádio fica 100% no scan e o JSON sai pela UART2 para o Pi. |
| `painel_teste/` | Hotspot + página que mostra GPIOs (com pull-up/down), histórico de mudanças de pino, scans BLE/WiFi e saúde do chip (heap, flash, partições). |
| `led_teste/` | Mínimo para provar a fiação do LED RGB de 4 pinos (GPIO25/26/27). |
| `teste_16mb/` | Prova, do lado do firmware, que os 16 MB estão visíveis. |

Cada sketch traz o seu `partitions.csv` (esquema *Custom* para 16 MB). Arduino IDE:
*Board* = ESP32 Dev Module, *Flash Size* = 16MB (128Mb), *Partition Scheme* = Custom.

Fiação (3,3 V dos dois lados, sem level shifter), em `sonda_i2c/sonda_i2c.ino`:
I2C não cruza (GPIO21↔SDA pino 3, GPIO22↔SCL pino 5); UART cruza (GPIO17→RXD pino 10,
GPIO16←TXD pino 8); GND comum (pino 6) obrigatório.

### Senhas

`sonda/` e `painel_teste/` usam um `segredos.h` que **não está no repo**. Antes de compilar:

```bash
cp sonda/segredos.exemplo.h sonda/segredos.h
cp painel_teste/segredos.exemplo.h painel_teste/segredos.h
# e troque "troque-esta-senha" (o AP do ESP exige >= 8 caracteres)
```

## Ferramentas (PowerShell, rodando a partir do WSL)

| Script | O que faz |
|---|---|
| `ler_sonda.ps1` | Lê a serial do ESP32. |
| `cmd_sonda.ps1` | Manda comandos avulsos para o terminal do ESP e mostra a resposta. |
| `ponte_sonda.ps1` | Fala com o console do Pi através da ponte do ESP. |
| `assiste_ponte.ps1` | Abre a ponte e grava o console do Pi em arquivo. |
| `espera_e_ponte.ps1` | Espera o ESP reaparecer no USB e abre a ponte sozinho. |
| `login_pi.ps1` | Loga no console do Pi pela ponte e roda comandos (senha só por `-Pass`). |
| `arma_e_loga.ps1` | Espera o replug, abre a ponte e loga sozinho (senha só por `-Pass`). |
| `testa_cores.ps1` | Demo de cores do LED RGB. |

`calc_led_comum.py` faz as contas do LED RGB com **um** resistor no comum.

## Documentos

- `PROJETO-ARGB.md` — projeto do "farol" da sonda com LED ARGB (WS2812B).
- `LED-RESISTOR-COMUM.md` — cálculo e firmware do LED RGB com um resistor no catodo comum.
- `_versoes/` — versões anteriores do `sonda_i2c` e o PDF de pinagem para soldagem.

## Fora do repo, de propósito

Dumps do chip (`*.bin` — o dump de flash cobre a área onde o ESP32 guarda a senha do WiFi),
capturas de console do Pi (`captura_*.txt`) e os `segredos.h`.

## Licença

MIT — ver [`LICENSE`](LICENSE).
