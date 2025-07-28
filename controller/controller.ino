#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SoftwareSerial.h>
#include <Adafruit_NeoPixel.h>

// -- Pin Definitions --
// Water Level Sensors
const int PIN_ATO_LEVEL_SENSOR_SUMP_HIGHER_THRESHOLD = 12;
const int PIN_ATO_LEVEL_SENSOR_SUMP_LOWER_THRESHOLD = 13;
const int PIN_ATO_LEVEL_SENSOR_RESERVOIR_HIGHER_THRESHOLD = 10;
const int PIN_ATO_LEVEL_SENSOR_RESERVOIR_LOWER_THRESHOLD = 11;
// Relay Pins
const int PIN_ATO_RELAY_RESERVOIR_TO_SUMP = 9;
const int PIN_ATO_RELAY_RO_TO_RESERVOIR = 8;
// Temperature Sensor
const int DS18B20_TEMP_SENSOR_BUS = 19;
// WS2812B RGB LED Ring
const int PIN_NEOPIXEL = 6;
const int NUMPIXELS = 16; // Number of LEDs in your ring

// -- MQTT Topics --
const char* MQTT_TOPIC_ATO_SWITCH_RO_TO_RESERVOIR = "ground-floor/living-room/aquarium/ato/switch/reservoir-fill";
const char* MQTT_TOPIC_ATO_SWITCH_RESERVOIR_TO_SUMP = "ground-floor/living-room/aquarium/ato/switch/sump-fill";
const char* MQTT_TOPIC_PUBLISH_SUMP_WATER_TEMPERATURE = "ground-floor/living-room/aquarium/sump/temperature";
const char* MQTT_TOPIC_ERROR = "aquarium/error";

// -- Global Variables & Objects --
// Serial for ESP8266 communication
SoftwareSerial espSerial(2, 3); // RX, TX
// Temperature Sensor Objects
OneWire oneWire(DS18B20_TEMP_SENSOR_BUS);
DallasTemperature sensors(&oneWire);
// NeoPixel LED Ring Object
Adafruit_NeoPixel pixels(NUMPIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

// State Machine
enum State { IDLE, FILLING_RESERVOIR, FILLING_SUMP };
State currentState = IDLE;
boolean ATO_PROCESS_RUNNING = false;
boolean ledsAreOn = false; // To prevent unnecessary LED updates

// Timers
unsigned long lastTempReport = 0;
const unsigned long TEMP_INTERVAL = 300000; // 5 minutes
unsigned long pumpStartTime = 0;
const unsigned long PUMP_TIMEOUT = 300000; // 5 minutes

// -- Core Functions --
void setup() {
  Serial.begin(57600);
  espSerial.begin(9600);
  Serial.setTimeout(500);

  // Initialize Pins
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(PIN_ATO_LEVEL_SENSOR_SUMP_HIGHER_THRESHOLD, INPUT);
  pinMode(PIN_ATO_LEVEL_SENSOR_SUMP_LOWER_THRESHOLD, INPUT);
  pinMode(PIN_ATO_LEVEL_SENSOR_RESERVOIR_HIGHER_THRESHOLD, INPUT);
  pinMode(PIN_ATO_LEVEL_SENSOR_RESERVOIR_LOWER_THRESHOLD, INPUT);
  pinMode(PIN_ATO_RELAY_RESERVOIR_TO_SUMP, OUTPUT);
  pinMode(PIN_ATO_RELAY_RO_TO_RESERVOIR, OUTPUT);

  // Set initial relay state to OFF (assuming active-low relays)
  digitalWrite(PIN_ATO_RELAY_RO_TO_RESERVOIR, HIGH);
  digitalWrite(PIN_ATO_RELAY_RESERVOIR_TO_SUMP, HIGH);

  // Initialize LED Ring
  pixels.begin();
  pixels.clear();
  pixels.show();
}

void loop() {
  checkAndExecuteAnyRemoteCommand();
  reportTemperature();
  handleLedStatus();

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

  // Main state machine
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

// -- LED Control Functions --

void handleLedStatus() {
  if (!reservoirHaveWater()) {
    reservoirEmptyAnimation();
    ledsAreOn = true;
  } else if (currentState == FILLING_RESERVOIR) {
    pixels.fill(pixels.Color(0, 0, 255)); // Solid blue for filling
    pixels.show();
    ledsAreOn = true;
  } else {
    if (ledsAreOn) { // Turn off LEDs if they were on
      pixels.clear();
      pixels.show();
      ledsAreOn = false;
    }
  }
}

void reservoirEmptyAnimation() {
  static unsigned long lastUpdate = 0;
  static int currentPixel = 0;
  unsigned long now = millis();

  if (now - lastUpdate > 100) { // Animation speed
    lastUpdate = now;
    pixels.clear();
    pixels.setPixelColor(currentPixel, pixels.Color(255, 0, 0)); // Red
    pixels.show();
    currentPixel++;
    if (currentPixel >= NUMPIXELS) {
      currentPixel = 0;
    }
  }
}

// -- Communication & Sensor Functions --

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
  String payload = "{"topic": "";
  payload += topic;
  payload += "", "sensorPayload": { "";
  payload += sensor;
  payload += "": "";
  payload += sensorValue;
  payload += ""}}";
  espSerial.println(payload);
}

void publishError(const char* message) {
  String payload = "{"topic": "aquarium/error", "sensorPayload": {"error": "" + String(message) + ""}}";
  espSerial.println(payload);
}