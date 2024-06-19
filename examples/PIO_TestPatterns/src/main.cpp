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
#include <WebServer.h>
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

WebServer server(80);

double vitesse = 50;
int distance = 3;

String vitesseString = "";
String distanceString = "";

void handleRoot()
{
  String htmlContent = R"rawliteral(
<!DOCTYPE html>
<html lang="fr">

<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Test modification des paramètres Vitesse/Distance</title>
  <style>
    body {
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
      margin: 0;
      padding: 0;
      text-align: center;
      background-color: #f5f5f7;
    }

    .container {
      max-width: 600px;
      margin: 50px auto;
      padding: 20px;
      background: #ffffff;
      box-shadow: 0 4px 12px rgba(0, 0, 0, 0.1);
      border-radius: 10px;
    }

    h1 {
      font-size: 2em;
      margin-bottom: 20px;
    }

    .slider-container {
      margin: 20px 0;
    }

    .slider-label {
      font-size: 1.2em;
      margin-bottom: 10px;
    }

    .slider {
      width: 100%;
    }

    .button {
      display: inline-block;
      padding: 10px 20px;
      font-size: 1.2em;
      color: #ffffff;
      background-color: #0070c9;
      border: none;
      border-radius: 5px;
      cursor: pointer;
      margin-top: 20px;
    }

    .values {
      margin-top: 20px;
      font-size: 1.2em;
    }
  </style>
</head>

<body>
  <div class="container">
    <h1>Test modification des paramètres Vitesse/Distance</h1>

    <div class="slider-container">
      <div class="slider-label">Vitesse: <span id="vitesse-val">50</span></div>
      <input type="range" id="vitesse" class="slider" min="0" max="100" value="50">
    </div>

    <div class="slider-container">
      <div class="slider-label">Distance: <span id="distance-val">50</span></div>
      <input type="range" id="distance" class="slider" min="0" max="100" value="50">
    </div>

    <button class="button" onclick="updateValues()">Actualiser les paramètres</button>

    <div class="values">
      <div>Valeur de la vitesse mise à jour: <span id="updated-vitesse">50</span></div>
      <div>Valeur de la distance mise à jour: <span id="updated-distance">50</span></div>
    </div>
  </div>

  <script>
    const vitesseSlider = document.getElementById('vitesse');
    const distanceSlider = document.getElementById('distance');
    const vitesseVal = document.getElementById('vitesse-val');
    const distanceVal = document.getElementById('distance-val');
    const updatedVitesse = document.getElementById('updated-vitesse');
    const updatedDistance = document.getElementById('updated-distance');

    vitesseSlider.oninput = function () {
      vitesseVal.textContent = this.value;
    }

    distanceSlider.oninput = function () {
      distanceVal.textContent = this.value;
    }

    function updateValues() {
      updatedVitesse.textContent = vitesseSlider.value;
      updatedDistance.textContent = distanceSlider.value;

      // Envoyer les données au serveur
      const xhr = new XMLHttpRequest();
      xhr.open("POST", "/update", true);
      xhr.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
      xhr.send("vitesse=" + vitesseSlider.value + "&distance=" + distanceSlider.value);

    }
  </script>
</body>

</html>
)rawliteral";

  server.setContentLength(htmlContent.length());
  server.send(200, "text/html", htmlContent);
}

void handleUpdate()
{
  if (server.hasArg("vitesse") && server.hasArg("distance"))
  {
    String vitesseUpdate = server.arg("vitesse");
    String distanceUpdate = server.arg("distance");
    

    Serial.print("Vitesse: ");
    Serial.println(vitesseUpdate);
    Serial.print("Distance: ");
    Serial.println(distanceUpdate);

    distance = distanceUpdate.toInt();
    vitesse = vitesseUpdate.toDouble();
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleNotFound()
{
  server.send(404, "text/plain", "404 : Not found");
}

void setup()
{

  Serial.begin(115200); // Initialiser le port série en début
  delay(1000);

  // Configuration du point d'accès WiFi
  WiFi.begin(ssid, password);
  Serial.println("Tentative de connexion ...");

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(100);
  }
  Serial.println("/n");
  Serial.println("Connexion établie!");
  Serial.print("Adresse IP: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/update", HTTP_POST, handleUpdate);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("Serveur web actif!");

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
}

uint8_t wheelval = 0;

void loop()
{
  
  server.handleClient();
#ifndef NO_GFX
  drawText("NO_GFX");
#endif

#ifndef VIRTUAL_PANE
    spacingAngelDisplay(distance, vitesse);
#endif
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

void spacingAngelDisplay(int distance, double vitesse)
{
  double tempsArret = distance / (3.6 * vitesse);
  int sizeText = 1;
  // Utiliser std::ostringstream pour formater le double
  std::ostringstream oss;
  oss.precision(1);
  oss << std::fixed << tempsArret;
  String tempsArretString = String(oss.str().c_str());

  distanceString = String(distance);

  if (distance <= 20 && distance > 15)
  {
    matrix->setTextSize(sizeText);
    matrix->setTextColor(matrix->color565(255, 0, 0));
    matrix->clearScreen();
    drawText(distanceString + "m");
    delay(PATTERN_DELAY);
    matrix->clearScreen();
    drawText(tempsArretString + "s");
    delay(PATTERN_DELAY);
  }
  else if (distance <= 15 && distance > 10)
  {
    matrix->setTextSize(sizeText);
    matrix->setTextColor(matrix->color565(255, 0, 255));
    matrix->clearScreen();
    drawText(distanceString + "m");
    delay(PATTERN_DELAY);
    matrix->clearScreen();
    drawText(tempsArretString + "s");
    delay(PATTERN_DELAY);
  }
  else if (distance <= 10 && distance > 5)
  {
    matrix->setTextSize(sizeText);
    matrix->setTextColor(matrix->color565(100, 0, 255));
    matrix->clearScreen();
    drawText(distanceString + "M");
    delay(PATTERN_DELAY);
    matrix->clearScreen();
    drawText(tempsArretString + "S");
    delay(PATTERN_DELAY);
  }
  else if (distance <= 5)
  {
    matrix->setTextSize(sizeText);
    matrix->setTextColor(matrix->color565(0, 0, 255));
    matrix->clearScreen();
    drawText(distanceString + "M");
    delay(PATTERN_DELAY);
    matrix->clearScreen();
    drawText(tempsArretString + "S");
    delay(PATTERN_DELAY);
  }
  else
  {
    matrix->setTextSize(sizeText);
    matrix->setTextColor(matrix->color565(0, 255, 0));
    matrix->clearScreen();
    drawText("Stand by");
    delay(PATTERN_DELAY);
  }
}
