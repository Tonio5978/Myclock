/*   myClock -- ESP8266 WiFi NTP Clock for pixel displays
     Copyright (c) 2018 David M Denney <dragondaud@gmail.com>
     distributed under the terms of the MIT License
*/

#include <ESP8266WiFi.h>        //https://github.com/esp8266/Arduino
#include <ESP8266HTTPClient.h>
#include <ArduinoOTA.h>
#include <time.h>
#include "FS.h"
#include <ArduinoJson.h>        // https://github.com/bblanchon/ArduinoJson/
#include <WiFiManager.h>        // https://github.com/tzapu/WiFiManager

#include "display.h"
#include "userconfig.h"
#define APPNAME "myClock"

// define in userconfig.h or uncomment here
//#undef DEBUG
//#define SYSLOG
//String syslogSrv = "daud-thinkpad";
//String tzKey = "APIKEY"           // from https://timezonedb.com/register
//String owKey = "APIKEY"           // from https://home.openweathermap.org/api_keys
//String softAPpass = "ConFigMe";  // password for SoftAP config

// Syslog
#ifdef SYSLOG
#include <Syslog.h>             // https://github.com/arcao/Syslog
WiFiUDP udpClient;
Syslog syslog(udpClient, SYSLOG_PROTO_IETF);
#endif

const char* UserAgent = "myClock/1.0 (Arduino ESP8266)";

time_t TWOAM, pNow, wDelay;
int pHH, pMM, pSS;
long offset;
String timezone, location;
char HOST[20];
bool saveConfig = false;

void setup() {
  Serial.begin(74880);            // match nodemcu bootloader speed
  //Serial.setDebugOutput(true);  // uncomment for extra debugging
  while (!Serial);
  Serial.println();
  readSPIFFS();

  display.begin(16);
  display_ticker.attach(0.002, display_updater);
  display.clearDisplay();
  display.setTextWrap(false);
  display.setCursor(2, row2);
  display.setTextColor(myColor);
  display.print(F("Connecting"));

  startWiFi();
  if (saveConfig) writeSPIFFS();

#ifdef SYSLOG
  syslog.server(syslogSrv.c_str(), syslogPort);
  syslog.deviceHostname(HOST);
  syslog.appName(APPNAME);
  syslog.defaultPriority(LOG_INFO);
#endif

  if (timezone == "") location = getIPlocation();

  display.clearDisplay();
  display.setFont(&Picopixel);
  display.setCursor(2, row1);
  display.setTextColor(myGREEN);
  display.print(HOST);
  display.setCursor(2, row2);
  display.setTextColor(myBLUE);
  display.print(WiFi.localIP());
  display.setCursor(2, row3);
  display.setTextColor(myMAGENTA);
  display.print(timezone);
  display.setCursor(2, row4);
  display.setTextColor(myCYAN);
  display.print(F("waiting for ntp"));

  setNTP(timezone);

  delay(1000);
  display.clearDisplay();
  time_t now = time(nullptr);
  int ss = now % 60;
  int mm = (now / 60) % 60;
  int hh = (now / (60 * 60)) % 24;
  digit1.DrawColon(myColor);
  digit3.DrawColon(myColor);
  digit0.Draw(ss % 10);
  digit1.Draw(ss / 10);
  digit2.Draw(mm % 10);
  digit3.Draw(mm / 10);
  digit4.Draw(hh % 10);
  digit5.Draw(hh / 10);
  pNow = now;
  getWeather();
} // setup

void loop() {
  ArduinoOTA.handle();
  time_t now = time(nullptr);
  if (now != pNow) {
    if (now > TWOAM) setNTP(timezone);
    int ss = now % 60;
    int mm = (now / 60) % 60;
    int hh = (now / (60 * 60)) % 24;
    if (ss != pSS) {
      int s0 = ss % 10;
      int s1 = ss / 10;
      if (s0 != digit0.Value()) digit0.Morph(s0);
      if (s1 != digit1.Value()) digit1.Morph(s1);
      pSS = ss;
    }

    if (mm != pMM) {
      int m0 = mm % 10;
      int m1 = mm / 10;
      if (m0 != digit2.Value()) digit2.Morph(m0);
      if (m1 != digit3.Value()) digit3.Morph(m1);
      pMM = mm;
    }

    if (hh != pHH) {
      int h0 = hh % 10;
      int h1 = hh / 10;
      if (h0 != digit4.Value()) digit4.Morph(h0);
      if (h1 != digit5.Value()) digit5.Morph(h1);
      pHH = hh;
    }
    pNow = now;
    if (now > wDelay) getWeather();
  }
}
