#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <esp_sleep.h>

#define TRIG_PIN    5
#define ECHO_PIN    18
#define BUZZER_PIN  19
#define POT_PIN     34

#define BUZZER_CH   0
#define BUZZER_FREQ 2000
#define BUZZER_RES  8

#define LED_PIN     2

#define INACTIVITY_MS  120000UL
#define SLEEP_WAKEUP_S 10

RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR unsigned long lastDistCm = 0;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

int mesafe = 0;
int esik = 50;
unsigned long sonHareket = 0;
unsigned long sonBip = 0;
int bipAralik = 0;
bool bipDurum = false;

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="tr">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Park Sens&ouml;r&uuml;</title>
<style>
  * { margin: 0; padding: 0; box-sizing: border-box; }
  body {
    font-family: -apple-system, system-ui, sans-serif;
    background: #1a1a2e; color: #eee;
    min-height: 100vh; display: flex;
    justify-content: center; align-items: center;
    transition: background .5s;
  }
  .card {
    background: rgba(255,255,255,.06);
    backdrop-filter: blur(12px);
    border-radius: 32px; padding: 40px;
    text-align: center; width: 340px;
    box-shadow: 0 20px 60px rgba(0,0,0,.5);
  }
  h1 { font-size: 14px; text-transform: uppercase; letter-spacing: 3px; opacity: .5; margin-bottom: 8px; }
  .distance {
    font-size: 88px; font-weight: 800;
    line-height: 1; margin: 12px 0;
  }
  .distance .unit { font-size: 28px; font-weight: 400; opacity: .6; }
  .threshold {
    font-size: 14px; opacity: .5; margin-bottom: 24px;
  }
  .bar-wrap {
    background: rgba(255,255,255,.08);
    border-radius: 8px; height: 12px; overflow: hidden;
    margin-bottom: 28px;
  }
  .bar {
    height: 100%; width: 0%;
    border-radius: 8px;
    background: linear-gradient(90deg, #00e676, #ffea00, #ff1744);
    transition: width .3s;
  }
  .status {
    display: inline-block;
    padding: 8px 20px; border-radius: 40px;
    font-size: 13px; font-weight: 600;
  }
  .safe { background: #00e67622; color: #00e676; }
  .warn { background: #ffea0022; color: #ffea00; }
  .danger { background: #ff174422; color: #ff1744; }
  .wifi {
    margin-top: 20px; font-size: 12px; opacity: .3;
  }
  .wifi.on { opacity: .7; color: #00e676; }
</style>
</head>
<body>
<div class="card">
  <h1>Park Sens&ouml;r&uuml;</h1>
  <div class="distance" id="dist">-- <span class="unit">cm</span></div>
  <div class="threshold" id="thr">Eşik: -- cm</div>
  <div class="bar-wrap"><div class="bar" id="bar"></div></div>
  <div class="status safe" id="status">G&Uuml;VENLİ</div>
  <div class="wifi" id="wifi">WiFi'ye bağlanıyor...</div>
</div>
<script>
  const el = (id) => document.getElementById(id);
  const distEl = el('dist'), thrEl = el('thr'), barEl = el('bar');
  const statusEl = el('status'), wifiEl = el('wifi');

  function update(d, t) {
    distEl.innerHTML = d + ' <span class="unit">cm</span>';
    thrEl.textContent = 'E\u015Fik: ' + t + ' cm';
    const pct = Math.min(100, (d / t) * 100);
    barEl.style.width = (100 - pct) + '%';
    if (d <= t * 0.3) {
      statusEl.className = 'status danger';
      statusEl.textContent = 'TEHL\u0130KE';
      document.body.style.background = '#4a0e0e';
    } else if (d <= t * 0.7) {
      statusEl.className = 'status warn';
      statusEl.textContent = 'D\u0130KKAT';
      document.body.style.background = '#4a3f0e';
    } else {
      statusEl.className = 'status safe';
      statusEl.textContent = 'G\u00DCNL\u0130';
      document.body.style.background = '#0e4a1a';
    }
  }

  function baglan() {
    let wsUrl = (location.protocol === 'https:' ? 'wss://' : 'ws://') + location.host + '/ws';
    let ws = new WebSocket(wsUrl);
    ws.onopen = () => { wifiEl.textContent = 'Ba\u011Fl\u0131'; wifiEl.className = 'wifi on'; };
    ws.onclose = () => {
      wifiEl.textContent = 'Ba\u011Flant\u0131 koptu, yeniden ba\u011Flan\u0131yor...';
      wifiEl.className = 'wifi';
      setTimeout(baglan, 1500);
    };
    ws.onmessage = (e) => {
      try {
        const o = JSON.parse(e.data);
        if (o.d !== undefined) update(o.d, o.t || 50);
      } catch(_) {}
    };
  }
  baglan();
</script>
</body>
</html>
)rawliteral";

int mesafeOlc() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long sure = pulseIn(ECHO_PIN, HIGH, 30000);
  if (sure == 0) return -1;
  return sure * 0.034 / 2;
}

void notifyClients() {
  String json = "{\"d\":" + String(mesafe) + ",\"t\":" + String(esik) + "}";
  ws.textAll(json);
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    String json = "{\"d\":" + String(mesafe) + ",\"t\":" + String(esik) + "}";
    client->text(json);
  }
}

void setupWebServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *r) {
    r->send(200, "text/html", index_html);
  });
  server.on("/api", HTTP_GET, [](AsyncWebServerRequest *r) {
    String json = "{\"d\":" + String(mesafe) + ",\"t\":" + String(esik) + "}";
    r->send(200, "application/json", json);
  });
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  server.begin();
  Serial.println("Web server baslatildi");
}

void setupWiFi() {
  Serial.println("\n--- AP modu baslatiliyor ---");

  WiFi.mode(WIFI_AP);
  WiFi.softAP("ParkSensoru", "123456789");
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));

  digitalWrite(LED_PIN, HIGH);
  Serial.println("AP aktif: 'ParkSensoru' (sifre: 123456789)");
  Serial.println("Telefonunu bu aga bagla, tarayicinda 192.168.4.1 ac");
}

void goToDeepSleep() {
  Serial.println("120sn hareketsiz -> Deep sleep");
  digitalWrite(LED_PIN, LOW);
  ws.closeAll();
  server.end();
  WiFi.mode(WIFI_OFF);
  delay(100);
  esp_sleep_enable_timer_wakeup(SLEEP_WAKEUP_S * 1000000ULL);
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);
  ledcSetup(BUZZER_CH, BUZZER_FREQ, BUZZER_RES);
  ledcAttachPin(BUZZER_PIN, BUZZER_CH);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(100);
  }

  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  if (cause == ESP_SLEEP_WAKEUP_TIMER) {
    bootCount++;
    Serial.printf("Uyandim #%d, son mesafe: %lu cm\n", bootCount, lastDistCm);
    int toplam = 0, adet = 0;
    for (int i = 0; i < 3; i++) {
      int d = mesafeOlc();
      if (d > 0) { toplam += d; adet++; }
      delay(30);
    }
    if (adet > 0) {
      int ort = toplam / adet;
      if (abs(ort - (int)lastDistCm) < 5) {
        Serial.printf("Fark yok (%d vs %lu), tekrar uyku\n", ort, lastDistCm);
        esp_sleep_enable_timer_wakeup(SLEEP_WAKEUP_S * 1000000ULL);
        esp_deep_sleep_start();
      } else {
        Serial.println("Hareket algilandi, uyanik kaliniyor");
      }
    } else {
      Serial.println("Sensor hatasi, tekrar uyku");
      esp_sleep_enable_timer_wakeup(SLEEP_WAKEUP_S * 1000000ULL);
      esp_deep_sleep_start();
    }
  }

  setupWiFi();
  setupWebServer();
  sonHareket = millis();
  Serial.println("Sistem aktif");
}

void loop() {
  ws.cleanupClients();

  esik = map(analogRead(POT_PIN), 0, 4095, 10, 150);
  int yeni = mesafeOlc();

  if (yeni > 0) {
    if (abs(yeni - mesafe) >= 3 || mesafe == 0) {
      sonHareket = millis();
    }
    mesafe = yeni;
    lastDistCm = mesafe;

    Serial.print("Mesafe: "); Serial.print(mesafe);
    Serial.print(" cm  Esik: "); Serial.println(esik);

    if (mesafe < esik) {
      bipAralik = map(mesafe, 0, esik, 50, 400);
      bipAralik = constrain(bipAralik, 50, 400);
      unsigned long now = millis();
      if (now - sonBip >= (unsigned long)bipAralik) {
        sonBip = now;
        bipDurum = !bipDurum;
        ledcWrite(BUZZER_CH, bipDurum ? 128 : 0);
      }
    } else {
      ledcWrite(BUZZER_CH, 0);
      bipDurum = false;
    }

    notifyClients();
    notifyClients();
  } else {
    Serial.println("Sensor okunamadi");
    ledcWrite(BUZZER_CH, 0);
    notifyClients();
  }

  if (millis() - sonHareket > INACTIVITY_MS) {
    goToDeepSleep();
  }

  delay(30);
}
