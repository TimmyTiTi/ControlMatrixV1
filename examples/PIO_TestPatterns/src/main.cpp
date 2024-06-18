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
#include "xtensa/core-macros.h"
#ifdef VIRTUAL_PANE
#include <ESP32-VirtualMatrixPanel-I2S-DMA.h>
#else
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#endif
#include "main.h"

const char* ssid = "ESP32_Access_Point";
const char* password = "12345678";

AsyncWebServer server(80);

double vitesse = 50;
double distance = 5;

String vitesseString = "";
String distanceString = "";

#define PIN_E -1
#define PANEL_WIDTH 64
#define PANEL_HEIGHT 32

#ifdef VIRTUAL_PANE
 #define PANELS_NUMBER 4
#else
 #define PANELS_NUMBER 1
#endif

#define PANE_WIDTH PANEL_WIDTH * PANELS_NUMBER
#define PANE_HEIGHT PANEL_HEIGHT
#define NUM_LEDS PANE_WIDTH * PANE_HEIGHT

#ifdef VIRTUAL_PANE
 #define NUM_ROWS 2
 #define NUM_COLS 2
 #define PANEL_CHAIN NUM_ROWS * NUM_COLS
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

const char *str = "* ESP32 I2S DMA *";

void setup() {
  Serial.begin(115200); // Initialiser le port série en début

  // Configuration du point d'accès WiFi
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // Configurer les routes du serveur Web
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Bonjour, vous pouvez contrôler la matrice LED!");
  });

  server.on("/setSpeed", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("speed")) {
      String speedParam = request->getParam("speed")->value();
      vitesse = speedParam.toDouble();
      vitesseString = speedParam;
      Serial.println("Vitesse reçue : " + speedParam);
      request->send(200, "text/plain", "Vitesse définie à " + speedParam);
    } else {
      request->send(400, "text/plain", "Paramètre 'speed' manquant");
    }
  });

  server.on("/setDistance", HTTP_GET, [](AsyncWebServerRequest *request) { // Correction ici
    if (request->hasParam("distance")) {
      String distanceParam = request->getParam("distance")->value();
      distance = distanceParam.toDouble();
      distanceString = distanceParam;
      Serial.println("Distance reçue : " + distanceParam);
      request->send(200, "text/plain", "Distance définie à " + distanceParam);
    } else {
      request->send(400, "text/plain", "Paramètre 'distance' manquant");
    }
  });

  // Démarrer le serveur
  server.begin();

  Serial.println("Starting pattern test...");

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

  matrix->setTextSize(1);
  matrix->setTextWrap(false);
  matrix->setCursor(1, 5);
  matrix->setTextColor(matrix->color565(255, 255, 255));
}

uint8_t wheelval = 0;

void loop() {
#ifndef NO_GFX
  drawText("NO_GFX");
#endif

#ifndef VIRTUAL_PANE
  spacingAngelDisplay(distance, vitesse);
  distance++;
  Serial.println("Nouvel affichage et distance : ");
  Serial.println(distance);
#endif
}

void buffclear(CRGB *buf) {
  memset(buf, 0x00, NUM_LEDS * sizeof(CRGB));
}

void IRAM_ATTR mxfill(CRGB *leds) {
  uint16_t y = PANE_HEIGHT;
  do {
    --y;
    uint16_t x = PANE_WIDTH;
    do {
      --x;
      uint16_t _pixel = y * PANE_WIDTH + x;
      matrix->drawPixelRGB888(x, y, leds[_pixel].r, leds[_pixel].g, leds[_pixel].b);
    } while (x);
  } while (y);
}

uint16_t XY16(uint16_t x, uint16_t y) {
  if (x < PANE_WIDTH && y < PANE_HEIGHT) {
    return (y * PANE_WIDTH) + x;
  } else {
    return 0;
  }
}

#ifdef NO_GFX
void drawText(String colorWheelOffset) {}
#else
void drawText(const String& printedText) {
  matrix->setTextWrap(false);
  matrix->setCursor(0, 0);
  matrix->print(printedText);
}
#endif

uint16_t colorWheel(uint8_t pos) {
  if (pos < 85) {
    return matrix->color565(pos * 3, 255 - pos * 3, 0);
  } else if (pos < 170) {
    pos -= 85;
    return matrix->color565(255 - pos * 3, 0, pos * 3);
  } else {
    pos -= 170;
    return matrix->color565(0, pos * 3, 255 - pos * 3);
  }
}

void spacingAngelDisplay(double distance, double vitesse) {
  double tempsArret = distance / (3.6 * vitesse);
  String tempsArretString = String(tempsArret);
  distanceString = String(distance);

  if (distance <= 20 && distance > 15) {
    matrix->setTextSize(2,4);
    matrix->setTextColor(matrix->color565(255,0,0));
    matrix->clearScreen();
    drawText(distanceString + " m");
    delay(PATTERN_DELAY);
    matrix->clearScreen();
    drawText(tempsArretString + " s");
    delay(PATTERN_DELAY);
  } else if (distance <= 15 && distance > 10) {
    matrix->setTextSize(2,4);
    matrix->setTextColor(matrix->color565(255, 0, 255));
    matrix->clearScreen();
    drawText(distanceString + "m");
    delay(PATTERN_DELAY);
    matrix->clearScreen();
    drawText(tempsArretString + "s");
    delay(PATTERN_DELAY);
  } else if (distance <= 10 && distance > 5) {
    matrix->setTextSize(2,4);
    matrix->setTextColor(matrix->color565(100, 0, 255));
    matrix->clearScreen();
    drawText(distanceString + "m");
    delay(PATTERN_DELAY);
    matrix->clearScreen();
    drawText(tempsArretString + "s");
    delay(PATTERN_DELAY);
  } else if (distance <= 5) {
    matrix->setTextSize(2,4);
    matrix->setTextColor(matrix->color565(0, 0, 255));
    matrix->clearScreen();
    drawText(distanceString + "m");
    delay(PATTERN_DELAY);
    matrix->clearScreen();
    drawText(tempsArretString + "s");
    delay(PATTERN_DELAY);
  } else {
     
    matrix->setTextSize(1);
    matrix->setTextColor(matrix->color565(0, 255, 0));
    matrix->clearScreen();
    drawText("Stand by");
    delay(PATTERN_DELAY);
  }
  
}
