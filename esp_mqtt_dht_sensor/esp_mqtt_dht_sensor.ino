#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <DHT.h>
#include "esp_mqtt_dht_sensor.h"

/*
 * add this into seperate header file
const char* ssid = "ssid";
const char* password = "password";
char* topic = "dht22";
char* server = "MQTT Broker address";
String clientName = "esp-sensor";
*/

#define DHTPIN 2 // what pin we're connected to
#define DHTTYPE DHT22 // DHT 11 
DHT dht(DHTPIN, DHTTYPE, 15);

WiFiClient wifiClient;
PubSubClient client(server, 1883, callback, wifiClient);

void callback(char* topic, byte* payload, unsigned int length) {
  // handle message arrived
}

void setup() {
  Serial.begin(115200);
  delay(10);
  dht.begin();
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.print("Connecting to ");
  Serial.print(server);
  Serial.print(" as ");
  Serial.println(clientName);
  if (client.connect((char*) clientName.c_str())) {
    Serial.println("Connected to MQTT broker");
    Serial.print("Topic is: ");
    Serial.println(topic);

  }
  else {
    Serial.println("MQTT connect failed");
    Serial.println("Will reset and try again...");
    abort();
  }
}

void loop() {

  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }


  static int counter = 0;
  String payload ;
  payload += t;
  payload += ":";
  payload += h;

  if (client.connected()) {
    Serial.print("Sending payload: ");
    Serial.println(payload);

    if (client.publish(topic, (char*) payload.c_str()))
    {
      Serial.println("Publish ok");
    }
    else {
      Serial.println("Publish failed");
    }

  }
  else {
    if (client.connect((char*) clientName.c_str())) {
      Serial.println("Connected to MQTT broker");
      Serial.print("Topic is: ");
      Serial.println(topic);
    }
  }
  ++counter;
  delay(15000);
}
