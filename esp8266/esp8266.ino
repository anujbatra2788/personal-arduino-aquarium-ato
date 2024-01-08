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
  String SSID;
  String wifiPassword;
  String mqttServer;
  int mqttPort;
  String mqttUser;
  String mqttPassword;
};


DynamicJsonDocument espChipConfig(1024);
ESPConfig espConfig;

ESP8266WebServer server(80);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

int count = 0;
void loop() {
  server.handleClient();
  if(WiFi.status() == WL_CONNECTED) {
    if (!mqttClient.connected()) {
      initiateMQTTConnect();
    }
    mqttClient.loop();
    publishAvailableStatus();
    
    String json = Serial.readString();
    if(json.length() > 0) {
      mqttClient.publish("gf/lr/aquarium-manager/message", json.c_str());
      delay(5000);
      if(json.startsWith("{")) {
        DynamicJsonDocument jsonRequest(512);
        deserializeJson(jsonRequest, json);
        publishMQTTMessage(jsonRequest);
      }
    }
  }
  delay(2000);
}

void setup() {
  Serial.begin(56700);
  Serial.setTimeout(500);

  Serial.println("ESP8266::Commencing EEPROM");
  EEPROM.begin(EEPROM_SIZE);
  
  Serial.println("ESP8266::Trying to read JSON config from EEPROM");
  readConfigFromEEPROM();

  Serial.print("ESP8266::Setup Read Config SSID: ");
  Serial.print(espConfig.SSID);
  Serial.print(", with password: ");
  Serial.println(espConfig.wifiPassword);

  server.begin();

  if(strcmp(espConfig.SSID.c_str(), "") != 0 && strcmp(espConfig.wifiPassword.c_str(), "") != 0) {
    initiateWifiConnect(espConfig.SSID, espConfig.wifiPassword, false);
    Serial.print("ESP8266::setup::3::Setup Read Config mqttServer: ");
    Serial.print(espConfig.mqttServer);
    Serial.print(", and mqttPort: ");
    Serial.println(espConfig.mqttPort);

    if(strcmp(espConfig.mqttServer.c_str(), "") != 0) {
      Serial.print("ESP8266:: Setting the server and port for mqtt");
      mqttClient.setServer(espConfig.mqttServer.c_str(), espConfig.mqttPort);
      mqttClient.setCallback(callback);
      initiateMQTTConnect();
    }
    
  } else {
    // Start in AP mode
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSSID, apPassword);

    // Serve HTML page for WiFi credentials
    
    server.on("/", HTTP_GET, handleRoot);
    server.on("/reset", HTTP_GET, resetEEPROM);
    server.on("/connect", HTTP_POST, handleConnect);
    Serial.print("ESP8266::Access Point started. Connect to WiFi network: AquariumManager. IP Address is ");
    Serial.println(WiFi.softAPIP());
  }
}

void publishMQTTMessage(DynamicJsonDocument &request) {
  String topic = request["topic"].as<String>();
  JsonObject sensorPayload = request["sensorPayload"].as<JsonObject>();
  String sensorPayloadAsStr;
  serializeJsonPretty(sensorPayload, sensorPayloadAsStr);
  mqttClient.publish(topic.c_str(), sensorPayloadAsStr.c_str());
}

void readConfigFromEEPROM() {
  // Read data from EEPROM into a buffer
  char eepromData[EEPROM_SIZE];
  for (int i = 0; i < EEPROM_SIZE; ++i) {
    eepromData[i] = EEPROM.read(EEPROM_ADDRESS + i);
  }
  Serial.print("ESP8266::Parsing JSON from: ");
  Serial.println(eepromData);
  // Parse the JSON data from the buffer
  deserializeJson(espChipConfig, eepromData);
  
  espConfig.SSID = espChipConfig["SSID"].as<String>();
  espConfig.wifiPassword = espChipConfig["PWD"].as<String>();
  espConfig.mqttServer = espChipConfig["mqttServer"].as<String>();
  espConfig.mqttPort = espChipConfig["mqttPort"].as<String>().toInt();
  espConfig.mqttUser = espChipConfig["mqttUser"].as<String>();
  espConfig.mqttPassword = espChipConfig["mqttPassword"].as<String>();
}



void initiateWifiConnect(String ssid, String password, boolean writeToEEPROM) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.println("ESP8266::Connecting to WiFi...");

  int timeout = millis();

  while (WiFi.status() != WL_CONNECTED && (millis() - timeout < 30000)) {
    delay(500);
    Serial.print(".");
  }

  if(WiFi.status() == WL_CONNECTED) {
    if(writeToEEPROM) {
      Serial.println("ESP8266::Writing Wifi SSID and PWD config to EEPROM");
      addToConfig("SSID", ssid);
      addToConfig("PWD", password);
    }
    
    // Set up proxy server
    server.on("/reset", HTTP_GET, resetEEPROM);
    server.on("/info", HTTP_GET, espChipConfigInfo);
    server.on("/", HTTP_GET, handleRoot);
    server.on("/connect", HTTP_POST, handleConnect);
    server.on("/send-to-arduino", HTTP_GET, sendMessageToArduino);

    Serial.print("ESP8266::\nConnected to WiFi!. Local IP is ");
    Serial.println(WiFi.localIP());
    server.send(200, "text/html", "WiFi connection successful. Reset is ready at /reset");
  }
}

void sendMessageToArduino() {
  String message = server.arg("m");
  Serial.print(message);
}

void initiateMQTTConnect() {
  while (!mqttClient.connected()) {
    Serial.print("ESP8266::Attempting MQTT connection...");
    String mqttClientId = "AquariumManager-" + String(millis());
    boolean connected;
    if(strcmp(espConfig.mqttUser.c_str(), "") != 0 && strcmp(espConfig.mqttPassword.c_str(), "") != 0) {
      Serial.print("ESP8266::Connecting MQTT with Client ID - ");
      Serial.print(mqttClientId);
      Serial.print(", user - ");
      Serial.print(espConfig.mqttUser);
      Serial.print(", and password - ");
      Serial.println(espConfig.mqttPassword);
      connected = mqttClient.connect(mqttClientId.c_str(), espConfig.mqttUser.c_str(), espConfig.mqttPassword.c_str());
    } else {
      Serial.print("ESP8266::Connecting MQTT with Client ID - ");
      Serial.print(mqttClientId);
      Serial.println(" anonymously.");
      connected = mqttClient.connect(mqttClientId.c_str());
    }
     
    if (connected) {
      Serial.println("ESP8266::connected");
      mqttClient.subscribe("gf/lr/aquarium-manager/command/#");
      publishAvailableStatus();
    } else {
      Serial.print("ESP8266::failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println("will try again in 5 seconds");
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String logMessage = "ESP8266::Message arrived on topic: '";
  logMessage += topic;
  logMessage += "' with payload: ";
  
  String payloadStr;
  for (unsigned int i = 0; i < length; i++) {
    payloadStr += (char)payload[i];
    // Serial.print((char)payload[i]);
  }
  
  logMessage += payloadStr;
  // String commandPayload = "{\"topic\": \"";
  // commandPayload += topic;
  // commandPayload += "\", \"commandPayload\":";
  // commandPayload += payload;
  // commandPayload += "}";

  //Serial.print(commandPayload);
  mqttClient.publish("gf/lr/aquarium-manager/esp8266-arduino/log", logMessage.c_str(), true);
  mqttClient.publish("gf/lr/aquarium-manager/command-received", payloadStr.c_str(), true);
  Serial.println(payloadStr.c_str());
  
}

void publishAvailableStatus() {
  mqttClient.publish("gf/lr/aquarium-manager/available", "ON", false);
  mqttClient.publish("gf/lr/aquarium-manager/esp8266/loop", String(millis()).c_str(), false);
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
  html += "   <label for='mqttServer' class='form-label'>MQTT Server IP / Hostname</label>";
  html += "   <input type='text' name='mqttServer' class='form-control' placeholder='MQTT Server IP / Hostname'>";
  html += " </div>";

  html += " <div class='form-floating mb-3'>";
  html += "   <label for='mqttPort' class='form-label'>MQTT Server Port</label>";
  html += "   <input type='text' name='mqttPort' class='form-control' placeholder='MQTT Server Port'>";
  html += " </div>";

  html += " <div class='form-floating mb-3'>";
  html += "   <label for='mqttUser' class='form-label'>MQTT Username (can be blank)</label>";
  html += "   <input type='text' name='mqttUser' class='form-control' placeholder='MQTT Username (can be blank)'>";
  html += " </div>";

  html += " <div class='form-floating mb-3'>";
  html += "   <label for='mqttPassword' class='form-label'>MQTT Password (can be blank)</label>";
  html += "   <input type='password' name='mqttPassword' class='form-control' placeholder='MQTT Password (can be blank)'>";
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

  String mqttServer = server.arg("mqttServer");
  String mqttPort = server.arg("mqttPort");
  String mqttUser = server.arg("mqttUser");
  String mqttPassword = server.arg("mqttPassword");
  espChipConfig.clear();
  initiateWifiConnect(ssid, password, true);

  if(WiFi.status() == WL_CONNECTED) {
    addToConfig("mqttServer", mqttServer);
    addToConfig("mqttPort", mqttPort);
    addToConfig("mqttUser", mqttUser);
    addToConfig("mqttPassword", mqttPassword);
  }
  
  server.send(200, "text/html", "WiFi connection successful. Please access ESP endpoints /reset and /info at " + WiFi.localIP().toString());
}


void addToConfig(String key, String value) {
  espChipConfig[key] = value;
  String jsonString;
  serializeJson(espChipConfig, jsonString);

  // Write the serialized JSON data to EEPROM
  for (int i = 0; i < jsonString.length(); ++i) {
    EEPROM.write(EEPROM_ADDRESS + i, jsonString[i]);
  }
  Serial.print("ESP8266::Wrting EEPROM config - ");
  Serial.println(jsonString);
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

void espChipConfigInfo() {
  String html = "<html><head>";
  html += "</head><body>";
  html += "<h1>ESP Chip Configuration</h1>";
  
  html += "<h3>SSID: ";
  html += espConfig.SSID;
  html += "</h3>";
  
  html += "<h3>Wifi Password: ";
  html += espConfig.wifiPassword;
  html += "</h3>";
  
  html += "<h3>MQTT Server: ";
  html += espConfig.mqttServer;
  html += "</h3>";
  
  html += "<h3>MQTT Port: ";
  html += String(espConfig.mqttPort);
  html += "</h3>";

  html += "<h3>MQTT User: ";
  html += espConfig.mqttUser;
  html += "</h3>";

  html += "<h3>MQTT Password: ";
  html += espConfig.mqttPassword;
  html += "</h3>";

  html += "<h3>MQTT Connected: ";
  html += mqttClient.connected() ? "true": "false";
  html += "</h3>";

  html += "</body></html>";
  server.send(200, "text/html", html);
}



