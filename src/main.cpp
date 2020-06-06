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
#ifdef ESP32
  #include <FS.h>
  #include <SPIFFS.h>
  #include <WiFi.h>
  #include <AsyncTCP.h>
  #include <ESP32Ticker.h>
  #define HOSTNAME "esp32"
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESPAsyncTCP.h>
  #include <ESP8266mDNS.h>
  #include <Ticker.h>
  #define HOSTNAME "esp8266"
#endif
#include <ESPAsyncWebServer.h>
#include <SPIFFSEditor.h>
#include <TelnetSpy.h>
#include <Servo.h>

const char *ap_ssid       = "ESP32-G-AP";
const char *ap_password   = "room03601";

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

int gMsgEventID = MSG_NOTHING;

String processor(const String &var)
{
  if (var == "STATE"){
    if (digitalRead(ledPin)){
      ledState = "ON";
    }
    else{
      ledState = "OFF";
    }
    return ledState;
  }
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

void setup()
{
  //Port Init
  myservo.attach(SERVO_NUM);
  myservo.write(90);

  pinMode(ledPin, OUTPUT);

  //Telnet Init
  SerialAndTelnet.setWelcomeMsg("Welcome to ESP Terminal.\r\n");
  SerialAndTelnet.setCallbackOnConnect(telnetConnected);
  SerialAndTelnet.setCallbackOnDisconnect(telnetDisconnected);
  Serial.begin(74880);
  delay(100); // Wait for serial port
  Serial.setDebugOutput(false);
  delay(1000);

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(HOSTNAME);
  WiFi.begin(sta_ssid, sta_password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("STA: Failed!");
    Serial.println("[Access Point] Mode");
    WiFi.disconnect(false);
    delay(5000);
    WiFi.begin(ap_ssid, ap_password);
  }
  else{
    Serial.println("STA: Success!");
    Serial.println("[Client] Mode");
  }

  Serial.println(WiFi.localIP().toString());

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("Route for root / web page");
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });

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

  //Web API
  // start timer
  server.on("/startcountdown", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("/startcountdown");
    gMsgEventID = MSG_TIMER_START;
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });

  // reset timer
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
  if (!SPIFFS.begin()){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  events.onConnect([](AsyncEventSourceClient *client){
    client->send("10", NULL, millis(), 1000);
  });

  server.addHandler(&events);

  Serial.print("Hostname: ");
  Serial.println(ArduinoOTA.getHostname());

  server.begin();

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
        
        delay(100);// wait 0.1s from MSG_IGNAITER_ON
        
        Serial.println("Servo ON!");
        myservo.write(148);
        delay(1500);

        gMsgEventID = MSG_TIMER_RESET;
    break;
    case MSG_IGNAITER_ON:
        Serial.println("Ignaiter ON!");

        digitalWrite(ledPin, HIGH);
        delay(50);
        digitalWrite(ledPin, LOW);

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
