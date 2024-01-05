#include <ArduinoJson.h>

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  // Serial.println("Loop Start");
  String message = Serial.readString();

  // digitalWrite(LED_BUILTIN, HIGH);
  // delay(5000);
  // digitalWrite(LED_BUILTIN, LOW);
  // delay(5000);

  if(message.startsWith("START")) {
    digitalWrite(LED_BUILTIN, HIGH);
  } else {
    digitalWrite(LED_BUILTIN, LOW);
  }
  delay(5000);
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
  delay(200);
}


