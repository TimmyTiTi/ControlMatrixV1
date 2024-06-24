#ifdef IDF_BUILD
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include "sdkconfig.h"
#endif

#include <Arduino.h>
#include <string>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include "ESPmDNS.h"
#include "xtensa/core-macros.h"
#ifdef VIRTUAL_PANE
#include <ESP32-VirtualMatrixPanel-I2S-DMA.h>
#else
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#endif
#include "main.h"
#include <sstream> // Pour std::ostringstream

#define PIN_E -1
#define PANEL_WIDTH 64
#define PANEL_HEIGHT 32

#ifdef VIRTUAL_PANE
#define PANELS_NUMBER 4
#else
#define PANELS_NUMBER 1
#endif

#define PANE_WIDTH PANEL_WIDTH *PANELS_NUMBER
#define PANE_HEIGHT PANEL_HEIGHT
#define NUM_LEDS PANE_WIDTH *PANE_HEIGHT

#ifdef VIRTUAL_PANE
#define NUM_ROWS 2
#define NUM_COLS 2
#define PANEL_CHAIN NUM_ROWS *NUM_COLS
#define SERPENT true
#define TOPDOWN false
#endif

#ifdef VIRTUAL_PANE
VirtualMatrixPanel *matrix = nullptr;
MatrixPanel_I2S_DMA *chain = nullptr;
#else
MatrixPanel_I2S_DMA *matrix = nullptr;
#endif

#define PATTERN_DELAY 2000

uint16_t time_counter = 0, cycles = 0, fps = 0;
unsigned long fps_timer;

CRGB *ledbuff;

unsigned long t1, t2, s1 = 0, s2 = 0, s3 = 0;
uint32_t ccount1, ccount2;

uint8_t color1 = 0, color2 = 0, color3 = 0;
uint16_t x, y;

const char *ssid = "Freebox-1828A5_2.4GHz";
const char *password = "mxvsnqv7zwqbt37bqsdb5t";

AsyncWebServer server(80);

double duration = 0;
int distance = 0;

String durationString = "";
String distanceString = "";
String alarmingString = "";



void setup()
{

  Serial.begin(115200); // Initialiser le port série en début
  delay(1000);

 

  // Redéfinir les pins si nécessaire
  HUB75_I2S_CFG mxconfig(PANEL_WIDTH, PANEL_HEIGHT, PANELS_NUMBER);
  mxconfig.gpio.e = PIN_E;
  mxconfig.driver = HUB75_I2S_CFG::FM6126A;

#ifndef VIRTUAL_PANE
  matrix = new MatrixPanel_I2S_DMA(mxconfig);
  matrix->begin();
  matrix->setBrightness8(255);
  matrix->clearScreen();
#else
  chain = new MatrixPanel_I2S_DMA(mxconfig);
  chain->begin();
  chain->setBrightness8(255);
  matrix = new VirtualMatrixPanel((*chain), NUM_ROWS, NUM_COLS, PANEL_WIDTH, PANEL_HEIGHT, CHAIN_TOP_LEFT_DOWN);
#endif

  ledbuff = (CRGB *)malloc(NUM_LEDS * sizeof(CRGB));
  buffclear(ledbuff);

  matrix->setTextWrap(false);
  matrix->setTextColor(matrix->color565(255, 255, 255));

  // Configuration du point d'accès WiFi
  WiFi.begin(ssid, password);
  Serial.println("Tentative de connexion ...");

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(100);
  }
  Serial.print("Connecté au WiFi IP:");Serial.println(WiFi.localIP());
  if (MDNS.begin("spangmatrix01")) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("mDNS advertising started");
  } else {
    Serial.println("Error starting mDNS");
  }

//  server.on("/", handleRoot);
//  server.on("/update", HTTP_POST, handleUpdate);
//  server.onNotFound(handleNotFound);
  server.on("/display", HTTP_GET, [](AsyncWebServerRequest *request){
    String message = "";
    if (request->hasParam("duration")){
      message += "Duration : " + request->getParam("duration")->value() + "\n";
      durationString = request->getParam("duration")->value();
    }
    if (request->hasParam("alarming")) {
      message += "Alarming: " + request->getParam("alarming")->value() + "\n";
      alarmingString = request->getParam("alarming")->value();
    }
    if (request->hasParam("distance")){
      message += "Distance : " + request->getParam("distance")->value() + "\n";
      distanceString = request->getParam("distance")->value();
    }
    request->send(200, "text/plain", message);
    spacingAngelDisplay(distanceString, durationString);
  });

  server.begin();
  Serial.println("Serveur web actif!");
}

uint8_t wheelval = 0;

void loop()
{
//#ifndef NO_GFX
//  drawText("NO_GFX");
//#endif

//#ifndef VIRTUAL_PANE
//    spacingAngelDisplay(distanceString, durationString);
//#endif
}

void buffclear(CRGB *buf)
{
  memset(buf, 0x00, NUM_LEDS * sizeof(CRGB));
}

void IRAM_ATTR mxfill(CRGB *leds)
{
  uint16_t y = PANE_HEIGHT;
  do
  {
    --y;
    uint16_t x = PANE_WIDTH;
    do
    {
      --x;
      uint16_t _pixel = y * PANE_WIDTH + x;
      matrix->drawPixelRGB888(x, y, leds[_pixel].r, leds[_pixel].g, leds[_pixel].b);
    } while (x);
  } while (y);
}

uint16_t XY16(uint16_t x, uint16_t y)
{
  if (x < PANE_WIDTH && y < PANE_HEIGHT)
  {
    return (y * PANE_WIDTH) + x;
  }
  else
  {
    return 0;
  }
}

#ifdef NO_GFX
void drawText(String colorWheelOffset) {}
#else
void drawText(const String &printedText)
{
  matrix->setTextWrap(false);
  matrix->setCursor(0, 2);
  matrix->print(printedText);
}
#endif

uint16_t colorWheel(uint8_t pos)
{
  if (pos < 85)
  {
    return matrix->color565(pos * 3, 255 - pos * 3, 0);
  }
  else if (pos < 170)
  {
    pos -= 85;
    return matrix->color565(255 - pos * 3, 0, pos * 3);
  }
  else
  {
    pos -= 170;
    return matrix->color565(0, pos * 3, 255 - pos * 3);
  }
}

void spacingAngelDisplay(String distanceString, String durationString)
{
  int sizeWideText = 4;
  int sizeHeightText = 3;
  distance = distanceString.toDouble();

  if (distance <= 20 && distance > 15)
  {
    matrix->setTextSize(sizeHeightText,sizeWideText);
    matrix->setTextColor(matrix->color565(255, 0, 0));
    matrix->clearScreen();
    drawText(distanceString + "m");
    delay(PATTERN_DELAY);
    matrix->clearScreen();
    drawText(durationString + "s");
    delay(PATTERN_DELAY);
  }
  else if (distance <= 15 && distance > 10)
  {
    matrix->setTextSize(sizeHeightText,sizeWideText);
    matrix->setTextColor(matrix->color565(255, 0, 255));
    matrix->clearScreen();
    drawText(distanceString + "m");
    delay(PATTERN_DELAY);
    matrix->clearScreen();
    drawText(durationString + "s");
    delay(PATTERN_DELAY);
  }
  else if (distance <= 10 && distance > 5)
  {
    matrix->setTextSize(sizeHeightText,sizeWideText);
    matrix->setTextColor(matrix->color565(100, 0, 255));
    matrix->clearScreen();
    drawText(distanceString + "m");
    delay(PATTERN_DELAY);
    matrix->clearScreen();
    drawText(durationString + "s");
    delay(PATTERN_DELAY);
  }
  else if (distance <= 5)
  {
    matrix->setTextSize(sizeHeightText,sizeWideText);
    matrix->setTextColor(matrix->color565(0, 0, 255));
    matrix->clearScreen();
    drawText(distanceString + "m");
    delay(PATTERN_DELAY);
    matrix->clearScreen();
    drawText(durationString + "s");
    delay(PATTERN_DELAY);
  }
  else
  {
    matrix->setTextSize(sizeHeightText,sizeWideText);
    matrix->setTextColor(matrix->color565(0, 255, 0));
    matrix->clearScreen();

    drawText("Stand by");
    delay(PATTERN_DELAY);
  }
}
