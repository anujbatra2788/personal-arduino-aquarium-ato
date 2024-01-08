#include <ArduinoJson.h>

void setup() {
  // put your setup code here, to run once:
  Serial.begin(57600);
  Serial.setTimeout(500);
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  delay(5000); // wait 10 sec between each send to the ESP  - when changed to 5 seconds did not work properly
  checkAndExecuteAnyRemoteCommand();
  // reportFloatSensorLevels();
  reportTemperature();
  // checkAndTopupSump();
  // checkAndTopupReservoir();
}

void checkAndExecuteAnyRemoteCommand() {
  if(!Serial.available()) {
    return;
  }
  while (Serial.available()) {
    String message = Serial.readString();
    if(message.startsWith("{")) {
      // Message contains a JsonObject and hence process it.
      digitalWrite(LED_BUILTIN, HIGH);
    } else{
      digitalWrite(LED_BUILTIN, LOW);
    }
  }
  delay(500);
}

void reportTemperature() {
  publishSensorData("gf/lr/aquarium/tank/temperature", "temperature", String(random(20,30)).c_str());
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


