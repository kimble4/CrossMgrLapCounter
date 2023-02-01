# CrossMgrLapCounter
Arduino library to access [CrossMgr](https://github.com/esitarski/CrossMgr)'s lapcounter websocket interface, compatible with ESP8266 and ESP32 architectures.

This allows you to build a hardware-agnostic lap and/or elapsed time display, to avoid the problems with making computer monitors readable in bright sunlight.  The ESPs have significantly more processor power and memory than the usual AVR Arduinos, and include built-in WiFi (ESP32 also supports wired Ethernet, with additional hardware - I can recommend the ET32-ETH01 as a useful building block for Ethernet-based projects).  Development boards containing these microcontrollers from the likes of WeMos and NodeMCU can be programmed in the same way as traditional Arduino boards, and are a convenient basis for a lap counter.

For the display, addressable LED strips work well, but this library could equally be used to drive a traditional seven-segment or matrix LED display.  If you build a lap counter using something exotic such as split-flaps or nixie tubes, please send photos :)

## Dependencies:

Arduino Websockets: https://github.com/Links2004/arduinoWebSockets

ArduinoJSON: https://arduinojson.org/

FastLED: http://fastled.io/ (we use the CRGB struct to represent colours for compatibility with FastLED)

**For ESP8266 only** (as it lacks POSIX time functions):
TimeLib: https://github.com/PaulStoffregen/Time

## Examples:
[Command reference](https://github.com/kimble4/CrossMgrLapCounter/blob/main/command_reference.md)

The examples show basic use of the library to output data on the serial terminal.  Driving an LED display is left as an exercise for the reader, but I suggest that addressable LED strips, of the type supported by the FastLED library, are an economical way to build a large, bright LED digital display with minimal additional electronics.

Further examples to come?

This library is derived from code we've been using to run an LED elapsed time clock at [BHPC](http://www.bhpc.org.uk/) races for a couple of years.
