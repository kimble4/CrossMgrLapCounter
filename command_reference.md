# Network connection

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
