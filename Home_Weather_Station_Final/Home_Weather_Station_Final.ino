/****************************************************************************
* Local Weather Station with Wunderground external data and Indoor data from DHT11
* https://www.wunderground.com/
* 
* Adapted from the original work of Daniel Eichhorn: https://github.com/squix78
* 
* Marcelo Rovai - 15March17
*****************************************************************************/
#include "stationDefines.h"       // Project definitions
#include "stationCredentials.h"
#include <ESP8266WiFi.h>
#include <JsonListener.h>

/* Ticker */
#include <Ticker.h>
Ticker ticker;

/* Wunderground */
#include "WundergroundClient.h"
#include "WeatherStationFonts.h"
#include "WeatherStationImages.h"
WundergroundClient wunderground(IS_METRIC); 

/* DHT22 */
#include "DHT.h"
DHT dht(DHTPIN, DHTTYPE);

/* OLED */
#include "SSD1306Wire.h"
#include "OLEDDisplayUi.h"
#include "Wire.h"
const int I2C_DISPLAY_ADDRESS = 0x3c;
const int SDA_PIN = 5;
const int SCL_PIN = 4;
SSD1306Wire     display(I2C_DISPLAY_ADDRESS, SDA_PIN, SCL_PIN);
OLEDDisplayUi   ui( &display );

/* Time Client */
#include "TimeClient.h"
TimeClient timeClient(UTC_OFFSET);

/* declaring prototypes */
void drawProgress(OLEDDisplay *display, int percentage, String label);
void updateData(OLEDDisplay *display);
void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawForecast(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawDHT(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex);
void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState* state);
void setReadyForWeatherUpdate();

/* frames */
FrameCallback frames[] = {drawDateTime, drawDHT, drawCurrentWeather, drawForecast};
int numberOfFrames = 4;

OverlayCallback overlays[] = { drawHeaderOverlay };
int numberOfOverlays = 1;


void setup() 
{
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  display.init();   // initialize display
  display.clear();
  display.display();

  //display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setContrast(255);

  WiFi.begin(WIFI_SSID, WIFI_PWD);

  int counter = 0;
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
    Serial.print(".");
    display.clear();
    display.flipScreenVertically();
    display.drawString(64, 10, "Conectando al WiFi");
    display.drawXbm(46, 30, 8, 8, counter % 3 == 0 ? activeSymbole : inactiveSymbole);
    display.drawXbm(60, 30, 8, 8, counter % 3 == 1 ? activeSymbole : inactiveSymbole);
    display.drawXbm(74, 30, 8, 8, counter % 3 == 2 ? activeSymbole : inactiveSymbole);
    display.display();

    counter++;
  }

  ui.setTargetFPS(30);
  
  ui.setActiveSymbol(emptySymbol);  // clear frames animation at bottonline
  ui.setInactiveSymbol(emptySymbol);
  ui.disableIndicator();

  ui.setFrames(frames, numberOfFrames);
  ui.setOverlays(overlays, numberOfOverlays);

  ui.init();   // Inital UI takes care of initalising the display too.

  Serial.println("");
  display.flipScreenVertically();
  updateData(&display);

  ticker.attach(UPDATE_INTERVAL_SECS, setReadyForWeatherUpdate);
}

void loop() 
{
  display.flipScreenVertically();
  if (readyForWeatherUpdate && ui.getUiState()->frameState == FIXED) 
  {
    updateData(&display);
  }

  int remainingTimeBudget = ui.update();

  if (remainingTimeBudget > 0) {
    // You can do some work here
    // Don't do stuff if you are below your
    // time budget.
    delay(remainingTimeBudget);
  }
}

/***************************************************
* Draw Progress bar during data update
****************************************************/
void drawProgress(OLEDDisplay *display, int percentage, String label) 
{
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64, 10, label);
  display->drawProgressBar(2, 28, 124, 10, percentage);
  display->display();
}

/***************************************************
* Get data
****************************************************/
void updateData(OLEDDisplay *display) 
{
  drawProgress(display, 10, "Updating time...");
  timeClient.updateTime();
  drawProgress(display, 30, "Updating conditions...");
  wunderground.updateConditions(WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_COUNTRY, WUNDERGROUND_CITY);
  drawProgress(display, 50, "Updating forecasts...");
  wunderground.updateForecast(WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_COUNTRY, WUNDERGROUND_CITY);
  drawProgress(display, 80, "Updating local data...");
  getDHT();
  lastUpdate = timeClient.getFormattedTime();
  readyForWeatherUpdate = false;
  drawProgress(display, 100, "Done...");
  delay(1000);
}

/***************************************************
* Draw Time page 
****************************************************/
void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) 
{
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  String date = wunderground.getDate();
  int textWidth = display->getStringWidth(date);
  display->drawString(64 + x, 5 + y, date);
  display->setFont(ArialMT_Plain_24);
  String time = timeClient.getFormattedTime();
  textWidth = display->getStringWidth(time);
  display->drawString(64 + x, 15 + y, time);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
}

/***************************************************
* Draw Current Temp/Conditions Page
****************************************************/
void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) 
{
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(60 + x, 5 + y, wunderground.getWeatherText());

  display->setFont(ArialMT_Plain_24);
  String temp = wunderground.getCurrentTemp() + "°C";
  display->drawString(60 + x, 15 + y, temp);
  int tempWidth = display->getStringWidth(temp);

  display->setFont(Meteocons_Plain_42);
  String weatherIcon = wunderground.getTodayIcon();
  int weatherIconWidth = display->getStringWidth(weatherIcon);
  display->drawString(32 + x - weatherIconWidth / 2, 05 + y, weatherIcon);
}

/***************************************************
* Draw Forecast page
****************************************************/
void drawForecast(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) 
{
  drawForecastDetails(display, x, y, 0);
  drawForecastDetails(display, x + 44, y, 2);
  drawForecastDetails(display, x + 88, y, 4);
}

/***************************************************
* Draw Forecast Page details
****************************************************/
void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex) 
{
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  String day = wunderground.getForecastTitle(dayIndex).substring(0, 3);
  day.toUpperCase();
  display->drawString(x + 20, y, day);

  display->setFont(Meteocons_Plain_21);
  display->drawString(x + 20, y + 12, wunderground.getForecastIcon(dayIndex));

  display->setFont(ArialMT_Plain_10);
  display->drawString(x + 20, y + 34, wunderground.getForecastLowTemp(dayIndex) + "|" + wunderground.getForecastHighTemp(dayIndex));
  display->setTextAlignment(TEXT_ALIGN_LEFT);
}

/***************************************************
* Draw Indoor Page
****************************************************/
void drawDHT(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) 
{
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(0 + x, 5 + y, "Hum");
  
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(43 + x, y, "INDOOR");

  display->setFont(ArialMT_Plain_24);
  String hum = String(localHum) + "%";
  display->drawString(0 + x, 15 + y, hum);
  int humWidth = display->getStringWidth(hum);

  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(95 + x, 5 + y, "Temp");

  display->setFont(ArialMT_Plain_24);
  String temp = String(localTemp) + "°C";
  display->drawString(70 + x, 15 + y, temp);
  int tempWidth = display->getStringWidth(temp);

}

/***************************************************
* Draw last line of display
****************************************************/
void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState* state) 
{
  display->setColor(WHITE);
  display->setFont(ArialMT_Plain_10);

  display->setTextAlignment(TEXT_ALIGN_LEFT);
  String tempIn = "In: "+ String(localTemp) + "°C";
  display->drawString(0, 54, tempIn);

  display->setColor(WHITE);
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(53, 54, String(state->currentFrame + 1) + "/" + String(numberOfFrames));
  
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  String tempOut = "Out: "+ wunderground.getCurrentTemp() + "°C";
  display->drawString(128, 54, tempOut);

  display->drawHorizontalLine(0, 52, 128);
}

/***************************************************
* 
****************************************************/
void setReadyForWeatherUpdate() 
{
  Serial.println("Setting readyForUpdate to true");
  readyForWeatherUpdate = true;
}

/***************************************************
* Get indoor Temp/Hum data
****************************************************/
void getDHT()
{
  float tempIni = localTemp;
  float humIni = localHum;
  localTemp = dht.readTemperature();
  localHum = dht.readHumidity();
  if (isnan(localHum) || isnan(localTemp))   // Check if any reads failed and exit early (to try again).
  {
    Serial.println("Failed to read from DHT sensor!");
    localTemp = tempIni;
    localHum = humIni;
    return;
  }
}


