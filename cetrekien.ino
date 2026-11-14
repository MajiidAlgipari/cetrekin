/*************************************************************
 * Cetrekan - Jembatan ESP32 <-> Web lewat MQTT (TLS)
 * Library: PubSubClient (Nick O'Leary), ESP32Servo
 *
 * Topik (PREFIX = "majiid.com/lampu"):
 *   majiid.com/lampu/cmd     web -> ESP   "1"/"on" (ON) atau "0"/"off" (OFF)
 *   majiid.com/lampu/status  ESP -> web   "1" / "0"   (retained)
 *   majiid.com/lampu/online  ESP -> web   "online" / "offline" (retained, LWT)
 *
 * Servo pindah ke posisi ON atau OFF dan TETAP di posisi itu
 * sampai ada perintah berikutnya (tidak kembali ke netral).
 *************************************************************/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>
#include <esp_system.h>

// ====== GANTI BAGIAN INI ======
const char* WIFI_SSID = "Vivo";
const char* WIFI_PASS = "11111111";

const char* MQTT_HOST = "broker.emqx.io";
const int   MQTT_PORT = 8883;                         // MQTT + TLS
const char* MQTT_USER = "public";
const char* MQTT_PASS = "public";

#define PREFIX "majiid.com/lampu"   // HARUS sama persis dengan Prefix topik di web
// ==============================

#define T_CMD    PREFIX "/cmd"
#define T_STATUS PREFIX "/status"
#define T_ONLINE PREFIX "/online"

#define SERVO_PIN 4         // pin aman di ESP32 / ESP32-C3
#define ON_POS    45        // sudut saat ON  (ubah sesuai kebutuhan, 0-180)
#define OFF_POS   0         // sudut saat OFF (ubah sesuai kebutuhan, 0-180)

Servo servo1;
WiFiClientSecure net;
PubSubClient mqtt(net);

bool switchOn = false;
bool known = false;         // status baru dikirim setelah ada perintah
unsigned long lastTry = 0;

const char* resetReason() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:  return "power on";
    case ESP_RST_BROWNOUT: return "BROWNOUT (daya kurang)";
    case ESP_RST_PANIC:    return "PANIC (error program)";
    case ESP_RST_SW:       return "software reset";
    case ESP_RST_WDT: case ESP_RST_TASK_WDT: case ESP_RST_INT_WDT: return "watchdog";
    default: return "lainnya";
  }
}

void publishStatus() {
  if (!known) return;   // jangan kirim status palsu setelah reboot
  mqtt.publish(T_STATUS, switchOn ? "1" : "0", true);   // retained
}

void tekan(bool on) {
  switchOn = on;
  known = true;
  if (!servo1.attached()) servo1.attach(SERVO_PIN, 500, 2400);
  servo1.write(on ? ON_POS : OFF_POS);   // pindah dan tetap di posisi ini
  Serial.println(on ? "Saklar ON" : "Saklar OFF");
  publishStatus();
}

void onMessage(char* topic, byte* payload, unsigned int len) {
  String p;
  for (unsigned int i = 0; i < len; i++) p += (char)tolower(payload[i]);
  p.trim();
  if (p == "1" || p == "on")       tekan(true);    // ON
  else if (p == "0" || p == "off") tekan(false);   // OFF
}

void connectMqtt() {
  if (mqtt.connected()) return;
  if (millis() - lastTry < 5000) return;
  lastTry = millis();

  String id = "cetrekan-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  Serial.print("Menghubungkan MQTT... ");

  // Last Will: otomatis "offline" kalau ESP putus tiba-tiba
  if (mqtt.connect(id.c_str(), MQTT_USER, MQTT_PASS, T_ONLINE, 1, true, "offline")) {
    Serial.println("terhubung");
    mqtt.subscribe(T_CMD, 1);
    mqtt.publish(T_ONLINE, "online", true);
    publishStatus();
  } else {
    Serial.print("gagal, kode ");
    Serial.println(mqtt.state());
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.print("Penyebab reset: ");
  Serial.println(resetReason());

  // Servo baru dipasang saat ada perintah pertama,
  // jadi tidak bergerak sendiri waktu ESP32 menyala.
  ESP32PWM::allocateTimer(0);
  servo1.setPeriodHertz(50);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Menghubungkan WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  net.setInsecure();   // enkripsi tetap jalan, sertifikat server tidak diverifikasi
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMessage);
  mqtt.setKeepAlive(30);
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) connectMqtt();
  mqtt.loop();
}
