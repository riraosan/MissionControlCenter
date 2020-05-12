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
#include <WiFiClient.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <Servo.h>

#define ESP32_AP_MODE true

#if ESP32_AP_MODE 
const char *ssid = "ESP32-G-AP";
const char *password = "room03601";
#else
const char *ssid = "Buffalo-G-FAA8";
const char *password = "34ywce7cffyup";
#endif

// Set LED GPIO
const int ledPin = 4;
// Stores LED state
String ledState;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

static const int SERVO_NUM = 14;

Servo myservo;            

#define MSG_NOTHING     0x00
#define MSG_TIMER_START 0x01

int gMsgEventID = MSG_NOTHING;

// Replaces placeholder with LED state value
String processor(const String &var)
{
  Serial.println(var);
  if (var == "STATE"){
    if (digitalRead(ledPin)){
      ledState = "ON";
    }
    else{
      ledState = "OFF";
    }
    Serial.println(ledState);
    return ledState;
  }
  return String();
}

void setup()
{
  delay(1000);

  myservo.attach(SERVO_NUM);
  
  // Serial port for debugging purposes
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);

  // Initialize SPIFFS
  if (!SPIFFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

#if (ESP32_AP_MODE == false)
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }

#else
  Serial.println();
  Serial.println("Configuring access point...");

  //アクセスポイントを起動する
  WiFi.softAP(ssid, password);

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  /* Set up mDNS */
  //http://esp32.local/
  if (!MDNS.begin("esp32"))
  {
    Serial.println("Error setting up MDNS responder!");
    while (1)
    {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");

  // Print ESP32 Local IP Address
  Serial.println(myIP.toString());
#endif

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

  // タイマーを開始する
  server.on("/startcountdown", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("/startcountdown");
    delay(1000);
    gMsgEventID = MSG_TIMER_START;
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });

  //タイマーをOFFする
  server.on("/stopcountdown", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("/stopcountdown");
    //リセット
    myservo.write(0);
    gMsgEventID = MSG_NOTHING;
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });

  //Webサーバー開始
  server.begin();

#if (ESP32_AP_MODE == flase)
  Serial.println("[Client] Mode");
  Serial.println("Web server started.");
  Serial.println(WiFi.localIP().toString());
#else
  Serial.println("[Access Point] Mode");
  Serial.println("Web server started.");
  Serial.println(myIP.toString());
#endif

  myservo.write(0);
}

//Event Message Loop
void loop()
{
  static int nCount = 10;

  switch(gMsgEventID){
    case MSG_TIMER_START:
      if(nCount > 0){
        delay(1000);
        nCount--;
      }
      else{
        //サーボ動作
        myservo.write(90);
        delay(200);
        //イグナイター操作
        digitalWrite(ledPin, HIGH);
        delay(50);
        digitalWrite(ledPin, LOW);
        //リセット
        delay(2000);
        myservo.write(0);
        nCount = 10;
        gMsgEventID = MSG_NOTHING;
      }
      return;
    break;
    default:
      ;
  }

  delay(1);
}
