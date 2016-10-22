/*

 Code for DS18B20 Sensors from: http://blog.wenzlaff.de/?p=1254
 Thanks to Thomas Wenzlaff

 Temperature Sensor DS18B20 on GPIO2
 Left    = GND,
 Middle  = Data,
 Right   = VCC,
 3k-10k Ohm Resistor from VCC to Data.
 */

#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <EEPROM.h>     

#define SLEEPTIME 30                 // ESP Sleep Time in s
#define WIFI_CONNECT_TIME 6         // time to connect to wifi in s
#define MQTT_SERVER_NAME_LENGTH 40  // max length for mqtt server names
#define TRIGGER_PIN 0               // set pin to LOW for config AP
#define ONE_WIRE_PIN 2              // GPIO pin on which the sensors are connected
#define MAX_NUMBER_OF_SENSORS 8     // maximum number of connectable sensors

#define DEBUG

#ifdef DEBUG
#define DEBUG_PRINT(x)    Serial.print (x)
#define DEBUG_PRINTDEC(x) Serial.print (x, DEC)
#define DEBUG_PRINTLN(x)  Serial.println (x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTDEC(x)
#define DEBUG_PRINTLN(x) 
#endif

// general settings
String clientName = "ESPSensor";
String sensorID = "ID_na";
String infoTopic = "";
String statusTopic = "";
String errorTopic = "";
String mqtt_server = "";
int _eepromStart = 0;

//flag for saving data
bool shouldSaveConfig = false;

// initialize Sensors
OneWire ourWire(ONE_WIRE_PIN);        // initialize oneWire instance
DallasTemperature sensors(&ourWire); // Dallas Temperature Library for oneWire Library

// initialize WifiClient
WiFiClient wifiClient;
PubSubClient mqttclient(wifiClient);

boolean mqttConnect() {
	int failCounter = 10;

	if (WiFi.status() == WL_CONNECTED) {
		if (mqttclient.connected()) {
			Serial.println("Already connected with MQTT Server");
		} else {
			Serial.println(
					"Connecting to MQTT server " + mqtt_server + " as "
					+ clientName);

			while (!mqttclient.connect((char*) clientName.c_str())
					&& failCounter >= 0) {
				Serial.println(
						"Connection to MQTT server failed with state: "
						+ mqttclient.state());
				delay(1000);
				failCounter--;
			}

			if (mqttclient.connected()) {
				Serial.println();
				Serial.println(
						"Connected to MQTT broker on host " + mqtt_server);
			} else {
				return false;
			}
		}
	} else {
		return false;
	}
	return true;
}

boolean readDallasSensor(DeviceAddress deviceAddress) {
	String t_payload = "";
	String message = "";

	String t_topic = sensorID;
	t_topic += "/dallas_";
	t_topic += getAddress(deviceAddress);
	t_topic += "/temp";

	// Read sensor
	float tempC = sensors.getTempC(deviceAddress);

	if (isnan(tempC) || tempC == -127 || tempC == 85) { // -127 is an error code, 85 is init state
		message = "Failed to read from DS18B20 sensor ";
		message += getAddress(deviceAddress);
		DEBUG_PRINTLN(message);
		mqttclient.publish((char*) errorTopic.c_str(), (char*) message.c_str());
		return false;
	} else {
		// OK, send data
		t_payload += tempC;

		if (mqttclient.publish((char*) t_topic.c_str(),
				(char*) t_payload.c_str())) {
			Serial.println(
					"Published temperature " + t_topic + ": " + t_payload);
		} else {
			Serial.println(
					"Publish temperature failed for sensor "
					+ getAddress(deviceAddress));
		}
	}
	return true;
}

void generateMQTTClientName() {
	// Generate client name based on MAC address and last 8 bits of microsecond counter
	clientName = "esp8266-";
	uint8_t mac[6];
	WiFi.macAddress(mac);
	clientName += macToStr(mac);
	clientName += "-";
	clientName += String(micros() & 0xff, 16);
}

void generateSensorID() {
	sensorID = "ID_";
	uint8_t mac[6];
	WiFi.macAddress(mac);
	sensorID += macToStrShort(mac);
}

String macToStr(const uint8_t* mac) {
	String result;
	for (int i = 0; i < 6; ++i) {
		result += String(mac[i], 16);
		if (i < 5)
			result += ':';
	}
	return result;
}

String macToStrShort(const uint8_t* mac) {
	String result;
	for (int i = 0; i < 6; ++i) {
		if (mac[i] < 16) {
			result += "0";
		}
		result += String(mac[i], 16);
	}
	return result;
}

// function to print a device address
void printAddress(DeviceAddress deviceAddress) {
	for (uint8_t i = 0; i < 8; i++) {
		// zero pad the address if necessary
		if (deviceAddress[i] < 16)
			Serial.print("0");
		Serial.print(deviceAddress[i], HEX);
	}
}

String getAddress(DeviceAddress deviceAddress) {
	String address = "";
	for (uint8_t i = 0; i < 8; i++) {
		// zero pad the address if necessary
		if (deviceAddress[i] < 16) {
			address += "0";
		}
		address += String(deviceAddress[i], HEX);
	}
	return address;
}

// function to print the temperature for a device
void printTemperature(DeviceAddress deviceAddress) {
	float tempC = sensors.getTempC(deviceAddress);
	Serial.print("Temp C: ");
	Serial.print(tempC);
}

// function to print a device's resolution
void printResolution(DeviceAddress deviceAddress) {
	Serial.print("Resolution: ");
	Serial.print(sensors.getResolution(deviceAddress));
	Serial.println();
}

// main function to print information about a device
void printData(DeviceAddress deviceAddress) {
	Serial.print("Device Address: ");
	printAddress(deviceAddress);
	Serial.print(" ");
	printTemperature(deviceAddress);
	Serial.print(" ");
	printResolution(deviceAddress);
	Serial.println();

}

String getEEPROMString(int start, int len) {
	EEPROM.begin(512);
	delay(10);
	String string = "";
	for (int i = _eepromStart + start; i < _eepromStart + start + len; i++) {
		string += char(EEPROM.read(i));
	}
	EEPROM.end();
	return string;
}

void setEEPROMString(int start, int len, String string) {
	EEPROM.begin(512);
	delay(10);
	int si = 0;
	for (int i = _eepromStart + start; i < _eepromStart + start + len; i++) {
		char c;
		if (si < string.length()) {
			c = string[si];
		} else {
			c = 0;
		}
		EEPROM.write(i, c);
		si++;
	}
	EEPROM.end();
	Serial.println("Wrote EEPROM with value: '" + string + "'");
}

String getMQTTServerFromEEPROM() {
	String server = getEEPROMString(0, MQTT_SERVER_NAME_LENGTH);
	server.trim();
	Serial.println("read MQTT server address from EEPROM: '" + server + "'");
	return (char*) server.c_str();
}

void setMQTTServerToEEPROM(String server) {
	server.trim();
	Serial.println("saving MQTT server address to EEPROM: '" + server + "'");
	setEEPROMString(0, MQTT_SERVER_NAME_LENGTH, server);
}

//callback notifying us of the need to save config
void saveConfigCallback() {
	Serial.println("We should save the config");
	shouldSaveConfig = true;
}

void setup() {
	Serial.begin(115200);
	Serial.println();
	mqtt_server = getMQTTServerFromEEPROM();

	// set up WifiManager
	WiFiManager wifiManager;
	wifiManager.setSaveConfigCallback(saveConfigCallback);

	WiFiManagerParameter custom_mqtt_server = WiFiManagerParameter("server",
			"mqtt server", (char*) mqtt_server.c_str(),
			MQTT_SERVER_NAME_LENGTH);
	wifiManager.addParameter(&custom_mqtt_server);

	//reset settings - for testing
	//wifiManager.resetSettings();

	wifiManager.setTimeout(180);

	/****
	 * if MQTT server address is empty or trigger pin is low
	 * start configuration AP
	 */
	if (mqtt_server == "" || digitalRead(TRIGGER_PIN) == LOW) {
		wifiManager.startConfigPortal("ESPConfigAP");
	} else {
		if (!wifiManager.autoConnect("AutoConnectAP")) {
			Serial.println("failed to connect wifi and hit timeout");
			delay(3000);
			//reset and try again, or maybe put it to deep sleep
			ESP.reset();
			delay(5000);
		}
	}
	Serial.println();
	Serial.println("WiFi connected to SSID " + WiFi.SSID());

	// save parameter if needed
	if (shouldSaveConfig) {
		String server = custom_mqtt_server.getValue();
		Serial.println("got MQTT server value: '" + server + "'");
		setMQTTServerToEEPROM(server);
		mqtt_server = getMQTTServerFromEEPROM();
	}

	// set MQTT values
	generateMQTTClientName();
	generateSensorID();

	infoTopic = sensorID;
	infoTopic += "/Info";

	statusTopic = sensorID;
	statusTopic += "/Status";

	errorTopic = sensorID;
	errorTopic += "/Error";

	// set MQTT server
	mqttclient.setServer((char*) mqtt_server.c_str(), 1883);

	//********************************************************
	//  do the work
	//********************************************************

	Serial.println();
	DEBUG_PRINTLN("Set up DS18B20 Sensors");
	sensors.begin();
	sensors.setResolution(TEMP_12_BIT); // Genauigkeit auf 12-Bit setzen

	// array to hold device addresses
	DeviceAddress sensor[ MAX_NUMBER_OF_SENSORS];

	int activeSensors = 0;

	for (int i = 0; i < MAX_NUMBER_OF_SENSORS; i++) {
		if (!sensors.getAddress(sensor[i], i)) {
			Serial.print("Unable to find address for Device ");
			Serial.println(i, DEC);
		} else {
			activeSensors++;
			Serial.print("Device ");
			Serial.print(i, DEC);
			Serial.print(" Address: ");
			printAddress(sensor[i]);
			Serial.println();
		}
	}

	if (mqttConnect()) {
		mqttclient.publish((char*) infoTopic.c_str(),
				"Start reading DS18B20 sensors");

		uint8_t returncode = 0;
		String status = "";

		DEBUG_PRINTLN("Requesting temperatures...");
		sensors.requestTemperatures();

		for (int i = 0; i < activeSensors; i++) {
			DEBUG_PRINTLN("read sensor " + i);

			returncode |= !readDallasSensor(sensor[i]);
			if (i < 8) {
				returncode <<= 1;
			}
			printData(sensor[i]);
		}

		DEBUG_PRINTLN("read all available DS18B20 sensors");
		DEBUG_PRINT("return code is ");
		DEBUG_PRINTLN(returncode);

		status += returncode;
		mqttclient.publish((char*) statusTopic.c_str(), (char*) status.c_str());

	} else {
		Serial.println("Wifi or MQTT was not connected, did nothing");
	}

	//********************************************************
	//  end of work
	//********************************************************

	// go to sleep
	delay(100);
	DEBUG_PRINTLN("");
	DEBUG_PRINT(
			"go to sleep for "+String((SLEEPTIME - WIFI_CONNECT_TIME), DEC));
	DEBUG_PRINTLN("sec");
	ESP.deepSleep((SLEEPTIME - WIFI_CONNECT_TIME) * 1000000);
}

void loop() {
}
