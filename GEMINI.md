# Arduino Aquarium Manager

This project is an automated aquarium water top-off (ATO) system using an Arduino. The aquarium has 3 parts 
    - Main tank (aka Display tank)
    - Sump (Hosts the skimmer, return pump, and heater)
    - Reservoir (Keeps the reserve water to fill the sump)


The arduino board checks if the water level in sump is low then use the reservoir water to fill the sump, until level is acheived. 

## Features

*   Monitors water level using float sensors of sump and reservoir.
*   Automatically refills the aquarium when the water level is low.
*   Prevents overfilling.
*   Prevents motor burns if reservoir is empty.
*   Executes the operation manually from home assistant using mqtt
*   Sends the updates of switches to MQTT

## Hardware

*   Arduino board (e.g., Uno with Wifi)
*   Float sensors
*   Relay module
*   Pump
*   Power supply

## Software

The `controller/controller.ino` file contains the Arduino sketch for the main controller Arduino board to perform the actions, by sesing the float switch states and controlling with relay. It sends and receives messages to ESP8266 board via Serial for communication.


The `esp8266/esp8266.ino` file is for an ESP8266 Wi-Fi module, which interacts with the Wifi and MQTT. It receives the commands to execute on arduino controller (from Home assistant via MQTT) to manually start the filling of sump. It also sends an update to MQTT for homeassistant to report the status


## Rules

1. Keep the code always neat, clean and modular
2. Anything that requires an input we need to set it using a webpage in ESP8266 existing configuration page
3. Always write code documentation
4. Optimize the performance of the board for memory and execution.