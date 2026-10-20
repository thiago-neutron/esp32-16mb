/*
 * SONDA ESP32 — bateria + scanner 2,4 GHz (WiFi/BLE), recon PASSIVO
 * ----------------------------------------------------------------
 * Papel: nó sensor. Mede uma célula LiPo e escaneia WiFi + BLE ao redor.
 *   - AP proprio "ESP-SONDA" + pagina web  -> testavel sozinho, do celular, JA
 *   - STA opcional: junta na rede do Pi e empurra JSON p/ ele a cada 1s (quando o Pi existir)
 *
 * Bateria: 1 célula LiPo lida por divisor 100k/100k no GPIO34 (ADC1 — funciona com WiFi).
 *          O ESP mede a célula; NAO se alimenta dela. Terra comum obrigatorio.
 *
 * 2,4 GHz apenas (limite fisico do radio do ESP32 — nao existe 5GHz em nenhum ESP).
 */
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <esp_flash.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// ---------------------------------------------------------------- configuração
static const char *AP_SSID = "ESP-SONDA";
#include "segredos.h"  // AP_PASS (>= 8 caracteres) e PI_PASS

// Hub (Pi). Enquanto o Pi nao existir, as tentativas falham em silencio — sem travar.
static bool        PUSH_PI  = true;
static const char *PI_SSID  = "PI-SONDA";
static const char *PI_URL   = "http://192.168.4.1/api/ingest";

// Bateria (1 célula LiPo)
static const int   PIN_BAT   = 34;             // ADC1, input-only
static const float DIVISOR   = 2.0f;           // 100k/100k -> multiplica a leitura por 2
static const float V_CHEIA   = 4.15f;
static const float V_OK      = 3.60f;
static const float V_BAIXA   = 3.40f;
static const float V_CRITICA = 3.20f;

// ---------------------------------------------------------------- estado
WebServer servidor(80);
float     baterioV = 0;
int       baterioPct = 0;
String    baterioMsg = "—";

struct RedeWiFi { String ssid; int rssi; int canal; String cripto; };
RedeWiFi wifiAch[24]; uint8_t wifiQtd = 0;

struct DispBLE { String end, nome; int rssi; };
DispBLE  bleAch[24]; uint8_t bleQtd = 0;
BLEScan *bleScan = nullptr; bool bleBusy = false; uint32_t bleT = 0;

uint32_t piUltimoOk = 0; int piEnvios = 0, piFalhas = 0; bool piConectado = false;

// ---------------------------------------------------------------- bateria
float lerBateria() {
  uint32_t soma = 0;
  for (int i = 0; i < 16; i++) { soma += analogReadMilliVolts(PIN_BAT); delay(2); }
  return (soma / 16.0f) * DIVISOR / 1000.0f;    // Volts na célula
}

int pctLiPo(float v) {                          // curva de descanso aproximada de 1 célula
  const float tab[][2] = {{4.20,100},{4.10,90},{4.00,80},{3.90,70},{3.85,60},
                          {3.80,50},{3.75,40},{3.70,30},{3.60,20},{3.50,12},
                          {3.40,7},{3.30,3},{3.00,0}};
  if (v >= 4.20) return 100;
  if (v <= 3.00) return 0;
  for (int i = 0; i < 12; i++)
    if (v <= tab[i][1] ? false : v >= tab[i+1][0]) {
      float f = (v - tab[i+1][0]) / (tab[i][0] - tab[i+1][0]);
      return (int)(tab[i+1][1] + f * (tab[i][1] - tab[i+1][1]));
    }
  return 50;
}

void atualizarBateria() {
  baterioV = lerBateria();
  baterioPct = pctLiPo(baterioV);
  if      (baterioV < V_CRITICA) baterioMsg = "CRITICA — desligue e carregue ja (risco de dano)";
  else if (baterioV < V_BAIXA)   baterioMsg = "carregar agora";
  else if (baterioV < V_OK)      baterioMsg = "carregar em breve";
  else if (baterioV >= V_CHEIA)  baterioMsg = "cheia / carregando";
  else                           baterioMsg = "ok";
}

// ---------------------------------------------------------------- BLE
class CbBLE : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice d) override {
    String e = d.getAddress().toString().c_str();
    for (uint8_t i = 0; i < bleQtd; i++) if (bleAch[i].end == e) { bleAch[i].rssi = d.getRSSI(); return; }
    if (bleQtd >= 24) return;
    bleAch[bleQtd].end  = e;
    bleAch[bleQtd].nome = d.haveName() ? String(d.getName().c_str()) : String("(sem nome)");
    bleAch[bleQtd].rssi = d.getRSSI(); bleQtd++;
  }
};
void scanBLE() {
  if (bleBusy) return;
  bleQtd = 0; bleBusy = true;
  bleScan->start(4, [](BLEScanResults r){ bleBusy = false; }, false);
  bleT = millis();
}

// ---------------------------------------------------------------- WiFi scan
const char* cripto(wifi_auth_mode_t m) {
  switch (m) { case WIFI_AUTH_OPEN: return "aberta"; case WIFI_AUTH_WEP: return "WEP";
    case WIFI_AUTH_WPA_PSK: return "WPA"; case WIFI_AUTH_WPA2_PSK: return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/2"; case WIFI_AUTH_WPA3_PSK: return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/3"; default: return "?"; }
}
void coletarWiFi() {
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_FAILED) { WiFi.scanNetworks(true, true); return; }
  if (n == WIFI_SCAN_RUNNING) return;
  wifiQtd = 0;
  for (int i = 0; i < n && wifiQtd < 24; i++) {
    wifiAch[wifiQtd] = { WiFi.SSID(i).length() ? WiFi.SSID(i) : String("(oculta)"),
                         WiFi.RSSI(i), WiFi.channel(i), String(cripto(WiFi.encryptionType(i))) };
    wifiQtd++;
  }
  WiFi.scanDelete();
  WiFi.scanNetworks(true, true);                 // dispara a proxima
}

// ---------------------------------------------------------------- JSON
String jsonEstado() {
  String j = "{";
  j += "\"bat\":{\"v\":"; j += String(baterioV, 3);
  j += ",\"pct\":"; j += baterioPct;
  j += ",\"msg\":\""; j += baterioMsg; j += "\"}";

  j += ",\"wifi\":[";
  for (uint8_t i = 0; i < wifiQtd; i++) {
    if (i) j += ",";
    j += "{\"s\":\""; j += wifiAch[i].ssid; j += "\",\"r\":"; j += wifiAch[i].rssi;
    j += ",\"c\":"; j += wifiAch[i].canal; j += ",\"k\":\""; j += wifiAch[i].cripto; j += "\"}";
  }
  j += "]";

  j += ",\"ble\":[";
  for (uint8_t i = 0; i < bleQtd; i++) {
    if (i) j += ",";
    j += "{\"e\":\""; j += bleAch[i].end; j += "\",\"n\":\""; j += bleAch[i].nome;
    j += "\",\"r\":"; j += bleAch[i].rssi; j += "}";
  }
  j += "]";

  uint32_t fl = 0; esp_flash_get_size(NULL, &fl);
  uint32_t up = millis() / 1000;
  j += ",\"sis\":{\"mac\":\""; j += WiFi.macAddress();
  j += "\",\"flash_mb\":"; j += fl / (1024 * 1024);
  j += ",\"heap_kb\":"; j += ESP.getFreeHeap() / 1024;
  j += ",\"up_s\":"; j += up;
  j += ",\"pi\":{\"on\":"; j += (PUSH_PI ? "true" : "false");
  j += ",\"conectado\":"; j += (piConectado ? "true" : "false");
  j += ",\"envios\":"; j += piEnvios; j += ",\"falhas\":"; j += piFalhas; j += "}}";
  j += "}";
  return j;
}

// ---------------------------------------------------------------- página local
static const char PAGINA[] PROGMEM = R"HTML(<!doctype html><html lang="pt-BR"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 — Sonda</title><style>
*{box-sizing:border-box}body{margin:0;background:#0b0f14;color:#c8d6e5;font:14px/1.5 ui-monospace,Consolas,monospace}
header{padding:14px 18px;border-bottom:1px solid #1e2a38}h1{font-size:16px;margin:0;color:#4ade80}
.wrap{padding:18px;display:grid;gap:18px;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));max-width:1300px}
.card{background:#111823;border:1px solid #1e2a38;border-radius:10px;padding:14px;overflow:auto}
.card h2{font-size:12px;margin:0 0 10px;color:#64748b;text-transform:uppercase;letter-spacing:1px}
.big{font-size:30px;font-weight:700}.gauge{height:12px;background:#0d131c;border-radius:6px;overflow:hidden;margin:8px 0;border:1px solid #1e2a38}
.gfill{height:100%;background:#4ade80}table{width:100%;border-collapse:collapse}td,th{padding:4px 6px;text-align:left;border-bottom:1px solid #1a2432;font-size:12px}
th{color:#64748b;font-weight:400}.dim{color:#64748b}.bar{display:inline-block;height:8px;background:#4ade80;border-radius:3px;vertical-align:middle}
.warn{color:#fbbf24}.crit{color:#f87171}.ok{color:#4ade80}
</style></head><body>
<header><h1>ESP32 · SONDA — bateria + scanner 2,4GHz</h1></header>
<div class="wrap">
  <div class="card"><h2>Bateria (1 célula LiPo)</h2>
    <div class="big" id="bv">—</div><div class="gauge"><div class="gfill" id="bg" style="width:0"></div></div>
    <div id="bmsg" class="dim">—</div></div>
  <div class="card"><h2>Redes WiFi 2,4GHz (<span id="wn">0</span>)</h2>
    <table id="twifi"><tr><th>rede</th><th>ch</th><th>seg</th><th>sinal</th></tr></table></div>
  <div class="card"><h2>Bluetooth LE (<span id="bn">0</span>)</h2>
    <table id="tble"><tr><th>endereço</th><th>nome</th><th>sinal</th></tr></table></div>
  <div class="card"><h2>Sonda</h2><table id="tsis"></table></div>
</div>
<script>
function sinal(r){const p=Math.max(0,Math.min(100,2*(r+100)));return `<span class="bar" style="width:${p*0.5}px"></span> ${r}`}
async function tick(){
  const x=await fetch("/api/estado");if(!x.ok)return;const d=await x.json();
  const b=d.bat;document.getElementById("bv").textContent=b.v.toFixed(2)+" V · "+b.pct+"%";
  const g=document.getElementById("bg");g.style.width=b.pct+"%";
  g.style.background=b.pct<15?"#f87171":b.pct<35?"#fbbf24":"#4ade80";
  const m=document.getElementById("bmsg");m.textContent="→ "+b.msg;
  m.className=b.msg.indexOf("CRITICA")>=0?"crit":b.msg.indexOf("carregar")>=0?"warn":"ok";
  document.getElementById("wn").textContent=d.wifi.length;
  document.getElementById("twifi").innerHTML='<tr><th>rede</th><th>ch</th><th>seg</th><th>sinal</th></tr>'+
    d.wifi.sort((a,b)=>b.r-a.r).map(w=>`<tr><td>${w.s}</td><td>${w.c}</td><td class="dim">${w.k}</td><td>${sinal(w.r)}</td></tr>`).join("");
  document.getElementById("bn").textContent=d.ble.length;
  document.getElementById("tble").innerHTML='<tr><th>endereço</th><th>nome</th><th>sinal</th></tr>'+
    (d.ble.length?d.ble.sort((a,b)=>b.r-a.r).map(v=>`<tr><td>${v.e}</td><td>${v.n}</td><td>${sinal(v.r)}</td></tr>`).join("")
                 :'<tr><td colspan=3 class="dim">escaneando…</td></tr>');
  const pi=d.sis.pi;
  document.getElementById("tsis").innerHTML=
    `<tr><td class="dim">MAC</td><td>${d.sis.mac}</td></tr>`+
    `<tr><td class="dim">flash</td><td>${d.sis.flash_mb} MB</td></tr>`+
    `<tr><td class="dim">heap livre</td><td>${d.sis.heap_kb} KB</td></tr>`+
    `<tr><td class="dim">uptime</td><td>${d.sis.up_s}s</td></tr>`+
    `<tr><td class="dim">link Pi</td><td>${pi.on?(pi.conectado?'<span class=ok>conectado</span>':'<span class=warn>procurando…</span>'):'desligado'} · ${pi.envios} ok / ${pi.falhas} falha</td></tr>`;
}
tick();setInterval(tick,1000);
</script></body></html>)HTML";

// ---------------------------------------------------------------- Pi push
void empurrarPi() {
  if (!PUSH_PI || WiFi.status() != WL_CONNECTED) { piConectado = false; return; }
  piConectado = true;
  HTTPClient http;
  http.setConnectTimeout(800); http.setTimeout(800);
  if (http.begin(PI_URL)) {
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(jsonEstado());
    if (code > 0 && code < 400) { piEnvios++; piUltimoOk = millis(); } else piFalhas++;
    http.end();
  } else piFalhas++;
}

// ---------------------------------------------------------------- setup / loop
void setup() {
  Serial.begin(115200); delay(300);
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_BAT, ADC_11db);   // faixa ~0..3.1V no pino

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS);
  if (PUSH_PI) WiFi.begin(PI_SSID, PI_PASS);
  Serial.printf("\nSonda no ar — AP \"%s\" -> http://%s\n", AP_SSID, WiFi.softAPIP().toString().c_str());

  BLEDevice::init("ESP-SONDA");
  bleScan = BLEDevice::getScan();
  bleScan->setAdvertisedDeviceCallbacks(new CbBLE(), true);
  bleScan->setActiveScan(true); bleScan->setInterval(100); bleScan->setWindow(99);

  servidor.on("/", [](){ servidor.send_P(200, "text/html; charset=utf-8", PAGINA); });
  servidor.on("/api/estado", [](){ servidor.send(200, "application/json", jsonEstado()); });
  servidor.begin();

  atualizarBateria();
  WiFi.scanNetworks(true, true);
  scanBLE();
}

void loop() {
  servidor.handleClient();

  static uint32_t t1 = 0;
  if (millis() - t1 >= 1000) {                   // ciclo de 1s: bateria + wifi + push
    t1 = millis();
    atualizarBateria();
    coletarWiFi();
    empurrarPi();
  }
  if (!bleBusy && millis() - bleT > 15000) scanBLE();
}
