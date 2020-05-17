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

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WiFiClient.h>
//#include <ESPmDNS.h>

#include <ArduinoMDNS.h>
//#include <EthernetUtil.h>
//#include <MDNS.h>


#include <ArduinoOTA.h>
#include <ESPAsyncWebServer.h>
//#include <SPIFFS.h>
//#include <LittleFS.h>
#include <Servo.h>
#include <TelnetSpy.h>
//#include <ESP32Ticker.h>
#include <Ticker.h>


#define ESP32_AP_MODE false

#if ESP32_AP_MODE 
const char *ssid = "ESP32-G-AP";
const char *password = "room03601";
#else
const char *ssid = "Buffalo-G-FAA8";
const char *password = "34ywce7cffyup";
#endif

Ticker countDown;

// Set LED GPIO
const int ledPin = 4;
// Stores LED state
String ledState;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

static const int SERVO_NUM = 14;

Servo myservo;            

TelnetSpy SerialAndTelnet;

#define _SERIAL SerialAndTelnet
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
  //Serial.println(var);
  if (var == "STATE"){
    if (digitalRead(ledPin)){
      ledState = "ON";
    }
    else{
      ledState = "OFF";
    }
    //Serial.println(ledState);
    return ledState;
  }
  return String();
}

void telnetConnected() {
  _SERIAL.println("Telnet connection established.");
}

void telnetDisconnected() {
  _SERIAL.println("Telnet connection closed.");
}

void sendCountDownMsg(int state){
  gMsgEventID = MSG_TIMER_COUNTDOWN;
}

void setup()
{
  SerialAndTelnet.setWelcomeMsg("Welcome to ESP32 WifiTerminal.\r\n");
  SerialAndTelnet.setCallbackOnConnect(telnetConnected);
  SerialAndTelnet.setCallbackOnDisconnect(telnetDisconnected);
  _SERIAL.begin(74880);
  delay(100); // Wait for serial port
  _SERIAL.setDebugOutput(false);

  delay(1000);

  myservo.attach(SERVO_NUM);
  
  // Serial port for debugging purposes
  //Serial.begin(115200);
  pinMode(ledPin, OUTPUT);

  // Initialize SPIFFS
  if (!LittleFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }


#if (ESP32_AP_MODE == false)
  // Connect to Wi-Fi
  ArduinoOTA.setHostname("esp32");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
#else
  Serial.println();
  Serial.println("Configuring access point...");

  //アクセスポイントを起動する
  WiFi.softAP(ssid, password);
  IPAddress myIP = WiFi.softAPIP();
#endif

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("Route for root / web page");
    request->send(LittleFS, "/index.html", String(), false, processor);
  });

  // Route to load style.css file
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("Route to load style.css file");
    request->send(LittleFS, "/style.css", "text/css");
  });

  // タイマーを開始する
  server.on("/startcountdown", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("/startcountdown");
    //タイマ開始  
    gMsgEventID = MSG_TIMER_START;
    request->send(LittleFS, "/index.html", String(), false, processor);
  });

  //タイマーをリセットする
  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("/reset");
    gMsgEventID = MSG_TIMER_RESET;
    request->send(LittleFS, "/index.html", String(), false, processor);
  });

  //Webサーバー開始
  server.begin();

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();

#if (ESP32_AP_MODE == flase)
  Serial.println("[Client] Mode");
  Serial.println("Web server started.");
  Serial.println(WiFi.localIP().toString());
#else
  Serial.println("[Access Point] Mode");
  Serial.println("Web server started.");
  Serial.println(myIP.toString());
#endif
  
  Serial.print("Hostname: ");
  Serial.println(ArduinoOTA.getHostname());

  myservo.write(0);

}

//Event Message Loop
void loop()
{
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
        if(nCount == 0){
          countDown.detach();
          gMsgEventID = MSG_SERVO_ON;
        }
        else{
          gMsgEventID = MSG_NOTHING;
        }
    break;
    case MSG_SERVO_ON://サーボ動作
        Serial.println("Servo ON!");
        myservo.write(10);
        delay(200);
        gMsgEventID = MSG_IGNAITER_ON;
    break;
    case MSG_IGNAITER_ON://イグナイター発火
        Serial.println("Ignaiter ON!");

        digitalWrite(ledPin, HIGH);
        delay(50);
        digitalWrite(ledPin, LOW);
        gMsgEventID = MSG_TIMER_RESET;
    break;
    case MSG_TIMER_RESET://リセット
        Serial.println("Reset");
        countDown.detach();
        myservo.write(0);
        nCount = 10;
        gMsgEventID = MSG_NOTHING;
    break;
    default:
      ;//MSG_NOTHING
  } 
}
