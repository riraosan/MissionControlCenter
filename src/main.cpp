/* 
The MIT License (MIT)

Copyright (c) 2019 riraotech.com

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
#include <ESP32_SPIFFS_ShinonomeFNT.h>
#include <ESP32_SPIFFS_UTF8toSJIS.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <stdio.h>
#define ARDUINOJSON_DECODE_UNICODE 1
#include <ArduinoJson.h>
#include <Wifi.h>

#define JST     3600* 9

//ポート設定
#define PORT_SE_IN 13
#define PORT_AB_IN 27
#define PORT_A3_IN 23
#define PORT_A2_IN 21
#define PORT_A1_IN 25
#define PORT_A0_IN 26
#define PORT_DG_IN 19
#define PORT_CLK_IN 18
#define PORT_WE_IN 17
#define PORT_DR_IN 16
#define PORT_ALE_IN 22

#define PANEL_NUM     2   //パネル枚数
#define R             1   //赤色
#define O             2   //橙色
#define G             3   //緑色

//これらのファイルをSPIFFS領域へコピーしておくこと
const char* UTF8SJIS_file         = "/Utf8Sjis.tbl";  //UTF8 Shift_JIS 変換テーブルファイル名を記載しておく
const char* Shino_Zen_Font_file   = "/shnmk16.bdf";   //全角フォントファイル名を定義
const char* Shino_Half_Font_file  = "/shnm8x16.bdf";  //半角フォントファイル名を定義

const char* appid       = "dj00aiZpPU5xUWRpRTlhZXpBMCZzPWNvbnN1bWVyc2VjcmV0Jng9MzY-";//APP ID
const char* zipcode     = "592-8344";//郵便番号
const char* output      = "json";//出力形式
const char* server      = "map.yahooapis.jp";
const int informPeriod  = 10;//10分間隔で降雨情報を取得

const char* yahooapi_root_ca= \
     "-----BEGIN CERTIFICATE-----\n" \
     "MIIDdzCCAl+gAwIBAgIEAgAAuTANBgkqhkiG9w0BAQUFADBaMQswCQYDVQQGEwJJ\n" \
     "RTESMBAGA1UEChMJQmFsdGltb3JlMRMwEQYDVQQLEwpDeWJlclRydXN0MSIwIAYD\n" \
     "VQQDExlCYWx0aW1vcmUgQ3liZXJUcnVzdCBSb290MB4XDTAwMDUxMjE4NDYwMFoX\n" \
     "DTI1MDUxMjIzNTkwMFowWjELMAkGA1UEBhMCSUUxEjAQBgNVBAoTCUJhbHRpbW9y\n" \
     "ZTETMBEGA1UECxMKQ3liZXJUcnVzdDEiMCAGA1UEAxMZQmFsdGltb3JlIEN5YmVy\n" \
     "VHJ1c3QgUm9vdDCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAKMEuyKr\n" \
     "mD1X6CZymrV51Cni4eiVgLGw41uOKymaZN+hXe2wCQVt2yguzmKiYv60iNoS6zjr\n" \
     "IZ3AQSsBUnuId9Mcj8e6uYi1agnnc+gRQKfRzMpijS3ljwumUNKoUMMo6vWrJYeK\n" \
     "mpYcqWe4PwzV9/lSEy/CG9VwcPCPwBLKBsua4dnKM3p31vjsufFoREJIE9LAwqSu\n" \
     "XmD+tqYF/LTdB1kC1FkYmGP1pWPgkAx9XbIGevOF6uvUA65ehD5f/xXtabz5OTZy\n" \
     "dc93Uk3zyZAsuT3lySNTPx8kmCFcB5kpvcY67Oduhjprl3RjM71oGDHweI12v/ye\n" \
     "jl0qhqdNkNwnGjkCAwEAAaNFMEMwHQYDVR0OBBYEFOWdWTCCR1jMrPoIVDaGezq1\n" \
     "BE3wMBIGA1UdEwEB/wQIMAYBAf8CAQMwDgYDVR0PAQH/BAQDAgEGMA0GCSqGSIb3\n" \
     "DQEBBQUAA4IBAQCFDF2O5G9RaEIFoN27TyclhAO992T9Ldcw46QQF+vaKSm2eT92\n" \
     "9hkTI7gQCvlYpNRhcL0EYWoSihfVCr3FvDB81ukMJY2GQE/szKN+OMY3EU/t3Wgx\n" \
     "jkzSswF07r51XgdIGn9w/xZchMB5hbgF/X++ZRGjD8ACtPhSNzkE1akxehi/oCr0\n" \
     "Epn3o0WC4zxe9Z2etciefC7IpJ5OCBRLbf1wbWsaY71k5h+3zvDyny67G7fyUIhz\n" \
     "ksLi4xaNmjICq44Y3ekQEe5+NauQrz4wlHrQMz2nZQ/1/I6eYs9HRCwBXbsdtTLS\n" \
     "R9I4LtD+gdwyah617jzV/OeBHRnDJELqYzmp\n" \
     "-----END CERTIFICATE-----\n";

ESP32_SPIFFS_ShinonomeFNT SFR;  //東雲フォントをSPIFFSから取得するライブラリ
WiFiClientSecure client;

SemaphoreHandle_t xMutex = NULL;

//LEDマトリクスの書き込みアドレスを設定するメソッド
void setRAMAdder(uint8_t lineNumber){
  uint8_t A[4] = {0};
  uint8_t adder = 0;

  adder = lineNumber;

  for(int i = 0; i < 4; i++){    
    A[i] = adder % 2;
    adder /= 2;
  }

  digitalWrite(PORT_A0_IN, A[0]);
  digitalWrite(PORT_A1_IN, A[1]);
  digitalWrite(PORT_A2_IN, A[2]);
  digitalWrite(PORT_A3_IN, A[3]);

}

////////////////////////////////////////////////////////////////////////////////////
//データをLEDマトリクスへ1行だけ書き込む
//
//iram_addr:データを書き込むアドレス（0~15）
//ifont_data:フォント表示データ(32*PANEL_NUM bit)
//color_data:フォント表示色配列（32*PANEL_NUM bit）Red:1 Orange:2 Green:3 
////////////////////////////////////////////////////////////////////////////////////
void send_line_data(uint8_t iram_adder, uint8_t ifont_data[], uint8_t color_data[]){

  uint8_t font[8]   = {0};
  uint8_t tmp_data  = 0;
  int k = 0;
  for(int j = 0; j < 4 * PANEL_NUM; j++){
    //ビットデータに変換
    tmp_data = ifont_data[j];   
    for(int i = 0; i < 8; i++){    
      font[i] = tmp_data % 2;
      tmp_data /= 2;
    }

    for(int i = 7; i >= 0; i--){
      digitalWrite(PORT_DG_IN, LOW);
      digitalWrite(PORT_DR_IN, LOW);
      digitalWrite(PORT_CLK_IN, LOW);

      if(font[i] == 1){
        if(color_data[k] == R ){
          digitalWrite(PORT_DR_IN, HIGH);
        }

        if(color_data[k] == G){
          digitalWrite(PORT_DG_IN, HIGH);
        }

        if(color_data[k] == O){
          digitalWrite(PORT_DR_IN, HIGH);
          digitalWrite(PORT_DG_IN, HIGH);
        }
      }else{
          digitalWrite(PORT_DR_IN, LOW);
          digitalWrite(PORT_DG_IN, LOW);
      }

      delayMicroseconds(1);
      digitalWrite(PORT_CLK_IN, HIGH);
      delayMicroseconds(1);

      k++;
    }
  }
  //アドレスをポートに入力
  setRAMAdder(iram_adder);
  //ALE　Highでアドレスセット
  digitalWrite(PORT_ALE_IN, HIGH);
  //WE Highでデータを書き込み
  digitalWrite(PORT_WE_IN, HIGH);
  //WE Lowをセット
  digitalWrite(PORT_WE_IN, LOW);
  //ALE Lowをセット
  digitalWrite(PORT_ALE_IN, LOW);
}

///////////////////////////////////////////////////////////////
//配列をnビット左へシフトする関数
//
//dist:格納先の配列
//src:入力元の配列
//len:配列の要素数
//n:一度に左シフトするビット数
///////////////////////////////////////////////////////////////
void shift_bit_left(uint8_t dist[], uint8_t src[], int len, int n){
  uint8_t mask = 0xFF << (8 - n);
  for(int i = 0; i < len; i++){
    if(i < len - 1){
      dist[i] = (src[i] << n) | ((src[i + 1] & mask) >> (8 - n));
    }else{
      dist[i] = src[i] << n;
    }
  }
}

void shift_color_left(uint8_t dist[], uint8_t src[], int len){
  for(int i = 0; i < len * 8; i++){
    if(i < len * 8 - 1){
      dist[i] = src[i + 1];
    }else{
      dist[i] = 0;
    }
  }
}

////////////////////////////////////////////////////////////////////
//フォントをスクロールしながら表示するメソッド
//
//sj_length:半角文字数
//font_data:フォントデータ（東雲フォント）
//color_data:フォントカラーデータ（半角毎に設定する）
//intervals:スクロール間隔(ms)
////////////////////////////////////////////////////////////////////
void scrollLEDMatrix(int16_t sj_length, uint8_t font_data[][16], uint8_t color_data[], uint16_t intervals){
  uint8_t src_line_data[sj_length] = {0};
  uint8_t dist_line_data[sj_length] = {0};
  uint8_t tmp_color_data[sj_length * 8] = {0};
  uint8_t tmp_font_data[sj_length][16] = {0};
  uint8_t ram = LOW;

  int n = 0;
  for(int i = 0; i < sj_length; i++){
  
    //8ビット毎の色情報を1ビット毎に変換する
    for(int j = 0; j < 8; j++){
      tmp_color_data[n++] = color_data[i];
    }
  
    //フォントデータを作業バッファにコピー
    for(int j = 0; j < 16; j++){
      tmp_font_data[i][j] = font_data[i][j];
    }

  }

  for(int k = 0; k < sj_length * 8 + 2; k++){
    ram = ~ram;
    digitalWrite(PORT_AB_IN, ram);//RAM-A/RAM-Bに書き込み
    for(int i = 0; i < 16; i++){
      for(int j = 0; j < sj_length; j++){       
        //フォントデータをビットシフト元バッファにコピー
        src_line_data[j] = tmp_font_data[j][i];
      }

      send_line_data(i, src_line_data, tmp_color_data);
      shift_bit_left(dist_line_data, src_line_data, sj_length, 1);

      //font_dataにシフトしたあとのデータを書き込む
      for(int j = 0; j < sj_length; j++){
        tmp_font_data[j][i] = dist_line_data[j];
      }
    }
    shift_color_left(tmp_color_data, tmp_color_data, sj_length);
    delay(intervals);
  }
}

////////////////////////////////////////////////////////////////////
//フォントを静的に表示するメソッド
//
//sj_length:半角文字数
//font_data:フォントデータ（東雲フォント）
//color_data:フォントカラーデータ（半角毎に設定する）//
////////////////////////////////////////////////////////////////////
void printLEDMatrix(int16_t sj_length, uint8_t font_data[][16], uint8_t color_data[]){
  uint8_t src_line_data[sj_length] = {0};
  uint8_t tmp_color_data[sj_length * 8] = {0};
  uint8_t tmp_font_data[sj_length][16] = {0};
  uint8_t ram = LOW;

  int n = 0;
  for(int i = 0; i < sj_length; i++){
  
    //8ビット毎の色情報を1ビット毎に変換する
    for(int j = 0; j < 8; j++){
      tmp_color_data[n++] = color_data[i];
    }
  
    //フォントデータを作業バッファにコピー
    for(int j = 0; j < 16; j++){
      tmp_font_data[i][j] = font_data[i][j];
    }

  }

  for(int k = 0; k < sj_length * 8 + 2; k++){
    ram = ~ram;
    digitalWrite(PORT_AB_IN, ram);//RAM-A/RAM-Bに書き込み
    for(int i = 0; i < 16; i++){
      for(int j = 0; j < sj_length; j++){       
        //フォントデータをビットシフト元バッファにコピー
        src_line_data[j] = tmp_font_data[j][i];
      }

    //    shift_bit_left(dist_line_data, src_line_data, sj_length, 1);
      send_line_data(i, src_line_data, tmp_color_data);
    }
  }
}

void setAllPortOutput(){
  pinMode(PORT_SE_IN, OUTPUT);
  pinMode(PORT_AB_IN, OUTPUT);
  pinMode(PORT_A3_IN, OUTPUT);
  pinMode(PORT_A2_IN, OUTPUT);
  pinMode(PORT_A1_IN, OUTPUT);
  pinMode(PORT_A0_IN, OUTPUT);
  pinMode(PORT_DG_IN, OUTPUT);
  pinMode(PORT_CLK_IN, OUTPUT);
  pinMode(PORT_WE_IN, OUTPUT);
  pinMode(PORT_DR_IN, OUTPUT);
  pinMode(PORT_ALE_IN, OUTPUT);
}

void setAllPortLow(){
  digitalWrite(PORT_SE_IN, LOW);
  digitalWrite(PORT_AB_IN, LOW);
  digitalWrite(PORT_A3_IN, LOW);
  digitalWrite(PORT_A2_IN, LOW);
  digitalWrite(PORT_A1_IN, LOW);
  digitalWrite(PORT_A0_IN, LOW);
  digitalWrite(PORT_DG_IN, LOW);
  digitalWrite(PORT_CLK_IN, LOW);
  digitalWrite(PORT_WE_IN, LOW);
  digitalWrite(PORT_DR_IN, LOW);
  digitalWrite(PORT_ALE_IN, LOW);
}

void setAllPortHigh(){
  digitalWrite(PORT_SE_IN, HIGH);
  digitalWrite(PORT_AB_IN, HIGH);
  digitalWrite(PORT_A3_IN, HIGH);
  digitalWrite(PORT_A2_IN, HIGH);
  digitalWrite(PORT_A1_IN, HIGH);
  digitalWrite(PORT_A0_IN, HIGH);
  digitalWrite(PORT_DG_IN, HIGH);
  digitalWrite(PORT_CLK_IN, HIGH);
  digitalWrite(PORT_WE_IN, HIGH);
  digitalWrite(PORT_DR_IN, HIGH);
  digitalWrite(PORT_ALE_IN, HIGH);
}

void PrintTime(String &str, int flag)
{
  char tmp_str[10] = {0};
  time_t t;
  struct tm *tm;

  t = time(NULL);
  tm = localtime(&t);

  if(flag == 0){
    sprintf(tmp_str, "  %02d:%02d ", tm->tm_hour, tm->tm_min);
  }else{
    sprintf(tmp_str, "  %02d %02d ", tm->tm_hour, tm->tm_min);
  }

  str = tmp_str;
}

void printTimeLEDMatrix(){
  //フォントデータバッファ
  uint8_t time_font_buf[8][16] = {0};
  String str;

  static int flag = 0;

  flag = ~flag;
  PrintTime(str, flag);

  //フォント色データ　str（半角文字毎に設定する）
  uint8_t time_font_color[8] = {G,G,G,G,G,G,G,G};
  uint16_t sj_length = SFR.StrDirect_ShinoFNT_readALL(str, time_font_buf);
  printLEDMatrix(sj_length, time_font_buf, time_font_color);
}

void makeHostStr(String &hostStr){
  hostStr = "Host: ";
  hostStr += server;
}

void makeAgentStr(String &agentStr){
  agentStr = "User-Agent: Yahoo AppID: ";
  agentStr += appid;
}

void getYahooApiJsonInfo(String httpRequest, String &resultJson){
  String getStr;
  String hostStr;
  String agentStr;

  client.setCACert(yahooapi_root_ca);

  Serial.println("\nStarting connection to server...");
  if (!client.connect(server, 443)){
    Serial.println("Connection failed!");
  }else {
    Serial.println("Connected to server!");

    client.println(httpRequest);
    client.println();

    delay(200);

    while (client.connected()) {
      String line = client.readStringUntil('\n');
      if (line == "\r") {
        Serial.println("headers received");
        break;
      }
    }

    delay(200);

    while (client.available()) {
      resultJson += (char)client.read();
    }

    client.stop();
  }
}

void makeGetZipCodeStr(String zipcode, String &getStr){
    getStr = "GET https://map.yahooapis.jp/search/zip/V1/zipCodeSearch?query=";
    getStr += zipcode;
    getStr += "&output=";
    getStr += output;
    getStr += "&appid=";
    getStr += appid;
    getStr += " HTTP/1.1";
} 

void makeZipCodeHttpRequestStr(String &httpRequest){
  String getStr;
  String hostStr;
  String agentStr;
  String coordinates;

  makeGetZipCodeStr(zipcode, getStr);
  makeHostStr(hostStr);
  makeAgentStr(agentStr);

  httpRequest = getStr;
  httpRequest += "\n";
  //httpRequest += hostStr;
  //httpRequest += "\n";
  //httpRequest += agentStr;
  //httpRequest += "\n";
  httpRequest += "Connection: close";
}

void getCoordinatesFromZipcode(String zipcode, String &coordinates){
  String httpRequest;
  String resultJson;
  
  //httpRequestを作成する
  makeZipCodeHttpRequestStr(httpRequest);

  Serial.println(httpRequest);

  getYahooApiJsonInfo(httpRequest, resultJson);

  //Serial.println(resultJson);

  const size_t capacity = JSON_ARRAY_SIZE(0) + JSON_ARRAY_SIZE(1) + JSON_ARRAY_SIZE(2) + JSON_ARRAY_SIZE(3) + 6*JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(7) + 2*JSON_OBJECT_SIZE(8) + 3*JSON_OBJECT_SIZE(9) + 990;

  DynamicJsonDocument doc(capacity);

  deserializeJson(doc, resultJson);

  JsonObject Feature_0 = doc["Feature"][0];

  const char* Feature_0_Geometry_Coordinates = Feature_0["Geometry"]["Coordinates"];

  Serial.printf("Coordinates = %s\n", Feature_0_Geometry_Coordinates);

  coordinates = Feature_0_Geometry_Coordinates;
}

void makeGetStr(String coordinates, String &getStr){
  getStr = "GET https://map.yahooapis.jp/weather/V1/place?coordinates=";
  getStr += coordinates;
  getStr += "&output=";
  getStr += output;
  getStr += "&appid=";
  getStr += appid;
  getStr += " HTTP/1.1";
}

void makeWeatherHttpRequestStr(String &httpRequest){
  String getStr;
  String hostStr;
  String agentStr;
  String coordinates;

  getCoordinatesFromZipcode(zipcode, coordinates);

  makeGetStr(coordinates, getStr);
  makeHostStr(hostStr);
  makeAgentStr(agentStr);

  httpRequest = getStr;
  httpRequest += "\n";
  //httpRequest += hostStr;
  //httpRequest += "\n";
  //httpRequest += agentStr;
  //httpRequest += "\n";
  httpRequest += "Connection: close";
}

void getWeatherStrings(JsonArray &i_weather, int i_index, String &o_type, String &o_date, float &o_rainfall){
  JsonObject Feature_0_Property_WeatherList_Weather_0 = i_weather[i_index];
  const char* Type = Feature_0_Property_WeatherList_Weather_0["Type"];
  const char* Date = Feature_0_Property_WeatherList_Weather_0["Date"];
  float Rainfall = Feature_0_Property_WeatherList_Weather_0["Rainfall"];

  Serial.printf("Type = %s, Date = %s, Rainfall= %f\n", Type, Date, Rainfall);

  o_type = Type;
  o_date = Date;
  o_rainfall = Rainfall;

}

//雨降りの状態
#define RAINFALL_END    0x01  //降雨が終わる時点
#define RAINFALL_NO     0x02  //雨が降っていない状態　
#define RAINFALL_START  0x03  //降雨が開始する時点
#define RAINFALL_NOW    0x04  //雨が継続的に降っている状態

uint16_t getWeatherInfo(int &forcast_time){

  String weatherJsonInfo;
  String httpRequest;

  String type;
  String date;
  float rainfall = 0;
  
  uint16_t weatherInfo = 0;

  makeWeatherHttpRequestStr(httpRequest);

  Serial.println(httpRequest);

  getYahooApiJsonInfo(httpRequest, weatherJsonInfo);

  //Serial.println(weatherJsonInfo);

  const size_t capacity = JSON_ARRAY_SIZE(1) + JSON_ARRAY_SIZE(7) + JSON_OBJECT_SIZE(1) + 3*JSON_OBJECT_SIZE(2) + 7*JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(7) + 660;
  DynamicJsonDocument doc(capacity);

  deserializeJson(doc, weatherJsonInfo);
  JsonObject Feature_0 = doc["Feature"][0];

  JsonArray WeatherList = Feature_0["Property"]["WeatherList"]["Weather"];
  
  getWeatherStrings(WeatherList, 0, type, date, rainfall);

  //現在雨が降っていない
  if(rainfall == 0.00){
    for(int i = 1; i < 7; i++){
      getWeatherStrings(WeatherList, i, type, date, rainfall);
      if(rainfall > 0.00){
        //(i*10-10)分後に雨が降ります。
        forcast_time = i * 10 - 10;
        weatherInfo = RAINFALL_START;
        break;
      }
      else{
        weatherInfo = RAINFALL_NO;
      }
    } 
  }else{//現在雨が降っている
    for(int i = 1; i < 7; i++){
      getWeatherStrings(WeatherList, i, type, date, rainfall);
      if(rainfall == 0.00){
        //(i*10-10)分後に雨が止みます。
        forcast_time = i * 10 - 10;
        weatherInfo = RAINFALL_END;
        break;
      }
      else{
        //しばらく雨が降ります。６０分後も降雨状態
        weatherInfo = RAINFALL_NOW;
      }
    }
  }

  return weatherInfo;
}

portTickType Delay1000 = 1000 / portTICK_RATE_MS; //freeRTOS 用の遅延時間定義
TaskHandle_t hClock;
TaskHandle_t hWeatherInfo;

void ClockTask(void *pvParameters) {
  Serial.printf("ClockTask coreID = %d, ClockTask priority = %d\n", xPortGetCoreID(), uxTaskPriorityGet(hClock));

  BaseType_t xStatus;
  const TickType_t xTicksToWait = 1000UL;
  xSemaphoreGive(xMutex);

  while(1){
      xStatus = xSemaphoreTake(xMutex, xTicksToWait);

      //Serial.println("check for mutex (ClockTask)");

      if(xStatus == pdTRUE){
        time_t t;
        struct tm *tm;

        t = time(NULL);
        tm = localtime(&t);
         
        if(tm->tm_min % informPeriod == 0 && tm->tm_sec < 3){
            Serial.printf("tm_sec = %d\n", tm->tm_sec);
            Serial.println("Give Semaphore(ClockTask)");
            xSemaphoreGive(xMutex);
        }
        else{
          printTimeLEDMatrix();
        }
      }

      //xSemaphoreGive(xMutex);
      delay(500);
  }
}

void printConnecting(void){
  //フォントデータバッファ
  uint8_t font_buf[8][16] = {0};
  //フォント色データ（半角文字毎に設定する）
  uint8_t font_color1[8] = {G,G,G,G,G,G,G,G};

  uint16_t sj_length = SFR.StrDirect_ShinoFNT_readALL("...     ", font_buf);
  printLEDMatrix(sj_length, font_buf, font_color1);
}

void WeatherInfoTask(void *pvParameters){
  Serial.printf("WeatherInfoTask coreID = %d, WeatherInfoTask priority = %d\n", xPortGetCoreID(), uxTaskPriorityGet(hWeatherInfo));

  BaseType_t xStatus;
  const TickType_t xTicksToWait = 500UL;
  xSemaphoreGive(xMutex);

  uint16_t sj_length = 0;//半角文字数 
    
  //フォントデータバッファ
  uint8_t font_buf[100][16] = {0};
  uint8_t yahoo_font_buf[8][16] = {0};

  //フォント色データ　str1（半角文字毎に設定する）
  uint8_t font_color1[100] = {G,G,G,G,G,G,G,G,G,G,
                              G,G,G,G,G,G,G,G,G,G,
                              G,G,G,G,G,G,G,G,G,G,
                              G,G,G,G,G,G,G,G,G,G,
                              G,G,G,G,G,G,G,G,G,G,
                              G,G,G,G,G,G,G,G,G,G,
                              G,G,G,G,G,G,G,G,G,G,
                              G,G,G,G,G,G,G,G,G,G,
                              G,G,G,G,G,G,G,G,G,G,
                              G,G,G,G,G,G,G,G,G,G};

  uint8_t yahoo_font_color[8] = {O,O,O,O,O,O,O,O};

  char tmp_str[100] = {0};

  int forcast_time;

  while(1){

    xStatus = xSemaphoreTake(xMutex, xTicksToWait);

    //Serial.println("check for mutex (WeatherInfoTask)");

    if(xStatus == pdTRUE ){
      
      printConnecting();

      uint16_t weather_state = getWeatherInfo(forcast_time);

      //ここでビープ音を出音する
      if(weather_state != RAINFALL_NO){
        sj_length = SFR.StrDirect_ShinoFNT_readALL(" Yahoo! ", yahoo_font_buf);
        printLEDMatrix(sj_length, yahoo_font_buf, yahoo_font_color);

        delay(1000);

        sj_length = SFR.StrDirect_ShinoFNT_readALL("気象情報", yahoo_font_buf);
        printLEDMatrix(sj_length, yahoo_font_buf, yahoo_font_color);

        delay(1000);

        switch(weather_state){
          case RAINFALL_END:
            if(forcast_time != 0){
              sprintf(tmp_str, "        %d分後に雨が止む予報です。", forcast_time);          
            }else{
              sprintf(tmp_str, "        すぐに雨が止む予報です。");          
            }
          break;
          case RAINFALL_NO://雨は降っていない。60分後の予報もない
            sprintf(tmp_str, "        雨が降る予報はありません。");          
          break;
          case RAINFALL_START:
            if(forcast_time != 0){
              sprintf(tmp_str, "        %d分後に雨が降る予報です。", forcast_time);          
            }else{
              sprintf(tmp_str, "        すぐに雨が降る予報です。");          
            }
          break;
          case RAINFALL_NOW:
            sprintf(tmp_str, "        雨が降っています。しばらく雨が続きます。");    
          break;
          default:
            ;//nothing
        }
        Serial.printf("%s\n", tmp_str);
        sj_length = SFR.StrDirect_ShinoFNT_readALL(tmp_str, font_buf);
        scrollLEDMatrix(sj_length, font_buf, font_color1, 30);
      }
    }

    xSemaphoreGive(xMutex);
    delay(1000);

  }
}

void setup() {

  uint16_t sj_length = 0;//半角文字数 

  delay(1000);
  Serial.begin(115200);
  setAllPortOutput();
  setAllPortLow();

  //手動で表示バッファを切り替える
  digitalWrite(PORT_SE_IN, HIGH);

  //フォントデータバッファ
  uint8_t font_buf[32][16] = {0};
  //フォント色データ　str1（半角文字毎に設定する）
  uint8_t font_color1[32] = {G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G,G};

  SFR.SPIFFS_Shinonome_Init3F(UTF8SJIS_file, Shino_Half_Font_file, Shino_Zen_Font_file);
 
  // 前回接続時情報で接続する
  Serial.println("WiFi begin");
  WiFi.begin();
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);

    // 10秒以上接続できなかったら抜ける
    if ( 10000 < millis() ) {
      break;
    }
  }
  Serial.println("");

  // 未接続の場合にはSmartConfig待受
  if ( WiFi.status() != WL_CONNECTED ) {
    WiFi.mode(WIFI_STA);
    WiFi.beginSmartConfig();

    Serial.println("Waiting for SmartConfig");
    while (!WiFi.smartConfigDone()) {
      delay(500);
      Serial.print("#");
      // 30秒以上接続できなかったら抜ける
      if ( 30000 < millis() ) {
        Serial.println("");
        Serial.println("Reset");
        ESP.restart();
      }
    }

    // Wi-fi接続
    Serial.println("");
    Serial.println("Waiting for WiFi");
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
      // 60秒以上接続できなかったら抜ける
      if ( 60000 < millis() ) {
        Serial.println("");
        Serial.println("Reset");
        ESP.restart();
      }
    }
    Serial.println("");
    Serial.println("WiFi Connected.");
  }

  Serial.println();
  Serial.printf("Connected, IP address: ");
  Serial.println(WiFi.localIP());

  sj_length = SFR.StrDirect_ShinoFNT_readALL("        WiFi Connected.", font_buf);
  scrollLEDMatrix(sj_length, font_buf, font_color1, 30);

  sj_length = SFR.StrDirect_ShinoFNT_readALL("        "+ WiFi.localIP().toString(), font_buf);
  scrollLEDMatrix(sj_length, font_buf, font_color1, 30);

  //時刻取得
  configTime( JST, 0, "ntp.nict.jp", "ntp.jst.mfeed.ad.jp");

  xMutex = xSemaphoreCreateMutex();

  if( xMutex != NULL ){
    xTaskCreatePinnedToCore(ClockTask, "ClockTask", 4096, NULL, 1, &hClock, 1); //ClockTask開始
    xTaskCreatePinnedToCore(WeatherInfoTask, "WeatherInfoTask", 8192, NULL, 6, &hWeatherInfo, 1); //WeatherInfoTask開始
  }else{
    while(1){
        Serial.println("rtos mutex create error, stopped");
        delay(1000);
    }
  }
}

void loop() {
  ;
}
