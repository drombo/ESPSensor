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
    Serial.print(t_topic);
    Serial.print(" and ");
    Serial.println(h_topic);

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
  String t_payload;
  String h_payload;

  t_payload += t;
  h_payload += h;

  if (client.connected()) {
    Serial.print("Sending payload: ");
    Serial.println(t_payload);
    Serial.println(h_payload);

    if (client.publish(t_topic, (char*) t_payload.c_str()))
    {
      Serial.println("Publish temp ok");
    }
    else {
      Serial.println("Publish temp failed");
    }

    if (client.publish(h_topic, (char*) h_payload.c_str()))
    {
      Serial.println("Publish humi ok");
    }
    else {
      Serial.println("Publish humi failed");
    }
  }
  else {
    if (client.connect((char*) clientName.c_str())) {
      Serial.println("Connected to MQTT broker");
      Serial.print("Topic is: ");
      Serial.print(t_topic);
      Serial.print(" and ");
      Serial.println(h_topic);
    }
  }
  ++counter;
  delay(15000);
}
