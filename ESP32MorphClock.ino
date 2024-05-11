/*
  remix from HarryFun's great Morphing Digital Clock idea https://github.com/hwiguna/HariFun_166_Morphing_Clock
  follow the great tutorial there and eventually use this code as alternative

  provided 'AS IS', use at your own risk
   mirel.t.lazar@gmail.com
*/

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <WiFi.h>
#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager
#include "FS.h"
#include "SPIFFS.h"
#include <ArduinoJson.h>
#include "TinyFont.h"
#include "Digit.h"
#include "time.h"
#include "HTTPClient.h"
#include <ESPNtpClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

const char JANUARY[]  = "JAN";
const char FEBRUARY[]  = "FEB";
const char MARCH[]  = "MAR";
const char APRIL[]  = "APR";
const char MAY[]  = "MAY";
const char JUNE[]  = "JUN";
const char JULY[]  = "JUL";
const char AUGUST[]  = "AUG";
const char SEPTEMBER[]  = "SEP";
const char OCTOBER[]  = "OCT";
const char NOVEMBER[]  = "NOV";
const char DECEMBER[]  = "DEC";

const char *const month_table[]  = {JANUARY, FEBRUARY, MARCH, APRIL, MAY, JUNE, JULY, AUGUST, SEPTEMBER, OCTOBER, NOVEMBER, DECEMBER};

struct DisplayColors {
  uint16_t white_digits;
  uint16_t white_temps;
  uint16_t red;
  uint16_t blue;
  uint16_t green;
};

const int buttonPin = 33;
const int oneWireBus = 32;

OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

//=== WIFI MANAGER ===
char wifiManagerAPName[] = "MorphClk";
char wifiManagerAPPassword[] = "MorphClk";

#define FORMAT_SPIFFS_IF_FAILED true

//== SAVING CONFIG ==
bool shouldSaveConfig = false; // flag for saving data
bool haveWeather = false;

//callback notifying us of the need to save config
void saveConfigCallback() {
  //Serial.println("Should save config");
  shouldSaveConfig = true;
}

#define PANEL_RES_X 64
#define PANEL_RES_Y 32
#define PANEL_CHAIN 1

MatrixPanel_I2S_DMA *display = nullptr;

//=== SEGMENTS ===
int highIntensity = 255;
int lowIntensity = 100;
DisplayColors lowColors;
DisplayColors highColors;
DisplayColors *currentColors;

Digit digit0(0, 63 - 1 - 9 * 1, 8, display->color565(highIntensity, highIntensity, highIntensity));
Digit digit1(0, 63 - 1 - 9 * 2, 8, display->color565(highIntensity, highIntensity, highIntensity));
Digit digit2(0, 63 - 4 - 9 * 3, 8, display->color565(highIntensity, highIntensity, highIntensity));
Digit digit3(0, 63 - 4 - 9 * 4, 8, display->color565(highIntensity, highIntensity, highIntensity));
Digit digit4(0, 63 - 7 - 9 * 5, 8, display->color565(highIntensity, highIntensity, highIntensity));
Digit digit5(0, 63 - 7 - 9 * 6, 8, display->color565(highIntensity, highIntensity, highIntensity));

void getWeather();

int oldTemp;
int newTemp;
char readMode = ' ';
char timezone[50] = "EST5EDT,M3.2.0,M11.1.0";
char military[2] = "N";     // 24 hour mode? Y/N
char u_metric[2] = "N";     // use metric for units? Y/N
char date_fmt[7] = "M/D/Y"; // date format: D.M.Y or M.D.Y or M.D or D.M or D/M/Y.. looking for trouble
char city_code[30] = "Phoenixville,US";
char api_key[20] = "1234";
//int digitColor;
int errColor = 0;
struct tm timeInfo;

int hour() {
  if (!getLocalTime(&timeInfo)) {
    Serial.println("Error getting local time.");
  }
  return timeInfo.tm_hour;
}

int minute() {
  if (!getLocalTime(&timeInfo)) {
    Serial.println("Error getting local time.");
  }
  return timeInfo.tm_min;
}

int second() {
  if (!getLocalTime(&timeInfo)) {
    Serial.println("Error getting local time.");
  }
  return timeInfo.tm_sec;
}

int day() {
  if (!getLocalTime(&timeInfo)) {
    Serial.println("Error getting local time.");
  }
  return timeInfo.tm_mday;
}

int month() {
  if (!getLocalTime(&timeInfo)) {
    Serial.println("Error getting local time.");
  }
  return timeInfo.tm_mon + 1;
}

int year() {
  if (!getLocalTime(&timeInfo)) {
    Serial.println("Error getting local time.");
  }
  return timeInfo.tm_year + 1900;
}

bool isAM() {
  return !isPM();
}

bool isPM() {
  return hour() >= 12;
}

int hourFormat12() {
  int h = hour();
  if (h == 0) {
    return 12; // 12 midnight
  } else {
    if (h > 12) {
      return h - 12;
    } else {
      return h;
    }
  }
}

void printLocalTime() {
  if (!getLocalTime(&timeInfo)) {
    Serial.println("Error getting local time.");
  }
  Serial.println(&timeInfo, "%A, %B %d %Y %H:%M:%S zone %Z %z ");
}

bool loadConfig() {
  if (!SPIFFS.begin(true)) {
    Serial.println(F("Error starting SPIIFS. Lets format it and try again."));
    return false;
  }
  Serial.println(F("Error starting SPIIFS. Lets format it and try again."));
  File configFile = SPIFFS.open("/config.json");
  if (!configFile) {
    Serial.println(F("Erroe opening config on SPIFFS"));
    return false;
  }

  Serial.println("Opened configuration file");
  StaticJsonDocument<512> json;
  DeserializationError error = deserializeJson(json, configFile);
  serializeJsonPretty(json, Serial);
  if (!error) {
    Serial.println("Parsing JSON");

    strcpy(timezone, json["timezone"]);
    strcpy(military, json["military"]);
    strcpy(u_metric, json["metric"]);;
    strcpy(date_fmt, json["date-format"]);
    strcpy(api_key, json["api-key"]);
    strcpy(city_code, json["city-code"]);

    return true;
  } else {
    Serial.println("Failed to load json config");
  }
  return true;
}

void saveConfig() {
  StaticJsonDocument<500> json;
  json["timezone"] = timezone;
  json["military"] = military;
  json["metric"] = u_metric;
  json["date-format"] = date_fmt;
  json["api-key"] = api_key;
  json["city-code"] = city_code;

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println(F("Error open SPIFFS config for w"));
  } else {
    Serial.println ("Saving configuration to file:");
    serializeJsonPretty(json, Serial);
    if (serializeJson(json, configFile) == 0) {
      Serial.println ("Error saving configuration to file");
    }
    configFile.close();
  }
}

void configModeCallback(WiFiManager *myWiFiManager)
// Called when config mode launched
{
  Serial.println("Entered Configuration Mode");

  Serial.print("Config SSID: ");
  Serial.println(myWiFiManager->getConfigPortalSSID());

  Serial.print("Config IP Address: ");
  Serial.println(WiFi.softAPIP());
}

const byte row0 = 2 + 0 * 10;
const byte row1 = 2 + 1 * 10;
const byte row2 = 2 + 2 * 10;
void wifi_setup() {

  //-- Config --
  loadConfig();

  //-- Display --
  // display->fillScreen(display->color565(0, 0, 0));
  // display->setTextColor(display->color565(0, 0, 255));

  //-- WiFiManager --
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.setAPCallback(configModeCallback);

  WiFiManagerParameter timeZoneParameter("timeZone", "Timezone (Posix format)", timezone, 50);
  wifiManager.addParameter(&timeZoneParameter);
  WiFiManagerParameter militaryParameter("military", "24Hr (Y/N)", military, 1);
  wifiManager.addParameter(&militaryParameter);
  WiFiManagerParameter metricParameter("metric", "Metric (Y/N)", u_metric, 1);
  wifiManager.addParameter(&metricParameter);
  WiFiManagerParameter dmydateParameter("date_fmt", "DateFormat (D.M.Y)", date_fmt, 6);
  wifiManager.addParameter(&dmydateParameter);
  WiFiManagerParameter apiKeyParameter("api_key", "openweather API Key", api_key, 20);
  wifiManager.addParameter(&apiKeyParameter);
  WiFiManagerParameter cityParameter("city", "openweather City code", city_code, 30);
  wifiManager.addParameter(&cityParameter);

  if (digitalRead(buttonPin) == LOW) {
    // poor mans debounce/press-hold, code not ideal for production
    delay(50);
    if (digitalRead(buttonPin) == LOW ) {

      delay(3000); // to fully reset delay hold button for 3 sec
      if (digitalRead(buttonPin) == LOW ) {
        Serial.println("Button still Held. Erasing Config, restarting");
        wifiManager.resetSettings();
        ESP.restart();
      }

      TFDrawText(display, wifiManagerAPName, 0, 1, display->color565(0, 0, 255));
      TFDrawText(display, wifiManagerAPPassword, 0, 10, display->color565(0, 0, 255));

      wifiManager.startConfigPortal(wifiManagerAPName, wifiManagerAPPassword);
    }
  } else {
    Serial.println(F("Normal mode"));

    TFDrawText(display, String("   CONNECTING   "), 0, 13, display->color565(0, 0, 255));

    //fetches ssid and pass from eeprom and tries to connect
    //if it does not connect it starts an access point with the specified name wifiManagerAPName
    //and goes into a blocking loop awaiting configuration
    wifiManager.autoConnect(wifiManagerAPName);
  }

  Serial.print(F("timezone="));
  Serial.println(timezone);
  Serial.print(F("military="));
  Serial.println(military);
  Serial.print(F("metric="));
  Serial.println(u_metric);
  Serial.print(F("date-format="));
  Serial.println(date_fmt);
  Serial.print(F("city="));
  Serial.println(city_code);
  Serial.print(F("apikey="));
  Serial.println(api_key);

  //timezone
  strcpy(timezone, timeZoneParameter.getValue());
  //military time
  strcpy(military, militaryParameter.getValue());
  //metric units
  strcpy(u_metric, metricParameter.getValue());
  //date format
  strcpy(date_fmt, dmydateParameter.getValue());
  //openweather city code
  strcpy(city_code, cityParameter.getValue());
  //openweather api key
  strcpy(api_key, apiKeyParameter.getValue());

  //display->fillScreen (0);
  //display->setCursor (2, row1);
  TFDrawText(display, String("     ONLINE     "), 0, 13, display->color565(0, 0, 255));
  TFDrawText(display, WiFi.localIP().toString(), 0, 20, display->color565(0, 0, 255));

  Serial.print(F("WiFi connected, IP address: "));
  Serial.println(WiFi.localIP());
  delay(5000);

  //start NTP
  //NTP.setTimeZone(TZ_Europe_Madrid);
  NTP.setInterval(600);
  NTP.setNTPTimeout(2000);
  // NTP.setMinSyncAccuracy (5000);
  // NTP.settimeSyncThreshold (3000);
  NTP.begin("pool.ntp.org");
  NTP.setTimeZone(timezone);

  Serial.print("TimeZone:");
  Serial.println(timezone);

  if (shouldSaveConfig) {
    saveConfig();
  }

  getWeather();
}

byte hh;
byte hh24;
byte mm;
byte ss;
byte firstDraw = 1;
byte ntpsynced = 0;
NTPEvent_t ntp_event;
byte ntpSyncReceived = 0;

void setup() {

  Serial.begin(115200);

  // Module configuration
  HUB75_I2S_CFG mxconfig(
    PANEL_RES_X,   // module width
    PANEL_RES_Y,   // module height
    PANEL_CHAIN    // Chain length
  );

  // Display Setup
  display = new MatrixPanel_I2S_DMA(mxconfig);
  display->begin();

  display->clearScreen();

  digit0.setPanel(display);
  digit0.setID(0);
  digit1.setPanel(display);
  digit1.setID(1);
  digit2.setPanel(display);
  digit2.setID(2);
  digit3.setPanel(display);
  digit3.setID(3);
  digit4.setPanel(display);
  digit4.setID(4);
  digit5.setPanel(display);
  digit5.setID(5);

  lowColors.white_digits = display->color565(lowIntensity, lowIntensity, lowIntensity);
  lowColors.white_temps = display->color565(lowIntensity, lowIntensity, lowIntensity);
  lowColors.red = display->color565(lowIntensity, 0, 0);
  lowColors.green = display->color565(0, lowIntensity, 0);
  lowColors.blue = display->color565(0, 0, lowIntensity);

  highColors.white_digits = display->color565(highIntensity, highIntensity, highIntensity);
  highColors.white_temps = display->color565(lowIntensity, lowIntensity, lowIntensity);
  highColors.red = display->color565(highIntensity, 0, 0);
  highColors.green = display->color565(0, highIntensity, 0);
  highColors.blue = display->color565(0, 0, highIntensity);

  currentColors = &highColors;

  TFDrawText(display, String("     STARTING     "), 0, 13, highColors.blue);

  pinMode(buttonPin, INPUT_PULLUP);

  wifi_setup ();

  NTP.onNTPSyncEvent ([] (NTPEvent_t event) {
    ntpsynced = 1;
    ntp_event = event;
    ntpSyncReceived = 1;
  });

  //prep screen for clock display

  display->fillScreen(0);

  Serial.print("highColors.white_digits=");
  Serial.println(highColors.white_digits);
  Serial.print("highColors.red=");
  Serial.println(highColors.red);
  Serial.print("highColors.green=");
  Serial.println(highColors.green);
  Serial.print("highColors.blue=");
  Serial.println(highColors.blue);

  Serial.print("lowColors.white_digits=");
  Serial.println(lowColors.white_digits);
  Serial.print("lowColors.red=");
  Serial.println(lowColors.red);
  Serial.print("lowColors.green=");
  Serial.println(lowColors.green);
  Serial.print("lowColors.blue=");
  Serial.println(lowColors.blue);

  // //reset digits color
  digit0.setColor(currentColors->white_digits);
  digit1.setColor(currentColors->white_digits);
  digit2.setColor(currentColors->white_digits);
  digit2.setColonLeft(false);
  digit3.setColor(currentColors->white_digits);
  digit4.setColonLeft(false);
  digit4.setColor(currentColors->white_digits);
  digit5.setColor(currentColors->white_digits);

  digit2.DrawColon(currentColors->white_digits);
  digit4.DrawColon(currentColors->white_digits);

  if (military[0] == 'N') {
    digit0.setSize(3);
    digit0.setY(digit0.getY() + 6);
    digit0.setX(digit0.getX() - 1);
    digit1.setSize(3);
    digit1.setX(digit1.getX() + 2);
    digit1.setY(digit1.getY() + 6);
  }

  sensors.begin();
}

//open weather map api key
String apiKey = "aec6c8810510cce7b0ee8deca174c79a"; //e.g a hex string like "abcdef0123456789abcdef0123456789"
//the city you want the weather for
String location = "Phoenixville,US"; //e.g. "Paris,FR"
char server[] = "api.openweathermap.org";
WiFiClient client;
HTTPClient http;
int tempMin = -10000;
int tempMax = -10000;
int tempM = -10000;
int presM = -10000;
int humiM = -10000;
String condS = "";

void getWeather() {
  errColor = 0;
  if (!apiKey.length()) {
    Serial.println(F("No API KEY for weather"));
    errColor = currentColors->red;
    return;
  }
  String serverPath = "http://api.openweathermap.org/data/2.5/weather?q=" + location + "&APPID=" + apiKey + "&cnt=1&units=";//
  if (*u_metric == 'Y') {
    serverPath = serverPath + "metric";
  } else {
    serverPath = serverPath + "imperial";
  }

  Serial.print(F("i:conn to weather URL:"));
  Serial.println(serverPath);
  // if you get a connection, report back via serial:

  http.begin(client, serverPath);
  int httpResult = http.GET();
  if (httpResult <= 0) {
    Serial.print("Weather API error:");
    Serial.println(httpResult);
  } else {
    String sval = "";
    //do your best
    String line = http.getString();
    Serial.print("Weather response:");
    Serial.println(line);
    http.end();
    if (!line.length()) {
      Serial.println(F("w:fail weather"));
      errColor = currentColors->red;
    } else {
      Serial.print(F("Raw weather output:"));
      Serial.println(line);

      StaticJsonDocument<1000> json;
      DeserializationError error = deserializeJson(json, line);
      if (error) {
        Serial.println("Error deserializing weather json.");
        return;
      }
      serializeJsonPretty(json, Serial);

      String sval = "";
      int bT, bT2;

      bT = line.indexOf("\"temp\":");
      if (bT > 0) {
        bT2 = line.indexOf(",", bT + 7);
        sval = line.substring(bT + 7, bT2);
        Serial.print(F("temp: "));
        Serial.println(sval);
        tempM = sval.toInt();
      } else {
        Serial.println(F("temp NF!"));
      }
      //tempMin
      bT = line.indexOf("\"temp_min\":");
      if (bT > 0) {
        bT2 = line.indexOf(",", bT + 11);
        sval = line.substring(bT + 11, bT2);
        Serial.print(F("temp min: "));
        Serial.println(sval);
        tempMin = sval.toInt();
      } else {
        Serial.println(F("t_min NF!"));
      }
      //tempMax
      bT = line.indexOf("\"temp_max\":");
      if (bT > 0) {
        bT2 = line.indexOf(",", bT + 11);
        sval = line.substring(bT + 11, bT2);
        Serial.print("temp max: ");
        Serial.println(sval);
        tempMax = sval.toInt();
      } else {
        Serial.println("t_max NF!");
      }
      //pressM
      bT = line.indexOf("\"pressure\":");
      if (bT > 0) {
        bT2 = line.indexOf(",", bT + 11);
        sval = line.substring(bT + 11, bT2);
        Serial.print(F("press "));
        Serial.println(sval);
        presM = sval.toInt();
      } else {
        Serial.println(F("pres NF!"));
      }
      //humiM
      bT = line.indexOf("\"humidity\":");
      if (bT > 0) {
        bT2 = line.indexOf("}", bT + 11);
        sval = line.substring (bT + 11, bT2);
        Serial.print(F("humi "));
        Serial.println(sval);
        humiM = sval.toInt();
      } else {
        Serial.println(F("hum NF!"));
      }
      Serial.println(tempM);
      Serial.println(tempMin);
      Serial.println(tempMax);
      Serial.println(presM);
      Serial.println(humiM);
    }
  }
  haveWeather = true;
}

int xo = 1;
int yo = 26;

void draw_weather() {
  if (haveWeather == false) {
    getWeather();
  }

  uint16_t cc_dgr = display->color565(30, 30, 30);
  //  Serial.println(F("showing the weather"));
  xo = 0;
  yo = 1;
  TFDrawText(display, String("                "), xo, yo, cc_dgr);
  if (tempM == -10000 || humiM == -10000 || presM == -10000) {
    //TFDrawText (&display, String("NO WEATHER DATA"), xo, yo, cc_dgr);
    //    Serial.println(F("!no weather data available"));
  } else {
    //weather below the clock
    //-temperature
    int lcc = currentColors->red;
    if (*u_metric == 'Y') {
      //C
      if (newTemp < 26) {
        lcc = currentColors->green;
      }
      if (newTemp < 18) {
        lcc = currentColors->blue;
      }
      if (newTemp < 6) {
        lcc = currentColors->white_digits;
      }
    } else {
      //F
      if (newTemp < 79) {
        lcc = currentColors->green;
      }
      if (newTemp < 64) {
        lcc = currentColors->blue;
      }
      if (newTemp < 43) {
        lcc = currentColors->white_digits;
      }
    }
    //
    String lstr = String(newTemp) + String((*u_metric == 'Y') ? "C" : "F");
    Serial.print(F("tempI:"));
    Serial.println(lstr);
    TFDrawText(display, lstr, xo, yo, lcc);

    lcc = currentColors->red;
    if (*u_metric == 'Y') {
      //C
      if (tempM < 26) {
        lcc = currentColors->green;
      }
      if (tempM < 18) {
        lcc = currentColors->blue;
      }
      if (tempM < 6) {
        lcc = currentColors->white_digits;
      }
    } else {
      //F
      if (tempM < 79) {
        lcc = currentColors->green;
      }
      if (tempM < 64) {
        lcc = currentColors->blue;
      }
      if (tempM < 43) {
        lcc = currentColors->white_digits;
      }
    }

    xo = TF_COLS * lstr.length();
    TFDrawText(display, String("/"), xo, yo, currentColors->green);

    xo += TF_COLS;
    lstr = String(tempM) + String((*u_metric == 'Y') ? "C" : "F");
    Serial.print(F("tempO:"));
    Serial.println(lstr);
    TFDrawText(display, lstr, xo, yo, lcc);

    //weather conditions
    //-humidity
    lcc = currentColors->red;
    if (humiM < 65) {
      lcc = currentColors->green;
    }
    if (humiM < 35) {
      lcc = currentColors->blue;
    }
    if (humiM < 15) {
      lcc = currentColors->white_digits;
    }
    lstr = String (humiM) + "%";
    xo = 8 * TF_COLS;
    TFDrawText(display, lstr, xo, yo, lcc);

    //-pressure
    lstr = String(presM);
    if (lstr.length() < 4) {
      lstr = " " + lstr;
    }
    xo = 12 * TF_COLS;
    TFDrawText(display, lstr, xo, yo, currentColors->green);

    //draw temp min
    if (tempMin > -10000) {
      xo = 0 * TF_COLS;
      yo = 27;
      TFDrawText(display, "   ", xo, yo, 0);
      lstr = String(tempMin);// + String((*u_metric=='Y')?"C":"F");
      //blue if negative
      uint16_t ct = lowColors.white_temps;
      // if (tempMin < 0) {
      //   ct = lowColors.blue;
      //   lstr = String(-tempMin);// + String((*u_metric=='Y')?"C":"F");
      // }
      Serial.print(F("temp min: "));
      Serial.println(lstr);
      TFDrawText(display, lstr, xo, yo, ct);
    }

    //draw temp max
    if (tempMax > -10000) {
      TFDrawText(display, "   ", 13 * TF_COLS, yo, 0);
      //move the text to the right or left as needed
      xo = 14 * TF_COLS;
      yo = 27;
      if (tempMax < 10) {
        xo = 15 * TF_COLS;
      }
      if (tempMax > 99) {
        xo = 13 * TF_COLS;
      }
      lstr = String (tempMax);// + String((*u_metric=='Y')?"C":"F");
      //blue if negative
      uint16_t ct = lowColors.white_temps;
      if (tempMax < 0) {
        ct = lowColors.blue;
        lstr = String(-tempMax);// + String((*u_metric=='Y')?"C":"F");
      }
      Serial.print(F("temp max: "));
      Serial.println(lstr);
      TFDrawText(display, lstr, xo, yo, ct);
    }
  }
}

//
void draw_date() {

  Serial.println(F("showing the date"));
  //for (int i = 0 ; i < 12; i++)
  //TFDrawChar (&display, '0' + i%10, xo + i * 5, yo, display->color565 (0, 255, 0));
  //date below the clock
  char mnth[5];
  byte m = month();
  Serial.print("Month()=");
  Serial.println(m);
  strcpy(mnth, month_table[month() - 1]);
  String lstr = String(mnth) + "-" + String(day());
  Serial.print("Month:");
  Serial.println(lstr);

  //  for (int i = 0; i < 5; i += 2) {
  //    switch (date_fmt[i]) {
  //      case 'D':
  //        lstr += (day(tnow) < 10 ? "0" + String(day(tnow)) : String(day(tnow)));
  //        if (i < 4)
  //          lstr += date_fmt[i + 1];
  //        break;
  //      case 'M':
  //        lstr += (month(tnow) < 10 ? "0" + String(month(tnow)) : String(month(tnow)));
  //        if (i < 4)
  //          lstr += date_fmt[i + 1];
  //        break;
  //      case 'Y':
  //        lstr += String(year(tnow));
  //        if (i < 4)
  //          lstr += date_fmt[i + 1];
  //        break;
  //    }
  //  }

  if (lstr.length() > 0) {
    xo = 5 * TF_COLS;
    yo = 27;
    TFDrawText(display, lstr, xo, yo, currentColors->green);
  }
}

void processSyncEvent (NTPEvent_t ntpEvent) {
  Serial.println("processSyncEvent");
  switch (ntpEvent.event) {
    case timeSyncd:
      Serial.print(F("Got NTP time:"));
      Serial.println(NTP.getTimeDateString(NTP.getLastNTPSync()));
      ntpsynced = 1;
      break;
    case partlySync:
    case syncNotNeeded:
    case accuracyError:
      Serial.printf ("[NTP-event] %s\n", NTP.ntpEvent2str (ntpEvent));
      break;
    default:
      break;
  }
}

void draw_digits() {
  digit0.setColor(currentColors->white_digits);
  digit1.setColor(currentColors->white_digits);
  digit2.setColor(currentColors->white_digits);
  digit3.setColor(currentColors->white_digits);
  digit4.setColor(currentColors->white_digits);
  digit5.setColor(currentColors->white_digits);

  display->fillScreen(0);

  digit2.DrawColon(currentColors->white_digits);
  digit4.DrawColon(currentColors->white_digits);

  digit0.Draw(digit0.Value());
  digit1.Draw(digit1.Value());
  digit2.Draw(digit2.Value());
  digit3.Draw(digit3.Value());
  digit4.Draw(digit4.Value());
  digit5.Draw(digit5.Value());

  display->drawPixel(62, 19, errColor);

  Serial.print("draw_digits with color:");
  Serial.println(currentColors->white_digits);
}

byte prevhh = 0;
byte prevmm = 0;
byte prevss = 0;
void loop() {

  if (ntpSyncReceived) {
    ntpSyncReceived = 0;
    processSyncEvent(ntp_event);
  }

  hh = hour();   //NTP.getHour ();
  hh24 = hh;
  mm = minute(); //NTP.getMinute ();
  ss = second(); //NTP.getSecond ();

  if (ntpsynced == 0) {
    Serial.println("Exiting loop. Waiting for ntp sync.");
    return;
  }

  if ((hh >= 8) && (hh < 20)) {
    if ((currentColors == &lowColors) || (firstDraw == 1)) {
      currentColors = &highColors;
      if (firstDraw == 1) {
        digit0.Draw(ss % 10);
        digit1.Draw(ss / 10);
        digit2.Draw(mm % 10);
        digit3.Draw(mm / 10);
        digit4.Draw(hh % 10);

        if ((military[0] == 'N') && (hh / 10 == 0)) {
          digit5.hide();
        } else {
          digit5.Draw(hh / 10);
        }
      }
      draw_digits();
      draw_weather();
      draw_date();
    }
  } else {
    if ((currentColors == &highColors) || (firstDraw == 1)) {
      currentColors = &lowColors;
      if (firstDraw == 1) {
        digit0.Draw(ss % 10);
        digit1.Draw(ss / 10);
        digit2.Draw(mm % 10);
        digit3.Draw(mm / 10);
        digit4.Draw(hh % 10);

        if ((military[0] == 'N') && (hh / 10 == 0)) {
          digit5.hide();
        } else {
          digit5.Draw(hh / 10);
        }
      }
      draw_digits();
      draw_weather();
      draw_date();
    }
  }

  firstDraw = 0;

  //   if (ntpsync) {

  // //    Serial.print(F("Mm3:"));
  // //    Serial.println(system_get_free_heap_size());

  //     ntpsync = 0;
  //     //
  //     prevss = ss;
  //     prevmm = mm;
  //     prevhh = hh;
  //     //brightness control: dimmed during the night(25), bright during the day(150)
  // //    if (hh >= 20 && cin == 150) {
  //  //     cin = 25;
  // //      Serial.println(F("night mode brightness"));
  // //    }
  //     if ((hh >= 8) && (hh < 20)) {
  //       display->setBrightness8(255); //0-255
  // //      Serial.println(F("night mode brightness"));
  //     } else {
  //       display->setBrightness8(90); //0-255
  //     }
  //     //during the day, bright
  // //    if (hh >= 8 && hh < 20 && cin == 25) {
  // //      cin = 150;
  // //      Serial.println(F("day mode brightness"));
  // //    }
  //     //we had a sync so draw without morphing
  //     int cc_gry = display->color565(255, 255, 255);
  //     int cc_dgr = display->color565(30, 30, 30);
  //     //dark blue is little visible on a dimmed screen
  //     //int cc_blu = display->color565(0, 0, cin);
  //     int cc_grn = display->color565(0, cin, 0);
  //     int cc_col = cc_gry;
  //     //
  //     //if (cin == 25) {
  //     //  cc_col = cc_dgr;
  //     //}
  //     //reset digits color
  //     digit0.setColor(cc_col);
  //     digit1.setColor(cc_col);
  //     digit2.setColor(cc_col);
  //     digit3.setColor(cc_col);
  //     digit4.setColor(cc_col);
  //     digit5.setColor(cc_col);
  //     digitColor = cc_col;

  //     //clear screen
  //     display->fillScreen(0);

  //     //date and weather
  //     draw_weather();
  //     draw_date();

  //     // Draw semicolon between digits
  //     digit2.DrawColon(cc_col);
  //     digit4.DrawColon(cc_col);

  //     //military time?
  //     if (military[0] == 'N') {
  //       hh = hourFormat12();
  //     }
  //     //
  //     digit0.Draw(ss % 10);
  //     digit1.Draw(ss / 10);
  //     digit2.Draw(mm % 10);
  //     digit3.Draw(mm / 10);
  //     digit4.Draw(hh % 10);

  //     if ((military[0] == 'N') && (hh/10==0)) {
  //       digit5.hide();
  //     } else {
  //       digit5.Draw(hh / 10);
  //     }

  //     if (military[0] == 'N') {
  //       TFDrawChar(display, (isAM()?'A':'P'), 63 - 1 + 3 - 9 * 2, 19, cc_grn);
  //       TFDrawChar(display, 'M', 63 - 1 - 2 - 9 * 1, 19, cc_grn);
  //     }

  //     // Draw the error pixel after a reset
  //     display->drawPixel(62,19,errColor);

  //   } else {

  //seconds
  if (ss != prevss) {

    // Draw the error pixel every second
    display->drawPixel(62, 19, errColor);

    //      Serial.print(F("Mm4:"));
    //      Serial.println(system_get_free_heap_size());

    int s0 = ss % 10;
    int s1 = ss / 10;
    if (s0 != digit0.Value()) {
      digit0.Morph(s0);
    }
    if (s1 != digit1.Value()) {
      digit1.Morph(s1);
    }
    prevss = ss;

    // //Tell slave the temperature unit, every 15 and 45 seconds
    // if (ss==15 || ss==45) {
    //   Serial.print(">");
    //   Serial.print(String((*u_metric=='Y')?"C":"F"));
    //   Serial.println("<");
    // }

    //get weather from API every 5mins and 30sec
    if (ss == 30 && ((mm % 5) == 0)) {
      getWeather();
    }
  }

  //minutes
  if (mm != prevmm) {

    char str[50];
    strftime(str, sizeof str, "%x - %I:%M%p", &timeInfo);
    Serial.println(str);

    sensors.requestTemperatures();
    if (*u_metric == 'Y') {
      newTemp = sensors.getTempCByIndex(0);
      Serial.print(newTemp);
      Serial.println("ºC");
    } else {
      newTemp = sensors.getTempFByIndex(0);
      Serial.print(newTemp);
      Serial.println("ºF");
    }

    draw_date();
    int m0 = mm % 10;
    int m1 = mm / 10;
    if (m0 != digit2.Value()) {
      digit2.Morph (m0);
    }
    if (m1 != digit3.Value()) {
      digit3.Morph (m1);
    }
    prevmm = mm;

    // Update weather info on display every minute
    draw_weather();
  }
  //hours
  if (hh != prevhh) {
    prevhh = hh;

    // Update the date info on the display only on the full hour
    draw_date();

    //brightness control: dimmed during the night(25), bright during the day(150)
    if (hh == 20 || hh == 8) {
      // ntpsync = 1;
      //bri change is taken care of due to the sync
    }
    //military time?
    if (military[0] == 'N') {
      hh = hourFormat12();
    }
    //
    int h0 = hh % 10;
    int h1 = hh / 10;
    if (h0 != digit4.Value()) {
      digit4.Morph(h0);
    }
    //if (h1 != digit5.Value ()) digit5.Morph (h1);

    if (military[0] != 'N') {
      if (h1 != digit5.Value()) {
        digit5.setColor(currentColors->white_digits);
        digit5.Morph(h1);
      }
    } else {
      if (h1 == 0) {
        digit5.hide();
      } else {
        if (h1 != digit5.Value()) {
          digit5.setColor(currentColors->white_digits);
          digit5.Morph(h1);
        }
      }
    }

    if (military[0] == 'N') {
      TFDrawChar(display, (isAM() ? 'A' : 'P'), 63 - 1 + 3 - 9 * 2, 19, currentColors->green);
      TFDrawChar(display, 'M', 63 - 1 - 2 - 9 * 1, 19, currentColors->green);
    }
  }//hh changed
  //  }
  //set NTP sync interval as needed
  if (NTP.getInterval() < 3600 && year() > 1970) {
    //reset the sync interval if we're already in sync
    NTP.setInterval(3600 * 24);//re-sync every 24 hours
  }
}
