# Setup and Network connection

`void crossMgrLoop()`

This must be called frequently from within the main `loop()` in order for the library to process incoming data.

`void crossMgrSetup(IPAddress ip, int reconnect_interval)`

`void crossMgrSetup(IPAddress ip, int reconnect_interval, CRGB default_fg, CRGB default_bg)`

Sets up and begins the connection to CrossMgr.  Takes an [IPAddress](https://links2004.github.io/Arduino/dd/d5c/class_i_p_address.html) and a reconnect interval in milliseconds.  Optional [CRGB](http://fastled.io/docs/3.1/struct_c_r_g_b.html) parameters override the default colours when supplied by CrossMgr, as they're poorly suited for LED displays.

`void crossMgrDisconnect()`

Disconnects from CrossMgr.

`void crossMgrConnect(IPAddress ip)`

Connects to a new address.

`boolean crossMgrConnected()`

Returns true if the WebSocket is currently connected; otherwise returns false.

`void crossMgrSetOnNetwork(void (*fp)(boolean connected))`

Sets a callback for network activity (including disconnection).  `connected` is true if the WebSocket is currently connected.  This can be used to blink an LED to indicate network traffic, or to clear a display when the connection fails.


# Race data

`boolean crossMgrRaceInProgress()`

Returns true if a race is currently in progress.  If the WebSocket is currently disconnected, it will return true if a race was in progress when the connection was lost.  Otherwise returns false.

`int crossMgrLaps(int group)`

Returns the remaining laps for the specified group.  The first counter is group 0.

`boolean crossMgrFlashLaps(int group)`

Returns true if the lap display for the specified group is about to change.

`boolean crossMgrWantsLapClock()`

Returns the state of CrossMgr's "Show Lap Elapsed Time" setting.

`unsigned long crossMgrLapStart(int group)`

Returns the time in milliseconds, relative to the `millis()` system clock, that the specified group's current lap started.

`unsigned long crossMgrLapElapsed(int group)`

Returns the elapsed time of the specified group's current lap in milliseconds.

`unsigned long crossMgrRaceStart()`

Returns the time in milliseconds, relative to the `millis()` system clock, that the race started.

`unsigned long crossMgrRaceElapsed()`

Returns the elapsed race time in milliseconds.

# Colours
Colours are stored using [FastLED](https://fastled.io/)'s [CRGB](http://fastled.io/docs/3.1/struct_c_r_g_b.html) struct.  This provides convenient ways to define and manpulate colours, which are particularly useful with RGB-capable LED displays.  In the interests of efficiency, colours from CrossMgr are only updated every 30 seconds.

`CRGB crossMgrGetFGColour(int group)`
`CRGB crossMgrGetBGColour(int group)`
`CRGB crossMgrGetColour(int group, boolean foreground)`

Return a [CRGB](http://fastled.io/docs/3.1/struct_c_r_g_b.html) for the given group's lap counter foreground and background colour respectively.  If the default colours were overriden in `crossMgrSetup()`, colours from CrossMgr will be ignored unless they are set to something else.

# Time of day

The library obtains time-of-day from CrossMrg on connection and at 5-minute intervals.  By default this is used to set the system time using TimeLib's `setTime()` or the ESP32 core's `settimeofday()`.

You can access the time of day from your program in the usual ways, eg. `now()` (ESP8266) or `time(nullptr)` (ESP32)

# Callbacks
`void crossMgrSetOnWallTime(void (*fp)(const time_t t, const int millis))`

Sets a callback to override what happens when the library parses the time-of-day from CrossMgr (which normally happens on initial connection, and subsequently at 5 minute intervals).  If this is unset, the library will call TimeLib's `setTime()` or the ESP32 core's `settimeofday()` accordingly.  You can then use TimeLib (on ESP8266) or POSIX (on ESP32) time functions to retrieve and manipulate the current time.

`t` is a time_t type, as defined by TimeLib or the ESP32 core respectively, which represents the unix epoch time in seconds since 1970-01-01-00:00UTC.  `millis` is an additional number of milliseconds after that time, for increased precision.

`void crossMgrSetOnGotRaceData(void (*fp)(const unsigned long t))`

Sets a callback for whenever race data arrives from CrossMgr.  Typically this happens once per second, but can occur more frequently.  `t` contains the time, relative to `millis()` that the data arrived.

`void crossMgrSetOnGotColours(void (*fp)(const int group))`

Sets a callback for when colour data is parsed for a given group.

`void crossMgrSetDebug(void (*fp)(const char * line))`

Sets a callback for the library's debugging output.  The C-string `line` may be `Serial.print()`ed, sent over the network, or whatever.

# Sprint Timer
If `ENABLE_SPRINT_EXTENSIONS` is `#define`ed in CrossMgrLapCounter.h, these additional functions are supported when connected to a sprint timer:

`double crossMgrSprintTime()`

The last rider's time, as a floating-point number of seconds, if available.

`double crossMgrSprintSpeed()`

The last rider's speed, as a unitless floating point number, if available.  The distance/time calculation is performed by the sprint timer itself.

`int crossMgrSprintBib()`

The last rider's bib number, if available.

`time_t crossMgrSprintStart()`

The wall time that the sprint was recorded.

`const char * crossMgrSprintUnit()`

Human-readable speed unit.

`unsigned long crossMgrSprintAge()`

The time, in milliseconds, since the current sprint data first arrived.

`void crossMgrSetOnGotSprintData(void (*fp)(const unsigned long t))`

Sets a callback for whenever sprint data arrives from the sprint timer.
