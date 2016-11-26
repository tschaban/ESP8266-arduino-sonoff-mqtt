/* Application
  LICENCE: http://opensource.org/licenses/MIT
  2016-11-18 tschaban https://github.com/tschaban */

#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include "Streaming.h"
#include <Ticker.h>

#define RELAY 12
#define LED 13
#define BUTTON 0

/* Configuration parameters */
const int ID = ESP.getChipId();                 // Device ID

const char* WIFI_SSID = "<WiFi>";               // WiFi Name
const char* WIFI_PASSWORD = "<Password>";       // WiFi Password

const char* MQTT_HOST = "<MQTT host>";          // MQTT Broker Host
const int   MQTT_PORT = 1883;                   // MQTT Port
const char* MQTT_USER = "<MQTT user>";          // MQTT User
const char* MQTT_PASSWORD = "<MQTT Password>";  // MQTT Password
const char* MQTT_TOPIC = "/sonoff/switch/";     // MQTT Topic

const int   CONNECTION_WAIT_TIME = 100;         // How long ESP8266 should wait before next attempt to connect to WiFi or MQTT Broker

char  mqttTopic[26];
Ticker btn_timer;
unsigned long pressedCount = 0;

WiFiClient esp;
PubSubClient client(esp);

void setup() {
  Serial.begin(115200);
  delay(10);
  client.setServer(MQTT_HOST, MQTT_PORT);
  client.setCallback(callbackMQTT);
  Serial.println();
  Serial << "---------------------------------------------------------------" << endl;
  Serial << " Initialization" << endl;
  pinMode(RELAY, OUTPUT);
  digitalWrite(RELAY, LOW);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);
  pinMode(BUTTON, INPUT_PULLUP);

  Serial << " Device ID: " << ID << endl;

  sprintf(mqttTopic, "%s%i", MQTT_TOPIC, ID);
  Serial << " Topic: " << mqttTopic << endl << endl;

  connectToWiFi();

  btn_timer.attach(0.05, button);
}

/* Blink LED, t defines for how long LED should be ON */
void blinkLED(int t=50) {
  if (digitalRead(LED)==HIGH) {
    digitalWrite(LED, LOW);
  }
  delay(t);
  digitalWrite(LED, HIGH);
}

/* Publishing state of the Relay to MQTT Broker */
void publishMessage() {
  char  tempString[33];
  sprintf(tempString,"%s/state", mqttTopic);
  Serial << " Publishing: ";

  if (digitalRead(RELAY)==LOW) {
    client.publish(tempString, "OFF");
    Serial << "OFF";
  } else {
      client.publish(tempString, "ON");
      Serial << "ON";
  }
  Serial << " to: " << tempString << ", completed" << endl;
  blinkLED();
}

/* Gets default value for the Relay. It should be implemented in your servce */
void getDefault() {
  char  tempString[31];
  sprintf(tempString,"%s/get", mqttTopic);
  Serial << endl << " Requesting default value";
  client.publish(tempString, "defaultState");
  Serial << ", completed" << endl;
  blinkLED();
  Serial << "---------------------------------------------------------------" << endl;
}

/* Set relay to ON */
void setON() {
  digitalWrite(RELAY, HIGH);
  publishMessage();
}

/* Set relay to OFF */
void setOFF() {
  digitalWrite(RELAY, LOW);
  publishMessage();
}

/* Connect to WiFI */
void connectToWiFi() {
  Serial << " Connecting to WiFI";
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    blinkLED(CONNECTION_WAIT_TIME/2);
    delay(CONNECTION_WAIT_TIME/2);
    Serial << ".";
  }
  Serial.println();
  Serial << " - Connected to WiFi: " << WIFI_SSID << endl;
  Serial << " - IP address: " << WiFi.localIP()  << endl;
}

/* Connected to MQTT Broker */
void connectToMQTT() {
  char  tempString[33];
  sprintf(tempString,"Sonoff (ID: %i)",ID);
  while (!client.connected()) {
    Serial << endl << " Connecting to MQTT Broker";
    if (client.connect(tempString, MQTT_USER, MQTT_PASSWORD)) {
        Serial.println();
        Serial << " - Connected to MQTT Broker: " << MQTT_HOST << ":" << MQTT_PORT << endl;
        sprintf(tempString,"%s/cmd", mqttTopic);
        client.subscribe(tempString);
        getDefault();
    } else {
      Serial << ".";
      blinkLED(CONNECTION_WAIT_TIME/2);
      delay(CONNECTION_WAIT_TIME/2);
    }
  }
}

/* Callback of MQTT Broker, it listens for messages */
void callbackMQTT(char* topic, byte* payload, unsigned int length) {
  blinkLED();
  Serial << " Message arrived " <<  topic << endl;
  if (length>=6) {
    if((char)payload[5] == 'N') { // got turnON
      setON();
    } else if((char)payload[5] == 'F') { // got turnOFF
      setOFF();
    }  else if((char)payload[0] == 'r') { // got reportState
      publishMessage();
    }
  }
}

/* Button pressed method. Short changes relay state, long reboot device */
void button() {
  if (!digitalRead(BUTTON)) {
    pressedCount++;
  }
  else {
    if (pressedCount > 1 && pressedCount <= 50) {
      blinkLED();
      if (digitalRead(RELAY)==LOW) {
          setON();
      } else {
          setOFF();
      }
    }
    else if (pressedCount >50){
      Serial << endl << " Restarting...." << endl;
      ESP.restart();
    }
  pressedCount = 0;
  }
}


void loop() {
  if (!client.connected()) {
    connectToMQTT();
  }
  client.loop();
}