#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SoftwareSerial.h>

// SoftwareSerial for communication with ESP8266
SoftwareSerial espSerial(2, 3); // RX, TX

// Pin definitions
const int PIN_ATO_LEVEL_SENSOR_SUMP_HIGHER_THRESHOLD = 12;
const int PIN_ATO_LEVEL_SENSOR_SUMP_LOWER_THRESHOLD = 13;
const int PIN_ATO_LEVEL_SENSOR_RESERVOIR_HIGHER_THRESHOLD = 10;
const int PIN_ATO_LEVEL_SENSOR_RESERVOIR_LOWER_THRESHOLD = 11;
const int PIN_ATO_RELAY_RESERVOIR_TO_SUMP = 9;
const int PIN_ATO_RELAY_RO_TO_RESERVOIR = 8;
const int DS18B20_TEMP_SENSOR_BUS = 19;

const char* MQTT_TOPIC_ATO_SWITCH_RO_TO_RESERVOIR = "ground-floor/living-room/aquarium/ato/switch/reservoir-fill";
const char* MQTT_TOPIC_ATO_SWITCH_RESERVOIR_TO_SUMP = "ground-floor/living-room/aquarium/ato/switch/sump-fill";
const char* MQTT_TOPIC_PUBLISH_SUMP_WATER_TEMPERATURE = "ground-floor/living-room/aquarium/sump/temperature";
const char* MQTT_TOPIC_ERROR = "aquarium/error";

boolean ATO_PROCESS_RUNNING = false;
OneWire oneWire(DS18B20_TEMP_SENSOR_BUS);
DallasTemperature sensors(&oneWire);

enum State { IDLE, FILLING_RESERVOIR, FILLING_SUMP };
State currentState = IDLE;
unsigned long lastTempReport = 0;
const unsigned long TEMP_INTERVAL = 300000; // 5 minutes
unsigned long pumpStartTime = 0;
const unsigned long PUMP_TIMEOUT = 300000; // 5 minutes

void setup() {
  Serial.begin(57600);
  espSerial.begin(9600);
  Serial.setTimeout(500);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(PIN_ATO_LEVEL_SENSOR_SUMP_HIGHER_THRESHOLD, INPUT);
  pinMode(PIN_ATO_LEVEL_SENSOR_SUMP_LOWER_THRESHOLD, INPUT);
  pinMode(PIN_ATO_LEVEL_SENSOR_RESERVOIR_HIGHER_THRESHOLD, INPUT);
  pinMode(PIN_ATO_LEVEL_SENSOR_RESERVOIR_LOWER_THRESHOLD, INPUT);
  pinMode(PIN_ATO_RELAY_RESERVOIR_TO_SUMP, OUTPUT);
  pinMode(PIN_ATO_RELAY_RO_TO_RESERVOIR, OUTPUT);
  digitalWrite(PIN_ATO_RELAY_RO_TO_RESERVOIR, HIGH);
  digitalWrite(PIN_ATO_RELAY_RESERVOIR_TO_SUMP, HIGH);
}

void loop() {
  checkAndExecuteAnyRemoteCommand();
  reportTemperature();

  // Check sensor validity
  bool sumpSensorsValid = isSensorValid(PIN_ATO_LEVEL_SENSOR_SUMP_LOWER_THRESHOLD, PIN_ATO_LEVEL_SENSOR_SUMP_HIGHER_THRESHOLD);
  bool reservoirSensorsValid = isSensorValid(PIN_ATO_LEVEL_SENSOR_RESERVOIR_LOWER_THRESHOLD, PIN_ATO_LEVEL_SENSOR_RESERVOIR_HIGHER_THRESHOLD);

  if (!sumpSensorsValid || !reservoirSensorsValid) {
    changePumpState(MQTT_TOPIC_ATO_SWITCH_RO_TO_RESERVOIR, PIN_ATO_RELAY_RO_TO_RESERVOIR, LOW);
    changePumpState(MQTT_TOPIC_ATO_SWITCH_RESERVOIR_TO_SUMP, PIN_ATO_RELAY_RESERVOIR_TO_SUMP, LOW);
    currentState = IDLE;
    publishError("Sensor fault detected");
    return;
  }

  // Check pump timeout
  if (ATO_PROCESS_RUNNING && millis() - pumpStartTime > PUMP_TIMEOUT) {
    if (currentState == FILLING_RESERVOIR) {
      changePumpState(MQTT_TOPIC_ATO_SWITCH_RO_TO_RESERVOIR, PIN_ATO_RELAY_RO_TO_RESERVOIR, LOW);
      publishError("Reservoir pump timeout");
    } else if (currentState == FILLING_SUMP) {
      changePumpState(MQTT_TOPIC_ATO_SWITCH_RESERVOIR_TO_SUMP, PIN_ATO_RELAY_RESERVOIR_TO_SUMP, LOW);
      publishError("Sump pump timeout");
    }
    currentState = IDLE;
  }

  switch (currentState) {
    case IDLE:
      if (!reservoirHaveWater()) {
        if (!ATO_PROCESS_RUNNING) {
          currentState = FILLING_RESERVOIR;
          changePumpState(MQTT_TOPIC_ATO_SWITCH_RO_TO_RESERVOIR, PIN_ATO_RELAY_RO_TO_RESERVOIR, HIGH);
        }
      } else if (reservoirHaveWater() && readDebounced(PIN_ATO_LEVEL_SENSOR_SUMP_LOWER_THRESHOLD) == LOW) {
        if (!ATO_PROCESS_RUNNING) {
          currentState = FILLING_SUMP;
          changePumpState(MQTT_TOPIC_ATO_SWITCH_RESERVOIR_TO_SUMP, PIN_ATO_RELAY_RESERVOIR_TO_SUMP, HIGH);
        }
      }
      break;
    case FILLING_RESERVOIR:
      if (readDebounced(PIN_ATO_LEVEL_SENSOR_RESERVOIR_HIGHER_THRESHOLD) == HIGH) {
        changePumpState(MQTT_TOPIC_ATO_SWITCH_RO_TO_RESERVOIR, PIN_ATO_RELAY_RO_TO_RESERVOIR, LOW);
        currentState = IDLE;
      }
      break;
    case FILLING_SUMP:
      if (readDebounced(PIN_ATO_LEVEL_SENSOR_SUMP_HIGHER_THRESHOLD) == HIGH || !reservoirHaveWater()) {
        changePumpState(MQTT_TOPIC_ATO_SWITCH_RESERVOIR_TO_SUMP, PIN_ATO_RELAY_RESERVOIR_TO_SUMP, LOW);
        currentState = IDLE;
      }
      break;
  }
}

void checkAndExecuteAnyRemoteCommand() {
  if (espSerial.available()) {
    String message = espSerial.readString();
    if (message.startsWith("{")) {
      DynamicJsonDocument jsonRequest(512);
      DeserializationError error = deserializeJson(jsonRequest, message);
      if (!error && jsonRequest.containsKey("command")) {
        String command = jsonRequest["command"];
        if (command == "fill_sump") {
          if (readDebounced(PIN_ATO_LEVEL_SENSOR_SUMP_HIGHER_THRESHOLD) == LOW && reservoirHaveWater()) {
            changePumpState(MQTT_TOPIC_ATO_SWITCH_RESERVOIR_TO_SUMP, PIN_ATO_RELAY_RESERVOIR_TO_SUMP, HIGH);
            currentState = FILLING_SUMP;
          } else {
            publishError("Cannot fill sump: high sensor active or reservoir empty");
          }
        } else if (command == "stop_sump") {
          changePumpState(MQTT_TOPIC_ATO_SWITCH_RESERVOIR_TO_SUMP, PIN_ATO_RELAY_RESERVOIR_TO_SUMP, LOW);
          currentState = IDLE;
        } else if (command == "fill_reservoir") {
          if (readDebounced(PIN_ATO_LEVEL_SENSOR_RESERVOIR_HIGHER_THRESHOLD) == LOW) {
            changePumpState(MQTT_TOPIC_ATO_SWITCH_RO_TO_RESERVOIR, PIN_ATO_RELAY_RO_TO_RESERVOIR, HIGH);
            currentState = FILLING_RESERVOIR;
          } else {
            publishError("Cannot fill reservoir: high sensor active");
          }
        } else if (command == "stop_reservoir") {
          changePumpState(MQTT_TOPIC_ATO_SWITCH_RO_TO_RESERVOIR, PIN_ATO_RELAY_RO_TO_RESERVOIR, LOW);
          currentState = IDLE;
        }
      }
    }
  }
}

void reportTemperature() {
  if (millis() - lastTempReport >= TEMP_INTERVAL) {
    sensors.requestTemperatures();
    float temperature = sensors.getTempCByIndex(0);
    if (temperature != -127.0) {
      publishSensorData((char*)MQTT_TOPIC_PUBLISH_SUMP_WATER_TEMPERATURE, (char*)"temperature", (char*)String(temperature).c_str());
      lastTempReport = millis();
    } else {
      publishError("Temperature sensor error");
    }
  }
}

bool readDebounced(int pin) {
  int reading = digitalRead(pin);
  delay(10);
  return digitalRead(pin) == reading ? reading : !reading;
}

boolean reservoirHaveWater() {
  return readDebounced(PIN_ATO_LEVEL_SENSOR_RESERVOIR_LOWER_THRESHOLD) == HIGH;
}

bool isSensorValid(int lowPin, int highPin) {
  return !(readDebounced(lowPin) == HIGH && readDebounced(highPin) == HIGH);
}

void changePumpState(const char* topic, int digitalRelayPin, int startOrStop) {
  if (startOrStop == LOW) {
    ATO_PROCESS_RUNNING = false;
    digitalWrite(digitalRelayPin, 1);
    digitalWrite(LED_BUILTIN, LOW);
  } else {
    ATO_PROCESS_RUNNING = true;
    digitalWrite(digitalRelayPin, 0);
    digitalWrite(LED_BUILTIN, HIGH);
    pumpStartTime = millis();
  }
  String sensorName = "switch" + String(digitalRelayPin);
  publishSensorData((char*)topic, (char*)sensorName.c_str(), (char*)String(startOrStop).c_str());
}

void publishSensorData(char* topic, char* sensor, char* sensorValue) {
  String payload = "{\"topic\": \"";
  payload += topic;
  payload += "\", \"sensorPayload\": { \"";
  payload += sensor;
  payload += "\": \"";
  payload += sensorValue;
  payload += "\"}}";
  espSerial.println(payload);
}

void publishError(const char* message) {
  String payload = "{\"topic\": \"aquarium/error\", \"sensorPayload\": {\"error\": \"" + String(message) + "\"}}";
  espSerial.println(payload);
}