#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ===== PIN I2C untuk ESP32-C3 =====
#define I2C_SDA 4
#define I2C_SCL 5

// ===== SENSOR =====
Adafruit_AHTX0 aht;

// ===== WIFI =====
const char* ssid = "SSID";
const char* password = "password";

// ===== MQTT =====
const char* mqtt_server = "ipaddress_server";
const int mqtt_port = 1884;
const char* mqtt_topic = "sensor/ruang_kebon/aht20";

// ===== DEVICE INFO =====
#define DEVICE_ID "ESP32_SUHU_SENSOR_01"
#define SENSOR_TYPE "AHT20"
#define DEVICE_LOCATION "Ruang_Kebon"
#define DEVICE_VERSION "1.1"

// ===== TIMING =====
unsigned long lastPublish = 0;
const unsigned long PUBLISH_INTERVAL = 5000;

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Fungsi heat index (sama seperti sebelumnya)
float calculateHeatIndex(float tempC, float hum) {
  if (tempC < 26 || hum < 40) return tempC;
  float T = tempC * 9.0 / 5.0 + 32.0;
  float R = hum;
  float HI = -42.379 +
             2.04901523 * T +
             10.14333127 * R -
             0.22475541 * T * R -
             0.00683783 * T * T -
             0.05481717 * R * R +
             0.00122874 * T * T * R +
             0.00085282 * T * R * R -
             0.00000199 * T * T * R * R;
  return (HI - 32.0) * 5.0 / 9.0;
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Tidak digunakan
}

void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT...");
    String clientId = DEVICE_ID + String("-") + WiFi.macAddress();
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println(" connected!");
    } else {
      Serial.print(" failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 sec");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000); // Penting untuk ESP32-C3

  Serial.println("\n=== ESP32-C3 AHT20 MQTT Publisher ===");

  // I2C init
  Wire.begin(I2C_SDA, I2C_SCL);
  if (!aht.begin()) {
    Serial.println("❌ AHT20 not detected! Check wiring.");
    while (1) delay(1000);
  }
  Serial.println("✅ AHT20 sensor ready");

  // WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (millis() - start > 20000) break;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi connected");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ WiFi failed, restarting...");
    ESP.restart();
  }

  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);
}

void loop() {
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();

  if (millis() - lastPublish >= PUBLISH_INTERVAL) {
    lastPublish = millis();

    sensors_event_t humEvent, tempEvent;
    if (!aht.getEvent(&humEvent, &tempEvent)) {
      Serial.println("❌ Sensor read failed");
      return;
    }

    float temp = tempEvent.temperature;
    float hum = humEvent.relative_humidity;
    float heatIndex = calculateHeatIndex(temp, hum);

    Serial.printf("Temp: %.1f C | Hum: %.1f %% | HI: %.1f C\n",
                  temp, hum, heatIndex);

    StaticJsonDocument<256> doc;
    doc["deviceId"] = DEVICE_ID;
    doc["sensorType"] = SENSOR_TYPE;
    doc["location"] = DEVICE_LOCATION;
    doc["version"] = DEVICE_VERSION;
    doc["temp"] = temp;
    doc["hum"] = hum;
    doc["heatIndex"] = heatIndex;
    doc["uptime"] = millis() / 1000;
    doc["timestamp"] = millis();

    String payload;
    serializeJson(doc, payload);

    if (mqttClient.publish(mqtt_topic, payload.c_str())) {
      Serial.println("✅ MQTT published");
    } else {
      Serial.println("❌ MQTT publish failed");
    }
  }
}
