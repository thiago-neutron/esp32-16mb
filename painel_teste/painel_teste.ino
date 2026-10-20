/*
 * Painel de teste — ESP32 com flash de 16MB
 * -----------------------------------------
 * Hotspot WiFi + página web que mostra, atualizando a cada 1s:
 *   - estado de cada GPIO (com modo de leitura alternável: pull-up / pull-down / flutuante)
 *   - histórico das últimas mudanças de pino (pra pegar pulso curto que o refresh perderia)
 *   - varredura BLE dos dispositivos ao redor
 *   - varredura das redes WiFi ao redor
 *   - saúde do chip: heap, uptime, flash, partições
 *   - botão liga/desliga do LED da placa
 *
 * Conecte no WiFi "ESP32-PAINEL" (senha em segredos.h) e abra http://192.168.4.1
 */
#include <WiFi.h>
#include <WebServer.h>
#include <esp_partition.h>
#include <esp_flash.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// ---------------------------------------------------------------- configuração
static const char *AP_SSID = "ESP32-PAINEL";
#include "segredos.h"  // AP_PASS (minimo 8 caracteres)
static const int   LED_PIN = 2;                 // LED onboard da maioria dos DevKit

// GPIOs monitorados. 6..11 ficam de fora DE PROPÓSITO: são a flash SPI —
// mexer neles trava o chip. 34..39 são input-only e não têm pull interno.
static const uint8_t PINOS[]      = {0, 4, 5, 12, 13, 14, 15, 16, 17, 18, 19,
                                     21, 22, 23, 25, 26, 27, 32, 33};
static const uint8_t PINOS_IN[]   = {34, 35, 36, 39};
static const size_t  N_PINOS      = sizeof(PINOS);
static const size_t  N_PINOS_IN   = sizeof(PINOS_IN);

// ---------------------------------------------------------------- estado global
WebServer servidor(80);
bool     ledLigado   = false;
uint8_t  modoLeitura = 0;                       // 0=pull-up  1=pull-down  2=flutuante
int      ultimoNivel[40];

struct Evento { uint32_t ms; uint8_t pino; uint8_t nivel; };
Evento  historico[24];
uint8_t histQtd = 0, histIni = 0;

struct DispBLE { String endereco, nome; int rssi; };
DispBLE  bleAchados[24];
uint8_t  bleQtd = 0;
uint32_t bleUltimoScan = 0;
bool     bleEscaneando = false;
BLEScan *bleScan = nullptr;

// ---------------------------------------------------------------- GPIO
void aplicarModo() {
  for (size_t i = 0; i < N_PINOS; i++) {
    if (PINOS[i] == LED_PIN) continue;
    pinMode(PINOS[i], modoLeitura == 0 ? INPUT_PULLUP
                    : modoLeitura == 1 ? INPUT_PULLDOWN
                                       : INPUT);
  }
  for (size_t i = 0; i < N_PINOS_IN; i++) pinMode(PINOS_IN[i], INPUT);
}

void registrarEvento(uint8_t pino, uint8_t nivel) {
  historico[(histIni + histQtd) % 24] = {millis(), pino, nivel};
  if (histQtd < 24) histQtd++; else histIni = (histIni + 1) % 24;
}

void varrerPinos() {
  for (size_t i = 0; i < N_PINOS; i++) {
    if (PINOS[i] == LED_PIN) continue;
    int v = digitalRead(PINOS[i]);
    if (v != ultimoNivel[PINOS[i]]) { ultimoNivel[PINOS[i]] = v; registrarEvento(PINOS[i], v); }
  }
  for (size_t i = 0; i < N_PINOS_IN; i++) {
    int v = digitalRead(PINOS_IN[i]);
    if (v != ultimoNivel[PINOS_IN[i]]) { ultimoNivel[PINOS_IN[i]] = v; registrarEvento(PINOS_IN[i], v); }
  }
}

// ---------------------------------------------------------------- BLE
class CallbackBLE : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice dispositivo) override {
    String end = dispositivo.getAddress().toString().c_str();
    for (uint8_t i = 0; i < bleQtd; i++)
      if (bleAchados[i].endereco == end) { bleAchados[i].rssi = dispositivo.getRSSI(); return; }
    if (bleQtd >= 24) return;
    bleAchados[bleQtd].endereco = end;
    bleAchados[bleQtd].nome     = dispositivo.haveName() ? String(dispositivo.getName().c_str()) : String("(sem nome)");
    bleAchados[bleQtd].rssi     = dispositivo.getRSSI();
    bleQtd++;
  }
};

void iniciarScanBLE() {
  if (bleEscaneando) return;
  bleQtd = 0;
  bleEscaneando = true;
  bleScan->start(4, [](BLEScanResults r) { bleEscaneando = false; }, false);
  bleUltimoScan = millis();
}

// ---------------------------------------------------------------- página
static const char PAGINA[] PROGMEM = R"HTML(<!doctype html><html lang="pt-BR"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 — Painel de teste</title><style>
*{box-sizing:border-box}body{margin:0;background:#0b0f14;color:#c8d6e5;font:14px/1.5 ui-monospace,Menlo,Consolas,monospace}
header{padding:14px 18px;border-bottom:1px solid #1e2a38;display:flex;flex-wrap:wrap;gap:12px;align-items:center}
h1{font-size:16px;margin:0;color:#4ade80;letter-spacing:.5px}
.wrap{padding:18px;display:grid;gap:18px;grid-template-columns:repeat(auto-fit,minmax(310px,1fr));max-width:1400px}
.card{background:#111823;border:1px solid #1e2a38;border-radius:10px;padding:14px;overflow:auto}
.card h2{font-size:12px;margin:0 0 10px;color:#64748b;text-transform:uppercase;letter-spacing:1px}
.pinos{display:grid;grid-template-columns:repeat(auto-fill,minmax(76px,1fr));gap:7px}
.pino{border:1px solid #1e2a38;border-radius:7px;padding:7px 4px;text-align:center;background:#0d131c}
.pino b{display:block;font-size:11px;color:#64748b}
.hi{color:#4ade80;border-color:#14532d;background:#0d1a12}.lo{color:#f87171;border-color:#4c1d1d;background:#1a0f0f}
button{background:#1e2a38;color:#c8d6e5;border:1px solid #2d3f54;border-radius:7px;padding:7px 13px;cursor:pointer;font:inherit}
button:hover{background:#2d3f54}button.on{background:#14532d;border-color:#4ade80;color:#4ade80}
table{width:100%;border-collapse:collapse}td,th{padding:4px 6px;text-align:left;border-bottom:1px solid #1a2432;font-size:12px}
th{color:#64748b;font-weight:400}.dim{color:#64748b}.ok{color:#4ade80}
</style></head><body>
<header>
  <h1>ESP32 · PAINEL DE TESTE</h1>
  <button id="led">LED: --</button>
  <button id="modo">modo: --</button>
  <button id="ble">escanear BLE</button>
  <span class="dim" id="rodape"></span>
</header>
<div class="wrap">
  <div class="card"><h2>GPIO <span class="dim" id="modoTxt"></span></h2><div class="pinos" id="pinos"></div></div>
  <div class="card"><h2>Bluetooth ao redor</h2><table id="tble"><tr><th>endereço</th><th>nome</th><th>rssi</th></tr></table></div>
  <div class="card"><h2>Redes WiFi ao redor</h2><table id="twifi"><tr><th>rede</th><th>canal</th><th>rssi</th></tr></table></div>
  <div class="card"><h2>Últimas mudanças de pino</h2><table id="thist"><tr><th>quando</th><th>pino</th><th>nível</th></tr></table></div>
  <div class="card"><h2>Chip e memória</h2><table id="tsis"></table></div>
  <div class="card"><h2>Partições da flash</h2><table id="tpart"><tr><th>nome</th><th>início</th><th>tamanho</th></tr></table></div>
</div>
<script>
const MODOS=["pull-up","pull-down","flutuante"];
async function tick(){
  const r=await fetch("/api/estado");if(!r.ok)return;const d=await r.json();
  document.getElementById("pinos").innerHTML=d.gpios.map(p=>
    `<div class="pino ${p.v?"hi":"lo"}"><b>GPIO${p.p}${p.ro?" ·ro":""}</b>${p.v?"ALTO":"BAIXO"}</div>`).join("");
  document.getElementById("tble").innerHTML='<tr><th>endereço</th><th>nome</th><th>rssi</th></tr>'+
    (d.ble.length?d.ble.map(b=>`<tr><td>${b.e}</td><td>${b.n}</td><td>${b.r} dBm</td></tr>`).join("")
                 :`<tr><td colspan=3 class="dim">${d.bleScan?"escaneando…":"nada encontrado — clique em escanear BLE"}</td></tr>`);
  document.getElementById("twifi").innerHTML='<tr><th>rede</th><th>canal</th><th>rssi</th></tr>'+
    (d.wifi.length?d.wifi.map(w=>`<tr><td>${w.s}</td><td>${w.c}</td><td>${w.r} dBm</td></tr>`).join("")
                  :'<tr><td colspan=3 class="dim">—</td></tr>');
  document.getElementById("thist").innerHTML='<tr><th>quando</th><th>pino</th><th>nível</th></tr>'+
    (d.hist.length?d.hist.map(h=>`<tr><td>${(h.t/1000).toFixed(1)}s</td><td>GPIO${h.p}</td><td class="${h.v?"ok":""}">${h.v?"ALTO":"BAIXO"}</td></tr>`).join("")
                  :'<tr><td colspan=3 class="dim">nenhuma mudança ainda — aterre um pino pra testar</td></tr>');
  document.getElementById("tsis").innerHTML=Object.entries(d.sis).map(([k,v])=>`<tr><td class="dim">${k}</td><td>${v}</td></tr>`).join("");
  document.getElementById("tpart").innerHTML='<tr><th>nome</th><th>início</th><th>tamanho</th></tr>'+
    d.part.map(p=>`<tr><td>${p.n}</td><td>${p.o}</td><td>${p.k} KB</td></tr>`).join("");
  const b=document.getElementById("led");b.textContent="LED: "+(d.led?"ligado":"desligado");b.className=d.led?"on":"";
  document.getElementById("modo").textContent="modo: "+MODOS[d.modo];
  document.getElementById("modoTxt").textContent="("+MODOS[d.modo]+")";
  document.getElementById("rodape").textContent=d.sis["clientes no hotspot"]+" cliente(s) · "+d.sis["heap livre"];
}
document.getElementById("led").onclick =()=>fetch("/api/led",{method:"POST"}).then(tick);
document.getElementById("modo").onclick=()=>fetch("/api/modo",{method:"POST"}).then(tick);
document.getElementById("ble").onclick =()=>fetch("/api/ble",{method:"POST"}).then(tick);
tick();setInterval(tick,1000);
</script></body></html>)HTML";

// ---------------------------------------------------------------- rotas
String jsonEstado() {
  String j = "{\"led\":"; j += ledLigado ? "true" : "false";
  j += ",\"modo\":"; j += modoLeitura;
  j += ",\"bleScan\":"; j += bleEscaneando ? "true" : "false";

  j += ",\"gpios\":[";
  bool primeiro = true;
  for (size_t i = 0; i < N_PINOS; i++) {
    if (PINOS[i] == LED_PIN) continue;
    if (!primeiro) j += ",";
    primeiro = false;
    j += "{\"p\":"; j += PINOS[i]; j += ",\"v\":"; j += digitalRead(PINOS[i]); j += ",\"ro\":0}";
  }
  for (size_t i = 0; i < N_PINOS_IN; i++) {
    j += ",{\"p\":"; j += PINOS_IN[i]; j += ",\"v\":"; j += digitalRead(PINOS_IN[i]); j += ",\"ro\":1}";
  }
  j += "]";

  j += ",\"ble\":[";
  for (uint8_t i = 0; i < bleQtd; i++) {
    if (i) j += ",";
    j += "{\"e\":\""; j += bleAchados[i].endereco;
    j += "\",\"n\":\""; j += bleAchados[i].nome;
    j += "\",\"r\":"; j += bleAchados[i].rssi; j += "}";
  }
  j += "]";

  j += ",\"wifi\":[";
  int n = WiFi.scanComplete();
  if (n > 0) {
    for (int i = 0; i < n && i < 20; i++) {
      if (i) j += ",";
      j += "{\"s\":\""; j += WiFi.SSID(i);
      j += "\",\"c\":"; j += WiFi.channel(i);
      j += ",\"r\":"; j += WiFi.RSSI(i); j += "}";
    }
  }
  j += "]";

  j += ",\"hist\":[";
  for (uint8_t k = 0; k < histQtd; k++) {
    Evento &e = historico[(histIni + histQtd - 1 - k) % 24];
    if (k) j += ",";
    j += "{\"t\":"; j += e.ms; j += ",\"p\":"; j += e.pino; j += ",\"v\":"; j += e.nivel; j += "}";
  }
  j += "]";

  uint32_t tamFlash = 0; esp_flash_get_size(NULL, &tamFlash);
  uint32_t up = millis() / 1000;
  j += ",\"sis\":{";
  j += "\"chip\":\""; j += ESP.getChipModel(); j += " rev "; j += ESP.getChipRevision(); j += "\",";
  j += "\"MAC\":\""; j += WiFi.macAddress(); j += "\",";
  j += "\"flash\":\""; j += tamFlash / (1024 * 1024); j += " MB\",";
  j += "\"sketch\":\""; j += ESP.getSketchSize() / 1024; j += " KB de ";
  j += (ESP.getSketchSize() + ESP.getFreeSketchSpace()) / 1024; j += " KB\",";
  j += "\"heap livre\":\""; j += ESP.getFreeHeap() / 1024; j += " KB\",";
  j += "\"heap minimo\":\""; j += ESP.getMinFreeHeap() / 1024; j += " KB\",";
  j += "\"uptime\":\""; j += up / 3600; j += "h "; j += (up % 3600) / 60; j += "m "; j += up % 60; j += "s\",";
  j += "\"clientes no hotspot\":"; j += WiFi.softAPgetStationNum();
  j += "}";

  j += ",\"part\":[";
  esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
  bool p1 = true;
  while (it) {
    const esp_partition_t *p = esp_partition_get(it);
    if (!p1) j += ",";
    p1 = false;
    char buf[16]; snprintf(buf, sizeof(buf), "0x%06lX", (unsigned long)p->address);
    j += "{\"n\":\""; j += p->label; j += "\",\"o\":\""; j += buf; j += "\",\"k\":"; j += p->size / 1024; j += "}";
    it = esp_partition_next(it);
  }
  esp_partition_iterator_release(it);
  j += "]}";
  return j;
}

// ---------------------------------------------------------------- setup / loop
void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  for (int i = 0; i < 40; i++) ultimoNivel[i] = -1;
  aplicarModo();

  WiFi.mode(WIFI_AP_STA);                       // AP pro painel + STA pra escanear redes
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.printf("\nHotspot \"%s\" no ar — abra http://%s\n", AP_SSID, WiFi.softAPIP().toString().c_str());

  BLEDevice::init("ESP32-PAINEL");
  bleScan = BLEDevice::getScan();
  bleScan->setAdvertisedDeviceCallbacks(new CallbackBLE(), true);
  bleScan->setActiveScan(true);
  bleScan->setInterval(100);
  bleScan->setWindow(99);

  servidor.on("/", []() { servidor.send_P(200, "text/html; charset=utf-8", PAGINA); });
  servidor.on("/api/estado", []() { servidor.send(200, "application/json", jsonEstado()); });
  servidor.on("/api/led",  HTTP_POST, []() { ledLigado = !ledLigado; digitalWrite(LED_PIN, ledLigado); servidor.send(200, "application/json", "{\"ok\":1}"); });
  servidor.on("/api/modo", HTTP_POST, []() { modoLeitura = (modoLeitura + 1) % 3; aplicarModo(); servidor.send(200, "application/json", "{\"ok\":1}"); });
  servidor.on("/api/ble",  HTTP_POST, []() { iniciarScanBLE(); servidor.send(200, "application/json", "{\"ok\":1}"); });
  servidor.onNotFound([]() { servidor.send(404, "text/plain", "nao existe"); });
  servidor.begin();

  WiFi.scanNetworks(true);                      // primeira varredura WiFi, assíncrona
  iniciarScanBLE();
}

void loop() {
  servidor.handleClient();
  varrerPinos();                                // pega pulso curto entre um refresh e outro

  static uint32_t tWifi = 0;
  if (millis() - tWifi > 20000) {               // rescan WiFi a cada 20s
    tWifi = millis();
    if (WiFi.scanComplete() != WIFI_SCAN_RUNNING) WiFi.scanNetworks(true);
  }
  if (!bleEscaneando && millis() - bleUltimoScan > 30000) iniciarScanBLE();
}
