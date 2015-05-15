/*

Code fuer die DS18B20 Sensoren von hier: http://blog.wenzlaff.de/?p=1254
Dank an Thomas Wenzlaff

Temperature Sensor DS18B20 an Digitalen Port Pin 2 wie folgt verbunden
Links=Masse,
Mitte=Data,
Rechts=+5V,
3300 to 4700 Ohm Widerstand von +5V nach Data.
*/

/*
 * add this into seperate header file
const char* ssid = "ssid";
const char* password = "password";
char* topic = "dht22";
char* server = "MQTT Broker address";
String clientName = "esp-sensor";
*/

#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "esp_mqtt_dht_sensor.h"

#define ONE_WIRE_PIN 4 /* Digitalport Pin 4 definieren */
#define DHTPIN 2 // what pin we're connected to
#define DHTTYPE DHT22 // DHT 11
#define LOOPDELAY 15000 // 15s Pause zwischen den Messungen

/*
GPIO Pins
0 2 4 5 12 13 14 (15 - GND) (16 - RST)
*/

//int possiblePins[] = {0, 2, 4, 5, 12, 13, 14};
//DHT dht_sensors[7];

String clientName = "ESPSensor";


// initialisiere Sensoren
OneWire ourWire(ONE_WIRE_PIN); /* Ini oneWire instance */
DallasTemperature sensors(&ourWire);/* Dallas Temperature Library für Nutzung der oneWire Library vorbereiten */

DHT dht(DHTPIN, DHTTYPE, 15);
DHT dht2(5, DHTTYPE, 15);

WiFiClient wifiClient;
PubSubClient mqttclient(server, 1883, callback, wifiClient);

int DHTreadFailCounter = 0;

void callback(char* topic, byte* payload, unsigned int length) {
  // handle message arrived, not used yet
}

void setup() {
  Serial.begin(115200);
  delay(10);

  Serial.println();
  Serial.println();

  wifiConnect();

  Serial.println();
  Serial.println("Set up DHT Sensors");

  dht.begin();
  dht2.begin();

  /*
    Serial.println("Set up DS Sensor");
    sensors.begin();
    sensors.setResolution(TEMP_12_BIT); // Genauigkeit auf 12-Bit setzen
    adresseAusgeben(); // Adresse der Devices ausgeben
  */

  generateMQTTClientName();

  mqttConnect();
}

void loop() {
  mqttConnect();

  mqttclient.publish("Haus/Keller/Status", "Starte neue Messung");

  while (! readDHTSensor(dht, t_topic, h_topic)) {
//  while (! readDHTSensor(dht, "Haus/Test/Temperatur", "Haus/Test/Luftfeuchte")) {
    DHTreadFailCounter++;
    delay(2000);
    if (DHTreadFailCounter > 20) {
      ESP.reset();
    }
  }

  DHTreadFailCounter = 0;

  while (! readDHTSensor(dht2, t_topic2, h_topic2)) {
    DHTreadFailCounter++;
    delay(2000);
    if (DHTreadFailCounter > 20) {
      ESP.reset();
    }
  }

  /*
    sensors.requestTemperatures(); // Temp abfragen

    Serial.print(sensors.getTempCByIndex(0) );
    Serial.print(" Grad Celsius 0 ");

    Serial.print(sensors.getTempCByIndex(1) );
    Serial.print(" Grad Celsius 1 ");

    Serial.print(sensors.getTempCByIndex(2) );
    Serial.print(" Grad Celsius 2 ");
    Serial.println();
  */
  delay(LOOPDELAY);
}

boolean readDHTSensor(DHT dht, char* t_topic, char* h_topic) {
  String t_payload = "";
  String h_payload = "";

  float h = dht.readHumidity();
  float t = dht.readTemperature();

  // Test auf Fehler
  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    String message = "DHT Sensor auslesen fehlgeschlagen (";
    message += t_topic;
    message += " oder ";
    message += h_topic;
    message += ")";
    
    if (! mqttclient.publish("Haus/Keller/Status", (char*) message.c_str())) {
      return false;
    }

    return false;
  } else {
    // Alles OK, sende Daten
    t_payload += t;
    h_payload += h;

    Serial.print("Sending payload: ");
    Serial.print(t_payload);
    Serial.print(" / ");
    Serial.println(h_payload);

    if (mqttclient.publish(t_topic, (char*) t_payload.c_str())) {
      Serial.print("Published temperature ");
      Serial.print(t_topic);
      Serial.print(": ");
      Serial.println(t_payload);
    } else {
      Serial.println("Publish temperature failed");
      return false;
    }

    if (mqttclient.publish(h_topic, (char*) h_payload.c_str()))
    {
      Serial.print("Published humidity: ");
      Serial.print(h_topic);
      Serial.print(": ");
      Serial.println(h_payload);
    } else {
      Serial.println("Publish humidity failed");
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

void mqttConnect() {
  if (mqttclient.connected()) {
    Serial.print("Bereits mit MQTT Server verbunden");
  } else {
    Serial.print("Connecting to MQTT Server ");
    Serial.print(server);
    Serial.print(" as ");
    Serial.println(clientName);

    int failCounter = 0;

    while (! mqttclient.connect((char*) clientName.c_str())) {
      delay(500);
      Serial.print(".");
      failCounter++;

      if (failCounter > 120) {
        ESP.reset();
      }
    }

    failCounter = 0;

    Serial.println("Connected to MQTT broker");
    mqttclient.publish("Haus/Keller/Status", "Da bin ich");
  }
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

void adresseAusgeben(void) {
  byte i;
  byte present = 0;
  byte data[12];
  byte addr[8];

  Serial.print("Suche 1-Wire-Devices...\n\r");// "\n\r" is NewLine
  while (ourWire.search(addr)) {
    Serial.print("\n\r\n\r1-Wire-Device gefunden mit Adresse:\n\r");
    for ( i = 0; i < 8; i++) {
      Serial.print("0x");
      if (addr[i] < 16) {
        Serial.print('0');
      }
      Serial.print(addr[i], HEX);
      if (i < 7) {
        Serial.print(", ");
      }
    }
    if ( OneWire::crc8( addr, 7) != addr[7]) {
      Serial.print("CRC is not valid!\n\r");
      return;
    }
  }
  Serial.println();
  ourWire.reset_search();
  return;
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
