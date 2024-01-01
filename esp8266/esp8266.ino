#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

#define EEPROM_ADDRESS 0
#define EEPROM_SIZE 512


const char *apSSID = "AquariumManager";
const char *apPassword = "12345678";

struct ESPConfig {
   String ssid;
   String wifiPassword;
   String mqttServer;
   int mqttPort;
};

DynamicJsonDocument espConfigAsJSON(512);
ESPConfig espConfig;

ESP8266WebServer server(80);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);


void loop() {
  server.handleClient();
  if(WiFi.status() == WL_CONNECTED) {
    if (!mqttClient.connected()) {
      initiateMQTTConnect();
    }
    mqttClient.loop();
    String json = Serial.readString();
    DynamicJsonDocument jsonRequest(512);
    if(json.length() > 0) {
      delay(5000);
      Serial.print("Received  to send: ");
      Serial.println(json);
      deserializeJson(jsonRequest, json);
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(9600);
  
  Serial.println("Commencing EEPROM");
  EEPROM.begin(EEPROM_SIZE);
  
  Serial.println("Trying to read JSON config from EEPROM");
  readConfigFromEEPROM();

  Serial.print("Setup Read Config SSID: ");
  Serial.print(espConfig.ssid);
  Serial.print(", with password: ");
  Serial.println(espConfig.wifiPassword);

  if(strcmp(espConfig.ssid.c_str(), "") != 0 && strcmp(espConfig.wifiPassword.c_str(), "") != 0) {
    initiateWifiConnect(espConfig.ssid, espConfig.wifiPassword, false);

    Serial.print("Setup Read Config mqttServer: ");
    Serial.print(espConfig.mqttServer);
    Serial.print(" at mqttPort: ");
    Serial.println(espConfig.mqttPort);

    if(strcmp(espConfig.mqttServer.c_str(), "") != 0) {
      mqttClient.setServer(espConfig.mqttServer.c_str(), espConfig.mqttPort);
      mqttClient.setCallback(callback);
    }
  } else {
    // Start in AP mode
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSSID, apPassword);

    // Serve HTML page for WiFi credentials
    server.on("/", HTTP_GET, handleRoot);
    server.on("/connect", HTTP_POST, handleConnect);
    server.begin();

    Serial.print("Access Point started. Connect to WiFi network: AquariumManager. IP Address is ");
    Serial.println(WiFi.softAPIP());
  }
}

void readConfigFromEEPROM() {
  // Read data from EEPROM into a buffer
  char eepromData[EEPROM_SIZE];
  for (int i = 0; i < EEPROM_SIZE; ++i) {
    eepromData[i] = EEPROM.read(EEPROM_ADDRESS + i);
  }
  Serial.print("Parsing JSON from: ");
  Serial.println(eepromData);
  // Parse the JSON data from the buffer
  deserializeJson(espConfigAsJSON, eepromData);

  espConfig.ssid = strdup(espConfigAsJSON["SSID"].as<String>().c_str());
  espConfig.wifiPassword = strdup(espConfigAsJSON["PWD"].as<String>().c_str());
  espConfig.mqttServer = strdup(espConfigAsJSON["mqttServer"].as<String>().c_str());
  espConfig.mqttPort = espConfigAsJSON["mqttPort"].as<String>().toInt();
}



void initiateWifiConnect(String ssid, String password, boolean writeToEEPROM) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");

  int timeout = millis();

  while (WiFi.status() != WL_CONNECTED && (millis() - timeout < 30000)) {
    delay(500);
    Serial.print(".");
  }

  if(WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to WiFi!");
    if(writeToEEPROM) {
      addToConfig("SSID", ssid);
      addToConfig("PWD", password);
    }
    // Set up proxy server
    server.on("/reset", HTTP_GET, resetEEPROM);
    server.send(200, "text/html", "WiFi connection successful. Reset is ready at /reset");
    Serial.print("\nConnected to WiFi!. Local IP is ");
    Serial.println(WiFi.localIP());
  }
}

void initiateMQTTConnect() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    String mqttClientId = "AquariumManager-" + millis();
    if (mqttClient.connect(mqttClientId.c_str())) {
      Serial.println("connected");
      mqttClient.subscribe("gf/lr/aquarium/command");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" will try again in 5 seconds");
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived on topic: '");
  Serial.print(topic);
  Serial.print("' with payload: ");
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    Serial1.print((char)payload[i]);
  }
  Serial.println();
  
}

void handleRoot() {

  int numNetworks = WiFi.scanNetworks();
  String html = "<html><head>";
  html += "</head><body>";
  html += "<h1>WiFi Configuration</h1>";
  html += "<form action='/connect' method='post'>";
  html += " <div class='form-floating mb-3'>";
  html += "   <label for='ssid' class='form-label'>Select Wifi to Connect</label>";
  html += "   <select name='ssid' class='form-select'>";

  for (int i = 0; i < numNetworks; ++i) {
    html += "<option value='" + WiFi.SSID(i) + "'>" + WiFi.SSID(i) + "</option>";
  }
  html += "    </select>";
  html += " </div>";
  html += " <div class='form-floating mb-3'>";
  html += "   <label for='password' class='form-label'>Password</label>";
  html += "   <input type='password' name='password' class='form-control' placeholder='Wifi Password'>";
  html += " </div>";
  html += " <div class='form-floating mb-3'>";
  html += "   <label for='mqttServer' class='form-label'>MQTT Hostname</label>";
  html += "   <input type='text' name='mqttServer' class='form-control' placeholder='MQTT Server hostname/IP'>";
  html += " </div>";
  html += " <div class='form-floating mb-3'>";
  html += "   <label for='mqttPort' class='form-label'>MQTT Port</label>";
  html += "   <input type='text' name='mqttPort' class='form-control' placeholder='MQTT Port'>";
  html += " </div>";
  html += " <div class='col-12'>";
  html += "   <button class='btn btn-primary' type='submit'>Connect Me!!!</button>";
  html += " </div>";
  html += "</form></body></html>";

  server.send(200, "text/html", html);
}

void handleConnect() {
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  initiateWifiConnect(ssid, password, true);

  String mqttServer = server.arg("mqttServer");
  String mqttPort = server.arg("mqttPort");
  addToConfig("mqttServer", mqttServer);
  addToConfig("mqttPort", mqttPort);
}


void addToConfig(String key, String value) {
  espConfigAsJSON[key] = value;
  String jsonString;
  serializeJson(espConfigAsJSON, jsonString);

  // Write the serialized JSON data to EEPROM
  for (int i = 0; i < jsonString.length(); ++i) {
    EEPROM.write(EEPROM_ADDRESS + i, jsonString[i]);
  }

  // Commit the changes to EEPROM
  EEPROM.commit();
}

void resetEEPROM() {
  for (int i = 0; i < EEPROM_SIZE; ++i) {
    EEPROM.write(i, 0xFF);
  }

  // Commit the changes to EEPROM
  EEPROM.commit();
  server.send(200, "text/html", "Reset Successfull!!!");
}



