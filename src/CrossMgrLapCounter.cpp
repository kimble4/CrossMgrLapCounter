#define CROSSMGR_LAP_COUNTER_VERSION 20230128.3
#include "CrossMgrLapCounter.h"

#define RACE_TIMEOUT 60000  // milliseconds - how long after CrossMgr stops sending data do we consider the race to be over?
#define COLOUR_SET_INTERVAL 30000  //how frequently colours from CrossMgr are parsed in milliseconds
#define CROSSMGR_PORT 8767  //this is the websocket port, not the web interface
#define CROSSMGR_CLOCK_SYNC_INTERVAL 300000 //how often to sync the walltime (milliseconds)
#define RACE_TIME_UPDATE_INTERVAL 30000  //how often to re-sync the local race clock, don't want to do this too often as it may cause visible jitter (milliseconds)
#define MAX_RACE_START_TIME_DELTA 750 //how many milliseconds do we allow the race start time to drift by without resetting
#define NUM_LAPCOUNTERS 6 //how many lap counter fields to parse

#define DEBUG
//#define DEBUG_JSON

boolean _crossmgr_overrride_default_colours = false;
unsigned long _crossmgr_race_start = 0;
boolean _crossmgr_race_in_progress = false;
boolean _crossmgr_wsc_connected = false;
boolean _crossmgr_lap_elapsed_clock = false;
int _crossmgr_laps[NUM_LAPCOUNTERS];
unsigned long _crossmgr_lap_start_times[NUM_LAPCOUNTERS];
boolean _crossmgr_flash_laps[NUM_LAPCOUNTERS];
unsigned long _crossmgr_last_got_race_time = -RACE_TIMEOUT;
unsigned long _crossmgr_last_updated_race_time = -RACE_TIME_UPDATE_INTERVAL;
unsigned long _crossmgr_last_colour_set = -COLOUR_SET_INTERVAL;
unsigned long _crossmgr_last_clock_set = 0;
unsigned long _crossmgr_set_clock_at = 0;  //zero means time should be set on first connect
time_t _crossmgr_time_to_set = 0;
#ifdef ENABLE_SPRINT_EXTENSIONS
unsigned long _crossmgr_last_got_sprint_data = -RACE_TIMEOUT;
double _crossmgr_sprint_time = -1;
double _crossmgr_sprint_speed = -1;
int _crossmgr_sprint_bib = -1;
time_t _crossmgr_sprint_start_time = 0;
char _crossmgr_sprint_unit[10] = "";
#endif

CRGB _crossmgr_fg_colour[NUM_LAPCOUNTERS];
CRGB _crossmgr_bg_colour[NUM_LAPCOUNTERS];


//the websocket
//note the TCP timeout setting in WebSockets.h:
//#define WEBSOCKETS_TCP_TIMEOUT (5000)
WebSocketsClient _crossmgr_webSocket;

// The filter: it contains "true" for each value we want to keep
/* size 208 calculated using https://arduinojson.org/v6/assistant/
for:
{
  "tNow": true,
  "curRaceTime": true,
  "raceStartTime": true,
  "labels": true,
  "foregrounds": true,
  "backgrounds": true,
  "lapElapsedClock": true,
  "sprintBib": true,
  "sprintDistance": true,
  "speedUnit": true,
  "sprintStart": true,
  "sprintTime": true,
  "sprintSpeed": true
}
size 112 calculated using https://arduinojson.org/v6/assistant/
for:
{
  "tNow": true,
  "curRaceTime": true,
  "raceStartTime": true,
  "labels": true,
  "foregrounds": true,
  "backgrounds": true,
  "lapElapsedClock": true
}
*/
#ifdef ENABLE_SPRINT_EXTENSIONS
StaticJsonDocument<208> filter;
#else
StaticJsonDocument<112> filter;
#endif

void crossMgrSetup(IPAddress ip, int reconnect_interval) {
	crossMgrSetup(ip, reconnect_interval, false, CRGB::White, CRGB::White);
}

void crossMgrSetup(IPAddress ip, int reconnect_interval, CRGB default_fg, CRGB default_bg) {
	crossMgrSetup(ip, reconnect_interval, true, default_fg, default_bg);
}

void crossMgrSetup(IPAddress ip, int reconnect_interval, boolean override_colours, CRGB default_fg, CRGB default_bg) {
	_crossmgr_overrride_default_colours = override_colours;
	//set up JSON filter to only process the fields we need
	filter["tNow"] = true;				//wall time
	//filter["raceStartTime"] = true;	//we don't need both of these
	filter["curRaceTime"] = true;		//this one is easier to parse
	filter["labels"] = true;			//lap counters
	filter["foregrounds"] = true;		//foreground colour
	filter["backgrounds"] = true;		//background colour
	filter["lapElapsedClock"] = true;	//enable lap elapsed time
	#ifdef ENABLE_SPRINT_EXTENSIONS
	//filter["sprintDistance"] = true;	//we don't use this
 	filter["sprintBib"] = true;			//bib number for sprint mode
 	filter["sprintStart"] = true;		//epioch time the sprint was recorded
 	filter["sprintTime"] = true;		//sprint time (float seconds)
 	filter["sprintSpeed"] = true;		//sprint speed (unitless float)
 	filter["speedUnit"] = true;		//sprint unit (string)
	#endif
	#if defined (DEBUG_JSON) || defined (DEBUG)
	char buf[200];
	#endif
	#if defined DEBUG_JSON
	serializeJsonPretty(filter, buf);
	crossMgrDebug(F(buf));
	crossMgrDebug(F("\r\n"));
	#endif
	//init lapcounter data
	for (int i = 0; i < NUM_LAPCOUNTERS; i++) {
		_crossmgr_laps[i] = 0;
		_crossmgr_flash_laps[i] = false;
		if (_crossmgr_overrride_default_colours) {  //override the default colours with something more appropriate for LED displays than the CrossMgr defaults
			_crossmgr_fg_colour[i] = default_fg;
			_crossmgr_bg_colour[i] = default_bg;
		}
	}
	//set up websocket  
	//server address, port and URL
	#ifdef DEBUG
	snprintf_P(buf, sizeof(buf), PSTR("[CMr] Connecting websocket client to %u.%u.%u.%u:%u\r\n"), ip[0], ip[1], ip[2], ip[3], CROSSMGR_PORT);
	crossMgrDebug(buf);
	#endif
	_crossmgr_webSocket.begin(ip, CROSSMGR_PORT, "/");
	//event handler
	_crossmgr_webSocket.onEvent(crossMgrWebSocketEvent);
	_crossmgr_webSocket.setReconnectInterval(reconnect_interval);
	// start heartbeat (optional)
	// ping server every 15000 ms
	// expect pong from server within 3000 ms
	// consider connection disconnected if pong is not received 2 times
	_crossmgr_webSocket.enableHeartbeat(reconnect_interval, 3000, 2);
}

void crossMgrDisconnect() {
	_crossmgr_webSocket.disconnect();
}

void crossMgrConnect(IPAddress ip) {
	_crossmgr_webSocket.begin(ip, CROSSMGR_PORT, "/");
}

boolean crossMgrConnected() {
	return(_crossmgr_wsc_connected);
}

boolean crossMgrRaceInProgress() {
	return(_crossmgr_race_in_progress);
}

int crossMgrLaps(int group) {
	return(_crossmgr_laps[group]);
}

boolean crossMgrFlashLaps(int group) {
	return(_crossmgr_flash_laps[group]);
}

boolean crossMgrWantsLapClock() {
	return(_crossmgr_lap_elapsed_clock);
}

unsigned long crossMgrLapStart(int group) {
return(_crossmgr_lap_start_times[group]);
}

unsigned long crossMgrLapElapsed(int group) {
	return(millis() - _crossmgr_race_start - _crossmgr_lap_start_times[group]);
}

unsigned long crossMgrRaceStart() {
	return(_crossmgr_race_start);
}

unsigned long crossMgrRaceElapsed() {
	return(millis() - _crossmgr_race_start);
}

CRGB crossMgrGetFGColour(int group) {
	return(crossMgrGetColour(group, true));
}

CRGB crossMgrGetBGColour(int group) {
	return(crossMgrGetColour(group, false));
}

CRGB crossMgrGetColour(int group, boolean foreground) {
	if (foreground) {
		return (_crossmgr_fg_colour[group]);
	} else {
		return (_crossmgr_bg_colour[group]);
	}
}

#ifdef ENABLE_SPRINT_EXTENSIONS
double crossMgrSprintTime() {
	return(_crossmgr_sprint_time);
}

double crossMgrSprintSpeed() {
	return(_crossmgr_sprint_speed);
}

int crossMgrSprintBib() {
	return(_crossmgr_sprint_bib);
}

time_t crossMgrSprintStart() {
	return(_crossmgr_sprint_start_time);
}

const char * crossMgrSprintUnit() {
	return(_crossmgr_sprint_unit);
}

unsigned long crossMgrSprintAge() {
	return(millis() - _crossmgr_last_got_sprint_data);
}

void (*fpOnGotSprintData)(const unsigned long t);
void crossMgrSetOnGotSprintData(void (*fp)(const unsigned long t)) {
	fpOnGotSprintData = fp;
}

void crossMgrOnGotSprintData(unsigned long t) {  //callback for when sprint data arrives
	if (0 != fpOnGotSprintData) {
		(*fpOnGotSprintData)(t);
	}
}
#endif

void (*fpOnWallTime)(const time_t t, const int millis);
void crossMgrSetOnWallTime(void (*fp)(const time_t t, const int millis)) {
	fpOnWallTime = fp;
}
void crossMgrOnWallTime(const time_t t, const int m) {  //CrossMgr sends local time
	if (0 != fpOnWallTime) {
		(*fpOnWallTime)(t, m);
	} else {
		#if defined (ARDUINO_ARCH_ESP32)
		//ESP32 allows us to set the system clock with high precision
		struct timeval tv;
		tv.tv_sec = t;
		tv.tv_usec = m * 1000;
		settimeofday(&tv, NULL);
		#else
		//set these variables, for higher precision clock will be set on the millisecond in the main loop
		_crossmgr_set_clock_at = millis() + 1000 - m;
		_crossmgr_time_to_set = t + 1;
		#endif
	}
}

void (*fpOnNetwork)(const boolean connected);
void crossMgrSetOnNetwork(void (*fp)(const boolean connected)) {
	fpOnNetwork = fp;
}

void crossMgrOnNetwork() {
	if (0 != fpOnNetwork) {
		(*fpOnNetwork)(_crossmgr_wsc_connected);
	}
}

void (*fpOnGotRaceData)(const unsigned long t);
void crossMgrSetOnGotRaceData(void (*fp)(const unsigned long t)) {
	fpOnGotRaceData = fp;
}

void crossMgrOnGotRaceData(unsigned long t) {  //callback for when race data arrives
	if (0 != fpOnGotRaceData) {
		(*fpOnGotRaceData)(t);
	}
}

void (*fpOnGotColours)(const int group);
void crossMgrSetOnGotColours(void (*fp)(const int group)) {
	fpOnGotColours = fp;
}

void crossMgrOnGotColours(int group) {  //callback for when colour data arrives for a group
	if (0 != fpOnGotColours) {
		(*fpOnGotColours)(group);
	}
}

void crossMgrDebug (const __FlashStringHelper * line) {
	char buf[508] = "";  //maximum safe UDP payload is 508 bytes
	strcpy_P(buf, (PGM_P)line);
	crossMgrDebug(buf);
}

void (*fpOnDebug)(const char * line);
void crossMgrSetDebug(void (*fp)(const char * line)) {
	fpOnDebug = fp;
}

void crossMgrDebug(const char * line) {  //callback for debugging output
	if (0 != fpOnDebug) {
		(*fpOnDebug)(line);
	}
}

void crossMgrLoop() {
	_crossmgr_webSocket.loop();
	#if ! defined (ARDUINO_ARCH_ESP32)
	if (_crossmgr_set_clock_at && millis() >= _crossmgr_set_clock_at) {  //set clock if scheduled
		setTime(_crossmgr_time_to_set);
		_crossmgr_set_clock_at = 0;
	}
	#endif
}

void crossMgrWebSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
long websocket_event_time = millis();
boolean haveJSON = false;
switch(type) {
	case WStype_DISCONNECTED:
		#ifdef DEBUG
		crossMgrDebug(F("[CMr] WebSocket disconnected!\r\n"));
		#endif
		_crossmgr_wsc_connected = false;
		crossMgrOnNetwork();
		//these are now unknown!
		for (int i = 0; i < NUM_LAPCOUNTERS; i++) {
			_crossmgr_laps[i] = 0;
			_crossmgr_flash_laps[i] = false;
		}
		break;
	case WStype_CONNECTED:
		_crossmgr_wsc_connected = true;
		crossMgrOnNetwork();
		#ifdef DEBUG
		char buf[50];
		snprintf_P(buf, sizeof(buf), PSTR("[CMr] WebSocket connected to url: %s\r\n"), payload);
		crossMgrDebug(buf);
		#endif
		_crossmgr_last_updated_race_time = 0;  //clear this so that race time updates immediately on reconnect
		break;
	case WStype_TEXT:
		{
			crossMgrOnNetwork();
			//allocate memory for JSON parsing document
			#if defined (ARDUINO_ARCH_ESP32)
			StaticJsonDocument<768> doc;
			#else
			StaticJsonDocument<384> doc;
			#endif
			//deserialize the JSON document
			DeserializationError error = deserializeJson(doc, (char*)payload, length, DeserializationOption::Filter(filter));  //using filter
			//DeserializationError error = deserializeJson(doc, (char*)payload, length);  //without filter
			//test if parsing succeeds...
			if (error) {
				crossMgrDebug("[Err] deserializeJson() failed: ");
				crossMgrDebug(error.c_str());
				crossMgrDebug(F("\r\n"));
			} else {
				#ifdef DEBUG_JSON
				char buf[400];
				serializeJsonPretty(doc, buf);
				crossMgrDebug(F(buf));
				crossMgrDebug(F("\r\n"));
				#endif
				//if we haven't recently, get the wall time and set the clock
				if (_crossmgr_last_clock_set == 0 || millis() - _crossmgr_last_clock_set >= CROSSMGR_CLOCK_SYNC_INTERVAL) {  
					const char* tNow = doc["tNow"];
					if (tNow) {  //if we have time data, parse it and set the clock
						char Y[5];
						Y[0] = tNow[0];
						Y[1] = tNow[1];
						Y[2] = tNow[2];
						Y[3] = tNow[3];
						Y[4] = '\0';
						char M[3];
						M[0] = tNow[5];
						M[1] = tNow[6];
						M[2] = '\0';
						char D[3];
						D[0] = tNow[8];
						D[1] = tNow[9];
						D[2] = '\0';
						char h[3];
						h[0] = tNow[11];
						h[1] = tNow[12];
						h[2] = '\0';
						char m[3];
						m[0] = tNow[14];
						m[1] = tNow[15];
						m[2] = '\0';
						char s[3];
						s[0] = tNow[17];
						s[1] = tNow[18];
						s[2] = '\0';
						char mi[4];
						mi[0] = tNow[20];
						mi[1] = tNow[21];
						mi[2] = tNow[22];
						mi[3] = '\0';
						time_t crossmgr_time = 0;
						#if defined (ARDUINO_ARCH_ESP32)
 						struct tm * timeinfo;
						timeinfo = localtime(&crossmgr_time);
						timeinfo->tm_year = atoi(Y) - 1900;
						timeinfo->tm_mon = atoi(M) - 1;
						timeinfo->tm_mday = atoi(D);
						timeinfo->tm_hour = atoi(h);
						timeinfo->tm_min = atoi(m);
						timeinfo->tm_sec = atoi(s);
						crossmgr_time = mktime(timeinfo);
						#else
						TimeElements tm;
						tm.Year = atoi(Y) - 1970;
						tm.Month = atoi(M);
						tm.Day = atoi(D);
						tm.Hour = atoi(h);
						tm.Minute = atoi(m);
						tm.Second = atoi(s);
						crossmgr_time = makeTime(tm);
						#endif
						unsigned int crossmgr_millis = atoi(mi);
						crossMgrOnWallTime(crossmgr_time, crossmgr_millis);
						#ifdef DEBUG
						//we do this after the time-critical bit
						char buf[100];
						snprintf_P(buf, sizeof(buf), PSTR("[CMr] Received wall time: %s (%u.%i)\r\n"), tNow, crossmgr_time, crossmgr_millis);
						crossMgrDebug(buf);
						#endif
						_crossmgr_last_clock_set = millis();
					#ifdef ENABLE_SPRINT_EXTENSIONS
					} else if (_crossmgr_last_clock_set != 0) {  //send local time to server (for sprint timer, which does not have its own RTC)
						StaticJsonDocument<30> timeDoc;
						#if defined (ARDUINO_ARCH_ESP32)
						timeDoc["time"] = time(nullptr);
						#else
						timeDoc["time"] = now();
						#endif
						char out_string[50];
						serializeJson(timeDoc, out_string);
						#ifdef DEBUG
						char buf[100];
						snprintf_P(buf, sizeof(buf), PSTR("[CMr] Sending: %s\r\n"), out_string);
						crossMgrDebug(buf);
						#endif
						_crossmgr_webSocket.sendTXT(out_string);
					#endif
					}
				}
				//update race in progress and start time
				double curRaceTime = doc["curRaceTime"];
				if (curRaceTime) {
					_crossmgr_last_got_race_time = websocket_event_time;
					_crossmgr_race_in_progress = true;
					long new_start = websocket_event_time - (curRaceTime * 1000);
					long diff = _crossmgr_race_start - new_start;
					if (_crossmgr_last_updated_race_time == 0 || (abs(diff) > MAX_RACE_START_TIME_DELTA && millis() - _crossmgr_last_updated_race_time > RACE_TIME_UPDATE_INTERVAL)) {
						_crossmgr_last_updated_race_time = millis();
						_crossmgr_race_start = new_start;
						#ifdef DEBUG
						char buf[100];
						snprintf_P(buf, sizeof(buf), PSTR("[CMr] Resetting race start to %u, delta is %i\r\n"), _crossmgr_race_start, diff);
						crossMgrDebug(buf);
						#endif
					}
				} else {
					_crossmgr_race_in_progress = false;
				}
				//display lap elapsed clock field
				_crossmgr_lap_elapsed_clock = doc["lapElapsedClock"];
				//lap counts
				for (int i = 0; i < NUM_LAPCOUNTERS; i++) {
					_crossmgr_laps[i] = doc["labels"][i][0];
					_crossmgr_flash_laps[i] = doc["labels"][i][1];
					double ltime = doc["labels"][i][2];
					_crossmgr_lap_start_times[i] = ltime * 1000.0;
				}
				//colours
				if (websocket_event_time - _crossmgr_last_colour_set > COLOUR_SET_INTERVAL || _crossmgr_last_colour_set == 0) {
					for (int i = 0; i < NUM_LAPCOUNTERS; i++) {
						const char* foreground_string = doc["foregrounds"][i];
						const char* background_string = doc["backgrounds"][i];
						if (foreground_string != nullptr && background_string != nullptr) {
							CRGB fg_colour = crossMgrParseColour(foreground_string);
							CRGB bg_colour = crossMgrParseColour(background_string);
							if (_crossmgr_overrride_default_colours && crossMgrColoursAreDefault(i, fg_colour, bg_colour)) {
								#ifdef DEBUG
								char buf[100];
								snprintf_P(buf, sizeof(buf), PSTR("[CMr] Ignoring default colours for [%i]\r\n"), i);
								crossMgrDebug(buf);
								#endif
							} else {
								_crossmgr_fg_colour[i] = fg_colour;
								_crossmgr_bg_colour[i] = bg_colour;
								#ifdef DEBUG
								char buf[100];
								snprintf_P(buf, sizeof(buf), PSTR("[CMr] Set colours for [%i]: fg=0x%02X%02X%02X bg=0x%02X%02X%02X\r\n"), i,
									_crossmgr_fg_colour[i].red, _crossmgr_fg_colour[i].green, _crossmgr_fg_colour[i].blue,
									_crossmgr_bg_colour[i].red, _crossmgr_bg_colour[i].green, _crossmgr_bg_colour[i].blue);
								crossMgrDebug(buf);
								#endif
								crossMgrOnGotColours(i);
							}
						}
					}
					_crossmgr_last_colour_set = websocket_event_time;
				}
				#ifdef ENABLE_SPRINT_EXTENSIONS
				//sprint fields 
				//(this is an extension to the CrossMgr protocol for displaying results from the BHPC sprint timing system)
				double sprintTime = doc["sprintTime"];
				double sprintSpeed = doc["sprintSpeed"];
				int sprintBib = doc["sprintBib"];
				time_t sprintStart = doc["sprintStart"];
				const char* speedUnit = doc["speedUnit"];
				boolean new_sprint = false;
				if (sprintTime > 0) {
					_crossmgr_last_got_sprint_data = websocket_event_time;
					if (sprintTime != _crossmgr_sprint_time) {
						new_sprint = true;
						_crossmgr_sprint_time = sprintTime;
						#ifdef DEBUG
						char buf[100];
						snprintf_P(buf, sizeof(buf), PSTR("[CMr] Got sprint time: %.3f\r\n"), _crossmgr_sprint_time);
						crossMgrDebug(buf);
						#endif
					}
				} else if (sprintTime < 0 ) {  //negative sprint time: timeout sprint immediately
					_crossmgr_last_got_sprint_data = websocket_event_time + RACE_TIMEOUT;
					//clear the data
					_crossmgr_sprint_time = -1;
					_crossmgr_sprint_speed = -1;
					_crossmgr_sprint_bib = -1;
				}
				if (sprintSpeed) {
					_crossmgr_last_got_sprint_data = websocket_event_time;
					if (sprintSpeed != _crossmgr_sprint_speed) {
						new_sprint = true;
						_crossmgr_sprint_speed = sprintSpeed;
						#ifdef DEBUG
						char buf[100];
						snprintf_P(buf, sizeof(buf), PSTR("[CMr] Got sprint speed: %.3f\r\n"), _crossmgr_sprint_speed);
						crossMgrDebug(buf);
						#endif
					}
				}
				if (sprintBib) {
					_crossmgr_last_got_sprint_data = websocket_event_time;
					int b = sprintBib;  //temp variable because we test sprintBib again below
					if (b == -1) {  // '0' is a valid value, because Mike Burrows, transmitted as -1
						b = 0;
					}
					if (b != _crossmgr_sprint_bib && b >= 0) {  //discard negative numbers
						new_sprint = true;
						_crossmgr_sprint_bib = b;
						#ifdef DEBUG
						char buf[100];
						snprintf_P(buf, sizeof(buf), PSTR("[CMr] Got sprint bib: %i\r\n"), _crossmgr_sprint_bib);
						crossMgrDebug(buf);
						#endif
					}
				}
				if (sprintStart) {
					_crossmgr_last_got_sprint_data = websocket_event_time;
					if (sprintStart != _crossmgr_sprint_start_time) {
						new_sprint = true;
						_crossmgr_sprint_start_time = sprintStart;
						#ifdef DEBUG
						char buf[100];
						snprintf_P(buf, sizeof(buf), PSTR("[CMr] Got sprint start time: %u\r\n"), _crossmgr_sprint_start_time);
						crossMgrDebug(buf);
						#endif
					}
				}
				if (speedUnit) {
					if (strcmp(speedUnit, _crossmgr_sprint_unit) != 0) {
						snprintf_P(_crossmgr_sprint_unit, sizeof(_crossmgr_sprint_unit), PSTR("%s"), speedUnit);
						#ifdef DEBUG
						char buf[100];
						snprintf_P(buf, sizeof(buf), PSTR("[CMr] Got new speed unit: %s\r\n"), _crossmgr_sprint_unit);
						crossMgrDebug(buf);
						#endif
					}
				}
				if (new_sprint) {
					if (!sprintSpeed) {  //we got new data but no speed
						crossMgrDebug(F("[CMr] Did not get a speed!\r\n"));
						_crossmgr_sprint_speed = -1;
					}
					if (!sprintTime) {  //we got new data but no time
						_crossmgr_sprint_time = -1;
						crossMgrDebug(F("[CMr] Did not get a time!\r\n"));
					}
					if (!sprintBib) {  //we got new data but no bib
						_crossmgr_sprint_bib = -1;  // negative number here denotes absence of data
						crossMgrDebug(F("[CMr] Did not get a bib!\r\n"));
					}
					if (!sprintStart) {  //we got new data but no start time
						_crossmgr_sprint_start_time = 0;
						crossMgrDebug(F("[CMr] Did not get a start time!\r\n"));
					}
					crossMgrOnGotSprintData(websocket_event_time);
				} else {
				#endif
				crossMgrOnGotRaceData(websocket_event_time);
				#ifdef ENABLE_SPRINT_EXTENSIONS
				}
				#endif
			}
		}
		break;
	case WStype_BIN:
		_crossmgr_wsc_connected = true;
		crossMgrOnNetwork();
		crossMgrDebug(F("[CMr] WebSocket got binary, ignoring.\r\n"));
		
		break;
	case WStype_PING:
		_crossmgr_wsc_connected = true;
		crossMgrOnNetwork();
		// pong will be sent automatically
		crossMgrDebug(F("[CMr] WebSocket got ping.\r\n"));
		break;
	case WStype_PONG:
		_crossmgr_wsc_connected = true;
		crossMgrOnNetwork();
		// answer to a ping we send
		crossMgrDebug(F("[CMr] WebSocket got pong.\r\n"));
		//if we're ponging but not getting data, race is unstarted or finished...
		if (websocket_event_time - _crossmgr_last_got_race_time > RACE_TIMEOUT) {  //time out
			crossMgrDebug(F("[CMr] Connected to WebSocket but not racing...\r\n"));
			_crossmgr_race_in_progress = false;
			for (int i = 0; i < NUM_LAPCOUNTERS; i++) {
			_crossmgr_laps[i] = 0;
			_crossmgr_flash_laps[i] = false;
			}
			crossMgrOnGotRaceData(websocket_event_time);  //call this here so application knows we've timed out
		}
		break;
	}
}

CRGB crossMgrParseColour(const char* colour_string) {
//parse string of the form "rgb(21, 1, 117)"
char c[4];
CRGB output = CRGB::Black;
int pointer = 0;
//get the red
while (!isDigit(colour_string[pointer])) {
	if (colour_string[pointer] == '\0') {
	crossMgrDebug(F("[CMr] parseColour() did not find a digit!\r\n"));
	return output;
	}
	pointer++;
}
for (int i = 0; i < 4; i++) {
	if (isDigit(colour_string[pointer])) {
	c[i] = colour_string[pointer];
	pointer++;
	} else {
	c[i] = '\0';
	}
}
output.red = atoi(c);
//get the green
while (!isDigit(colour_string[pointer])) {
	if (colour_string[pointer] == '\0') {
	crossMgrDebug(F("[CMr] parseColour() string terminated before green!\r\n"));
	return output;
	}
	pointer++;
}
for (int i = 0; i < 4; i++) {
	if (isDigit(colour_string[pointer])) {
	c[i] = colour_string[pointer];
	pointer++;
	} else {
	c[i] = '\0';
	}
}
output.green = atoi(c);
//get the blue
while (!isDigit(colour_string[pointer])) {
	if (colour_string[pointer] == '\0') {
	crossMgrDebug(F("[Cmr] parseColour() string terminated before blue!\r\n"));
	return output;
	}
	pointer++;
}
for (int i = 0; i < 4; i++) {
	if (isDigit(colour_string[pointer])) {
	c[i] = colour_string[pointer];
	pointer++;
	} else {
	c[i] = '\0';
	}
}
output.blue = atoi(c);
return output;
}

boolean crossMgrColoursAreDefault(int group, CRGB fg_colour, CRGB bg_colour) {
	switch(group) {
		case 0:
			if (fg_colour == CRGB(255,255,255) && bg_colour == CRGB(16,16,16)) {
				return(true);
			}
			break;
		case 1:
			if (fg_colour == CRGB(255,255,255) && bg_colour == CRGB(34,139,34)) {
				return(true);
			}
			break;
		case 2:
			if (fg_colour == CRGB(255,255,255) && bg_colour == CRGB(235,155,0)) {
				return(true);
			}
			break;
		case 3:
			if (fg_colour == CRGB(255,255,255) && bg_colour == CRGB(147,112,219)) {
				return(true);
			}
			break;
		case 4:
			if (fg_colour == CRGB(255,255,255) && bg_colour == CRGB(0,0,139)) {
				return(true);
			}
			break;
		case 5:
			if (fg_colour == CRGB(255,255,255) && bg_colour == CRGB(139,0,0)) {
				return(true);
			}
			break;
		default:
			return(false);
			break;
	}
	return(false);
}
