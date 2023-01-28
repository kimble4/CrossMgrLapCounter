# CrossMgrLapCounter
Arduino library to access [CrossMgr](https://github.com/esitarski/CrossMgr)'s lapcounter websocket interface, intended for ESP8266 and ESP32 architectures.

Dependencies:

Arduino Websockets: https://github.com/Links2004/arduinoWebSockets
ArduinoJSON: https://arduinojson.org/
FastLED: http://fastled.io/ (we use the CRGB struct to represent colours for compatibility with FastLED)

For ESP8266 only (as it lacks POSIX time functions):
TimeLib: https://github.com/PaulStoffregen/Time


This library is derived from mature code we've been using to run an LED elapsed time clock at [BHPC](http://www.bhpc.org.uk/) races for a couple of years.

Still a work in progress.  Documentation and more examples to come.
