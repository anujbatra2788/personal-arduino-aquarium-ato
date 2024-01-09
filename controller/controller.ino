#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>

const int			PIN_ATO_LEVEL_SENSOR_SUMP_HIGHER_THRESHOLD				                                              = 12;
const int			PIN_ATO_LEVEL_SENSOR_SUMP_LOWER_THRESHOLD				                                                = 13;
const int			PIN_ATO_LEVEL_SENSOR_RESERVOIR_HIGHER_THRESHOLD			                                            = 10;
const int			PIN_ATO_LEVEL_SENSOR_RESERVOIR_LOWER_THRESHOLD			                                            = 11;
const int			PIN_ATO_RELAY_RESERVOIR_TO_SUMP							                                                    = 9;
const int			PIN_ATO_RELAY_RO_TO_RESERVOIR							                                                      = 8;

const int			DS18B20_TEMP_SENSOR_BUS									                                                        = 19;

const char* 		MQTT_TOPIC_ATO_SWITCH_RO_TO_RESERVOIR					                                                = "ground-floor/living-room/aquarium/ato/switch/reservoir-fill";
const char* 		MQTT_TOPIC_ATO_SWITCH_RESERVOIR_TO_SUMP					                                              = "ground-floor/living-room/aquarium/ato/switch/sump-fill";
const char* 		MQTT_TOPIC_PUBLISH_SUMP_WATER_TEMPERATURE 				                                            = "ground-floor/living-room/aquarium/sump/temperature";
const char* 		MQTT_TOPIC_ATO_SENSOR_SUMP_HIGHER_THRESHOLD				                                            = "ground-floor/living-room/aquarium/sump/water-level/higher-threshold";
const char* 		MQTT_TOPIC_ATO_SENSOR_SUMP_LOWER_THRESHOLD				                                            = "ground-floor/living-room/aquarium/sump/water-level/lower-threshold";
const char* 		MQTT_TOPIC_ATO_SENSOR_RESERVOIR_HIGHER_THRESHOLD		                                          = "ground-floor/living-room/aquarium/reservoir/water-level/higher-threshold";
const char* 		MQTT_TOPIC_ATO_SENSOR_RESERVOIR_LOWER_THRESHOLD			                                          = "ground-floor/living-room/aquarium/reservoir/water-level/lower-threshold";


boolean				ATO_PROCESS_RUNNING										                                                          = false;

OneWire oneWire(DS18B20_TEMP_SENSOR_BUS);
DallasTemperature sensors(&oneWire);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(57600);
  Serial.setTimeout(500);
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  checkAndExecuteAnyRemoteCommand();
  // reportFloatSensorLevels();
  reportTemperature();
  checkAndTopupReservoir();
  checkAndTopupSump();
  
}

void checkAndExecuteAnyRemoteCommand() {
  if(!Serial.available()) {
    return;
  }
  while (Serial.available()) {
    String message = Serial.readString();
    if(message.startsWith("{")) {
      DynamicJsonDocument jsonRequest(512);
      deserializeJson(jsonRequest, message);
      
    }
  }
  delay(500);
}

void reportFloatSensorLevels() {
  // Sump Lower threshold
  publishSensorData(MQTT_TOPIC_ATO_SENSOR_SUMP_LOWER_THRESHOLD, "water-level", String(digitalRead(PIN_ATO_LEVEL_SENSOR_SUMP_LOWER_THRESHOLD)).c_str());
  publishSensorData(MQTT_TOPIC_ATO_SENSOR_SUMP_HIGHER_THRESHOLD, "water-level", String(digitalRead(PIN_ATO_LEVEL_SENSOR_SUMP_HIGHER_THRESHOLD)).c_str());
  publishSensorData(MQTT_TOPIC_ATO_SENSOR_RESERVOIR_LOWER_THRESHOLD, "water-level", String(digitalRead(PIN_ATO_LEVEL_SENSOR_RESERVOIR_LOWER_THRESHOLD)).c_str());
  publishSensorData(MQTT_TOPIC_ATO_SENSOR_RESERVOIR_HIGHER_THRESHOLD, "water-level", String(digitalRead(PIN_ATO_LEVEL_SENSOR_RESERVOIR_HIGHER_THRESHOLD)).c_str());
}

void reportTemperature() {
  // publishSensorData("gf/lr/aquarium/tank/temperature", "temperature", String(random(20,30)).c_str());
  sensors.requestTemperatures();
  
  float temperature = sensors.getTempCByIndex(0);
  temperature = temperature * 0.48828125;
  publishSensorData(MQTT_TOPIC_PUBLISH_SUMP_WATER_TEMPERATURE, "temperature", String(temperature).c_str());

  // temperature = analogRead(PIN_TEMP_SENSOR_TANK);
  // temperature = temperature * 0.48828125;
  // publishWithHTTP(HTTP_API_PUBLISH_TANK_WATER_TEMPERATURE, String(temperature).c_str());
}

void checkAndTopupReservoir() {
  // Check if reservoir has water
  if(!reservoirHaveWater()) {
    changePumpState(MQTT_TOPIC_ATO_SWITCH_RO_TO_RESERVOIR, PIN_ATO_RELAY_RO_TO_RESERVOIR, HIGH);
    while (digitalRead(PIN_ATO_LEVEL_SENSOR_RESERVOIR_HIGHER_THRESHOLD) == LOW) {
      delay(1000);
    }
    changePumpState(MQTT_TOPIC_ATO_SWITCH_RO_TO_RESERVOIR, PIN_ATO_RELAY_RO_TO_RESERVOIR, LOW);
  } else {
    changePumpState(MQTT_TOPIC_ATO_SWITCH_RO_TO_RESERVOIR, PIN_ATO_RELAY_RO_TO_RESERVOIR, LOW);
  }
}

void checkAndTopupSump() {
  // Check if reservoir has water
  if (reservoirHaveWater()) {
    // Serial.println("Reservoir have water. Checking if sump needs to be filled."); delay(200);
    if (digitalRead(PIN_ATO_LEVEL_SENSOR_SUMP_LOWER_THRESHOLD) == LOW) {
      Serial.println(F("Filling sump NOW!!!"));
      // It's ok to fill the sump from reservoir as both float sensors are low
      changePumpState(MQTT_TOPIC_ATO_SWITCH_RESERVOIR_TO_SUMP, PIN_ATO_RELAY_RESERVOIR_TO_SUMP, HIGH);
    }

    while (digitalRead(PIN_ATO_LEVEL_SENSOR_SUMP_LOWER_THRESHOLD) == LOW && reservoirHaveWater()) {
      // Serial.println(F("Filling sump now"));
      if(digitalRead(PIN_ATO_LEVEL_SENSOR_SUMP_HIGHER_THRESHOLD) == HIGH || !reservoirHaveWater()) {
        break;
      }

      delay(5000);
    }
    if (digitalRead(PIN_ATO_LEVEL_SENSOR_SUMP_LOWER_THRESHOLD) == HIGH || digitalRead(PIN_ATO_LEVEL_SENSOR_SUMP_HIGHER_THRESHOLD) == HIGH) {
      // Serial.println("Sump is full or Filling sump complete!!!");
      // Stop filling the sump from reservoir as one of the float sensors is high
      changePumpState(MQTT_TOPIC_ATO_SWITCH_RESERVOIR_TO_SUMP, PIN_ATO_RELAY_RESERVOIR_TO_SUMP, LOW);
    }
  } else {
    // Serial.println("Reservoir doesn't have water. Skipping to fill sump!!!");
    changePumpState(MQTT_TOPIC_ATO_SWITCH_RESERVOIR_TO_SUMP, PIN_ATO_RELAY_RESERVOIR_TO_SUMP, LOW);
  }
}

boolean reservoirHaveWater() {
  if (digitalRead(PIN_ATO_LEVEL_SENSOR_RESERVOIR_LOWER_THRESHOLD) == LOW) {
    return false;
  }
  return true;
}

void changePumpState(char* topic, int digitalRelayPin, int startOrStop) {
  if (startOrStop == LOW) {
    // Stop
    ATO_PROCESS_RUNNING = false;
    digitalWrite(digitalRelayPin, 1);
  } else {
    // Start
    ATO_PROCESS_RUNNING = true;
    digitalWrite(digitalRelayPin, 0);
  }
  String sensorName = "switch" + String(digitalRelayPin);
  publishSensorData(topic, sensorName.c_str(), startOrStop);
}


void publishSensorData(char *topic, char *sensor, char *sensorValue) {
  String payload = "{\"topic\": \"";
  payload += topic;
  payload += "\", \"sensorPayload\": { \"";
  payload += sensor;
  payload += "\": \"";
  payload += sensorValue;
  payload += "\"}}";
  Serial.println(payload);
  delay(500);
}


