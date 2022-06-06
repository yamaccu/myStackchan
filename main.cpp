#include <Arduino.h>
#include <M5Unified.h>
#include <Avatar.h>
#include <aquestalk.h>
#include <ServoEasing.hpp>
#include <WiFiClientSecure.h>

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
const char *ssid = "aterm-a49c44-g";
const char *password = "06fda97276af7";
const int port = 443;

const char *jp_weather_host = "www.jma.go.jp";
String office_code = "120000"; //千葉県
String jp_weather_area_url = "/bosai/forecast/data/forecast/" + office_code + ".json";
String area_code_str = "120020"; //千葉北東

String search_tag = "code\":\"" + area_code_str + "\"},\"weatherCodes\":[\""; //★要修正

WiFiClientSecure client;

const char* classifyWeatherCode(uint16_t weather_code)
{
  const char* weatherText;

  switch (weather_code)
  {
  //--------Clear（晴れ）-----------------
  case 100:
  case 123:
  case 124:
  case 130:
  case 131:
    weatherText = "hare";
    Serial.println("晴れ");
    break;

  //--------晴れ時々（一時）曇り----------------
  case 101:
  case 132:
    Serial.println("晴れ時々曇り");
    break;

  //--------晴れ時々（一時）雨----------------
  case 102:
  case 103:
  case 106:
  case 107:
  case 108:
  case 120:
  case 121:
  case 140:
    Serial.println("晴れ時々雨");
    break;

  //--------晴れ時々（一時）雪----------------
  case 104:
  case 105:
  case 160:
  case 170:
    Serial.println("晴れ時々雪");
    break;

  //--------晴れ後曇り----------------
  case 110:
  case 111:
    Serial.println("晴れ後曇り");
    break;

  //--------晴れ後雨----------------
  case 112:
  case 113:
  case 114:
  case 118:
  case 119:
  case 122:
  case 125:
  case 126:
  case 127:
  case 128:
    Serial.println("晴れ後雨");
    break;

  //--------晴れ後雪----------------
  case 115:
  case 116:
  case 117:
  case 181:
    Serial.println("晴れ後雪");
    break;

  //--------曇り-----------------
  case 200:
  case 209:
  case 231:
    Serial.println("曇り");
    break;

  //--------曇り時々晴れ-----------------
  case 201:
  case 223:
    Serial.println("曇り時々晴れ");
    break;

  //--------曇り時々雨-----------------
  case 202:
  case 203:
  case 206:
  case 207:
  case 208:
  case 220:
  case 221:
  case 240:
    weatherText = "kumori'/tokidoki/a'me.";
    sprintf(text, "Sunny & cloudy");
    Serial.println("曇り時々雨");
    break;

  //--------曇り一時雪-----------------
  case 204:
  case 205:
  case 250:
  case 260:
  case 270:
    Serial.println("曇り一時雪");
    break;

  //--------曇り後晴れ-----------------
  case 210:
  case 211:
    Serial.println("曇り後晴れ");
    break;

  //--------曇り後雨-----------------
  case 212:
  case 213:
  case 214:
  case 218:
  case 219:
  case 222:
  case 224:
  case 225:
  case 226:
    Serial.println("曇り後雨");
    break;

  //--------曇り後雪-----------------
  case 215:
  case 216:
  case 217:
  case 228:
  case 229:
  case 230:
  case 281:
    Serial.println("曇り後雪");
    break;

  //--------雨-----------------
  case 300:
  case 304:
  case 306:
  case 328:
  case 329:
  case 350:
    Serial.println("雨");
    break;

  //--------雨時々晴れ-----------------
  case 301:
    Serial.println("雨時々晴れ");
    break;

  //--------雨時々曇り-----------------
  case 302:
    Serial.println("雨時々曇り");
    break;

  //--------雨時々雪-----------------
  case 303:
  case 309:
  case 322:
    Serial.println("雨時々雪");
    break;

  //--------暴風雨-----------------
  case 308:
    Serial.println("暴風雨");
    break;

  //--------雨後晴れ-----------------
  case 311:
  case 316:
  case 320:
  case 323:
  case 324:
  case 325:
    Serial.println("雨後晴れ");
    break;

  //--------雨後曇り-----------------
  case 313:
  case 317:
  case 321:
    Serial.println("雨後曇り");
    break;

  //--------雨後雪-----------------
  case 314:
  case 315:
  case 326:
  case 327:
    Serial.println("雨後雪");
    break;

  //--------雪-----------------
  case 340:
  case 400:
  case 405:
  case 425:
  case 426:
  case 427:
  case 450:
    Serial.println("雪");
    break;

  //--------雪時々晴れ-----------------
  case 401:
    Serial.println("雪時々晴れ");
    break;

  //--------雪時々曇り-----------------
  case 402:
    Serial.println("雪時々曇り");
    break;

  //--------雪時々雨-----------------
  case 403:
  case 409:
    Serial.println("雪時々雨");
    break;

  //--------暴風雪-----------------
  case 406:
  case 407:
    Serial.println("暴風雪");
    break;

  //--------雪後晴れ-----------------
  case 361:
  case 411:
  case 420:
    Serial.println("雪後晴れ");
    break;

  //--------雪後曇り-----------------
  case 371:
  case 413:
  case 421:
    Serial.println("雪後曇り");
    break;

  //--------雪後雨-----------------
  case 414:
  case 422:
  case 423:
    Serial.println("雪後雨");
    break;

  default:
    break;
  }

  return weatherText;
}

void extractWeatherCodes(String ret_str, String weather_code_from_key)
{
  uint8_t from1 = ret_str.indexOf(weather_code_from_key, 0) + weather_code_from_key.length();
  uint8_t to1 = from1 + 3;
  String today_w_code_str = ret_str.substring(from1, to1);
  uint16_t today_weather_code = atoi(today_w_code_str.c_str());
  //Serial.print("Today Weather Coode = ");
  //Serial.println(today_weather_code);
  //Serial.print("今日の天気：");
  playAquesTalk("kyo'-no/te'nnkiwa");
  waitAquesTalk();

  playAquesTalk(classifyWeatherCode(today_weather_code));
  waitAquesTalk();

  delay(100);

  playAquesTalk("dayo-");
  waitAquesTalk();
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
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Power.begin();

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
  bool quizMode = false;
  unsigned long now = millis();

  if ((now - startTime) > moveServoInterval)
  {
    moveServo();
    startTime = millis();
    moveServoInterval = 5000 + random(10000);
    avatar.setSpeechText("");
  }

  M5.update();
  if (M5.BtnA.wasClicked()) // 簡易ストップウォッチ
  {
    servo_x.setEaseToD(90, 1000);
    servo_y.setEaseToD(80, 1000);
    stopWatchMode = true;
    startTime = millis();
    playAquesTalk("taima--/suta-to/suruyo-");
    avatar.setSpeechText(text);
  }
  else if (M5.BtnB.wasClicked())
  {
    playAquesTalk("kyouno/tenki");
  }
  else if (M5.BtnC.wasClicked())
  {
    weatherMode = true;
    avatar.setSpeechText(text);
  }

  while (stopWatchMode)
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
      playAquesTalk("<NUMK VAL=30 COUNTER=byo->/tattayo-");
    }
    else if ((collapseTime / 1000) % 60 == 0 && collapseTime / 1000 != 0)
    {
      int minutes = collapseTime / 60000;
      sprintf(talk, "<NUMK VAL=%d COUNTER=funn>/ta'ttayo-.", minutes);
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

      if (finishTimeSecond % 2 == 0)
      {
        playAquesTalk("gannba'ttane'");
        waitAquesTalk();
      }
      else
      {
        playAquesTalk("o_tsukaresamade'shi'ta-");
        waitAquesTalk();
      }

      stopWatchMode = false;
      startTime = millis();
      avatar.setExpression(expressions[5]);
    }
    pinMode(26, INPUT_PULLDOWN); // スピーカーノイズ対策
  }


  if (weatherMode)
  {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
      sprintf(text, "connecting");
      playAquesTalk("tsu-shinnchu-.");
      waitAquesTalk();
      delay(1000);
    }
    client.setInsecure(); //ルートCA無しの場合

    uint32_t time_out = millis();
    while (true)
    {
      if (client.connect(jp_weather_host, port))
      {
        playAquesTalk("tsu-shinnseikou.");
        sprintf(text, "connected");

        String req_header_str = String("GET ");
        req_header_str += String(jp_weather_area_url) + " HTTP/1.1\r\n";
        req_header_str += "Host: ";
        req_header_str += String(jp_weather_host) + "\r\n";
        req_header_str += "User-Agent: BuildFailureDetectorESP32\r\n";
        req_header_str += "Accept: text/html,application/xhtml+xml,application/xml\r\n";
        req_header_str += "Connection: close\r\n\r\n";

        client.print(req_header_str);
        break;
      }

      if ((millis() - time_out) > 8000)
      {
        playAquesTalk("tsu-shinn/sippai.");
        sprintf(text, "failed");
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        break;
      }
      delay(1);
    }

    String ret_str;
    time_out = millis();

    if (client)
    {
      String tmp_str;
      if (client.connected())
      {
        while (true)
        {
          if ((millis() - time_out) > 60000)
          {
            sprintf(text, "failed");
            break;
          }

          //★★ここから要修正
          tmp_str = client.readStringUntil(']');
          Serial.println(tmp_str);
          if (tmp_str.indexOf(search_tag) >= 0)
          {
            ret_str += tmp_str;
            ret_str += "] ";
            break;
          }
          delay(1);
        }

        while (client.available())
        {
          if ((millis() - time_out) > 60000)
            break; // 60seconds Time Out
          client.read();
          delay(1);
        }

        delay(10);
        client.stop();
        delay(10);
      }
    }

    if (ret_str.length() < 20)
      ret_str = "※JSON GETできませんでした";

    if (client)
    {
      delay(10);
      client.stop();
      delay(10);
    }

    Serial.print("抽出した文字列：");
    Serial.println(ret_str);

    String weather_code_from_key = "weatherCodes\":[\"";
    extractWeatherCodes(ret_str, weather_code_from_key);

    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);

    weatherMode = false;
    startTime = millis();
  }

  pinMode(26, INPUT_PULLDOWN); // スピーカーノイズ対策
}