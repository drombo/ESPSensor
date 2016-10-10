#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <DHT.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <EEPROM.h>     

#define DHTTYPE DHT22 // DHT 11
#define LOOPDELAY 30000 // 30s Pause zwischen den Messungen

/*
GPIO Pins
0 2 4 5 12 13 14 (15 - GND) (16 - RST)
*/

String clientName = "ESPSensor";
String sensorID = "ID_na";
String statusTopic="";
String mqtt_server = "";
int _eepromStart;


DHT dht_pin00(0, DHTTYPE, 15);
DHT dht_pin02(2, DHTTYPE, 15);
DHT dht_pin04(4, DHTTYPE, 15);
DHT dht_pin05(5, DHTTYPE, 15);
DHT dht_pin12(12, DHTTYPE, 15);
DHT dht_pin13(13, DHTTYPE, 15);
DHT dht_pin14(14, DHTTYPE, 15);

WiFiClient wifiClient;
PubSubClient mqttclient(wifiClient);

int DHTreadFailCounter = 0;

WiFiManagerParameter custom_mqtt_server("server", "mqtt server", (char*) mqtt_server.c_str(), 15);

//flag for saving data
//bool shouldSaveConfig = false;

void mqttConnect() {
  int failCounter = 0;

  if (mqttclient.connected()) {
    Serial.println("Already connected with MQTT Server");
  } else {
    Serial.print("Connecting to MQTT Server ");
    Serial.print(mqtt_server);
    Serial.print(" as ");
    Serial.println(clientName);

    while (! mqttclient.connect((char*) clientName.c_str())) {
      Serial.print(".");
      delay(1000);

      failCounter++;

      if (failCounter > 60) {
        ESP.reset();
      }
    }
    Serial.println();
    Serial.println("Connected to MQTT broker");
    //mqttclient.publish((char*) statusTopic.c_str(), "- new connection from sensor");
  }
}

boolean readDHTSensor(DHT dht, uint8_t pin) {
  String t_payload = "";
  String h_payload = "";
  
  String t_topic = sensorID;
  t_topic +="/dht22_pin";
  t_topic += pin;
  t_topic += "/temp";

  String h_topic = sensorID;
  h_topic +="/dht22_pin";
  h_topic += pin;
  h_topic += "/humidity";

  mqttConnect();

  // Temperature
  float t = dht.readTemperature();
 
  // Error check
  if (isnan(t)) {
    String message = "Error: Failed to read temperature from DHT sensor at pin ";
    message += pin;

    Serial.println(message);
    mqttclient.publish((char*) statusTopic.c_str(), (char*) message.c_str());
 
  } else {
    // OK, send data
    t_payload += t;

    if (mqttclient.publish( (char*) t_topic.c_str(), (char*) t_payload.c_str())) {
      Serial.print("Published temperature ");
      Serial.print(t_topic);
      Serial.print(": ");
      Serial.println(t_payload);
    } else {
      Serial.print("Publish temperature failed for sensor at pin ");
      Serial.println(pin);
    }
  }

  // Humidity
  float h = dht.readHumidity();
  // Error check
  if (isnan(h)) {
    String message = "Error: Failed to read humidity from DHT sensor at pin ";
    message += pin;

    Serial.println(message);
    mqttclient.publish((char*) statusTopic.c_str(), (char*) message.c_str());
 
   } else {
    // OK, send data
    h_payload += h;

    if (mqttclient.publish((char*) h_topic.c_str(), (char*) h_payload.c_str()))
    {
      Serial.print("Published humidity: ");
      Serial.print(h_topic);
      Serial.print(": ");
      Serial.println(h_payload);
    } else {
      Serial.print("Publish humidity failed for sensor at pin ");
      Serial.println(pin);
    }
  }
}


void wifiConnect() {
  WiFiManager wifiManager;
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.addParameter(&custom_mqtt_server);

  /* check if server address starts with a number 
   * (at the moment only ip adresses possible)
   * otherwise start configuration AP
   */
  if(mqtt_server[0] < 48 || mqtt_server[0] > 57) {
    wifiManager.startConfigPortal("OnDemandAP");
  } else {
    wifiManager.autoConnect("AutoConnectAP");
  }
  Serial.println("WiFi connected");
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
  Serial.println("Wrote EEPROM " + string);
}


String getMQTTServer() {
  if (mqtt_server == "") {
    mqtt_server = getEEPROMString(0, 15);
  }

  Serial.println("read MQTT server address from EEPROM: " + mqtt_server);
  
  return (char*) mqtt_server.c_str();
}

void setMQTTServer(String server) {
  Serial.println("saving MQTT server address to EEPROM: " + server);
  
  mqtt_server = server;
  setEEPROMString(0, 15, mqtt_server);
}


//callback notifying us of the need to save config
void saveConfigCallback () {
  char server[15];
  strcpy(server, custom_mqtt_server.getValue());

  Serial.print("save MQTT server '");
  Serial.print(server);
  Serial.println("'");
  
//  shouldSaveConfig = true;
  setMQTTServer(server);
}

void setup() {
  Serial.begin(115200);
  delay(10);

  Serial.println();
  Serial.println();

  mqtt_server = getMQTTServer();

  wifiConnect();

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

  generateMQTTClientName();
  generateSensorID();
  
  statusTopic = sensorID;
  statusTopic += "/Status";

  mqttConnect();
}

void loop() {
  mqttConnect();
  mqttclient.publish((char*) statusTopic.c_str(), "- Start reading DHT sensors");

  readDHTSensor(dht_pin00, 0);
  readDHTSensor(dht_pin02, 2);
  readDHTSensor(dht_pin04, 4);
  readDHTSensor(dht_pin05, 5);
  readDHTSensor(dht_pin12, 12);
  readDHTSensor(dht_pin13, 13);
  readDHTSensor(dht_pin14, 14);

  delay(LOOPDELAY);
}

