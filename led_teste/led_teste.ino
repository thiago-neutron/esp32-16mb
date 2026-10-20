/*
 * led_teste — firmware MINIMO so pra provar a fiacao do LED RGB de 4 pinos.
 * Nao tem WiFi, BLE, I2C nem bateria: so acende um canal por vez, no maximo (255),
 * pra cima e repetindo pra sempre. Serial 115200 diz qual GPIO esta ligado agora.
 *
 * Ciclo (2 s cada, com 250 ms apagado entre eles pra separar bem):
 *   GPIO25 -> GPIO26 -> GPIO27 -> os tres juntos (branco)
 *
 * Se um passo ficar APAGADO, aquele canal esta com problema fisico
 * (perna nao soldada, resistor aberto ou o die daquela cor morto).
 */

static const int PIN_25 = 25, PIN_26 = 26, PIN_27 = 27;

// false = catodo comum (comum no GND): ligar = nivel ALTO
// true  = anodo  comum (comum no 3,3V): ligar = nivel BAIXO
static const bool COMMON_ANODE = false;

static const int  NIVEL   = 255;   // todas as cores no maximo
static const int  MS_COR  = 2000;  // tempo aceso por cor
static const int  MS_GAP  = 250;   // apagado entre as cores

void escreve(int p25, int p26, int p27) {
  if (COMMON_ANODE) { p25 = 255 - p25; p26 = 255 - p26; p27 = 255 - p27; }
  analogWrite(PIN_25, p25);
  analogWrite(PIN_26, p26);
  analogWrite(PIN_27, p27);
}
void apaga() { escreve(0, 0, 0); delay(MS_GAP); }

void setup() {
  Serial.begin(115200);
  delay(300);
  pinMode(PIN_25, OUTPUT); pinMode(PIN_26, OUTPUT); pinMode(PIN_27, OUTPUT);
  Serial.println();
  Serial.println("led_teste PRONTO — ciclo GPIO25 -> GPIO26 -> GPIO27 -> branco, 2s cada");
}

void loop() {
  Serial.println("GPIO25 = 255  (canal que deveria ser VERMELHO)");
  escreve(NIVEL, 0, 0); delay(MS_COR); apaga();

  Serial.println("GPIO26 = 255  (canal que deveria ser VERDE)");
  escreve(0, NIVEL, 0); delay(MS_COR); apaga();

  Serial.println("GPIO27 = 255  (canal que deveria ser AZUL)");
  escreve(0, 0, NIVEL); delay(MS_COR); apaga();

  Serial.println("os tres = 255 (deveria dar BRANCO)");
  escreve(NIVEL, NIVEL, NIVEL); delay(MS_COR); apaga();
}
