
/*************************************************************************************************
										E S P		8 2 6 6
 *************************************************************************************************/
const char 			WIFI_SSID[] 											= "Batras";            // your network SSID (name)
const char 			WIFI_PASSWORD[] 										= "Exc3lsi0r"; 
int 				WIFI_STATUS												= WL_IDLE_STATUS;
boolean				INTERNET_AVAILABLE										= false;
/*************************************************************************************************
										M Q T T		S E T T I N G S
 *************************************************************************************************/
//const IPAddress 	HTTP_SERVER 											= {192,168,68,51};
//const int 			MQTT_PORT 												= 1883;
//boolean				MQTT_CONNECTION_STATUS									= false;
const char			API_SERVER[]											= "192.168.68.51";
const int 			API_SERVER_PORT 										= 8080;
// Availability
const char* 		MQTT_TOPIC_PUBLISH_MANAGER_WIFI_STATUS 					= "living-room/aquarium/ato/availability";
// ATO runs
const char* 		MQTT_TOPIC_ATO_SWITCH_RESERVOIR_TO_SUMP					= "living-room/aquarium/ato/switch/sump-fill";
const char* 		MQTT_TOPIC_ATO_SWITCH_RO_TO_RESERVOIR					= "living-room/aquarium/ato/switch/reservoir-fill";

const char* 		HTTP_API_ATO_SWITCH_RESERVOIR_TO_SUMP					= "/manager/sump/water-fill?value=";
const char* 		HTTP_API_ATO_SWITCH_RO_TO_RESERVOIR						= "/manager/reservoir/water-fill?value=";


// Water change
const char* 		MQTT_TOPIC_PUBLISH_WATER_CHANGER_RUNS 					= "living-room/aquarium/water-changer";
// Temperature
const char* 		MQTT_TOPIC_PUBLISH_SUMP_WATER_TEMPERATURE 				= "living-room/aquarium/temperature/sump";
const char* 		MQTT_TOPIC_PUBLISH_TANK_WATER_TEMPERATURE 				= "living-room/aquarium/temperature/tank";

const char* 		HTTP_API_PUBLISH_SUMP_WATER_TEMPERATURE 				= "/manager/sump/temperature?temperature=";
const char* 		HTTP_API_PUBLISH_TANK_WATER_TEMPERATURE 				= "/manager/tank/temperature?temperature=";


// Manual commands
const char*			MQTT_TOPIC_SUBSCRIBE_MANAGER_COMMAND_OPS				= "living-room/aquarium/manager/command_ops";
// Float sensor state
const char* 		MQTT_TOPIC_ATO_SENSOR_SUMP_HIGHER_THRESHOLD				= "living-room/aquarium/ato/sensor/sump/higher-float";
const char* 		MQTT_TOPIC_ATO_SENSOR_SUMP_LOWER_THRESHOLD				= "living-room/aquarium/ato/sensor/sump/lower-float";
const char* 		MQTT_TOPIC_ATO_SENSOR_RESERVOIR_HIGHER_THRESHOLD		= "living-room/aquarium/ato/sensor/reservoir/higher-float";
const char* 		MQTT_TOPIC_ATO_SENSOR_RESERVOIR_LOWER_THRESHOLD			= "living-room/aquarium/ato/sensor/reservoir/higher-float";


const char*			HTTP_API_ENDPOINT_ATO_SENSOR_SUMP_LOWER_THRESHOLD		= "/manager/sump/float?position=low&value=";
const char*			HTTP_API_ENDPOINT_ATO_SENSOR_SUMP_HIGHER_THRESHOLD		= "/manager/sump/float?position=high&value=";
const char*			HTTP_API_ENDPOINT_ATO_SENSOR_RESERVOIR_LOWER_THRESHOLD	= "/manager/tank/float?position=low&value=";
const char*			HTTP_API_ENDPOINT_ATO_SENSOR_RESERVOIR_HIGHER_THRESHOLD	= "/manager/tank/float?position=high&value=";

/*************************************************************************************************
										A T O		V A R I A B L E S
 *************************************************************************************************/
const int			PIN_ATO_LEVEL_SENSOR_SUMP_HIGHER_THRESHOLD				= 12;
const int			PIN_ATO_LEVEL_SENSOR_SUMP_LOWER_THRESHOLD				= 13;
const int			PIN_ATO_LEVEL_SENSOR_RESERVOIR_HIGHER_THRESHOLD			= 10;
const int			PIN_ATO_LEVEL_SENSOR_RESERVOIR_LOWER_THRESHOLD			= 11;
const int			PIN_ATO_RELAY_RESERVOIR_TO_SUMP							= 9;
const int			PIN_ATO_RELAY_RO_TO_RESERVOIR							= 8;

boolean				ATO_PROCESS_RUNNING										= false;


/*************************************************************************************************
							W A T E R		C H A N G E R		V A R I A B L E S
 *************************************************************************************************/
const String 		COMMAND_CHANGE_WATER									= "change-water";
const String 		COMMAND_TURN_OFF_RESERVOIR_TO_SUMP						= "turn-off-reservoir-to-sump";
const String 		COMMAND_TURN_OFF_RO_TO_RESERVOIR						= "turn-off-ro-to-reservoir";
const String 		COMMAND_TURN_ON_RESERVOIR_TO_SUMP						= "turn-on-reservoir-to-sump";
const String 		COMMAND_TURN_ON_RO_TO_RESERVOIR							= "turn-on-ro-to-reservoir";


/*************************************************************************************************
								T E M P E R A T U R E		S E N S O R
 *************************************************************************************************/
 
const int			PIN_TEMP_SENSOR_SUMP									= A0;
const int			PIN_TEMP_SENSOR_TANK									= A1;
long 				LAST_REPORTED_TEMP_MILLIS								= 0;
