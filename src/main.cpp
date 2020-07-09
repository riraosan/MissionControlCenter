/* 
The MIT License (MIT)

Copyright (c) 2020 riraotech.com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#ifdef ESP32
  #include <FS.h>
  #include <SPIFFS.h>
  #include <WiFi.h>
  #include <AsyncTCP.h>
  #include <ESPmDNS.h>
  #include <ESP32Ticker.h>
  #define HOSTNAME "esp32"
  #define MONITOR_SPEED 115200

  const char *ap_ssid       = "ESP32-G-AP";
  const char *ap_password   = "room03601";

#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESPAsyncTCP.h>
  #include <ESP8266mDNS.h>
  #include <Ticker.h>
  #define HOSTNAME "esp8266"
  #define MONITOR_SPEED 74880

  const char *ap_ssid       = "ESP8266-G-AP";
  const char *ap_password   = "room03601";

#endif
#include <ESPAsyncWebServer.h>
#include <SPIFFSEditor.h>
#include <TelnetSpy.h>
#include <Servo.h>

const char *sta_ssid      = "Buffalo-G-FAA8";
const char *sta_password  = "34ywce7cffyup";

Ticker countDown;

// Set LED GPIO
const int ledPin = 5;
// Stores LED state
String ledState;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncEventSource events("/events");

static const int SERVO_NUM = 4;

Servo myservo;            

TelnetSpy SerialAndTelnet;

//#define _SERIAL SerialAndTelnet
#define Serial SerialAndTelnet

#define MSG_NOTHING           0x00
#define MSG_TIMER_START       0x01
#define MSG_TIMER_RESET       0x02
#define MSG_TIMER_COUNTDOWN   0x03
#define MSG_SERVO_ON          0x04
#define MSG_IGNAITER_ON       0x05

int gMsgEventID   = MSG_NOTHING;

int gSPERK_TIME    = 50;//default
int gRELEASE_TIME  = 10;//default

String processor(const String &var)
{
  //nothing
  return String();
}

void telnetConnected() {
  Serial.println("Telnet connection established.");
}

void telnetDisconnected() {
  Serial.println("Telnet connection closed.");
}

void sendCountDownMsg(int state){
  gMsgEventID = MSG_TIMER_COUNTDOWN;
}

void onRequest(AsyncWebServerRequest *request){
  Serial.println("onRequest() Handle Unknown Request");
  //Handle Unknown Request
  request->send(404);
}

void onUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
  Serial.println("onUpload() Handle upload");
  //Handle upload
}

void onBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
  Serial.println("onBody()");
  String jsonBody;

  const size_t capacity = JSON_OBJECT_SIZE(2) + 30;
  DynamicJsonDocument doc(capacity);

  for(size_t i = 0; i < len; i++){
    jsonBody += (char)data[i];
  }

  //Serial.println(jsonBody);

  deserializeJson(doc, jsonBody);

  gSPERK_TIME = doc["SPERK_TIME"];
  gRELEASE_TIME = doc["RELEASE_TIME"];

  Serial.printf("SPERK_TIME = %d RELEASE_TIME = %d\n", gSPERK_TIME, gRELEASE_TIME); 

}

void setup()
{
  //Port Init
  myservo.attach(SERVO_NUM);
  myservo.write(90);

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  //Telnet Init
  SerialAndTelnet.setWelcomeMsg("Welcome to ESP Terminal.\r\n");
  SerialAndTelnet.setCallbackOnConnect(telnetConnected);
  SerialAndTelnet.setCallbackOnDisconnect(telnetDisconnected);
  Serial.begin(MONITOR_SPEED);
  delay(100); // Wait for serial port
  Serial.setDebugOutput(false);
  delay(1000);

  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(sta_ssid, sta_password);

  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("STA: Failed!");
    Serial.println("[Access Point] Mode");
    WiFi.disconnect(false);
    delay(5000);
    WiFi.softAP(ap_ssid, ap_password);
    Serial.println(WiFi.softAPIP().toString());
  }
  else{
    Serial.println("STA: Success!");
    Serial.println("[Client] Mode");
    Serial.println(WiFi.localIP().toString());
  }

  if (!MDNS.begin(HOSTNAME)){
    Serial.println("Error setting up MDNS responder!");
    while (1){
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");

  // Route to load style.css file
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("Route to load style.css file");
    request->send(SPIFFS, "/style.css", "text/css");
  });

  // Route to load favicon.ico file
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("Route to load favicon.ico file");
    request->send(SPIFFS, "/favicon.ico", "icon");
  });

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("[HTTP_GET] /");
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });

  server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
    Serial.println("[HTTP_POST] /");
    request->send(SPIFFS, "/index.html", String(), false, processor);
  }, NULL, onBody);

  // Route for /settings web page
  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("[HTTP_GET] /settings");
    request->send(SPIFFS, "/settings.html", String(), false, processor);
  });

  server.on("/settings", HTTP_POST, [](AsyncWebServerRequest *request) {
    Serial.println("[HTTP_POST] /settings");
  }, NULL, onBody);

  //REST API
  //Start timer
  server.on("/start", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("/start");
    gMsgEventID = MSG_TIMER_START;
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });

  //Reset timer
  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("/reset");
    gMsgEventID = MSG_TIMER_RESET;
    request->send(SPIFFS, "/index.html", String(), false, processor);
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

  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.begin();
  
  // Initialize SPIFFS
  Serial.println("Mounting FS...");
  if (!SPIFFS.begin()){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  events.onConnect([](AsyncEventSourceClient *client){
    client->send("10", NULL, millis(), 1000);
  });

  server.addHandler(&events);

  // Catch-All Handlers
  // Any request that can not find a Handler that canHandle it
  // ends in the callbacks below.
  server.onNotFound(onRequest);
  server.onFileUpload(onUpload);
  //server.onRequestBody(onBody);

  Serial.print("Hostname: ");
  Serial.println(ArduinoOTA.getHostname() + ".local");

  server.begin();
  Serial.println("Server Started");

}

//Event Message Loop
void loop()
{
  char p[32] = {0};
  static int nCount = 10;

  SerialAndTelnet.handle();
  ArduinoOTA.handle();

  switch(gMsgEventID){
    case MSG_TIMER_START:
        countDown.attach_ms(1000, sendCountDownMsg, 0);

        gMsgEventID = MSG_NOTHING;
    break;
    case MSG_TIMER_COUNTDOWN:
        
        nCount--;
        
        Serial.printf("Timer = %d\r\n", nCount);

        sprintf(p, "%d", nCount);
        events.send(p, "count");

        if(nCount == 0){
          countDown.detach();
          gMsgEventID = MSG_IGNAITER_ON;
        }
        else{
          gMsgEventID = MSG_NOTHING;
        }
    break;
    case MSG_SERVO_ON:
        Serial.printf("RELEASE_TIME = %d[ms]\n", gRELEASE_TIME);        
        delay(gRELEASE_TIME);// wait from MSG_IGNAITER_ON
        
        Serial.println("Servo ON!");
        myservo.write(148);
        delay(1500);

        gMsgEventID = MSG_TIMER_RESET;
    break;
    case MSG_IGNAITER_ON:

        Serial.printf("SPERK_TIME = %d[ms]\n", gSPERK_TIME);
        Serial.println("Ignaiter ON!");

        digitalWrite(ledPin, HIGH);
        delay(gSPERK_TIME);
        digitalWrite(ledPin, LOW);
        Serial.println("Ignaiter OFF!");
        gMsgEventID = MSG_SERVO_ON;
    break;
    case MSG_TIMER_RESET:
        Serial.println("Reset");
        countDown.detach();        
        myservo.write(90);
        nCount = 10;

        gMsgEventID = MSG_NOTHING;
    break;
    default:
      ;//MSG_NOTHING
  } 
}
