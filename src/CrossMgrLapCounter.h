#ifndef CROSSMGR_LAP_COUNTER
#define CROSSMGR_LAP_COUNTER
#define ENABLE_SPRINT_EXTENSIONS  //extensions to the protocol used for displaying results from a sprint timer that pretends to be CrossMgr
#include <Arduino.h>
#include <WebSocketsClient.h>   //connecting to CrossMgr https://github.com/Links2004/arduinoWebSockets
#include <ArduinoJson.h>        //parsing JSON https://arduinojson.org/
#include <FastLED.h>            //LED strip http://fastled.io/  (we use the CRGB struct)
#if ! defined (ARDUINO_ARCH_ESP32)
#include <TimeLib.h>            //general clockery https://github.com/PaulStoffregen/Time
#endif

void crossMgrSetup(IPAddress ip, int reconnect_interval);

void crossMgrSetup(IPAddress ip, int reconnect_interval, CRGB default_fg, CRGB default_bg);

void crossMgrSetup(IPAddress ip, int reconnect_interval, boolean override_colours, CRGB default_fg, CRGB default_bg);

void crossMgrDisconnect();

void crossMgrConnect(IPAddress ip);

boolean crossMgrConnected();

boolean crossMgrRaceInProgress();

int crossMgrLaps(int group);

boolean crossMgrFlashLaps(int group);

boolean crossMgrWantsLapClock();

unsigned long crossMgrLapStart(int group);

unsigned long crossMgrLapElapsed(int group);

unsigned long crossMgrRaceStart();

unsigned long crossMgrRaceElapsed();

CRGB crossMgrGetFGColour(int group);

CRGB crossMgrGetBGColour(int group);

CRGB crossMgrGetColour(int group, boolean foreground);

#ifdef ENABLE_SPRINT_EXTENSIONS
double crossMgrSprintTime();

double crossMgrSprintSpeed();

int crossMgrSprintBib();

unsigned long crossMgrSprintAge();

void crossMgrSetOnGotSprintData(void (*fp)(const unsigned long t));

void crossMgrOnGotSprintData(unsigned long t);
#endif

void crossMgrSetOnWallTime(void (*fp)(const time_t, const int millis));

void crossMgrOnWallTime(const time_t t, const int millis);

void crossMgrSetOnNetwork(void (*fp)(boolean connected));

void crossMgrOnNetwork();

void crossMgrSetOnGotRaceData(void (*fp)(const unsigned long t));

void crossMgrOnGotRaceData(const unsigned long t);

void crossMgrSetOnGotColours(void (*fp)(const int group));

void crossMgrOnGotColours(int group);


void crossMgrDebug (const __FlashStringHelper * line);

void crossMgrSetDebug(void (*fp)(const char * line));

void crossMgrDebug(const char * line);

void crossMgrLoop();

void crossMgrWebSocketEvent(WStype_t type, uint8_t * payload, size_t length);

CRGB crossMgrParseColour(const char* colour_string);

boolean crossMgrColoursAreDefault(int group, CRGB fg_colour, CRGB bg_colour);

#endif
