# CrossMgrLapCounter
Arduino library to access [CrossMgr](https://github.com/esitarski/CrossMgr)'s lapcounter websocket interface, compatible with ESP8266 and ESP32 architectures, which have built-in WiFi.

This allows you to build a hardware-agnostic lap and/or elapsed time display, to avoid the problems with making computer monitors readable in bright sunlight.  Addressable LED strips work well, but this could be used to drive a traditional seven-segment or matrix LED display.  If you build a lap counter using something exotic such as split-flaps or nixie tubes, please send photos :)

## Dependencies:

Arduino Websockets: https://github.com/Links2004/arduinoWebSockets

ArduinoJSON: https://arduinojson.org/

FastLED: http://fastled.io/ (we use the CRGB struct to represent colours for compatibility with FastLED)

**For ESP8266 only** (as it lacks POSIX time functions):
TimeLib: https://github.com/PaulStoffregen/Time

## Examples:
Documentation and examples to come.
This library is derived from mature code we've been using to run an LED elapsed time clock at [BHPC](http://www.bhpc.org.uk/) races for a couple of years.

