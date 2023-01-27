#ifndef CROSSMGR_LAP_COUNTER
#define CROSSMGR_LAP_COUNTER
#include <Arduino.h>
#include <WebSocketsClient.h>   //connecting to CrossMgr https://github.com/Links2004/arduinoWebSockets
#include <ArduinoJson.h>        //parsing JSON https://arduinojson.org/
#include <FastLED.h>            //LED strip http://fastled.io/  (we use the CRGB struct)
#include <TimeLib.h>            //general clockery https://www.pjrc.com/teensy/td_libs_Time.html

void crossmgrSetup(IPAddress ip, int reconnect_interval);

void crossmgrSetup(IPAddress ip, int reconnect_interval, CRGB default_fg, CRGB default_bg);

void crossmgrSetup(IPAddress ip, int reconnect_interval, boolean override_colours, CRGB default_fg, CRGB default_bg);

boolean crossmgrConnected();

boolean crossmgrRaceInProgress();

int crossmgrLaps(int group);

boolean crossmgrWantsLapClock();

unsigned long crossmgrLapStart(int group);

unsigned long crossmgrLapElapsed(int group);

unsigned long crossmgrRaceStart();

unsigned long crossmgrRaceElapsed();

#ifdef ENABLE_SPRINT_EXTENSIONS
double crossmgrSprintTime();

double crossmgrSprintSpeed();

int crossmgrSprintBib();

unsigned long crossmgrSprintAge();
#endif

void crossmgrSetOnWallTime(void (*fp)(const time_t, const int millis));

void crossmgrOnWallTime(const time_t t, const int millis);

void crossmgrSetOnNetwork(void (*fp)());

void crossmgrOnNetwork();

void crossmgrLoop();

void crossmgrWebSocketEvent(WStype_t type, uint8_t * payload, size_t length);

CRGB crossmgrParseColour(const char* colour_string);

boolean crossmgrColoursAreDefault(int group, CRGB fg_colour, CRGB bg_colour);

#endif
