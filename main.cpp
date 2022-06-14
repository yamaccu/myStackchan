#include <Arduino.h>
#include <M5Unified.h>
#include <Avatar.h>
#include <aquestalk.h>
#include <ServoEasing.hpp>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <wire.h>

// avatar ============================================================
using namespace m5avatar;
Avatar avatar;

char text[25]; //吹き出し用
char talk[50]; // AquesTalk用

const Expression expressions[] = {
    Expression::Angry,
    Expression::Sleepy,
    Expression::Happy,
    Expression::Sad,
    Expression::Doubt,
    Expression::Neutral};
int idx = 0;

// aques talk =========================================================
const char *AQUESTALK_KEY = "XXXX-XXXX-XXXX-XXXX";

/// set M5Speaker virtual channel (0-7)
static constexpr uint8_t m5spk_virtual_channel = 0;
static constexpr uint8_t LEN_FRAME = 32;
static uint32_t workbuf[AQ_SIZE_WORKBUF];
static TaskHandle_t task_handle = nullptr;
volatile bool is_talking = false;

static void talk_task(void *)
{
  int16_t wav[3][LEN_FRAME];
  int tri_index = 0;
  for (;;)
  {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // wait notify
    while (is_talking)
    {
      uint16_t len;
      if (CAqTkPicoF_SyntheFrame(wav[tri_index], &len))
      {
        is_talking = false;
        break;
      }
      M5.Speaker.playRaw(wav[tri_index], len, 8000, false, 1, m5spk_virtual_channel, false);

      // Serial.printf("level%d\n", wav[tri_index][0]);

      float ratio = (float)(abs(wav[tri_index][0]) / 12000.0f);
      if (ratio > 1.0f)
      {
        ratio = 1.0f;
      }
      avatar.setMouthOpenRatio(ratio);
      tri_index = tri_index < 2 ? tri_index + 1 : 0;
    }
  }
}

/// 音声再生の終了を待機する;
static void waitAquesTalk(void)
{
  while (is_talking)
  {
    vTaskDelay(1);
  }
}

/// 音声再生を停止する;
static void stopAquesTalk(void)
{
  if (is_talking)
  {
    is_talking = false;
    vTaskDelay(1);
  }
}

/// 音声再生を開始する。(再生中の場合は中断して新たな音声再生を開始する) ;
static void playAquesTalk(const char *koe)
{
  stopAquesTalk();

  int iret = CAqTkPicoF_SetKoe((const uint8_t *)koe, 100, 0xFFu);
  if (iret)
  {
    Serial.println("ERR:CAqTkPicoF_SetKoe");
  }

  is_talking = true;
  xTaskNotifyGive(task_handle);
}

// wifi ===============================================================
const char *ssid = "xxx";
const char *password = "xxx";

// 画像を入れるフォルダ名
const char *pictureFolder = "/pictures/";

// 天気予報取得先
const char *endpointTenki = "https://www.drk7.jp/weather/json/13.js";
const char *region = "東京地方";

// AquesTalk内容のソフトコーディング用
const char *endpointTalk = "https://api.sssapi.app/g3n1Esr9UnOOaLRd5sls_";

DynamicJsonDocument weatherInfo(20000);
DynamicJsonDocument talkInfo(20000);

// JSONP形式からJSON形式に変える
String createJson(String jsonString)
{
  jsonString.replace("drk7jpweather.callback(", "");
  return jsonString.substring(0, jsonString.length() - 2);
}

DynamicJsonDocument getJson(const char *endpoint)
{
  DynamicJsonDocument doc(20000);

  if ((WiFi.status() == WL_CONNECTED))
  {
    HTTPClient http;
    http.begin(endpoint);
    int httpCode = http.GET();
    if (httpCode > 0 && endpoint == endpointTenki)
    {
      // jsonオブジェクトの作成
      String jsonString = createJson(http.getString());
      deserializeJson(doc, jsonString);
    }
    else if (httpCode > 0)
    {
      String jsonString = http.getString();
      deserializeJson(doc, jsonString);
      Serial.println(jsonString);
    }
    if (httpCode <= 0)
    {
      Serial.println("Error on HTTP request");
    }
    http.end(); //リソースを解放
  }
  return doc;
}

void drawTemperature(String maxTemperature, String minTemperature)
{
  M5.Lcd.setTextColor(RED);
  M5.Lcd.setTextSize(4);
  M5.Lcd.setCursor(15, 80);
  M5.Lcd.print(maxTemperature);

  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setCursor(70, 80);
  M5.Lcd.print("|");

  M5.Lcd.setTextColor(BLUE);
  M5.Lcd.setCursor(105, 80);
  M5.Lcd.print(minTemperature);
}

void drawRainfallChancce(String rfc0_6, String rfc6_12, String rfc12_18, String rfc18_24)
{
  M5.Lcd.setTextSize(3);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setCursor(27, 200);
  M5.Lcd.print(rfc0_6);

  M5.Lcd.setCursor(92, 200);
  M5.Lcd.print(rfc6_12);

  M5.Lcd.setCursor(165, 200);
  M5.Lcd.print(rfc12_18);

  M5.Lcd.setCursor(245, 200);
  M5.Lcd.print(rfc18_24);
}

void drawDate(String date)
{
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setTextSize(3);
  M5.Lcd.setCursor(10, 10);
  M5.Lcd.print(date);
}

void drawWeather(String infoWeather)
{
  M5.Lcd.clear();
  DynamicJsonDocument doc(20000);
  deserializeJson(doc, infoWeather);
  String weather = doc["weather"];
  String filename = "";

  if (weather.indexOf("雨") != -1)
  {
    if (weather.indexOf("曇") != -1)
    {
      filename = "rainyandcloudy.jpg";
      playAquesTalk("kumori/tokidoki/a'me");
    }
    else
    {
      filename = "rainy.jpg";
      playAquesTalk("a'me");
    }
  }
  else if (weather.indexOf("晴") != -1)
  {
    if (weather.indexOf("曇") != -1)
    {
      filename = "sunnyandcloudy.jpg";
      playAquesTalk("kumori/tokidoki/hare");
    }
    else
    {
      filename = "sunny.jpg";
      playAquesTalk("hare");
    }
  }
  else if (weather.indexOf("雪") != -1)
  {
    filename = "snow.jpg";
    playAquesTalk("yuki");
  }
  else if (weather.indexOf("曇") != -1)
  {
    filename = "cloudy.jpg";
    playAquesTalk("kumori");
  }

  if (filename.equals(""))
  {
    return;
  }

  String filePath = "/" + filename;
  M5.Lcd.drawJpgFile(SPIFFS, filePath);

  String maxTemperature = doc["temperature"]["range"][0]["content"];
  String minTemperature = doc["temperature"]["range"][1]["content"];
  drawTemperature(maxTemperature, minTemperature);

  String railfallchance0_6 = doc["rainfallchance"]["period"][0]["content"];
  String railfallchance6_12 = doc["rainfallchance"]["period"][1]["content"];
  String railfallchance12_18 = doc["rainfallchance"]["period"][2]["content"];
  String railfallchance18_24 = doc["rainfallchance"]["period"][3]["content"];
  drawRainfallChancce(railfallchance0_6, railfallchance6_12, railfallchance12_18, railfallchance18_24);

  drawDate(doc["date"]);
  waitAquesTalk();
  playAquesTalk("dayo.");
}

void drawTodayWeather()
{
  String today = weatherInfo["pref"]["area"][region]["info"][0];
  Serial.println(today);
  drawWeather(today);
}

void drawTomorrowWeather()
{
  String tomorrow = weatherInfo["pref"]["area"][region]["info"][1];
  Serial.println(tomorrow);
  drawWeather(tomorrow);
}

// サーボ ==============================================================
#define SERVO_PIN_X G5
#define SERVO_PIN_Y G2
#define START_DEGREE_VALUE_X 90
#define START_DEGREE_VALUE_Y 90

ServoEasing servo_x;
ServoEasing servo_y;
unsigned long startTime = 0;
int moveServoInterval = 8000;

void moveServo()
{
  int x = random(60, 120);
  int y = random(72, 93);
  int delay_time = random(900);
  servo_x.setEaseToD(x, 400 + delay_time);
  servo_y.setEaseToD(y, 400 + delay_time);
  synchronizeAllServosStartAndWaitForAllServosToStop();
}

// main =====================================================================

void setup()
{
  Serial.begin(115200);
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Power.begin();
  SPIFFS.begin();

  servo_x.attach(SERVO_PIN_X,
                 START_DEGREE_VALUE_X,
                 DEFAULT_MICROSECONDS_FOR_0_DEGREE,
                 DEFAULT_MICROSECONDS_FOR_180_DEGREE);
  servo_y.attach(SERVO_PIN_Y,
                 START_DEGREE_VALUE_Y,
                 DEFAULT_MICROSECONDS_FOR_0_DEGREE,
                 DEFAULT_MICROSECONDS_FOR_180_DEGREE);
  servo_x.setEasingType(EASE_QUADRATIC_IN_OUT);
  servo_y.setEasingType(EASE_QUADRATIC_IN_OUT);
  setSpeedForAllServos(200);

  M5.Lcd.setBrightness(30);
  M5.Lcd.clear();
  int iret = CAqTkPicoF_Init(workbuf, LEN_FRAME, AQUESTALK_KEY);
  if (iret)
  {
    M5.Display.println("ERR:CAqTkPicoF_Init");
  }
  avatar.init();
  xTaskCreateUniversal(talk_task, "talk_task", 4096, nullptr, 1, &task_handle, APP_CPU_NUM);

  M5.Speaker.setVolume(70);
}

void loop()
{
  bool stopWatchMode = false;
  bool weatherMode = false;
  bool questionMode = false;
  unsigned long now = millis();

  if ((now - startTime) > moveServoInterval)
  {
    moveServo();
    startTime = millis();
    moveServoInterval = 5000 + random(10000);
    avatar.setSpeechText("");
  }

  M5.update();
  if (M5.BtnA.wasClicked()) {stopWatchMode = true;}
  else if (M5.BtnB.wasClicked()) {questionMode = true;}
  else if (M5.BtnC.wasClicked()) {weatherMode = true;}


  if (stopWatchMode) //=========================================================
  {
    servo_x.setEaseToD(90, 1000);
    servo_y.setEaseToD(80, 1000);
    startTime = millis();
    playAquesTalk("ta'ima-/_suta'-to/suruyo-.");
    avatar.setSpeechText(text);

    while (true)
    {
      unsigned long collapseTime = millis() - startTime;
      if (collapseTime / 1000 <= 60)
      {
        sprintf(text, "%d sec", collapseTime / 1000);
      }
      else
      {
        int minutes = collapseTime / 60000;
        sprintf(text, "%d min %d sec", minutes, (collapseTime / 1000) % 60);
      }

      if (collapseTime / 1000 == 30)
      {
        playAquesTalk("<NUMK VAL=30 COUNTER=byo->/ke-ka.");
      }
      else if ((collapseTime / 1000) % 60 == 0 && collapseTime / 1000 != 0)
      {
        int minutes = collapseTime / 60000;
        sprintf(talk, "<NUMK VAL=%d COUNTER=funn>/ke-ka.", minutes);
        playAquesTalk(talk);
      }
      delay(10);

      M5.update();
      if (M5.BtnA.wasClicked() || M5.BtnB.wasClicked() || M5.BtnC.wasClicked())
      {
        int finishTimeMinute = collapseTime / 60000;
        int finishTimeSecond = (collapseTime / 1000) % 60;
        avatar.setExpression(expressions[2]);

        playAquesTalk("kaka'tta/jikannwa");
        waitAquesTalk();
        if (finishTimeMinute > 0)
        {
          sprintf(talk, "<NUMK VAL=%d COUNTER=funn>", finishTimeMinute);
          playAquesTalk(talk);
          waitAquesTalk();
        }
        sprintf(talk, "<NUMK VAL=%d COUNTER=byo->", finishTimeSecond);
        playAquesTalk(talk);
        waitAquesTalk();
        playAquesTalk("dayo");
        waitAquesTalk();
        delay(200);

        playAquesTalk("o_tsukaresamade'shi'ta-");
        waitAquesTalk();

        startTime = millis();
        avatar.setExpression(expressions[5]);
        break;
      }
      pinMode(26, INPUT_PULLDOWN); // スピーカーノイズ対策
    }
  }

  if (weatherMode)  //天気予報表示 https://kuracux.hatenablog.jp/entry/2019/07/13/101143
  { 
    avatar.setSpeechText("");
    WiFi.begin(ssid, password);

    startTime = millis();
    while (WiFi.status() != WL_CONNECTED)
    {
      int wifiTryTimes = 0;
      delay(1000);
      Serial.println("Connecting to WiFi..");
      playAquesTalk("tu-shintyu-");
      waitAquesTalk();

      if (millis() - startTime > 6000)
      {
        wifiTryTimes++;
        startTime = millis();
        WiFi.disconnect(true);
        delay(100);
        WiFi.begin(ssid, password);
      }

      if (wifiTryTimes > 2)
      {
        playAquesTalk("tu-shin/sippai");
        break;
      }
    }
    Serial.println("Connected to the WiFi network");

    weatherInfo = getJson(endpointTenki);
    WiFi.disconnect(true);

    playAquesTalk("kyo'-no/te'nnkiwa.");
    waitAquesTalk();
    avatar.stop();

    delay(500);

    drawTodayWeather();

    while (true)
    {
      M5.update();
      if (M5.BtnA.wasClicked() || M5.BtnB.wasClicked() || M5.BtnC.wasClicked())
      {
        playAquesTalk("a_shitano/te'nnkiwa.");
        waitAquesTalk();
        drawTomorrowWeather();
        break;
      }
    }

    while (true)
    {
      M5.update();
      if (M5.BtnA.wasClicked() || M5.BtnB.wasClicked() || M5.BtnC.wasClicked())
      {
        break;
      }
    }
    avatar.start();
    startTime = millis();
  }

  if (questionMode)  //========================================================
  {
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
      delay(1000);
      Serial.println("Connecting to WiFi..");
      playAquesTalk("tu-shintyu-");
      waitAquesTalk();
    }
    Serial.println("Connected to the WiFi network");
    talkInfo = getJson(endpointTalk);
    WiFi.disconnect(true);
  }

  pinMode(26, INPUT_PULLDOWN); // スピーカーノイズ対策
}