#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <DHT.h>
#include <U8x8lib.h> // Ultra-lightweight non-buffered text display driver

// --- Pin Definitions ---
#define DHTPIN 4
#define DHTTYPE DHT22

#define TRIG_PIN 5
#define ECHO_PIN 18

#define GAS_PIN 34

#define LED_RED 25
#define LED_YELLOW 26
#define LED_GREEN 27

#define BUZZER_PIN 19

#define MPU6050_ADDR 0x68

// --- WiFi & MQTT Credentials ---
const char* WIFI_SSID = "Wokwi-GUEST"; // Replace WIFI_SSID WITH YOURS;
const char* WIFI_PASSWORD = ""; // Replace WIFI_PASSWORD WITH YOURS;
const char* MQTT_BROKER = "broker.hivemq.com";
const int MQTT_PORT = 1883;
const char* PUB_TOPIC = "disaster/data";
const char* SUB_TOPIC = "disaster/control";

// --- Fast OLED Driver Initialization ---
U8X8_SSD1306_128X64_NONAME_HW_I2C u8x8(U8X8_PIN_NONE);

// --- Sensor & Network Objects ---
DHT dht(DHTPIN, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);

// --- Global Variables ---
bool alarmMuted = false;
unsigned long lastMsg = 0;
const long interval = 1000;

// Direct I2C initialization for MPU6050 (Fastest build time)
void initMPU6050() {
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x6B); // PWR_MGMT_1 register
  Wire.write(0);    // Wake up MPU6050
  Wire.endTransmission(true);
}

// Fast read raw accelerometer without external heavy libs
float readMPUAccelMagnitude() {
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x3B); // Accel Xout High register
  Wire.endTransmission(false);
  Wire.requestFrom(MPU6050_ADDR, 6, true);

  if (Wire.available() < 6) return 9.8f;

  int16_t rawX = (Wire.read() << 8) | Wire.read();
  int16_t rawY = (Wire.read() << 8) | Wire.read();
  int16_t rawZ = (Wire.read() << 8) | Wire.read();

  // Convert raw values to m/s^2 (Scale factor for +/- 2g range: 16384 LSB/g)
  float ax = (rawX / 16384.0f) * 9.81f;
  float ay = (rawY / 16384.0f) * 9.81f;
  float az = (rawZ / 16384.0f) * 9.81f;

  return sqrt(ax * ax + ay * ay + az * az);
}

void setupWiFi() {
  Serial.print("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char message[32] = {0};
  unsigned int len = length < 31 ? length : 31;
  for (unsigned int i = 0; i < len; i++) {
    message[i] = (char)payload[i];
  }

  // Fast string search without heavy JSON deserializers
  if (strstr(message, "MUTE")) {
    alarmMuted = true;
    digitalWrite(BUZZER_PIN, LOW);
    Serial.println("Alarm Muted");
  } else if (strstr(message, "RESET")) {
    alarmMuted = false;
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_YELLOW, LOW);
    digitalWrite(LED_GREEN, HIGH);
    Serial.println("System Reset");
  }
}

void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    char clientId[32];
    snprintf(clientId, sizeof(clientId), "ESP32-%lX", random(0xffff));
    if (client.connect(clientId)) {
      Serial.println("connected");
      client.subscribe(SUB_TOPIC);
    } else {
      delay(1000);
    }
  }
}

float measureWaterLevel() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return 0.0f;

  float distanceCm = duration * 0.0343f / 2.0f;
  return map(constrain((long)distanceCm, 2, 400), 400, 2, 0, 100);
}

void updateOLED(float temp, float hum, float water, int gas, const char* status) {
  char buf[32];

  u8x8.setFont(u8x8_font_chroma48medium8_r);
  u8x8.drawString(0, 0, "DISASTER MONITOR");

  snprintf(buf, sizeof(buf), "Temp : %.1f C", temp);
  u8x8.drawString(0, 2, buf);

  snprintf(buf, sizeof(buf), "Hum  : %.1f %%", hum);
  u8x8.drawString(0, 3, buf);

  snprintf(buf, sizeof(buf), "Water: %.1f %%", water);
  u8x8.drawString(0, 4, buf);

  snprintf(buf, sizeof(buf), "Gas  : %d   ", gas);
  u8x8.drawString(0, 5, buf);

  snprintf(buf, sizeof(buf), "ST   : %s        ", status);
  u8x8.drawString(0, 7, buf);
}

void setup() {
  Serial.begin(115200);

  pinMode(LED_RED, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(BUZZER_PIN, LOW);

  dht.begin();
  Wire.begin(21, 22);
  initMPU6050();

  u8x8.begin();
  u8x8.setFont(u8x8_font_chroma48medium8_r);
  u8x8.drawString(0, 2, "Initializing...");

  setupWiFi();
  client.setServer(MQTT_BROKER, MQTT_PORT);
  client.setCallback(mqttCallback);
}

void loop() {
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();

  unsigned long now = millis();
  if (now - lastMsg > interval) {
    lastMsg = now;

    float temp = dht.readTemperature();
    float hum = dht.readHumidity();
    if (isnan(temp)) temp = 0.0f;
    if (isnan(hum)) hum = 0.0f;

    float waterLevel = measureWaterLevel();
    int gasVal = analogRead(GAS_PIN);
    float accelMag = readMPUAccelMagnitude();
    bool earthquake = (accelMag > 30.0f);

    const char* status = "SAFE";
    bool isRedAlert = false;
    bool isYellowAlert = false;

    if (temp >= 45.0f) {
      status = "FIRE";
      isRedAlert = true;
    } else if (waterLevel >= 80.0f) {
      status = "FLOOD";
      isRedAlert = true;
    } else if (gasVal >= 2500) {
      status = "GAS LEAK";
      isRedAlert = true;
    } else if (earthquake) {
      status = "EARTHQUAKE";
      isRedAlert = true;
    } else if (temp > 38.0f || waterLevel > 60.0f || gasVal > 1500) {
      status = "WARNING";
      isYellowAlert = true;
    }

    if (!isRedAlert) alarmMuted = false;

    // Actuators Control
    if (isRedAlert) {
      digitalWrite(LED_RED, HIGH);
      digitalWrite(LED_YELLOW, LOW);
      digitalWrite(LED_GREEN, LOW);
      digitalWrite(BUZZER_PIN, alarmMuted ? LOW : HIGH);
    } else if (isYellowAlert) {
      digitalWrite(LED_RED, LOW);
      digitalWrite(LED_YELLOW, HIGH);
      digitalWrite(LED_GREEN, LOW);
      digitalWrite(BUZZER_PIN, LOW);
    } else {
      digitalWrite(LED_RED, LOW);
      digitalWrite(LED_YELLOW, LOW);
      digitalWrite(LED_GREEN, HIGH);
      digitalWrite(BUZZER_PIN, LOW);
    }

    // Serial Printout
    Serial.print("Temp: "); Serial.print(temp);
    Serial.print(" C | Water: "); Serial.print(waterLevel);
    Serial.print(" % | Gas: "); Serial.print(gasVal);
    Serial.print(" | Status: "); Serial.println(status);

    // OLED Update
    updateOLED(temp, hum, waterLevel, gasVal, status);

    // Fast Zero-Dependency JSON Generation
    char jsonBuffer[256];
    snprintf(jsonBuffer, sizeof(jsonBuffer),
      "{\"temperature\":%.1f,\"humidity\":%.1f,\"water\":%.1f,\"gas\":%d,\"earthquake\":%s,\"status\":\"%s\"}",
      temp, hum, waterLevel, gasVal, earthquake ? "true" : "false", status
    );

    client.publish(PUB_TOPIC, jsonBuffer);
  }
}
