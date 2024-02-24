#include "M5Dial.h"
#include "FS.h"
#include "SPIFFS.h"
#include "ConnectWifi.h"
#include "AWSIoTClient.h"
M5Canvas img(&M5Dial.Display);

// メニューリスト
String menuItems[9] = {"20sec Timer", "10min Timer", "25min Timer", "Stop Watch", "Exit", "Status", "Reset", "Timer", "Tweet"};
int numItem = sizeof(menuItems) / sizeof(menuItems[0]);
int viewItemIndex[5] = {0, 1, 2, 3, 4};
uint16_t baseItemPosition[5][2] = {
    {10 + 240 / 2, 0 + 20},
    {240 / 2.5, 10 + 240 / 4},
    {240 / 4, 240 / 2},
    {240 / 2.5, -10 + 240 / 2 + 240 / 4},
    {10 + 240 / 2, 240 - 20}};
double viewItemTextSize[5] = {0.5, 0.7, 1, 0.7, 0.5};
uint16_t viewItemTextColor[5] = {TFT_DARKGREY, TFT_DARKGREY, TFT_WHITE, TFT_DARKGREY, TFT_DARKGREY};

// 状態変数
int mode = -1; // -1: メニュー, 0以上: メニューリストのインデックス
bool hasStartedTimer = 0;
bool alert = false;
bool flicker = false;
long oldDialPosition = -999;
int timeCount = 0;
int totalTimeCount = 0;
int tweetedTotalTimeCount = 0;
bool enableTouchSound = true;
int chosen = 2;

// ファイルシステムへの設定
// 一行目：totalTimeCount
bool successSPIFFSMount = false;
String ConfigFilePath = "/timer.config";

// タイマ割込み設定
volatile int count = 0;
volatile int secUp = 0;
hw_timer_t *tim0 = NULL;
volatile SemaphoreHandle_t timerSemaphore;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
void IRAM_ATTR onTimer()
{
  portENTER_CRITICAL_ISR(&timerMux);
  count++;
  if (count >= 10)
  {
    secUp = 1;
  }
  portEXIT_CRITICAL_ISR(&timerMux);
  xSemaphoreGiveFromISR(timerSemaphore, NULL);
}

// ネットワーク設定
bool successConnectWifi = false;
bool successConnectAWSIoT = false;
void setupConnectAWSIoT(void *arg)
{
  successConnectWifi = connectWifi();
  successConnectAWSIoT = connectAWSIoT();
  vTaskDelete(NULL);
}

void setup()
{
  // M5Dial設定
  auto cfg = M5.config();
  M5Dial.begin(cfg, true, true);
  M5.Speaker.setVolume(1);
  M5Dial.Display.setBrightness(24);
  img.createSprite(240, 240);
  img.setTextDatum(5);
  img.setFont(&fonts::Font4);
  img.setTextColor(TFT_WHITE);
  img.setTextDatum(5);
  oldDialPosition = M5Dial.Encoder.read();

  // ファイルシステム
  if (!SPIFFS.begin(true))
    return;
  successSPIFFSMount = true;
  loadConfig();

  // タイマ割込み設定
  timerSemaphore = xSemaphoreCreateBinary();
  tim0 = timerBegin(0, 80, true);
  timerAttachInterrupt(tim0, &onTimer, true);
  timerAlarmWrite(tim0, 100000, true); // 100msec
  timerAlarmEnable(tim0);

  // シリアル通信
  Serial.begin(115200);
  while (!Serial)
    ;
  Serial.println("プログラム開始");

  // wifi, pubsub設定
  xTaskCreatePinnedToCore(setupConnectAWSIoT, "SetupConnectAWSIoT", 4096, NULL, 1, NULL, 1);

  delay(200);
}

void loop()
{
  M5Dial.update();

  // ダイアル操作
  handleDial();

  // タッチセンサー
  handleTouch();

  // 物理ボタン
  handleButton();

  if (hasStartedTimer == 1)
    updateTime();

  draw();

  if (successConnectWifi && successConnectAWSIoT)
  {
    loopMQTT();
  }
}

void draw()
{
  img.fillScreen(TFT_BLACK);
  img.setTextColor(TFT_WHITE);

  if (alert)
  {
    delay(100);
    flicker = !flicker;
    if (flicker)
    {
      img.fillSprite(WHITE);
      img.setTextColor(BLACK);
    }
    else
    {
      img.fillSprite(BLACK);
      img.setTextColor(WHITE);
    }
  }

  // Main menu
  if (mode == -1)
  {
    for (int i = 0; i < 5; i++)
    {
      img.setFont(&fonts::FreeSans12pt7b);
      img.setTextSize(viewItemTextSize[i]);
      img.setTextColor(viewItemTextColor[i]);
      String label = menuItems[viewItemIndex[i]];
      uint16_t x = 0;
      if (i == 2)
      {
        img.setFont(&fonts::FreeSansBold12pt7b);
        if (label.length() > 9)
        {
          x = label.length() + 1 / 2;
        }
      }
      img.drawString(label, baseItemPosition[i][0] + x, baseItemPosition[i][1]);
    }
  }
  // 20sec Timer
  else if (mode == 0)
  {
    img.setTextSize(2);
    img.setFont(&fonts::Font7);
    String *numS = getStrTimeArray(timeCount);
    img.drawString(numS[2], 120, 120);
  }
  // 10 or 25min Timer
  else if (mode == 1 || mode == 2)
  {
    img.setTextSize(1.5);
    img.setFont(&fonts::Font7);
    String *numS = getStrTimeArray(timeCount);
    img.drawString(numS[1] + ":" + numS[2], 120, 120);
  }
  // Stop Watch
  else if (mode == 3)
  {
    img.setFont(&fonts::Font7);
    img.setTextSize(1);
    String *numS = getStrTimeArray(timeCount);
    img.drawString(numS[0] + ":" + numS[1] + ":" + numS[2], 120, 120);

    img.setTextSize(0.5);
    img.setTextColor(TFT_DARKGREY);
    numS = getStrTimeArray(totalTimeCount);
    img.drawString(numS[0] + ":" + numS[1] + ":" + numS[2], 120, 180);
  }
  // Exit
  else if (mode == 4)
  {
    img.setTextSize(1);
    img.setFont(&fonts::FreeSans12pt7b);
    img.drawString("Touch to shutdown.", 120, 120);
  }
  // Status
  else if (mode == 5)
  {
    img.setTextSize(0.5);
    img.setFont(&fonts::FreeSans24pt7b);
    img.drawString("SPIFFS: " + String(successSPIFFSMount ? "true" : "false"), 120, 60);
    img.drawString("Wi-Fi: " + String(successConnectWifi ? "true" : "false"), 120, 80);
    img.drawString("AWS IoT Core: " + String(successConnectAWSIoT ? "true" : "false"), 120, 100);
    String *numS = getStrTimeArray(totalTimeCount);
    img.drawString("Total time: " + numS[0] + ":" + numS[1] + ":" + numS[2], 120, 120);
    numS = getStrTimeArray(tweetedTotalTimeCount);
    img.drawString("Tweet time: " + numS[0] + ":" + numS[1] + ":" + numS[2], 120, 140);
  }
  // Reset
  else if (mode == 6)
  {
    img.setTextSize(1);
    img.setFont(&fonts::FreeSans12pt7b);
    img.drawString("Touch to reset", 120, 100);
    img.drawString("total time count.", 120, 140);
  }
  else if (mode == 7)
  {
    img.setFont(&fonts::Font7);
    img.setTextSize(1);
    String *numS = getStrTimeArray(timeCount);
    img.drawString(numS[0] + ":" + numS[1] + ":" + numS[2], 120, 120);
    img.fillRect(14 + (chosen * 76), 150, 59, 4, GREEN);
    img.drawString(hasStartedTimer ? "STOP" : "START", 120, 190, 4);
  }
  else if (mode == 8)
  {
    img.setTextSize(1);
    img.setFont(&fonts::FreeSans12pt7b);
    img.drawString("Touch to tweet", 120, 120);
    String *numS = getStrTimeArray(totalTimeCount - tweetedTotalTimeCount);
    img.drawString(numS[0] + ":" + numS[1] + ":" + numS[2], 120, 140);

    if (!successConnectWifi || !successConnectAWSIoT)
    {
      img.setTextColor(RED);
      img.drawString("Can't tweet!", 120, 180);
      img.drawString("Invalid state.", 120, 200);
    }
  }
  else
  {
    img.setTextSize(2);
    img.drawString(menuItems[mode], 120, 120);
  }

  img.pushSprite(0, 0);
}

void handleButton()
{
  if (!M5Dial.BtnA.isPressed())
    return;
  if (mode == 3)
    writeConfig();
  hasStartedTimer = 0;
  alert = false;
  mode = -1;
  sound();
  delay(200);
}

void handleTouch()
{
  auto t = M5Dial.Touch.getDetail();
  if (!t.isPressed())
    return;
  // Main menu：各サービスへ遷移
  if (mode == -1)
  {
    mode = viewItemIndex[2];
    resetTime(mode);
  }
  // タイマー
  else if (mode == 0 || mode == 1 || mode == 2)
  {
    resetTime(mode);
    hasStartedTimer = !hasStartedTimer;
    alert = false;
  }
  // ストップウォッチ
  else if (mode == 3)
  {
    hasStartedTimer = !hasStartedTimer;
    alert = false;
  }
  // Exit
  else if (mode == 4)
  {
    M5Dial.Power.powerOff();
  }
  // Reset
  else if (mode == 6)
  {
    totalTimeCount = 0;
    tweetedTotalTimeCount = 0;
    writeConfig();
  }
  // 可変タイマー
  else if (mode == 7)
  {
    if (t.y > 160)
    {
      hasStartedTimer = !hasStartedTimer;
      alert = false;
    };
    if (t.y > 86 && t.y < 150)
    {
      if (t.x > 10 && t.x < 90)
        chosen = 0;
      if (t.x > 90 && t.x < 166)
        chosen = 1;
      if (t.x > 166 && t.x < 224)
        chosen = 2;
    }
  }
  // Tweet
  else if (mode == 8)
  {
    if (successConnectWifi && successConnectAWSIoT && totalTimeCount - tweetedTotalTimeCount != 0)
    {
      publish(totalTimeCount - tweetedTotalTimeCount);
      tweetedTotalTimeCount = totalTimeCount;
      writeConfig();
      mode = -1;
    }
  }

  sound();
  resetTimerCounter();
  delay(200);
}

void handleDial()
{
  long newPosition = M5Dial.Encoder.read();
  if (newPosition == oldDialPosition)
    return;
  if (mode == -1)
  {
    // 右回し
    // 先頭を末尾に移動して描画
    if (newPosition > oldDialPosition)
      for (int i = 0; i < 5; i++)
        if (viewItemIndex[i] < numItem - 1)
          viewItemIndex[i] = viewItemIndex[i] + 1;
        else
          viewItemIndex[i] = 0;
    // 左回し
    // 末尾を先頭に移動して描画
    else
      for (int i = 0; i < 5; i++)
        if (viewItemIndex[i] > 0)
          viewItemIndex[i] = viewItemIndex[i] - 1;
        else
          viewItemIndex[i] = numItem - 1;
  }
  else if (mode == 7)
  {
    int v = 1;
    if (chosen == 0)
    {
      v = 60 * 60;
    }
    else if (chosen == 1)
    {
      v = 60;
    }

    // 右回し
    if (newPosition > oldDialPosition)
    {
      timeCount += v;
    }
    // 左回し
    else
    {
      if (timeCount - v > 0)
        timeCount -= v;
    }
  }
  oldDialPosition = newPosition;
  sound();
}

void updateTime()
{
  if (mode == -1 || hasStartedTimer == 0)
    return;

  if (timeCount == 0 && mode != 3)
  {
    alert = true;
    return;
  }

  if (xSemaphoreTake(timerSemaphore, 0) != pdTRUE || secUp == 0)
    return;

  resetTimerCounter();

  if (mode == 3)
  {
    timeCount++;
    totalTimeCount++;
  }
  else
  {
    timeCount--;
  }
}

void resetTime(int resetMode)
{
  if (resetMode == 0)
  {
    timeCount = 20;
  }
  else if (resetMode == 1)
  {
    timeCount = 600;
  }
  else if (resetMode == 2)
  {
    timeCount = 1500;
  }
  else if (resetMode == 3)
  {
    timeCount = 0;
  }
  else if (resetMode == 7)
  {
    timeCount = 600;
  }
}

void resetTimerCounter()
{
  portENTER_CRITICAL(&timerMux);
  secUp = 0;
  count = 0;
  portEXIT_CRITICAL(&timerMux);
}

String *getStrTimeArray(int t)
{
  int h = t / 3600;
  int s = t % 3600;
  int m = s / 60;
  s = s % 60;

  static String r[3];
  r[0] = getStrTime(h);
  r[1] = getStrTime(m);
  r[2] = getStrTime(s);

  return r;
}

String getStrTime(int t)
{
  if (t < 10)
    return "0" + String(t);
  else
    return String(t);
}

void loadConfig()
{
  File file = SPIFFS.open(ConfigFilePath, "r");
  if (!file || file.isDirectory())
    return;

  String loadTotalTimeCount = file.readStringUntil('\n');
  String loadTweetedTimeCount = file.readStringUntil('\n');
  file.close();

  totalTimeCount = loadTotalTimeCount.toInt();
  tweetedTotalTimeCount = loadTweetedTimeCount.toInt();
}

void writeConfig()
{
  // 経過時間をファイルに保存
  File file = SPIFFS.open(ConfigFilePath, "w");
  file.println(String(totalTimeCount));
  file.println(String(tweetedTotalTimeCount));
  file.close();
}

void sound()
{
  if (enableTouchSound)
    M5Dial.Speaker.tone(4000, 100);
}
