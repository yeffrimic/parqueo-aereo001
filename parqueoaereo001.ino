
//-------- Required Libraries --------//
#include <ESP8266WiFi.h>
#include <PubSubClient.h> // https://github.com/knolleary/pubsubclient/releases/tag/v2.3
#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson/releases/tag/v5.0.7
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <TimeLib.h> // TimeTracking
#include <WiFiUdp.h> // UDP packet handling for NTP request

//-------- ntp setup --------//
//NTP Servers:
IPAddress timeServer(129, 6, 15, 28); // time.nist.gov NTP server
const char* ntpServerName = "time.nist.gov";

const int timeZone = -6;  // Eastern central Time (USA)

WiFiUDP Udp;
unsigned int localPort = 2390;  // local port to listen for UDP packets

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

String ISO8601;


//-------- IBM Bluemix Info ---------//
#define ORG "qnu3pg"
#define DEVICE_TYPE "pruebayeffri"
#define DEVICE_ID "ParqueoAereo001"
#define TOKEN "23lcKp_f1KRqN@RinG"
//-------- Customise the above values --------//

char server[] = ORG ".messaging.internetofthings.ibmcloud.com";
char authMethod[] = "use-token-auth";
char token[] = TOKEN;
char clientId[] = "d:" ORG ":" DEVICE_TYPE ":" DEVICE_ID;

//-------- Topics to suscribe --------//
const char publishTopic[] = "iot-2/evt/status/fmt/json";
const char conectionStatusTopic[] = "iot-2/evt/conection/fmt/json";
const char StateTopic[] = "iot-2/evt/state/fmt/json";
const char responseTopic[] = "iotdm-1/response";
const char manageTopic[] = "iotdevice-1/mgmt/manage";
const char updateTopic[] = "iotdm-1/device/update";
const char rebootTopic[] = "iotdm-1/mgmt/initiate/device/reboot";

//--------Define the specifications of your device --------//
#define SERIALNUMBER "ESP0001"
#define MANUFACTURER "flatbox"
#define MODEL "ESP8266"
#define DEVICECLASS "Wifi"
#define DESCRIPTION "Wifinode"
int FWVERSION = 1;
#define HWVERSION "ESPTOY1.22"
#define DESCRIPTIVELOCATION "CAMPUSTEC"
String myName = String(ESP.getChipId());


//-------- MQTT information --------//
WiFiClient wifiClient;
PubSubClient client(server, 1883, callback, wifiClient);


//-------- Ultrasonic sensor pins and variables --------//
int iPinTrigger = 5; //D1
int iPinEcho =  4; //D2
String StateParking = "Desocupado";


//-------- Global variables --------//
int publishInterval = 10000; // 30 seconds
long lastPublishMillis;
long lastpub;
long lastMsg = 0;
int event = 0;
boolean NTP = false, a, b, c = true;
int comparative;


//-------- HandleUpdate function receive the data, parse and update the info --------//
void handleUpdate(byte* payload) {
  StaticJsonBuffer<1024> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject((char*)payload);
  if (!root.success()) {
    Serial.println(F("handleUpdate: payload parse FAILED"));
    return;
  }
  Serial.println(F("handleUpdate payload:"));
  root.prettyPrintTo(Serial);
  Serial.println();
  JsonObject& d = root["d"];
  JsonArray& fields = d["fields"];
  for (JsonArray::iterator it = fields.begin();
       it != fields.end();
       ++it) {
    JsonObject& field = *it;
    const char* fieldName = field["field"];
    if (strcmp (fieldName, "metadata") == 0) {
      JsonObject& fieldValue = field["value"];
      if (fieldValue.containsKey("myName")) {
        const char* nodeID = "";
        nodeID = fieldValue["myName"];
        myName = String(nodeID);
        Serial.print(F("myName:"));
        Serial.println(myName);
      }
    }
    if (strcmp (fieldName, "deviceInfo") == 0) {
      JsonObject& fieldValue = field["value"];
      if (fieldValue.containsKey("fwVersion")) {
        FWVERSION = fieldValue["fwVersion"];
        Serial.print(F("fwVersion:"));
        Serial.println(FWVERSION);
      }
    }
  }
}

//-------- Callback function. Receive the payload from MQTT topic and translate it to handle  --------//

void callback(char* topic, byte* payload, unsigned int payloadLength) {
  Serial.print(F("callback invoked for topic: "));
  Serial.println(topic);
  if (strcmp (responseTopic, topic) == 0) {
    return; // just print of response for now
  }

  if (strcmp (rebootTopic, topic) == 0) {
    Serial.println(F("Rebooting..."));
    ESP.restart();
  }

  if (strcmp (updateTopic, topic) == 0) {
    handleUpdate(payload);
  }
}

//-------- mqttConnect fuunction. Connect to mqtt Server  --------//
void mqttConnect() {
  if (!!!client.connected()) {
    Serial.print(F("Reconnecting MQTT client to "));
    Serial.println(server);
    while (!!!client.connect(clientId, authMethod, token)) {
      Serial.print(F("."));
      delay(500);
    }
    Serial.println();
  }
}

//-------- initManageDevice function. suscribe topics, Send metadata and supports to bluemix --------//
void initManagedDevice() {
  if (client.subscribe("iotdm-1/response")) {
    Serial.println(F("subscribe to responses OK"));
  }
  else {
    Serial.println(F("subscribe to responses FAILED"));
  }

  if (client.subscribe(rebootTopic)) {
    Serial.println(F("subscribe to reboot OK"));
  }
  else {
    Serial.println(F("subscribe to reboot FAILED"));
  }

  if (client.subscribe("iotdm-1/device/update")) {
    Serial.println(F("subscribe to update OK"));
  }
  else {
    Serial.println(F("subscribe to update FAILED"));
  }
  int a = 0;
  if (a == 0) {
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    JsonObject& d = root.createNestedObject("d");
    JsonObject& supports = d.createNestedObject("supports");
    supports["deviceActions"] = true;
    char buff[100];
    root.printTo(buff, sizeof(buff));
    Serial.println(F("publishing device metadata:"));
    Serial.println(buff);

    if (client.publish(manageTopic, buff)) {
      Serial.println(F("device Publish ok"));
    }

    else {
      Serial.print(F("device Publish failed:"));
    }
    a = 1;
  }
  if (a == 1) {
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    JsonObject& d = root.createNestedObject("d");
    JsonObject& metadata = d.createNestedObject("metadata");
    metadata["myName"] = myName;
    char buff[100];
    root.printTo(buff, sizeof(buff));
    Serial.println(F("publishing device metadata:"));
    Serial.println(buff);

    if (client.publish(manageTopic, buff)) {
      Serial.println(F("device Publish ok"));
    }

    else {
      Serial.print(F("device Publish failed:"));
    }
    a = 2;
  }
}
//---------- timestamp functions-------//
// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

time_t getNtpTime()
{
  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println(F("Transmit NTP Request"));
  sendNTPpacket(timeServer);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {

      Serial.println(F("Receive NTP Response"));
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      NTP = true;
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println(F("No NTP Response :-("));
  return 0; // return 0 if unable to get the time
}

void udpConnect() {
  Serial.println(F("Starting UDP"));
  Udp.begin(localPort);
  Serial.print(F("Local port: "));
  Serial.println(Udp.localPort());
  Serial.println(F("waiting for sync"));
  setSyncProvider(getNtpTime);
}


void ISO8601TimeStampDisplay() {
  ISO8601 = String (year(), DEC);
  if (month() < 10)
  {
    ISO8601 += "0";
  }
  ISO8601 += month();
  if (day() < 10)
  {
    ISO8601 += "0";
  }
  ISO8601 += day();
  ISO8601 += " ";
  if (hour() < 10)
  {
    ISO8601 += "0";
  }
  ISO8601 += hour();
  ISO8601 += ":";
  if (minute() < 10)
  {
    ISO8601 += "0";
  }
  ISO8601 += minute();
  ISO8601 += ":";
  if (second() < 10)
  {
    ISO8601 += "0";
  }
  ISO8601 += second();
  //  ISO8601 += "-06:00";
  Serial.println(ISO8601);

}

time_t prevDisplay = 0; // when the digital clock was displayed

void checkTime () {
  if (timeStatus() != timeNotSet) {
    if (now() != prevDisplay) { //update the display only if time has changed
      prevDisplay = now();
      ISO8601TimeStampDisplay();
    }
  }
}




//--------  anager function. Configure the wifi connection if not connect put in mode AP--------//
boolean wifimanager() {

  WiFiManager wifiManager;
  Serial.println(F("empezando"));
  while (!  wifiManager.autoConnect("flatwifi")) {
    Serial.println(F("error no conecto"));

    if (!wifiManager.startConfigPortal("FlatWifi")) {
      Serial.println(F("failed to connect and hit timeout"));

      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(5000);
    }
    Serial.print(F("."));
  }
  //if you get here you have connected to the WiFi
  return true;
  Serial.println(F("connected...yeey :)"));

}

//-------- publishData function. Publish the data to MQTT server, the payload should not be bigger than 45 characters name field and data field counts. --------//

void publishData() {
  checkTime();
  if (!client.loop()) {
    mqttConnect();
  }
  StaticJsonBuffer<200> jsonbuffer;
  JsonObject& root = jsonbuffer.createObject();
  JsonObject& d = root.createNestedObject("d");
  JsonObject& data = d.createNestedObject("data");
  data["id"] = myName;
  data["state"] = StateParking;
  data["time"] = ISO8601;
  data["event"] = event;
  char payload[100];
  root.printTo(payload, sizeof(payload));

  Serial.print(F("Sending payload: "));
  Serial.println(payload);
  if (client.publish(publishTopic, payload, byte(sizeof(payload)))) {
    Serial.println("Publish OK");
  }
  else {
    Serial.println(F("Publish FAILED"));
    delay(1000);
    publishData();
  }
}


/* MQTTPublishStates()
 * publish the the words that receive in json format
 *
 *
 *
 */

void MQTTPublishStates(String data1, String text) {

  StaticJsonBuffer<1024> jsonbuffer;
  JsonObject& root = jsonbuffer.createObject();
  JsonObject& d = root.createNestedObject("d");
  JsonObject& data = d.createNestedObject("data");
  data["id"] = myName;
  data[data1] = text;
  char payload[200];
  root.printTo(payload, sizeof(payload));

  Serial.print(F("Sending payload: "));
  Serial.println(payload);
  if (client.publish(StateTopic, payload, byte(sizeof(payload)))) {
    Serial.println(F("Publish OK"));
  }
  else {
    Serial.println("Publish FAILED");
  }
  delay(1000);

}




//-------- setup  --------//

void setup() {
  Serial.begin(115200);
  Serial.println();

  pinMode(iPinTrigger, OUTPUT);
  pinMode(iPinEcho, INPUT);
  wifimanager();
  while (NTP == false) {
    udpConnect ();
  }
  mqttConnect();
  MQTTPublishStates("online", "conectado");
  // delay(15000);
  initManagedDevice();
  measureAverage();

}
long timenow;

void loop() {
  if (ultrasonicsensor() < comparative - 30) {
    a = true;
  } else {
    a = false;
  }

  if (a != b) {
    lastpub = millis();
    b = a;
  }
  if (timenow - lastpub > publishInterval) {
    if (c != b) {
      if (b) {
        StateParking = "Ocupado";
      } else {
        StateParking = "Desocupado";
      }
      lastpub = millis();
      publishData();
      if(!b){
        event++;
      }
      c=b;
    }

  } else {
    timenow = millis();
  }
  delay(1000);
}

void measureAverage() {
  for (int i = 0; i < 10; i++) {
    comparative += ultrasonicsensor();
  }
  comparative /= 10;
  MQTTPublishStates("average", String(comparative));
}

int ultrasonicsensor() {

  digitalWrite(iPinTrigger, LOW);
  digitalWrite(iPinTrigger, HIGH);
  delayMicroseconds(10);
  digitalWrite(iPinTrigger, LOW);


  //Read the data from the sensor
  double fTime = pulseIn(iPinEcho, HIGH);
  //convert to seconds
  fTime = fTime / 1000000;
  double fSpeed = 347.867;
  //Get the distance in meters
  double fDistance = fSpeed * fTime;
  //Convert to centimeters;
  fDistance = fDistance * 100;
  //Get one way distance
  fDistance = fDistance / 2;

  Serial.print(fDistance);
  Serial.println(F("cm"));
  delay(100);
  return fDistance;
}
