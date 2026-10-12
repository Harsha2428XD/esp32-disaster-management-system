# ESP32-Powered IoT Disaster Management System

An advanced, multi-sensor IoT environmental monitoring and disaster warning system built around the ESP32 microcontroller to detect hazards in real-time and stream telemetry data to a live web dashboard.

---

## 🚀 Key Features
- **Multi-Sensor Integration:** Monitors critical environmental parameters (e.g., temperature, humidity, gas/smoke, flame detection, or water level depending on your sensor array).
- **Real-Time Web Telemetry:** Streams live sensor data over Wi-Fi to a remote web dashboard for instant visualization and early warning alerts.
- **Edge Processing & Alerts:** Triggers local buzzers/LED indicators upon crossing critical safety thresholds.

---

## 🛠️ Hardware & Components Used
- **Microcontroller:** ESP32 Development Board
- **Sensors:** 
  - DHT11 / DHT22 (Temperature & Humidity)
  - MQ-2 / MQ-135 (Gas & Smoke Detection)
  - Flame Sensor / Water Level Sensor
- **Actuators:** Buzzer, Status LEDs
- **Communication:** Wi-Fi (MQTT / HTTP protocol)

---

## 📊 System Architecture & Wiring
*(Add a brief description of how your ESP32 connects to the sensor nodes and communicates with the cloud/dashboard.)*

> **Wiring Diagram:** Check the `schematics/` folder for full pin-mapping layouts.

---

## 💻 Getting Started

### 1. Firmware Setup
1. Clone this repository or download the source files.
2. Open the `firmware/main_node.ino` file in the **Arduino IDE**.
3. Install the required libraries (e.g., `DHT sensor library`, `WiFi.h`, etc.).
4. Update your Wi-Fi credentials in the code:
   ```cpp
   const char* ssid = "YOUR_SSID";
   const char* password = "YOUR_PASSWORD";
