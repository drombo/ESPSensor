/*
 * add this into seperate header file
const char* ssid = "ssid";
const char* password = "password";
char* server = "MQTT Broker address";
*/

#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <DHT.h>
#include "esp_mqtt_dht_sensor.h"

#define DHTTYPE DHT22 // DHT 11
#define LOOPDELAY 30000 // 15s Pause zwischen den Messungen

/*
GPIO Pins
0 2 4 5 12 13 14 (15 - GND) (16 - RST)
*/

String clientName = "ESPSensor";
String sensorID = "ID_na";
String statusTopic="";

DHT dht_pin00(0, DHTTYPE, 15);
DHT dht_pin02(2, DHTTYPE, 15);
DHT dht_pin04(4, DHTTYPE, 15);
DHT dht_pin05(5, DHTTYPE, 15);
DHT dht_pin12(12, DHTTYPE, 15);
DHT dht_pin13(13, DHTTYPE, 15);
DHT dht_pin14(14, DHTTYPE, 15);

WiFiClient wifiClient;
PubSubClient mqttclient(server, 1883, wifiClient);

int DHTreadFailCounter = 0;

void mqttConnect() {
  int failCounter = 0;

  if (mqttclient.connected()) {
    Serial.println("Already connected with MQTT Server");
  } else {
    Serial.print("Connecting to MQTT Server ");
    Serial.print(server);
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

    Serial.println("Connected to MQTT broker");
    mqttclient.publish((char*) statusTopic.c_str(), "new connection from sensor");
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

  float h = dht.readHumidity();
  float t = dht.readTemperature();

  mqttConnect();

  // Test auf Fehler
  if (isnan(h) || isnan(t)) {
    Serial.print("Failed to read from DHT sensor at pin ");
    Serial.println(pin);

    String message = "Error: ";
    message += sensorID;
    message += ", Pin: ";
    message += pin;

    mqttclient.publish((char*) statusTopic.c_str(), (char*) message.c_str());
 
    return false;
  } else {
    // OK, send data
    t_payload += t;
    h_payload += h;

    if (mqttclient.publish( (char*) t_topic.c_str(), (char*) t_payload.c_str())) {
      Serial.print("Published temperature ");
      Serial.print(t_topic);
      Serial.print(": ");
      Serial.println(t_payload);
    } else {
      Serial.print("Publish temperature failed for pin ");
      Serial.println(pin);

      return false;
    }

    if (mqttclient.publish((char*) h_topic.c_str(), (char*) h_payload.c_str()))
    {
      Serial.print("Published humidity: ");
      Serial.print(h_topic);
      Serial.print(": ");
      Serial.println(h_payload);
    } else {
      Serial.print("Publish humidity failed for pin ");
      Serial.println(pin);

      return false;
    }
  }
}

void wifiConnect() {
  Serial.print("WiFi Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  Serial.print(".");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
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

void setup() {
  Serial.begin(115200);
  delay(10);

  Serial.println();
  Serial.println();

  wifiConnect();

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
  mqttclient.publish((char*) statusTopic.c_str(), "Start new measurement");

  readDHTSensor(dht_pin00, 0);
  readDHTSensor(dht_pin02, 2);
  readDHTSensor(dht_pin04, 4);
  readDHTSensor(dht_pin05, 5);
  readDHTSensor(dht_pin12, 12);
  readDHTSensor(dht_pin13, 13);
  readDHTSensor(dht_pin14, 14);

  delay(LOOPDELAY);
}

