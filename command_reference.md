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

