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
#define HOSTNAME "esp32"
#define MONITOR_SPEED 115200
#define AP_NAME "ESP32-G-AP"
#define EEPROM_SIZE 512
#elif defined(ESP8266)
#include <LittleFS.h>
#include <ESP8266WiFi.h>
#define HOSTNAME "esp8266"
#define MONITOR_SPEED 74880
#define AP_NAME "ESP8266-G-AP"
#define EEPROM_SIZE 512
#endif

#include <ESPAsyncWebServer.h>
#include <ESPAsyncDNSServer.h>
#include <ESPAsyncWiFiManager.h>
#include <TelnetSpy.h>
#include <Servo.h>
#include <EEPROM.h>
#include <Ticker.h>
#include <ArduinoJson.h>
#include <StreamUtils.h>
#include <ESPRGBLed.h>

AsyncWebServer server(80);
AsyncDNSServer dns;
AsyncWiFiManager wifiManager(&server, &dns);
AsyncEventSource events("/events");

static const int RELAY_NUM = 5;
static const int SERVO_NUM = 16;
static const int RED_PIN = 13;
static const int GREEN_PIN = 12;
static const int BLUE_PIN = 14;

Servo myservo;
Ticker countDown;
Ticker mode;
TelnetSpy SerialAndTelnet;
RGBLed led(RED_PIN, GREEN_PIN, BLUE_PIN, COMMON_ANODE);

const size_t capacity = JSON_OBJECT_SIZE(3) + 40;
DynamicJsonDocument doc(capacity);

#define Serial SerialAndTelnet

//Message Name
#define MSG_NOTHING 0x00
#define MSG_START_TIMER 0x01
#define MSG_RESET_TIMER 0x02
#define MSG_COUNT_TIMER 0x03
#define MSG_SERVO_ON 0x04
#define MSG_IGNAITER_ON 0x05
#define MSG_CHANGE_MODE 0x06
#define MSG_SET_TIME 0x07
#define MSG_ENTER_SETTING 0x08
#define MSG_ENTER_MAIN 0x09
#define MSG_BLYNK_LED 0x10
#define MSG_RESET_ESP 0xFF

int gMsgEventID = MSG_NOTHING;

void SendMessage(int nMessageID)
{
    gMsgEventID = nMessageID;
}

String processor(const String &var)
{
    Serial.println("processor()");

    if (var == "SPERK_TIME")
    {
        int SPERK_TIME = doc["SPERK_TIME"];
        String value = String(SPERK_TIME);
        return value;
    }
    else if (var == "RELEASE_TIME")
    {
        int RELEASE_TIME = doc["RELEASE_TIME"];
        String value = String(RELEASE_TIME);
        return value;
    }

    return "0";
}

void telnetConnected()
{
    Serial.println("Telnet connection established.");
    led.flash(RGBLed::BLUE, 100);
}

void telnetDisconnected()
{
    Serial.println("Telnet connection closed.");

    for (int i = 0; i < 3; i++)
    {
        led.flash(RGBLed::BLUE, 100);
    }
}

void sendCountDownMsg(int state)
{
    SendMessage(MSG_COUNT_TIMER);
}

void blynkModeLed(int state)
{
    SendMessage(MSG_BLYNK_LED);
}

void onRequest(AsyncWebServerRequest *request)
{
    Serial.println("onRequest() Handle Unknown Request");
    //Handle Unknown Request
    request->send(404);
}

void onUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
    Serial.println("onUpload() Handle upload");
    //Handle upload
}

void onBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
    Serial.println("onBody()");

    String jsonBody;

    for (size_t i = 0; i < len; i++)
    {
        jsonBody += (char)data[i];
    }

    //jsonBody to doc
    deserializeJson(doc, jsonBody);

    String mode = String((const char *)doc["MODE"]);
    int SPERK_TIME = doc["SPERK_TIME"];
    int RELEASE_TIME = doc["RELEASE_TIME"];

    Serial.printf("MODE = %s ", mode.c_str());
    Serial.printf("SPERK_TIME = %d ", SPERK_TIME);
    Serial.printf("RELEASE_TIME = %d \n", RELEASE_TIME);

    if (mode == "SUBMIT")
    {
        SendMessage(MSG_SET_TIME);
    }
    else
    {
        SendMessage(MSG_CHANGE_MODE);
    }
}

void initLeds()
{
    Serial.println("Initializing RGB Led...");

    led.brightness(RGBLed::RED, 100);
    delay(1000);
    led.brightness(RGBLed::GREEN, 100);
    delay(1000);
    led.brightness(RGBLed::BLUE, 100);
    delay(1000);
    led.off();
}

//for anti-sperking-noise
void closeLedPort()
{
    pinMode(RED_PIN, OUTPUT_OPEN_DRAIN);
    pinMode(GREEN_PIN, OUTPUT_OPEN_DRAIN);
    pinMode(BLUE_PIN, OUTPUT_OPEN_DRAIN);
}
//to normal
void openLedPort()
{
    pinMode(RED_PIN, OUTPUT);
    pinMode(GREEN_PIN, OUTPUT);
    pinMode(BLUE_PIN, OUTPUT);
}

void initPort()
{
    Serial.println("Initializing Ports...");

    pinMode(SERVO_NUM, INPUT_PULLUP);
    pinMode(RELAY_NUM, INPUT_PULLUP);

    //init open IO for anti-noise
    pinMode(4, OUTPUT_OPEN_DRAIN);
    pinMode(16, OUTPUT_OPEN_DRAIN);
    pinMode(1, OUTPUT_OPEN_DRAIN);
    pinMode(3, OUTPUT_OPEN_DRAIN);
}

void initTelnet()
{
    Serial.println("Initializing Telnet...");

    Serial.begin(MONITOR_SPEED);
    delay(500); // Wait for serial port
    Serial.setDebugOutput(false);
    delay(1000);

    SerialAndTelnet.setWelcomeMsg("Welcome to ESP Terminal.\n");
    SerialAndTelnet.setCallbackOnConnect(telnetConnected);
    SerialAndTelnet.setCallbackOnDisconnect(telnetDisconnected);
}

void initEEPROM()
{
    Serial.println("Initializing EEPROM...");
    EEPROM.begin(EEPROM_SIZE);

    EepromStream settings(0, EEPROM_SIZE / 2);

    //settings(eeprom) to doc
    deserializeJson(doc, settings);
}

void initWiFi()
{
    EepromStream settings(0, EEPROM_SIZE / 2);

    wifiManager.setDebugOutput(true);

    String mode = String((const char *)doc["MODE"]);
    Serial.printf("MODE = %s\n", mode.c_str());

    if (mode == "STA_MODE")
    {
        led.flash(RGBLed::BLUE, 100);
        led.setColor(RGBLed::BLUE);

        wifiManager.setTimeout(180);
        wifiManager.resetSettings();
        if (!wifiManager.startConfigPortal(AP_NAME))
        {
            wifiManager.autoConnect(AP_NAME);
        }
    }
    else if (mode == "AP_MODE")
    {
        led.flash(RGBLed::RED, 100);
        led.setColor(RGBLed::RED);

        wifiManager.setTimeout(1);
        wifiManager.resetSettings();
        wifiManager.autoConnect(AP_NAME);
    }
    else
    {
        wifiManager.setTimeout(180);
        if (!wifiManager.startConfigPortal(AP_NAME))
        {
            wifiManager.autoConnect(AP_NAME);
        }
        led.flash(RGBLed::GREEN, 100);
        led.setColor(RGBLed::GREEN);
    }

    //SUBMIT
    doc["MODE"] = "SUBMIT";
    serializeJson(doc, settings);
    settings.flush(); //write to eeprom

    Serial.println("WiFi Started");
}

void initServer()
{
    // Route to load style.css file
    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("Route to load style.css file");
        request->send(LittleFS, "/style.css", "text/css");
    });

    server.on("/jquery-3.5.1.slim.min.js", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("Route to load jquery-3.5.1.slim.min.js file");
        request->send(LittleFS, "/jquery-3.5.1.slim.min.js", "text/javascript");
    });

    // Route to load favicon.ico file
    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("Route to load favicon.ico file");
        request->send(LittleFS, "/favicon.ico", "icon");
    });

    // Route for root / web page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("[HTTP_GET] /");
        request->send(LittleFS, "/index.html", String(), false, processor);
        SendMessage(MSG_ENTER_MAIN);
    });

    server.on(
        "/", HTTP_POST, [](AsyncWebServerRequest *request) {
            Serial.println("[HTTP_POST] /");
        },
        NULL, onBody);

    // Route for /settings web page
    server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("[HTTP_GET] /settings");
        request->send(LittleFS, "/settings.html", String(), false, processor);
        SendMessage(MSG_ENTER_SETTING);
    });

    //REST API
    //Start timer
    server.on("/start", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("/start");
        request->send(LittleFS, "/index.html", String(), false, processor);
        SendMessage(MSG_START_TIMER);
    });

    //Reset timer
    server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("/reset");
        request->send(LittleFS, "/index.html", String(), false, processor);
        SendMessage(MSG_RESET_TIMER);
    });

    events.onConnect([](AsyncEventSourceClient *client) {
        client->send("10", NULL, millis(), 1000);
    });

    server.addHandler(&events);

    // Catch-All Handlers
    // Any request that can not find a Handler that canHandle it
    // ends in the callbacks below.
    server.onNotFound(onRequest);
    server.onFileUpload(onUpload);

    server.begin();
    Serial.println("Server Started");

    led.flash(RGBLed::BLUE, 500);
}

void initOta()
{
    ArduinoOTA.onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
            type = "sketch";
        else
            type = "filesystem";

        Serial.println("Start updating " + type);
    });

    ArduinoOTA.onEnd([]() {
        Serial.println("\nEnd");
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });

    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR)
            Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR)
            Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR)
            Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR)
            Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR)
            Serial.println("End Failed");
    });

    ArduinoOTA.setHostname(HOSTNAME);

    Serial.print("Hostname: ");
    Serial.println(ArduinoOTA.getHostname() + ".local");

    ArduinoOTA.begin();
    Serial.println("OTA Started");

    led.setColor(RGBLed::GREEN);
    led.fadeOut(RGBLed::GREEN, 5, 100);
    led.fadeIn(RGBLed::GREEN, 5, 100);
}

void initLittleFS()
{
    // Initialize LittleFS
    Serial.println("Mounting LittleFS...");
    if (!LittleFS.begin())
    {
        Serial.println("An Error has occurred while mounting LittleFS");
        return;
    }
}

void setup()
{
    initLeds();
    initTelnet();
    initEEPROM();
    initWiFi();
    initLittleFS();
    initPort();
    initServer();
    initOta();
}

//Event Message Loop
void loop()
{
    char p[32] = {0};
    static int nCount = 10;

    SerialAndTelnet.handle();
    ArduinoOTA.handle();

    switch (gMsgEventID)
    {
    case MSG_BLYNK_LED:
        led.flash(RGBLed::GREEN, 500);
        SendMessage(MSG_NOTHING);
        break;
    case MSG_ENTER_MAIN:
        mode.detach();
        led.setColor(RGBLed::GREEN);
        SendMessage(MSG_NOTHING);
        break;
    case MSG_ENTER_SETTING:
        mode.attach_ms(1000, blynkModeLed, 0);
        SendMessage(MSG_NOTHING);
        break;
    case MSG_CHANGE_MODE:
    {
        Serial.print("Change Mode to ");
        Serial.println((const char *)doc["MODE"]);

        EepromStream settings(0, EEPROM_SIZE / 2);
        //doc to settings(eeprom)
        serializeJson(doc, settings);
        settings.flush(); //write to eeprom

        Serial.println("Reset...");
        SendMessage(MSG_RESET_ESP);
    }
    break;
    case MSG_SET_TIME:
    {
        Serial.println("Set times.");

        EepromStream settings(0, EEPROM_SIZE / 2);
        //doc to settings(eeprom)
        serializeJson(doc, settings);
        settings.flush(); //write to eeprom

        SendMessage(MSG_NOTHING);
    }
    break;
    case MSG_START_TIMER:
        countDown.attach_ms(1000, sendCountDownMsg, 0);

        SendMessage(MSG_NOTHING);
        break;
    case MSG_COUNT_TIMER:

        nCount--;

        Serial.printf("Timer = %d\r\n", nCount);

        sprintf(p, "%d", nCount);
        events.send(p, "count");

        if (nCount == 0)
        {
            countDown.detach();
            SendMessage(MSG_IGNAITER_ON);
        }
        else
        {
            SendMessage(MSG_NOTHING);
            led.flash(RGBLed::GREEN, 100);
        }
        break;
    case MSG_SERVO_ON:
    {
        int release_time = doc["RELEASE_TIME"];
        Serial.printf("RELEASE_TIME = %d[ms]\n", release_time);

        myservo.attach(SERVO_NUM);
        myservo.write(90);

        delay(release_time); // wait from MSG_IGNAITER_ON

        Serial.println("Servo ON!");
        myservo.write(150);
        delay(1000);
        myservo.detach();
        pinMode(SERVO_NUM, INPUT_PULLUP);

        SendMessage(MSG_RESET_TIMER);
    }
    break;
    case MSG_IGNAITER_ON:
    {
        for (int i = 0; i < 5; i++)
        {
            led.flash(RGBLed::RED, 100);
        }

        closeLedPort();
        delay(100);

        int sperk_time = doc["SPERK_TIME"];
        Serial.printf("SPERK_TIME = %d[ms]\n", sperk_time);

        pinMode(RELAY_NUM, OUTPUT);

        digitalWrite(RELAY_NUM, HIGH);
        Serial.println("Ignaiter ON!");
        delay(sperk_time);
        digitalWrite(RELAY_NUM, LOW);
        Serial.println("Ignaiter OFF!");

        pinMode(RELAY_NUM, INPUT_PULLUP);

        SendMessage(MSG_SERVO_ON);
    }
    break;
    case MSG_RESET_TIMER:
        Serial.println("Reset");
        countDown.detach();

        nCount = 10;

        sprintf(p, "%d", nCount);
        events.send(p, "count");

        openLedPort();
        delay(100);

        led.flash(RGBLed::GREEN, 100);
        led.flash(RGBLed::GREEN, 100);

        SendMessage(MSG_NOTHING);
        break;
    case MSG_RESET_ESP:
        led.flash(RGBLed::BLUE, 100);
        led.flash(RGBLed::BLUE, 100);
        delay(5000);
#if defined(ESP8266)
        ESP.reset();
#else
        ESP.restart();
#endif
        delay(2000);
    default:
        //MSG_NOTHING
        break;
    }
}
