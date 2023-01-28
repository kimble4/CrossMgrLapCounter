#include <WiFi.h>   //wireless networking
#include <CrossMgrLapCounter.h>

#define CROSSMGR_IP 192,168,1,15  //for websocket to connect to (note commas!)
#define WEBSOCKET_RECONNECT_INTERVAL 15000 //milliseconds
#define LED_BUILTIN 2  //for Wemos D1 R32

const char * _wifi_ssid = "ssid"; // your network SSID (name)
const char * _wifi_pass = "password";  // your network password
IPAddress _crossmgr_ip;
time_t _last_sec = 0;
unsigned long _network_light_time = 0;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.begin(115200);
  Serial.print("\r\n");
  Serial.flush();
  delay(100);
  Serial.print(F("\r\n\r\nCrossMgrLapCounterTest "));
//  Serial.print(F("\r\n[Sys] ESP CPU frequency: "));
//  Serial.print(ESP.getCpuFreqMHz());
//  Serial.print(F("MHz\r\n[Sys] Last reset due to: "));
//  Serial.print(ESP.getResetReason());
//  Serial.print(F("\r\n[Sys] Free sketch space: "));
//  Serial.print(ESP.getFreeSketchSpace());
  Serial.print(F("\r\n"));

  Serial.print("Connecting to ");
  Serial.print(_wifi_ssid);
  Serial.print(F("\r\n"));
  WiFi.mode(WIFI_STA);
  WiFi.begin(_wifi_ssid, _wifi_pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print(F("\r\n"));
  Serial.print("WiFi connected.  IP address: ");
  Serial.print(WiFi.localIP());
  Serial.print(F("\r\n"));

  //set up CrossMgr
  _crossmgr_ip=IPAddress(CROSSMGR_IP);
  //set ip address of CrossMgr machine, reconnect interval in milliseconds
  crossMgrSetup(_crossmgr_ip, WEBSOCKET_RECONNECT_INTERVAL);
  //configure callbacks for debugging output and network activity
  crossMgrSetDebug(debug);
  crossMgrSetOnNetwork(networkLight);
}

void debug(const char * line) {  //just print debug output to serial
  Serial.print(line);
}

void networkLight(boolean web_socket_connected) {  //turn the BUILTIN_LED on; it is timed out from the main loop
  if (web_socket_connected) {
    digitalWrite(LED_BUILTIN, LOW);
    _network_light_time = millis();
  }
}

void loop() {
  if (crossMgrRaceInProgress()) { //if there's a race currently in progress
    if (crossMgrRaceElapsed() - _last_sec >= 1000) {  //do this every second
      //We adjust this so the next iteration occurs on the whole second
      //This is useful if you want to build a clock that diplays race time
      _last_sec = crossMgrRaceElapsed() - crossMgrRaceElapsed()%1000;
      
      //The CrossMgrLapCounter library obtains the time of day from CrossMgr and uses it to set the ESP32's clock
      //We can use the stanard POSIX time functions to access it:
      time_t epoch_time = time(nullptr);  //gets the current time as a time_t (number of seconds since 1970-01-01-00:00:00 UTC)

      struct tm * timeinfo;
      timeinfo = localtime(&epoch_time);
      int h = timeinfo->tm_hour;
      int m = timeinfo->tm_min;
      int s = timeinfo->tm_sec;

      //This is an easy way to format time fields as text in arduino.  See https://en.wikipedia.org/wiki/Printf_format_string for details
      char buf[100];  //first, define a c-string buffer to store the output
      snprintf_P(buf, sizeof(buf), PSTR("Wall time: %2u:%02u:%02u"), h, m, s);  //use printf-style formatting to pretty-print the time
      Serial.print(buf);  //finally, output the contents of the buffer

      //print some more information
      Serial.print(F("  Race time: "));
      Serial.print(crossMgrRaceElapsed());  //the library returns times as unsigned long numbers of milliseconds
      Serial.print(F(", Laps: "));
      Serial.print(crossMgrLaps(0));  //first group's lap counter
      Serial.print(F(", "));
      Serial.print(crossMgrLaps(1));  //second groups's lap counter
      Serial.print(F("  start: "));
      Serial.print(crossMgrLapStart(0)/1000);  //we can do integer division to obtain a whole number of seconds
      Serial.print(F(", "));
      Serial.print(crossMgrLapStart(1)/1000.0);  //or floating-point division to obtain fractional seconds
      Serial.print(F("  elapsed: "));
      Serial.print(crossMgrLapElapsed(0)/1000);
      Serial.print(F("."));
      Serial.print(crossMgrLapElapsed(0)%10);  //or tenths of seconds
      Serial.print(F(", "));
      Serial.print(crossMgrLapElapsed(1)/1000);
      Serial.print(F("."));
      int hundredths = crossMgrLapElapsed(1)%100;  //note that leading zeros must be handled with care
      if (hundredths < 10) {  //if you're driving your own digital display, you'll likely have to do something like this
        Serial.print(F("0"));
      }
      Serial.print(hundredths);
      Serial.print(F("\r\n"));
    }
  } else {  //race is not in progress, display a simple wall clock
    if (time(nullptr) != _last_sec) {
      //As CrossMgr only sends the time of day during a race, this will start counting from 1970 on bootup
      //If this is important consider other time sources, such as NTP from the host computer
      _last_sec = time(nullptr);
      struct tm * timeinfo;
      timeinfo = localtime(&_last_sec);
      int y = timeinfo->tm_year + 1900;  //tm stores years counting from 1900, so we have to apply a fudge factor
      int m = timeinfo->tm_mon + 1;
      char buf[100];
      snprintf_P(buf, sizeof(buf), PSTR("%04u-%02u-%02u %02u:%02u:%02u - not currently racing.\r\n"),
        y, m, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
      Serial.print(buf);
    }
  }
  
  //Call this frequently so that incoming data is processed
  //we are using ArduinoWebsockets client (see https://github.com/Links2004/arduinoWebSockets) in blocking mode - 
  //this seems to be more robust at reconnecting after network errors than the non-blocking version.
  //Consequently your program may freeze here while the network connection times out.
  //Note the TCP timeout setting is defined in WebSockets.h line 98:  "#define WEBSOCKETS_TCP_TIMEOUT (5000)"
  crossMgrLoop();
  
  //timeout the network LED
  if (millis() - _network_light_time > 300) {
    digitalWrite(LED_BUILTIN, HIGH);
  }
}
