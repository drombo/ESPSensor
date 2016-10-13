#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <DHT.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <EEPROM.h>     

#define DHTTYPE DHT22 // DHT 11
#define SLEEPTIME 30 // Sleep Time in s
#define WIFI_CONNECT_TIME 6 // time to connect to wifi in s
#define MQTT_SERVER_NAME_LENGTH 40 // max length for server names
#define TRIGGER_PIN 14  // set pin to LOW for config AP

/*
GPIO Pins
0 2 4 5 12 13 14 (15 - GND) (16 - RST)
*/

String clientName = "ESPSensor";
String sensorID = "ID_na";
String infoTopic="";
String statusTopic="";
String errorTopic="";
String mqtt_server = "";
int _eepromStart;
//flag for saving data
bool shouldSaveConfig = false;

DHT dht_pin00(0, DHTTYPE, 15);
DHT dht_pin02(2, DHTTYPE, 15);
DHT dht_pin04(4, DHTTYPE, 15);
DHT dht_pin05(5, DHTTYPE, 15);
DHT dht_pin12(12, DHTTYPE, 15);
DHT dht_pin13(13, DHTTYPE, 15);
DHT dht_pin14(14, DHTTYPE, 15);

WiFiClient wifiClient;
PubSubClient mqttclient(wifiClient);

boolean mqttConnect() {
  int failCounter = 10;

  if (WiFi.status() == WL_CONNECTED) {
    if (mqttclient.connected()) {
      Serial.println("Already connected with MQTT Server");
    } else {
      Serial.println("Connecting to MQTT server " + mqtt_server + " as " + clientName);
  
      while (! mqttclient.connect((char*) clientName.c_str()) && failCounter >= 0) {
        Serial.println("Connection to MQTT server failed with state: " + mqttclient.state());
        delay(1000);        
        failCounter--;
      }

      if (mqttclient.connected()) {
        Serial.println();
        Serial.println("Connected to MQTT broker on host " + mqtt_server);
      } else {
        return false;
      }
    }
  } else {
    return false;
  }
  return true;
}

boolean readDHTSensor(DHT dht, uint8_t pin) {
  String t_payload = "";
  String h_payload = "";
  String message = "";
  
  String t_topic = sensorID;
  t_topic +="/dht22_pin";
  t_topic += pin;
  t_topic += "/temp";

  String h_topic = sensorID;
  h_topic +="/dht22_pin";
  h_topic += pin;
  h_topic += "/humidity";

  // Read sensor
  float t = dht.readTemperature();
  float h = dht.readHumidity();
 
  // Error check
  if (isnan(t) || isnan(h)) {
    message = "Failed to read from DHT sensor at pin ";
    message += pin; 
    Serial.println(message);
    mqttclient.publish((char*) errorTopic.c_str(), (char*) message.c_str()); 
    return false;
  } else {
    // OK, send data
    t_payload += t;
    h_payload += h;

    if (mqttclient.publish( (char*) t_topic.c_str(), (char*) t_payload.c_str())) {
      Serial.println("Published temperature " + t_topic + ": " + t_payload);
    } else {
      Serial.println("Publish temperature failed for sensor at pin " + pin);
    }

    if (mqttclient.publish((char*) h_topic.c_str(), (char*) h_payload.c_str()))
    {
      Serial.println("Published humidity " + h_topic + ": " + h_payload);
    } else {
      Serial.println("Publish humidity failed for sensor at pin " + pin);
    }
  }
  return true;
}

void generateMQTTClientName(){
  // Generate client name based on MAC address and last 8 bits of microsecond counter
  clientName = "esp8266-";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientName += macToStr(mac);
  clientName += "-";
  clientName += String(micros() & 0xff, 16);
}

void generateSensorID(){
  sensorID = "ID_";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  sensorID += macToStrShort(mac);
}

String macToStr(const uint8_t* mac)
{
  String result;
  for (int i = 0; i < 6; ++i) {
    result += String(mac[i], 16);
    if (i < 5)
      result += ':';
  }
  return result;
}

String macToStrShort(const uint8_t* mac)
{
  String result;
  for (int i = 0; i < 6; ++i) {
    result += String(mac[i], 16);
  }
  return result;
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
void saveConfigCallback () {
  Serial.println("We should save the config");
  shouldSaveConfig = true;
}

void setup() {  
  Serial.begin(115200);
  Serial.println();
  mqtt_server = getMQTTServerFromEEPROM();
  
  WiFiManager wifiManager;
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  WiFiManagerParameter custom_mqtt_server = WiFiManagerParameter("server", "mqtt server", (char*) mqtt_server.c_str(), MQTT_SERVER_NAME_LENGTH);
  wifiManager.addParameter(&custom_mqtt_server);

  //reset settings - for testing
  //wifiManager.resetSettings();

  wifiManager.setTimeout(180);

  /* if MQTT server address is empty or trigger pin is low
   * start configuration AP
   */
  if(mqtt_server == "" || digitalRead(TRIGGER_PIN) == LOW) {
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
  Serial.println("WiFi connected to SSID "+WiFi.SSID());

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
  
  Serial.println();
  Serial.println("Set up DHT Sensors");

  dht_pin00.begin();
  dht_pin02.begin();
  dht_pin04.begin();
  dht_pin05.begin();
  dht_pin12.begin();
  dht_pin13.begin();
  dht_pin14.begin();

  if (mqttConnect()) {
    mqttclient.publish((char*) infoTopic.c_str(), "Start reading DHT sensors");
  
    uint8_t returncode = 0;  
    String status = "";
  
    returncode |= ! readDHTSensor(dht_pin00, 0);
    returncode <<= 1;
    returncode |= ! readDHTSensor(dht_pin02, 2);
    returncode <<= 1;
    returncode |= ! readDHTSensor(dht_pin04, 4);
    returncode <<= 1;
    returncode |= ! readDHTSensor(dht_pin05, 5);
    returncode <<= 1;
    returncode |= ! readDHTSensor(dht_pin12, 12);
    returncode <<= 1;
    returncode |= ! readDHTSensor(dht_pin13, 13);
    returncode <<= 1;
    returncode |= ! readDHTSensor(dht_pin14, 14);
  
    Serial.print("return code is ");
    Serial.println(returncode);
  
    status += returncode;   
    mqttclient.publish((char*) statusTopic.c_str(), (char*) status.c_str());
  } else {
    Serial.println("Wifi or MQTT was not connected, did nothing");
  }

  delay(100);
  ESP.deepSleep((SLEEPTIME - WIFI_CONNECT_TIME) * 1000000);
}

void loop() {
}

