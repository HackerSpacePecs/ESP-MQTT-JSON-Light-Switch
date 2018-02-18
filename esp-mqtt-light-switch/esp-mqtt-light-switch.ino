#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>


/************ WIFI and MQTT INFORMATION (CHANGE THESE FOR YOUR SETUP) ******************/
#define wifi_ssid "SSID"
#define wifi_password "password"
#define mqtt_server "192.168.1.1"
#define mqtt_user "homeassistant"
#define mqtt_password "HTTPAPIPassword"
#define mqtt_port 1883

/************* MQTT TOPICS (change these topics as you wish)  **************************/

#define lightTopic "angie/living-room-light-switch"
#define commandTopic "angie/living-room-light-switch/set"

const char* ON = "ON";
const char* OFF = "OFF";
const char* previousState = OFF;
const char* previousHWState = OFF;
unsigned long previousMillis = 0;
unsigned long interval = 500; // Half second
boolean isStateChanged = false;

/**************************** FOR OTA **************************************************/
#define SENSORNAME "esp8266"
#define OTApassword "password" // change this to whatever password you want to use when you upload OTA
int OTAport = 8266;

/**************************** PIN DEFINITIONS ********************************************/
#define INPUT_PIN 3
#define OUTPUT_PIN 2

/**************************** SENSOR DEFINITIONS *******************************************/
#define MQTT_MAX_PACKET_SIZE 512
const int BUFFER_SIZE = 300;

WiFiClient espClient;
PubSubClient client(espClient);

/********************************** START SETUP*****************************************/
void setup() {

  pinMode(INPUT_PIN, INPUT);
  pinMode(OUTPUT_PIN, OUTPUT);
  Serial.begin(115200,SERIAL_8N1,SERIAL_TX_ONLY);
  
  delay(10);

  ArduinoOTA.setPort(OTAport);
  ArduinoOTA.setHostname(SENSORNAME);
  ArduinoOTA.setPassword((const char *)OTApassword);

  Serial.print("calibrating sensor ");
  for (int i = 0; i < 0; i++) {
    Serial.print(".");
    delay(1000);
  }

  Serial.println("Starting Node named: " + String(SENSORNAME));
  setup_wifi();

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  ArduinoOTA.onStart([]() {
    Serial.println("Starting");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  reconnect();
}


/********************************** START SETUP WIFI*****************************************/
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected, IP address:");
  Serial.println(WiFi.localIP());
}


/********************************** START CALLBACK*****************************************/

  /*
  SAMPLE PAYLOAD:
    {
      "state": "ON"
    }
  */
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.println("]:");

  char message[length + 1];
  for (int i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }
  message[length] = '\0';
  Serial.println(message);

  if (!processJson(message)) {
    return;
  }
  
  Serial.println("Message processed.");
}

/********************************** START PROCESS JSON*****************************************/
bool processJson(char* message) {
  StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;

  JsonObject& root = jsonBuffer.parseObject(message);

  if (!root.success()) {
    Serial.println("parseObject() failed");
    return false;
  }

  if (root.containsKey("state")) {
    if (strcmp(root["state"], ON) == 0) {
      sendState(ON);
    }
    else if (strcmp(root["state"], OFF) == 0) {
      sendState(OFF);
    }
  }

  return true;
}

/********************************** START SEND STATE*****************************************/
void sendState(const char* message) {
  StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["state"] = message;
  previousState = message;
  char buffer[root.measureLength() + 1];
  root.printTo(buffer, sizeof(buffer));
  Serial.print("Publishing: ");
  Serial.println(buffer);
  client.publish(lightTopic, buffer, true);
  if(message == ON){
     digitalWrite(OUTPUT_PIN, digitalRead(INPUT_PIN) == HIGH ? LOW : HIGH);  
  }
  else {
     digitalWrite(OUTPUT_PIN, digitalRead(INPUT_PIN));  
  }
  
}

/********************************** START RECONNECT*****************************************/
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(SENSORNAME, mqtt_user, mqtt_password)) {
      Serial.println("connected");
      client.subscribe(commandTopic);
      previousHWState = digitalRead(INPUT_PIN) == HIGH ? ON : OFF;
      sendState(previousHWState);
    }
    else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

/********************************** START MAIN LOOP***************************************/
void loop() { 
  ArduinoOTA.handle();
    
  if (!client.connected()) {
    software_Reset();
  }
  client.loop();

  const char * currentState = digitalRead(INPUT_PIN) == HIGH ? ON : OFF;  
  if(previousHWState != currentState){
    sendState(previousState == ON ? OFF : ON); 
    previousHWState = currentState;
  }
}
/****reset***/
void software_Reset(){ // Restarts program from beginning but does not reset the peripherals and registers
  Serial.print("MQTT connection lost, resetting");
  ESP.reset(); 
}
