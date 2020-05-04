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
#include <ESP32_SPIFFS_ShinonomeFNT.h>
#include <ESP32_SPIFFS_UTF8toSJIS.h>
#include <time.h>
#include <stdio.h>
#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include "SPIFFS.h"
#include <ESPmDNS.h>
#include <WiFiClient.h>
#include <Servo.h>

#define ESP32_AP_MODE false

#define JST 3600 * 9

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

#define PANEL_NUM 2      //パネル枚数
#define R 1              //赤色
#define O 2              //橙色
#define G 3              //緑色
#define FONT_BUF_SIZE 100 //半角文字数

//これらのファイルをSPIFFS領域へコピーしておくこと
const char *UTF8SJIS_file = "/Utf8Sjis.tbl";        //UTF8 Shift_JIS 変換テーブルファイル名を記載しておく
const char *Shino_Zen_Font_file = "/shnmk16.bdf";   //全角フォントファイル名を定義
const char *Shino_Half_Font_file = "/shnm8x16.bdf"; //半角フォントファイル名を定義

ESP32_SPIFFS_ShinonomeFNT SFR; //東雲フォントをSPIFFSから取得するライブラリ

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

//LEDマトリクスの書き込みアドレスを設定するメソッド
void setRAMAdder(uint8_t lineNumber)
{
  uint8_t A[4] = {0};
  uint8_t adder = 0;

  adder = lineNumber;

  for (int i = 0; i < 4; i++)
  {
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
void send_line_data(uint8_t iram_adder, uint8_t ifont_data[], uint8_t color_data[])
{

  uint8_t font[8] = {0};
  uint8_t tmp_data = 0;
  int k = 0;
  for (int j = 0; j < 4 * PANEL_NUM; j++)
  {
    //ビットデータに変換
    tmp_data = ifont_data[j];
    for (int i = 0; i < 8; i++)
    {
      font[i] = tmp_data % 2;
      tmp_data /= 2;
    }

    for (int i = 7; i >= 0; i--)
    {
      digitalWrite(PORT_DG_IN, LOW);
      digitalWrite(PORT_DR_IN, LOW);
      digitalWrite(PORT_CLK_IN, LOW);

      if (font[i] == 1)
      {
        if (color_data[k] == R)
        {
          digitalWrite(PORT_DR_IN, HIGH);
        }

        if (color_data[k] == G)
        {
          digitalWrite(PORT_DG_IN, HIGH);
        }

        if (color_data[k] == O)
        {
          digitalWrite(PORT_DR_IN, HIGH);
          digitalWrite(PORT_DG_IN, HIGH);
        }
      }
      else
      {
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
void shift_bit_left(uint8_t dist[], uint8_t src[], int len, int n)
{
  uint8_t mask = 0xFF << (8 - n);
  for (int i = 0; i < len; i++)
  {
    if (i < len - 1)
    {
      dist[i] = (src[i] << n) | ((src[i + 1] & mask) >> (8 - n));
    }
    else
    {
      dist[i] = src[i] << n;
    }
  }
}

void shift_color_left(uint8_t dist[], uint8_t src[], int len)
{
  for (int i = 0; i < len * 8; i++)
  {
    if (i < len * 8 - 1)
    {
      dist[i] = src[i + 1];
    }
    else
    {
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
void scrollLEDMatrix(int16_t sj_length, uint8_t font_data[][16], uint8_t color_data[], uint16_t intervals)
{
  uint8_t src_line_data[sj_length] = {0};
  uint8_t dist_line_data[sj_length] = {0};
  uint8_t tmp_color_data[sj_length * 8] = {0};
  uint8_t tmp_font_data[sj_length][16] = {0};
  uint8_t ram = LOW;

  int n = 0;
  for (int i = 0; i < sj_length; i++)
  {

    //8ビット毎の色情報を1ビット毎に変換する
    for (int j = 0; j < 8; j++)
    {
      tmp_color_data[n++] = color_data[i];
    }

    //フォントデータを作業バッファにコピー
    for (int j = 0; j < 16; j++)
    {
      tmp_font_data[i][j] = font_data[i][j];
    }
  }

  for (int k = 0; k < sj_length * 8 + 2; k++)
  {
    ram = ~ram;
    digitalWrite(PORT_AB_IN, ram); //RAM-A/RAM-Bに書き込み
    for (int i = 0; i < 16; i++)
    {
      for (int j = 0; j < sj_length; j++)
      {
        //フォントデータをビットシフト元バッファにコピー
        src_line_data[j] = tmp_font_data[j][i];
      }

      send_line_data(i, src_line_data, tmp_color_data);
      shift_bit_left(dist_line_data, src_line_data, sj_length, 1);

      //font_dataにシフトしたあとのデータを書き込む
      for (int j = 0; j < sj_length; j++)
      {
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
void printLEDMatrix(int16_t sj_length, uint8_t font_data[][16], uint8_t color_data[])
{
  uint8_t src_line_data[sj_length] = {0};
  uint8_t tmp_color_data[sj_length * 8] = {0};
  uint8_t tmp_font_data[sj_length][16] = {0};
  uint8_t ram = LOW;

  int n = 0;
  for (int i = 0; i < sj_length; i++)
  {

    //8ビット毎の色情報を1ビット毎に変換する
    for (int j = 0; j < 8; j++)
    {
      tmp_color_data[n++] = color_data[i];
    }

    //フォントデータを作業バッファにコピー
    for (int j = 0; j < 16; j++)
    {
      tmp_font_data[i][j] = font_data[i][j];
    }
  }

  for (int k = 0; k < sj_length * 8 + 2; k++)
  {
    ram = ~ram;
    digitalWrite(PORT_AB_IN, ram); //RAM-A/RAM-Bに書き込み
    for (int i = 0; i < 16; i++)
    {
      for (int j = 0; j < sj_length; j++)
      {
        //フォントデータをビットシフト元バッファにコピー
        src_line_data[j] = tmp_font_data[j][i];
      }

      //    shift_bit_left(dist_line_data, src_line_data, sj_length, 1);
      send_line_data(i, src_line_data, tmp_color_data);
    }
  }
}

void setAllPortOutput()
{
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

void setAllPortLow()
{
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

void setAllPortHigh()
{
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

  if (flag == 0)
  {
    sprintf(tmp_str, "  %02d:%02d ", tm->tm_hour, tm->tm_min);
  }
  else
  {
    sprintf(tmp_str, "  %02d %02d ", tm->tm_hour, tm->tm_min);
  }

  str = tmp_str;
}

void PrintTime(){

  time_t t;
  struct tm *tm;

  t = time(NULL);
  tm = localtime(&t);

  Serial.printf("%d年%d月%d日%d時%d分%d秒\n",tm->tm_year + 1900, 
                                            tm->tm_mon + 1,
                                            tm->tm_mday,
                                            tm->tm_hour,
                                            tm->tm_min,
                                            tm->tm_sec);

}

void printTimeLEDMatrix()
{
  //フォントデータバッファ
  uint8_t time_font_buf[8][16] = {0};
  String str;

  static int flag = 0;

  flag = ~flag;
  PrintTime(str, flag);

  //フォント色データ　str（半角文字毎に設定する）
  uint8_t time_font_color[8] = {G, G, G, G, G, G, G, G};
  uint16_t sj_length = SFR.StrDirect_ShinoFNT_readALL(str, time_font_buf);
  printLEDMatrix(sj_length, time_font_buf, time_font_color);
}

void printStatic(String staticStr)
{
  //フォントデータバッファ
  uint8_t font_buf[8][16] = {0};
  //フォント色データ（半角文字毎に設定する）
  uint8_t font_color[8] = {G, G, G, G, G, G, G, G};

  uint16_t sj_length = SFR.StrDirect_ShinoFNT_readALL(staticStr, font_buf);
  
  if (sj_length < 9){
    printLEDMatrix(sj_length, font_buf, font_color);
  }
  else{
    Serial.printf("printStatic():Too many characters! length = %d\n", staticStr.length());
  }
}

void printScroll(String scrollStr)
{
  //フォントデータバッファ
  uint8_t font_buf[FONT_BUF_SIZE][16] = {0};
  //フォント色データ　str1（半角文字毎に設定する）
  uint8_t font_color[FONT_BUF_SIZE] = {G, G, G, G, G, G, G, G, G, G,
                                       G, G, G, G, G, G, G, G, G, G,
                                       G, G, G, G, G, G, G, G, G, G,
                                       G, G, G, G, G, G, G, G, G, G,
                                       G, G, G, G, G, G, G, G, G, G,
                                       G, G, G, G, G, G, G, G, G, G,
                                       G, G, G, G, G, G, G, G, G, G,
                                       G, G, G, G, G, G, G, G, G, G,
                                       G, G, G, G, G, G, G, G, G, G,
                                       G, G, G, G, G, G, G, G, G, G};

  uint16_t sj_length = SFR.StrDirect_ShinoFNT_readALL(scrollStr, font_buf);
  
  if (sj_length < FONT_BUF_SIZE + 1){
    scrollLEDMatrix(sj_length, font_buf, font_color, 30);
  }
  else{
    Serial.printf("printScroll():Too many characters! length = %d\n", scrollStr.length());
  }
}

void ReleaseRocket(){


}

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
  setAllPortOutput();
  setAllPortLow();

  //手動で表示バッファを切り替える
  digitalWrite(PORT_SE_IN, HIGH);

  SFR.SPIFFS_Shinonome_Init3F(UTF8SJIS_file, Shino_Half_Font_file, Shino_Zen_Font_file);

  myservo.attach(SERVO_NUM);
  //pinMode(SERVO_NUM, OUTPUT);
  
  // Serial port for debugging purposes
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);

  printStatic("Init....");

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
  
  //時刻取得
  configTime(JST, 0, "ntp.nict.jp", "ntp.jst.mfeed.ad.jp");

  PrintTime();

#else
  Serial.println();
  Serial.println("Configuring access point...");

  //アクセスポイントを起動する
  WiFi.softAP(ssid, password);

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  /* Set up mDNS */
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
    printStatic("Connect.");
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
    printStatic("TimerON");
    delay(1000);
    gMsgEventID = MSG_TIMER_START;
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });

  //タイマーをOFFする
  server.on("/stopcountdown", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("/stopcountdown");
    printStatic("Stopped.");
    //リセット
    myservo.write(0);
    gMsgEventID = MSG_NOTHING;
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });

  //Webサーバー開始
  server.begin();

#if (ESP32_AP_MODE == flase)
  //printScroll("        [Client] Mode");
  //printScroll("        Web server started.");
  //printScroll("        " + WiFi.localIP().toString());
  Serial.println("[Client] Mode");
  Serial.println("Web server started.");
  Serial.println(WiFi.localIP().toString());
#else
  printScroll("        [Access Point] Mode");
  printScroll("        Web server started.");
  printScroll("        " + myIP.toString());
  Serial.println("[Access Point] Mode");
  Serial.println("Web server started.");
  Serial.println(myIP.toString());
#endif

  myservo.write(0);

  printStatic("Ready...");
}

//Event Message Loop
void loop()
{
  char tmp_str[10] = {0};
  static int nCount = 10;
  String sCountDown;

  switch(gMsgEventID){
    case MSG_TIMER_START:
      if(nCount > 0){
        sprintf(tmp_str, "      %02d", nCount);
        sCountDown = tmp_str;
        printStatic(sCountDown);
        delay(1000);
        nCount--;
      }
      else{
        printStatic(">>GO!!<<");
        //サーボ動作
        myservo.write(90);
        delay(100);
        //イグナイター操作
        digitalWrite(ledPin, HIGH);
        delay(50);
        digitalWrite(ledPin, LOW);
        //リセット
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
