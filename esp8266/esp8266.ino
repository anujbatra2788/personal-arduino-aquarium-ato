#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266WebServer.h>
#include <SoftwareSerial.h>

// SoftwareSerial for communication with Arduino
SoftwareSerial espSerial(2, 3); // RX, TX

// ESP8266 Configuration Structure
struct ESPConfig {
  char SSID[32];
  char wifiPassword[32];
  char mqttServer[32];
  int mqttPort;
  char mqttUser[32];
  char mqttPassword[32];
};

// Global variables
ESPConfig config;
WiFiClient espClient;
PubSubClient mqttClient(espClient);
ESP8266WebServer server(80);
const int EEPROM_SIZE = 512;
const int CONFIG_ADDRESS = 0;

// HTML for configuration page
const char* configPage = R"(
<!DOCTYPE html>
<html>
<head>
  <title>Aquarium Manager Setup</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial, sans-serif; max-width: 600px; margin: 20px auto; padding: 20px; }
    input { width: 100%; padding: 8px; margin: 10px 0; }
    button { padding: 10px 20px; background: #007bff; color: white; border: none; cursor: pointer; }
    button:hover { background: #0056b3; }
  </style>
</head>
<body>
  <h2>Aquarium Manager Configuration</h2>
  <form action="/save" method="POST">
    <label>WiFi SSID:</label><input type="text" name="ssid" required><br>
    <label>WiFi Password:</label><input type="password" name="password" required><br>
    <label>MQTT Server:</label><input type="text" name="mqttServer" required><br>
    <label>MQTT Port:</label><input type="number" name="mqttPort" required><br>
    <label>MQTT User:</label><input type="text" name="mqttUser"><br>
    <label>MQTT Password:</label><input type="password" name="mqttPassword"><br>
    <button type="submit">Save Configuration</button>
  </form>
</body>
</html>
)";

// HTML for info page
const char* infoPage = R"(
<!DOCTYPE html>
<html>
<head>
  <title>Aquarium Manager Info</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial, sans-serif; max-width: 600px; margin: 20px auto; padding: 20px; }
    .info { margin: 10px 0; }
  </style>
</head>
<body>
  <h2>Aquarium Manager Info</h2>
  <div class="info">SSID: %SSID%</div>
  <div class="info">MQTT Server: %MQTT_SERVER%</div>
  <div class="info">MQTT Port: %MQTT_PORT%</div>
  <div class="info">MQTT User: %MQTT_USER%</div>
  <a href="/reset">Reset Configuration</a>
</body>
</html>
)";

void setup() {
  Serial.begin(9600);
  espSerial.begin(9600);
  
  // Initialize EEPROM
  EEPROM.begin(EEPROM_SIZE);
  readConfig();

  // Setup WiFi and MQTT
  setupWiFi();
  setupMQTT();
  
  // Setup web server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/info", HTTP_GET, handleInfo);
  server.on("/reset", HTTP_GET, handleReset);
  server.begin();
}

void loop() {
  server.handleClient();
  if (!mqttClient.connected()) {
    connectMQTT();
  }
  mqttClient.loop();
  
  // Relay data from Arduino to MQTT
  if (espSerial.available()) {
    String data = espSerial.readStringUntil('\n');
    mqttClient.publish("aquarium/data", data.c_str());
  }
}

// MQTT callback
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  // Send to Arduino
  espSerial.println(message);
}

void setupWiFi() {
  if (strlen(config.SSID) > 0) {
    WiFi.begin(config.SSID, config.wifiPassword);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      attempts++;
    }
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP("AquariumManager", "12345678");
  }
}

void setupMQTT() {
  if (strlen(config.mqttServer) > 0) {
    mqttClient.setServer(config.mqttServer, config.mqttPort);
    mqttClient.setCallback(mqttCallback);
    connectMQTT();
  }
}

void connectMQTT() {
  if (strlen(config.mqttPassword) > 0) {
    Serial.println("Connecting to MQTT with authentication");
    if (mqttClient.connect("AquariumClient", config.mqttUser, config.mqttPassword)) {
      Serial.println("Connected with authentication");
      mqttClient.subscribe("gf/lr/aquarium-manager/command/#");
    } else {
      Serial.println("Failed to connect with authentication");
    }
  } else {
    Serial.println("Connecting to MQTT without authentication");
    if (mqttClient.connect("AquariumClient")) {
      Serial.println("Connected without authentication");
      mqttClient.subscribe("gf/lr/aquarium-manager/command/#");
    } else {
      Serial.println("Failed to connect without authentication");
    }
  }
  publishAvailableStatus();
}

void publishAvailableStatus() {
  mqttClient.publish("gf/lr/aquarium-manager/available", "ON", false);
}

void readConfig() {
  EEPROM.get(CONFIG_ADDRESS, config);
}

void writeConfig() {
  EEPROM.put(CONFIG_ADDRESS, config);
  EEPROM.commit();
}

void clearConfig() {
  for (int i = 0; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  ESP.restart();
}

void handleRoot() {
  server.send(200, "text/html", configPage);
}

void handleSave() {
  strncpy(config.SSID, server.arg("ssid").c_str(), sizeof(config.SSID));
  strncpy(config.wifiPassword, server.arg("password").c_str(), sizeof(config.wifiPassword));
  strncpy(config.mqttServer, server.arg("mqttServer").c_str(), sizeof(config.mqttServer));
  config.mqttPort = server.arg("mqttPort").toInt();
  strncpy(config.mqttUser, server.arg("mqttUser").c_str(), sizeof(config.mqttUser));
  strncpy(config.mqttPassword, server.arg("mqttPassword").c_str(), sizeof(config.mqttPassword));
  
  writeConfig();
  server.send(200, "text/plain", "Configuration saved. Rebooting...");
  ESP.restart();
}

void handleInfo() {
  String page = infoPage;
  page.replace("%SSID%", config.SSID);
  page.replace("%MQTT_SERVER%", config.mqttServer);
  page.replace("%MQTT_PORT%", String(config.mqttPort));
  page.replace("%MQTT_USER%", config.mqttUser);
  server.send(200, "text/html", page);
}

void handleReset() {
  clearConfig();
}