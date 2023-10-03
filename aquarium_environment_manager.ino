#include <WiFiEspAT.h>
#include <ArduinoJson.h>
#include "utilities/variables.h"
#include <LowPower.h>
#include <ArduinoHttpClient.h>

#ifndef HAVE_HWSERIAL1
#include "SoftwareSerial.h"
SoftwareSerial espSerial(6, 7);  // RX, TX
#endif


/*************************************************************************************************
                            M Q T T       C A L L B A C K
 *************************************************************************************************/

void mqttSubscriptionCallback(char* topic, byte* payload, unsigned int length) {
  char received_topic[128];
  byte received_payload[128];
  unsigned int received_length;

  strcpy(received_topic, topic);
  memcpy(received_payload, payload, length);
  received_length = length;

  String receivedTopicAsString = String(received_topic);
  String receivedPayloadAsString = String((char*)received_payload);

  Serial.print("Message arrived on [");
  Serial.print("] = ");
  Serial.print(receivedPayloadAsString);
  if (receivedTopicAsString.equals(MQTT_TOPIC_SUBSCRIBE_MANAGER_COMMAND_OPS) && receivedPayloadAsString.equals(COMMAND_CHANGE_WATER)) {
    commenceWaterChange();
  } else if (receivedTopicAsString.equals(MQTT_TOPIC_SUBSCRIBE_MANAGER_COMMAND_OPS) && receivedPayloadAsString.equals(COMMAND_TURN_OFF_RESERVOIR_TO_SUMP)) {
    changePumpState(MQTT_TOPIC_ATO_SWITCH_RESERVOIR_TO_SUMP, PIN_ATO_RELAY_RESERVOIR_TO_SUMP, LOW);
  } else if (receivedTopicAsString.equals(MQTT_TOPIC_SUBSCRIBE_MANAGER_COMMAND_OPS) && receivedPayloadAsString.equals(COMMAND_TURN_OFF_RO_TO_RESERVOIR)) {
    changePumpState(MQTT_TOPIC_ATO_SWITCH_RO_TO_RESERVOIR, PIN_ATO_RELAY_RESERVOIR_TO_SUMP, LOW);
  } else if (receivedTopicAsString.equals(MQTT_TOPIC_SUBSCRIBE_MANAGER_COMMAND_OPS) && receivedPayloadAsString.equals(COMMAND_TURN_ON_RESERVOIR_TO_SUMP)) {
    fillSumpWithThreshold();
  } else if (receivedTopicAsString.equals(MQTT_TOPIC_SUBSCRIBE_MANAGER_COMMAND_OPS) && receivedPayloadAsString.equals(COMMAND_TURN_ON_RO_TO_RESERVOIR)) {
    changePumpState(MQTT_TOPIC_ATO_SWITCH_RO_TO_RESERVOIR, PIN_ATO_RELAY_RESERVOIR_TO_SUMP, HIGH);
    fillReservoirWithThreshold();
  }
}

/*************************************************************************************************
                            N E T W O R K         C L I E N T S
 *************************************************************************************************/

WiFiClient espClient;
HttpClient client = HttpClient(espClient, API_SERVER, API_SERVER_PORT);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(19200);
  espSerial.begin(9600);
  setupPins();
  connectToWifi();
}

void loop() {
  if (WIFI_STATUS != WL_CONNECTED) {
    connectToWifi();
  }

  reportFloatSensorLevels();
  reportTemperature();

  topOffWaterToReservoir(false);
  topOffWaterToSump();
  delay(1000);
  Serial.println("Powering off now.");
  int powerOffCounter = 0;
  while(powerOffCounter++ <= 3) {
    Serial.println("Powering off now.");
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
    Serial.print("Powering ONNNNN now. Counter is ");
    Serial.println(powerOffCounter);
  }
  
}

void commenceWaterChange() {
}

/*************************************************************************************************
                            F O R C E   T O P P I N G   U P
 *************************************************************************************************/
void fillSumpWithThreshold() {
  changePumpState(HTTP_API_ATO_SWITCH_RESERVOIR_TO_SUMP, PIN_ATO_RELAY_RESERVOIR_TO_SUMP, HIGH);
  ATO_PROCESS_RUNNING = true;

  while (digitalRead(PIN_ATO_LEVEL_SENSOR_SUMP_HIGHER_THRESHOLD) == LOW) {
    delay(5000);
  }
  changePumpState(HTTP_API_ATO_SWITCH_RESERVOIR_TO_SUMP, PIN_ATO_RELAY_RESERVOIR_TO_SUMP, LOW);
  ATO_PROCESS_RUNNING = false;
}

void fillReservoirWithThreshold() {
  topOffWaterToReservoir(true);
}

void publishWithHTTP(char *endpoint, char *value) {
  // Do some magic here 
  int size = strlen(endpoint) + strlen(value) + 1;
  char *concatenatedString = malloc(size);
  strcpy(concatenatedString, endpoint);
  strcpy(concatenatedString, value);
  
  client.sendHeader("Content-Type", "Content-Type: application/json"); 

  int httpResponseCode = client.post(concatenatedString); //Send the actual POST request
  Serial.println(client.responseBody());
  delay(500);

}

void reportFloatSensorLevels() {
  // Sump Lower threshold
  publishWithHTTP(HTTP_API_ENDPOINT_ATO_SENSOR_SUMP_LOWER_THRESHOLD, String(digitalRead(PIN_ATO_LEVEL_SENSOR_SUMP_LOWER_THRESHOLD)).c_str());
  publishWithHTTP(HTTP_API_ENDPOINT_ATO_SENSOR_SUMP_HIGHER_THRESHOLD, String(digitalRead(PIN_ATO_LEVEL_SENSOR_SUMP_HIGHER_THRESHOLD)).c_str());
  publishWithHTTP(HTTP_API_ENDPOINT_ATO_SENSOR_RESERVOIR_LOWER_THRESHOLD, String(digitalRead(PIN_ATO_LEVEL_SENSOR_RESERVOIR_LOWER_THRESHOLD)).c_str());
  publishWithHTTP(HTTP_API_ENDPOINT_ATO_SENSOR_RESERVOIR_HIGHER_THRESHOLD, String(digitalRead(PIN_ATO_LEVEL_SENSOR_RESERVOIR_HIGHER_THRESHOLD)).c_str());
}

void topOffWaterToReservoir(boolean forceFillStart) {
  // Check if reservoir has water
  if (!reservoirHaveWater() || forceFillStart) {
    // Serial.println("Reservoir doesn't has water. Filling it!!!");
    // Fill the reservoir until both are high
    // It's ok to fill the reservoir from ro as both float sensors are low
    if (digitalRead(PIN_ATO_LEVEL_SENSOR_RESERVOIR_HIGHER_THRESHOLD) == LOW) {
      changePumpState(HTTP_API_ATO_SWITCH_RO_TO_RESERVOIR, PIN_ATO_RELAY_RO_TO_RESERVOIR, HIGH);
    }

    while (digitalRead(PIN_ATO_LEVEL_SENSOR_RESERVOIR_HIGHER_THRESHOLD) == LOW) {
      // Serial.println("Filling Reservoir");
      delay(1000);
    }

    // Stop filling if highre float sensor has reached it's limit
    if (digitalRead(PIN_ATO_LEVEL_SENSOR_RESERVOIR_HIGHER_THRESHOLD) == HIGH) {
      // Stop filling the reservoir from ro as higher level float sensors is high
      // Serial.println("Reservoir Filling complete!!!");
      changePumpState(HTTP_API_ATO_SWITCH_RO_TO_RESERVOIR, PIN_ATO_RELAY_RO_TO_RESERVOIR, LOW);
    }
  } else {
    // Serial.println("Reservoir has water. Skipping to fill reservoir");
    changePumpState(HTTP_API_ATO_SWITCH_RO_TO_RESERVOIR, PIN_ATO_RELAY_RO_TO_RESERVOIR, LOW);
  }
}

void topOffWaterToSump() {
  // Check if reservoir has water
  if (reservoirHaveWater()) {
    // Serial.println("Reservoir have water. Checking if sump needs to be filled."); delay(200);
    if (digitalRead(PIN_ATO_LEVEL_SENSOR_SUMP_LOWER_THRESHOLD) == LOW && digitalRead(PIN_ATO_LEVEL_SENSOR_SUMP_HIGHER_THRESHOLD) == LOW) {
      // Serial.println("Filling sump NOW!!!");
      // It's ok to fill the sump from reservoir as both float sensors are low
      changePumpState(HTTP_API_ATO_SWITCH_RESERVOIR_TO_SUMP, PIN_ATO_RELAY_RESERVOIR_TO_SUMP, HIGH);
    }

    while (digitalRead(PIN_ATO_LEVEL_SENSOR_SUMP_LOWER_THRESHOLD) == LOW && digitalRead(PIN_ATO_LEVEL_SENSOR_SUMP_HIGHER_THRESHOLD) == LOW && reservoirHaveWater()) {
      // Serial.println("Filling sump now");
      delay(5000);
    }
    if (digitalRead(PIN_ATO_LEVEL_SENSOR_SUMP_LOWER_THRESHOLD) == HIGH || digitalRead(PIN_ATO_LEVEL_SENSOR_SUMP_HIGHER_THRESHOLD) == HIGH) {
      // Serial.println("Sump is full or Filling sump complete!!!");
      // Stop filling the sump from reservoir as one of the float sensors is high
      changePumpState(HTTP_API_ATO_SWITCH_RESERVOIR_TO_SUMP, PIN_ATO_RELAY_RESERVOIR_TO_SUMP, LOW);
    }
  } else {
    // Serial.println("Reservoir doesn't have water. Skipping to fill sump!!!");
    changePumpState(HTTP_API_ATO_SWITCH_RESERVOIR_TO_SUMP, PIN_ATO_RELAY_RESERVOIR_TO_SUMP, LOW);
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
  publishWithHTTP(topic, startOrStop);
}

void reportTemperature() {
  if (millis() - LAST_REPORTED_TEMP_MILLIS > 30000) {
    float temperature = analogRead(PIN_TEMP_SENSOR_SUMP);
    temperature = temperature * 0.48828125;
    publishWithHTTP(HTTP_API_PUBLISH_SUMP_WATER_TEMPERATURE, String(temperature).c_str());

    temperature = analogRead(PIN_TEMP_SENSOR_TANK);
    temperature = temperature * 0.48828125;
    publishWithHTTP(HTTP_API_PUBLISH_TANK_WATER_TEMPERATURE, String(temperature).c_str());

    LAST_REPORTED_TEMP_MILLIS = millis();
  }
}

// Last executed Date
// Did it run or not
//
void publishUpdate(const char* topic, const char* message) {
  if (INTERNET_AVAILABLE) {
    
  }
}

/*************************************************************************************************
                            N E T W O R K         C O N N E C T I O N S
 *************************************************************************************************/

void connectToWifi() {
  Serial.println("Connecting to Wifi");
  // initialize ESP module
  espSerial.setTimeout(5000);
  WiFi.init(&espSerial);
  delay(2000);
  // check for the presence of the shield
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WIFI:: WiFi shield not present");
    // don't continue
    return;
  }

  // This is done because ESP8266 takes a long time to connect if set to true.
  WiFi.setPersistent(false);
  int retryCount = 0;
  // attempt to connect to WiFi network
  while (WIFI_STATUS != WL_CONNECTED && retryCount++ < 5) {
    // Connect to WPA/WPA2 network
    WIFI_STATUS = WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("WIFI_STATUS:: ");
    Serial.println(WIFI_STATUS);
  }
  if (retryCount < 5) {
    INTERNET_AVAILABLE = true;
    Serial.println("You're connected to the network");
  }
}

void setupPins() {

  // Float sensors
  pinMode(PIN_ATO_LEVEL_SENSOR_SUMP_HIGHER_THRESHOLD, INPUT_PULLUP);
  pinMode(PIN_ATO_LEVEL_SENSOR_SUMP_LOWER_THRESHOLD, INPUT_PULLUP);
  pinMode(PIN_ATO_LEVEL_SENSOR_RESERVOIR_HIGHER_THRESHOLD, INPUT_PULLUP);
  pinMode(PIN_ATO_LEVEL_SENSOR_RESERVOIR_LOWER_THRESHOLD, INPUT_PULLUP);

  // Relays
  pinMode(PIN_ATO_RELAY_RESERVOIR_TO_SUMP, OUTPUT);
  pinMode(PIN_ATO_RELAY_RO_TO_RESERVOIR, OUTPUT);

  changePumpState(HTTP_API_ATO_SWITCH_RESERVOIR_TO_SUMP, PIN_ATO_RELAY_RESERVOIR_TO_SUMP, LOW);
  changePumpState(HTTP_API_ATO_SWITCH_RO_TO_RESERVOIR, PIN_ATO_RELAY_RO_TO_RESERVOIR, LOW);
}