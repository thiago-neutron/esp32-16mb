/*
 * SONDA ESP32 (versão SERIAL) — bateria + scanner 2,4GHz, envia por UART ao Pi
 * --------------------------------------------------------------------------
 * Sem WiFi de dados, sem AP: o rádio do ESP fica 100% pro scan.
 * A cada 1s manda uma linha JSON pela UART2 -> o Pi lê e serve o painel.
 *
 * Fiação (níveis 3,3V dos dois lados — SEM level shifter):
 *   ESP GPIO17 (TX2) --> Pi GPIO15 (RXD, pino 10)
 *   ESP GPIO16 (RX2) <-- Pi GPIO14 (TXD, pino 8)
 *   ESP GND         <-> Pi GND (pino 6)      [obrigatorio]
 *   Alimentacao: cada um no seu USB (mais seguro). Se for alimentar o ESP pelo
 *   Pi, use Pi 5V (pino 2) -> ESP VIN/5V. NUNCA 3V3<->3V3, NUNCA 5V num GPIO.
 *
 * Bateria: 1 célula LiPo por divisor 100k/100k no GPIO34 (ADC1, ok com radio).
 */
#include <WiFi.h>
#include <esp_flash.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// UART2 pro Pi
static const int PIN_TX2 = 17, PIN_RX2 = 16;
static const uint32_t BAUD_PI = 115200;

// Bateria
static const int   PIN_BAT   = 34;
static const float DIVISOR   = 2.0f;
static const float V_CHEIA=4.15f, V_OK=3.60f, V_BAIXA=3.40f, V_CRITICA=3.20f;

float baterioV = 0; int baterioPct = 0; String baterioMsg = "—";

struct RedeWiFi { String ssid; int rssi, canal; String cripto; };
RedeWiFi wifiAch[24]; uint8_t wifiQtd = 0;
struct DispBLE { String end, nome; int rssi; };
DispBLE bleAch[24]; uint8_t bleQtd = 0;
BLEScan *bleScan = nullptr; bool bleBusy = false; uint32_t bleT = 0;

float lerBateria() {
  uint32_t s = 0; for (int i=0;i<16;i++){ s+=analogReadMilliVolts(PIN_BAT); delay(2);}
  return (s/16.0f)*DIVISOR/1000.0f;
}
int pctLiPo(float v){
  const float t[][2]={{4.20,100},{4.10,90},{4.00,80},{3.90,70},{3.85,60},{3.80,50},
                      {3.75,40},{3.70,30},{3.60,20},{3.50,12},{3.40,7},{3.30,3},{3.00,0}};
  if(v>=4.20)return 100; if(v<=3.00)return 0;
  for(int i=0;i<12;i++) if(!(v<=t[i][1]?false:v>=t[i+1][0])){
    float f=(v-t[i+1][0])/(t[i][0]-t[i+1][0]); return (int)(t[i+1][1]+f*(t[i][1]-t[i+1][1]));}
  return 50;
}
void atualizarBateria(){
  baterioV=lerBateria(); baterioPct=pctLiPo(baterioV);
  if(baterioV<V_CRITICA) baterioMsg="CRITICA — desligue e carregue ja (risco de dano)";
  else if(baterioV<V_BAIXA) baterioMsg="carregar agora";
  else if(baterioV<V_OK) baterioMsg="carregar em breve";
  else if(baterioV>=V_CHEIA) baterioMsg="cheia / carregando";
  else baterioMsg="ok";
}

class CbBLE : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice d) override {
    String e=d.getAddress().toString().c_str();
    for(uint8_t i=0;i<bleQtd;i++) if(bleAch[i].end==e){bleAch[i].rssi=d.getRSSI();return;}
    if(bleQtd>=24)return;
    bleAch[bleQtd].end=e; bleAch[bleQtd].nome=d.haveName()?String(d.getName().c_str()):String("(sem nome)");
    bleAch[bleQtd].rssi=d.getRSSI(); bleQtd++;
  }
};
void scanBLE(){ if(bleBusy)return; bleQtd=0; bleBusy=true;
  bleScan->start(4,[](BLEScanResults r){bleBusy=false;},false); bleT=millis(); }

const char* cripto(wifi_auth_mode_t m){ switch(m){
  case WIFI_AUTH_OPEN:return "aberta"; case WIFI_AUTH_WEP:return "WEP";
  case WIFI_AUTH_WPA_PSK:return "WPA"; case WIFI_AUTH_WPA2_PSK:return "WPA2";
  case WIFI_AUTH_WPA_WPA2_PSK:return "WPA/2"; case WIFI_AUTH_WPA3_PSK:return "WPA3";
  case WIFI_AUTH_WPA2_WPA3_PSK:return "WPA2/3"; default:return "?";} }
void coletarWiFi(){
  int n=WiFi.scanComplete();
  if(n==WIFI_SCAN_FAILED){ WiFi.scanNetworks(true,true); return; }
  if(n==WIFI_SCAN_RUNNING) return;
  wifiQtd=0;
  for(int i=0;i<n&&wifiQtd<24;i++){
    wifiAch[wifiQtd]={ WiFi.SSID(i).length()?WiFi.SSID(i):String("(oculta)"),
                       WiFi.RSSI(i), WiFi.channel(i), String(cripto(WiFi.encryptionType(i))) };
    wifiQtd++;
  }
  WiFi.scanDelete(); WiFi.scanNetworks(true,true);
}

String jsonEstado(){
  String j="{\"bat\":{\"v\":"; j+=String(baterioV,3); j+=",\"pct\":"; j+=baterioPct;
  j+=",\"msg\":\""; j+=baterioMsg; j+="\"}";
  j+=",\"wifi\":[";
  for(uint8_t i=0;i<wifiQtd;i++){ if(i)j+=",";
    j+="{\"s\":\""; j+=wifiAch[i].ssid; j+="\",\"r\":"; j+=wifiAch[i].rssi;
    j+=",\"c\":"; j+=wifiAch[i].canal; j+=",\"k\":\""; j+=wifiAch[i].cripto; j+="\"}"; }
  j+="]";
  j+=",\"ble\":[";
  for(uint8_t i=0;i<bleQtd;i++){ if(i)j+=",";
    j+="{\"e\":\""; j+=bleAch[i].end; j+="\",\"n\":\""; j+=bleAch[i].nome;
    j+="\",\"r\":"; j+=bleAch[i].rssi; j+="}"; }
  j+="]";
  uint32_t fl=0; esp_flash_get_size(NULL,&fl); uint32_t up=millis()/1000;
  j+=",\"sis\":{\"mac\":\""; j+=WiFi.macAddress();
  j+="\",\"flash_mb\":"; j+=fl/(1024*1024);
  j+=",\"heap_kb\":"; j+=ESP.getFreeHeap()/1024;
  j+=",\"up_s\":"; j+=up; j+=",\"link\":\"serial\"}}";
  return j;
}

void setup(){
  Serial.begin(115200);                                   // USB: debug/flash
  Serial2.begin(BAUD_PI, SERIAL_8N1, PIN_RX2, PIN_TX2);   // link com o Pi
  analogReadResolution(12); analogSetPinAttenuation(PIN_BAT, ADC_11db);

  WiFi.mode(WIFI_STA); WiFi.disconnect();                 // radio ligado SO pra escanear
  BLEDevice::init("");
  bleScan=BLEDevice::getScan();
  bleScan->setAdvertisedDeviceCallbacks(new CbBLE(), true);
  bleScan->setActiveScan(true); bleScan->setInterval(100); bleScan->setWindow(99);

  atualizarBateria();
  WiFi.scanNetworks(true,true);
  scanBLE();
  Serial.println("sonda serial pronta -> enviando JSON pela UART2 (GPIO17=TX)");
}

void loop(){
  static uint32_t t1=0;
  if(millis()-t1>=1000){ t1=millis();
    atualizarBateria();
    String j=jsonEstado();
    Serial2.println(j);       // vai pro Pi
    Serial.println(j);        // espelho no USB p/ debug
  }
  static uint32_t tw=0;
  if(millis()-tw>=8000){ tw=millis(); coletarWiFi(); }
  if(!bleBusy && millis()-bleT>15000) scanBLE();
}
