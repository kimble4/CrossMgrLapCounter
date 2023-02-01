//NeoPixel based 2-digit lap-counter for CrossMgr
//This was written for WS2812B LEDs and a WeMos D1 Mini dev board
//see comments in drawDight() for details of LED strip layout

#define DEBUG
#define DEBUG_SEVEN_SEG  //7-segment display rendering debugging

#define OTA_UPDATES //enable OTA updates

#include <ESP8266WiFiMulti.h>   //wireless networking
#include <ArduinoOTA.h>         //firmware update over WiFi
#include <CrossMgrLapCounter.h> //lapcounter API

//select serial device
#define DEBUG_SERIAL Serial
//#define DEBUG_SERIAL Serial.1

//debugging mode
#if defined (DEBUG) || defined (DEBUG_ESP_PORT)
#define DEBUG_PRINT(...) DEBUG_SERIAL.print( __VA_ARGS__ )
#else
#define DEBUG_PRINT(...)
#endif

//pin assignment
#define NETWORK_LED D0   //low for on
#define NEOPIXEL_DATA_PIN D4
#define WIFI_LED D7  //low for on
#define WEBSOCKET_LED D8 //high for on
#define LIGHT_SENSOR A0  //brighter pulls higher

//network config - see startup section for defining WiFi SSIDs and passwords
#define WIFI_CONNECT_TIMEOUT 5000
#define CROSSMGR_IP 192,168,1,15  //for websocket to connect to (note commas!)
#define WEBSOCKET_RECONNECT_INTERVAL 15000 //milliseconds

//display and UI settings
#define CROSSMGR_LAPCOUNTER_GROUP 0  //which lapcounter to follow
#define NETWORK_LED_TIMEOUT 300  //milliseconds
#define LED_REFRESH_INTERVAL 500 //milliseconds
#define USE_HEARTBEAT  //if set, draws a 'heartbeat' on the last pixel, for checking the LED strip continuity

//brightness settings
#define LIGHT_SENSOR_READ_INTERVAL 5123  // milliseconds
#define LED_MIN_BRIGHTNESS 5  //0-255
#define LED_MAX_BRIGHTNESS 255  //0-255
//#define FULL_BRIGHTNESS  //disables brightness scaling

//digit positions on LED strip
#define DIGIT_0 0
#define DIGIT_1 91
#define NUM_LEDS 182
#define NUM_DIGITS 2
#define HEARTBEAT_LED NUM_LEDS-1
//character definitions
#define CHARACTER_A 10
#define CHARACTER_b 11
#define CHARACTER_c 12
#define CHARACTER_C 13
#define CHARACTER_E 14
#define CHARACTER_h 15
#define CHARACTER_i 16
#define CHARACTER_L 17
#define CHARACTER_n 18
#define CHARACTER_o 19
#define CHARACTER_P 20
#define CHARACTER_r 21
#define CHARACTER_S 22
#define CHARACTER_t 23
#define CHARACTER_DEGREES 24
#define CHARACTER_MINUS 25
#define CHARACTER_POINT 26
#define CHARACTER_COLON 27
#define CHARACTER_EXCLAMATION 28
#define CHARACTER_ERROR 29
#define CHARACTER_FILLED 30
#define LAST_CHARACTER 31

//multiple SSID support
ESP8266WiFiMulti wifiMulti;

//IP of CrossMgr host
IPAddress crossMgrIP;

// Define the array of leds and some colours
CRGB leds[NUM_LEDS];

//global variables
unsigned long _last_light_sensor_poll = 0;
unsigned long _network_LED_time = 0;
unsigned long _last_LED_refresh = 0;
int _brightness = LED_MIN_BRIGHTNESS;
boolean _full_brightness = false;

void setup() {
  pinMode(NETWORK_LED, OUTPUT);
  digitalWrite(NETWORK_LED, HIGH);
  pinMode(WIFI_LED, OUTPUT);
  digitalWrite(WIFI_LED, HIGH);
  pinMode(WEBSOCKET_LED, OUTPUT);
  digitalWrite(WEBSOCKET_LED, LOW);
  pinMode(LIGHT_SENSOR, INPUT);

  //serial init
  DEBUG_SERIAL.begin(115200);
  DEBUG_SERIAL.println("\r\n");
  DEBUG_SERIAL.flush();
  delay(100);
  DEBUG_SERIAL.print(F("\r\n\r\nCrossMgr lap counter"));

  //wifi off
  //ESP.eraseConfig();
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  //wifi debugging output to serial
  DEBUG_SERIAL.setDebugOutput(false);

  //initialise FastLED and clear array
  DEBUG_PRINT(F("\n[LED] Initialising LED array...\r\n"));
  #ifdef FULL_BRIGHTNESS
  full_brightness = true;
  DEBUG_PRINT(F("\n[LED] Brightness sensor disabled, will operate at 100%\r\n"));
  pollLightSensor();
  #else
  DEBUG_PRINT(F("\n[LED] Brightness sensor enabled, will poll at "));
  DEBUG_PRINT(LIGHT_SENSOR_READ_INTERVAL/1000);
  DEBUG_PRINT(F(" second intervals\r\n"));
  pollLightSensor();
  #endif
  FastLED.addLeds<NEOPIXEL, NEOPIXEL_DATA_PIN>(leds, NUM_LEDS);  // GRB ordering is assumed
  fill_solid(&(leds[0]), NUM_LEDS, CRGB::Black);
  FastLED.show();

  //station mode
  //set physical mode to 802.11b for increased range
  WiFi.setPhyMode(WIFI_PHY_MODE_11B);
  //dhcp configuration
  DEBUG_PRINT(F("\n[Net] Configuring DHCP: "));
  IPAddress zero(0,0,0,0);
  if (WiFi.config(zero, zero, zero)) {
    DEBUG_PRINT(F("Success.\r\n"));
  } else {
    DEBUG_PRINT(F("Failed!\r\n"));
  }
  
  //start wifi station - add your network details here
  DEBUG_PRINT(F("[Net] Starting WiFiMulti...\r\n"));
  wifiMulti.addAP("BHPC_Timing", "");
  
  //wait to let WiFi connect
  DEBUG_PRINT(F("\n[Net] Waiting for WiFi..."));
  for (int i = 0; i < 30; i++) {
    updateWiFiLED();
    delay(500);
    DEBUG_PRINT(".");
    FastLED.show();  //to clear glitch on display
    if (wifiMulti.run(WIFI_CONNECT_TIMEOUT) == WL_CONNECTED) {
      DEBUG_PRINT(F("\n[Net] Connected to "));
      DEBUG_PRINT(WiFi.SSID());
      DEBUG_PRINT(F(", my IP is "));
      DEBUG_PRINT(WiFi.localIP());
      DEBUG_PRINT(F("\r\n"));
      updateWiFiLED();
      timeoutNetworkLight();
      break;
    }
  }
  if (wifiMulti.run(WIFI_CONNECT_TIMEOUT) != WL_CONNECTED) {
    DEBUG_PRINT(F("\n[ERR] Continuing startup without network connection!\r\n"));
  }

  //OTA updates
  #ifdef OTA_UPDATES
  //ArduinoOTA.setPassword((const char *)"kingcycle");
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }
    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
  });
  ArduinoOTA.onEnd([]() {
    DEBUG_SERIAL.print(F("\r\n[OTA] End\r\n"));
    digitalWrite(NETWORK_LED, HIGH);  //set LED off
    digitalWrite(WIFI_LED, HIGH);  //set LED off
    digitalWrite(WEBSOCKET_LED, LOW);  //set LED off
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    int percent = progress / (total / 100);
    DEBUG_SERIAL.printf("[OTA] Progress: %u%%\r", percent);
    //blink some LEDs
    if (percent%2 == 0) {
      digitalWrite(NETWORK_LED, HIGH);
      digitalWrite(WEBSOCKET_LED, HIGH);
    } else {
      digitalWrite(NETWORK_LED, LOW);
      digitalWrite(WEBSOCKET_LED, LOW);
    }
  });
  ArduinoOTA.onError([](ota_error_t error) {
    DEBUG_SERIAL.printf("[OTA] Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      DEBUG_SERIAL.print(F("Auth Failed\r\n"));
    } else if (error == OTA_BEGIN_ERROR) {
      DEBUG_SERIAL.print(F("Begin Failed\r\n"));;
    } else if (error == OTA_CONNECT_ERROR) {
      DEBUG_SERIAL.print(F("Connect Failed\r\n"));
    } else if (error == OTA_RECEIVE_ERROR) {
      DEBUG_SERIAL.print(F("Receive Failed\r\n"));
    } else if (error == OTA_END_ERROR) {
      DEBUG_SERIAL.print(F("End Failed\r\n"));
    }
  });
  DEBUG_PRINT(F("\r\n[Sys] Starting OTA update listener...\r\n"));
  ArduinoOTA.begin();
  #endif

  //set up the connection to CrossMgr
  crossMgrIP=IPAddress(CROSSMGR_IP);
  DEBUG_PRINT(F("\n[CMr] Using "));
  DEBUG_PRINT(crossMgrIP);
  DEBUG_PRINT(F(" for server IP address.\r\n"));
  crossMgrSetup(crossMgrIP, WEBSOCKET_RECONNECT_INTERVAL);
  crossMgrSetOnNetwork(onNetwork);
  #ifdef DEBUG
  crossMgrSetDebug(onCmrDebug);
  #endif
  
  DEBUG_PRINT(F("\n[Sys] Startup complete.\r\n\r\n"));
}

void onNetwork(boolean wsc_connected) {  //called on websocket activity
  if (wsc_connected) {
    networkLightOn();
    digitalWrite(WEBSOCKET_LED, HIGH);
  } else {
    digitalWrite(WEBSOCKET_LED, LOW);
  }
}

#ifdef DEBUG
void onCmrDebug(const char * line) {  //print debugging output to serial
  DEBUG_PRINT(line);
}
#endif

void loop() {
  if (millis() - _last_LED_refresh >= LED_REFRESH_INTERVAL) {  //refresh main LED display
    _last_LED_refresh = millis();
    if (crossMgrRaceInProgress()) {
      //calculate what colour to use
      CRGB c;
      if (!(crossMgrFlashLaps(CROSSMGR_LAPCOUNTER_GROUP) && (millis()/LED_REFRESH_INTERVAL)%2 > 0)) {
        //normal brightness / light state of flash
        c = crossMgrGetFGColour(CROSSMGR_LAPCOUNTER_GROUP);
      } else if (crossMgrFlashLaps(CROSSMGR_LAPCOUNTER_GROUP)) {
        //dark state of flash
        c = crossMgrGetFGColour(CROSSMGR_LAPCOUNTER_GROUP);
        // Reduce color to 75% (192/256ths) of its previous value
        // using "video" scaling, meaning: never fading to full black
        c.nscale8_video(64);
      }
      //clear the display
      fill_solid(&(leds[0]), NUM_LEDS, CRGB::Black);
      //draw the least significant digit
      int laps = crossMgrLaps(CROSSMGR_LAPCOUNTER_GROUP);
      int digit = laps%10;
      drawDigit(DIGIT_0, digit, c);
      //now draw the tens
      if (laps >=10) {
        digit = (laps/10)%10;
        drawDigit(DIGIT_1, digit, c);
      }
    } else {  //no race in progress
      //clear the display
      fill_solid(&(leds[0]), NUM_LEDS, CRGB::Black);
      //write "no" in red
      CRGB c = CRGB::Red;
      drawDigit(DIGIT_1, CHARACTER_n, c);
      drawDigit(DIGIT_0, CHARACTER_o, c);
    } 
    #ifdef USE_HEARTBEAT
    if ((millis()/LED_REFRESH_INTERVAL)%2 > 0) {  //draw a heartbeat on the last LED to show the strip is working
      leds[HEARTBEAT_LED] = CRGB::DarkRed;    
    }
    #endif
    //finally, update the LED strip
    FastLED.show();
  }
  //call these regularly...
  crossMgrLoop();
  wifiMulti.run(WIFI_CONNECT_TIMEOUT);
  updateWiFiLED();
  timeoutNetworkLight();
  #ifdef OTA_UPDATES
  ArduinoOTA.handle();
  yield();
  #endif
  pollLightSensor();
}


/* Uses FastLED's setBrightness() function to dim the display in response to ambient light
 * connect a TEPT4400 between the LIGHT_SENSOR pin and 3.3V, and 
 * a 6k resistor between the LIGHT_SENSOR pin and ground.
  */
void pollLightSensor() {  
  if (millis() - _last_light_sensor_poll > LIGHT_SENSOR_READ_INTERVAL) {
    _last_light_sensor_poll = millis();
    int sensor_value = 1023;
    #ifndef FULL_BRIGHTNESS
    sensor_value = analogRead(LIGHT_SENSOR);
    #endif
    int b = map(sensor_value, 0, 1023, LED_MIN_BRIGHTNESS, LED_MAX_BRIGHTNESS);
    if (b != _brightness) {
      _brightness = b;
      DEBUG_PRINT(F("[LED] Light level: "));
      DEBUG_PRINT(map(sensor_value, 0, 1023, 0, 100));
      DEBUG_PRINT(F("%, setting brightness to: "));
      DEBUG_PRINT(_brightness);
      DEBUG_PRINT(F("\r\n"));
    }
    FastLED.setBrightness(_brightness);
  }
}

void timeoutNetworkLight() {
  if (millis() - _network_LED_time >= NETWORK_LED_TIMEOUT) {
    digitalWrite(NETWORK_LED, HIGH);
  }
}

void networkLightOn() {
  digitalWrite(NETWORK_LED, LOW);
  _network_LED_time = millis();
}

void updateWiFiLED() {  //LED indicates WiFi connection status
  int status = WiFi.status();
  if (status == WL_CONNECTED) {
    digitalWrite(WIFI_LED, LOW);
  } else if (status == WL_DISCONNECTED) {
    digitalWrite(WIFI_LED, HIGH);
  } else {
    if ((millis()/1000)%2 > 0) {  //anything other than WL_DISCONNECTED is an error
      digitalWrite(WIFI_LED, LOW);
    } else {
      digitalWrite(WIFI_LED, HIGH);
    }
  }
}

void drawDigit(int start_pos, int digit, CRGB colour) {
  if (start_pos > DIGIT_1) {
    #ifdef DEBUG_SEVEN_SEG
    DEBUG_PRINT(F("[Err] Cannot draw digit; start position "));
    DEBUG_PRINT(start_pos);
    DEBUG_PRINT(F("too high!\r\n"));
    #endif
    return;
  }
  #ifdef DEBUG_SEVEN_SEG
  DEBUG_PRINT(F("[LED] Drawing digit: "));
  DEBUG_PRINT(digit);
  DEBUG_PRINT(F(" @ "));
  DEBUG_PRINT(start_pos);
  DEBUG_PRINT(F("\r\n"));
  #endif
  /* Digit layout is:
   *  
   *       <
   *     dcccb
   *  |  d < b  /\
   *  |  dgggb  |  direction of data
   *  \/ e   a  | 
   *     e > a     
   *     efffa  
   * 
   * c,f,g are 11 pixels long
   * a+b, d+e are 29 pixels long
   * 
   * GPIO output connects to the first pixel of segment 'a' on the least significant (rightmost) digit.
   * Connect the output of the last pixel of segment 'g' to the input of the first pixel of segment 'a' on the next digit to the left
   *
   * Readability is improved if the digit is slanted to the right at an angle of 2.5 degrees (look at some commercial 7-segment displays)
   * 
   */
  #define A_LENGTH 14
  #define AB_LENGTH 29
  #define C_LENGTH 11
  if (start_pos > NUM_LEDS - C_LENGTH - C_LENGTH - AB_LENGTH - C_LENGTH - AB_LENGTH) {
    DEBUG_PRINT(F("[Err] Digit out of bounds! "));
    DEBUG_PRINT(start_pos);
    DEBUG_PRINT(F(" > "));
    DEBUG_PRINT(NUM_LEDS - C_LENGTH - C_LENGTH - AB_LENGTH - C_LENGTH - AB_LENGTH);
    DEBUG_PRINT(F("\r\n"));
    return;
  }
  /* Lookup table for numeric digits and a subset of other characters
   * As this is a 91-pixel display not a true 7-segment display, we improve the legibility
   * by tweaking individual pixels at the corners as 'serifs'
   */
  switch (digit) {                  
    case 0:
      //a,b,c,d,e,f
      for (int i=(start_pos + 1); i < (start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH + C_LENGTH); i++) {
        leds[i] = colour;
      }
      leds[start_pos + A_LENGTH + A_LENGTH] = CRGB::Black;
      leds[start_pos + AB_LENGTH + C_LENGTH] = CRGB::Black;
      leds[start_pos + AB_LENGTH + C_LENGTH + A_LENGTH + A_LENGTH] = CRGB::Black;
      break;
    case 1:
      //a,b
      for (int i=start_pos; i < (start_pos + AB_LENGTH); i++) {
        leds[i] = colour;
      }
      break;
    case 2:
      //first dot
      leds[start_pos] = colour;
      //b,c
      for (int i=(start_pos + A_LENGTH + 1); i < (start_pos + A_LENGTH + 1 + A_LENGTH + C_LENGTH); i++) {
        leds[i] = colour;
      }
      leds[start_pos + A_LENGTH + A_LENGTH] = CRGB::Black;
      leds[start_pos + AB_LENGTH + C_LENGTH + 1] = colour;
      //e,f,g
      for (int i=(start_pos + AB_LENGTH + C_LENGTH + A_LENGTH + 1); i < (start_pos + AB_LENGTH + C_LENGTH + A_LENGTH + 1 + A_LENGTH + C_LENGTH + C_LENGTH); i++) {
        leds[i] = colour;
      }
      //leds[start_pos + AB_LENGTH + C_LENGTH + A_LENGTH + A_LENGTH] = CRGB::Black;
      break;
    case 3:
      //a,b,c
      for (int i=(start_pos + 1); i < (start_pos + AB_LENGTH + C_LENGTH); i++) {
        leds[i] = colour;
      }
      leds[start_pos + A_LENGTH] = CRGB::Black;
      leds[start_pos + A_LENGTH + A_LENGTH] = CRGB::Black;
      leds[start_pos + AB_LENGTH + C_LENGTH + 1] = colour;
      leds[start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH -2] = colour;
      //f,g
      for (int i=(start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH); i < (start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH + C_LENGTH + C_LENGTH); i++) {
        leds[i] = colour;
      }
      break;
    case 4:
      //a,b
      for (int i=start_pos; i < (start_pos + AB_LENGTH); i++) {
        leds[i] = colour;
      }
      //d
      for (int i=(start_pos + AB_LENGTH + C_LENGTH); i < (start_pos + AB_LENGTH + C_LENGTH + A_LENGTH); i++) {
        leds[i] = colour;
      }
      //g
      for (int i=(start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH + C_LENGTH); i < (start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH + C_LENGTH + C_LENGTH); i++) {
        leds[i] = colour;
      }
      break;
    case 5:
      //a
      for (int i=(start_pos + 1); i < (start_pos + A_LENGTH); i++) {
        leds[i] = colour;
      }
      //c,d
      for (int i=(start_pos + A_LENGTH + A_LENGTH); i < (start_pos + A_LENGTH + A_LENGTH + 1 + C_LENGTH + A_LENGTH); i++) {
        leds[i] = colour;
      }
      //f,g
      for (int i=(start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH); i < (start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH + C_LENGTH + C_LENGTH); i++) {
        leds[i] = colour;
      }
      leds[start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH -2] = colour;
      break;
    case 6:
      //a
      for (int i=(start_pos + 1); i < (start_pos + A_LENGTH); i++) {
        leds[i] = colour;
      }
      //c,d,e,f,g
      for (int i=(start_pos + AB_LENGTH); i < (start_pos + A_LENGTH + A_LENGTH + 1 + C_LENGTH + AB_LENGTH + C_LENGTH + C_LENGTH); i++) {
        leds[i] = colour;
      }
      leds[start_pos + AB_LENGTH + C_LENGTH] = CRGB::Black;
      leds[start_pos + AB_LENGTH + C_LENGTH + A_LENGTH + A_LENGTH] = CRGB::Black;
      break;
    case 7:
      //a,b,c
      for (int i=start_pos; i < (start_pos + AB_LENGTH + C_LENGTH); i++) {
        leds[i] = colour;
      }
      break;
    case 8:
      //a,b,c,d,e,f,g
      for (int i=(start_pos + 1); i < (start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH + C_LENGTH + C_LENGTH); i++) {
        leds[i] = colour;
      }
      leds[start_pos + A_LENGTH] = CRGB::Black;
      leds[start_pos + A_LENGTH + A_LENGTH] = CRGB::Black;
      leds[start_pos + AB_LENGTH + C_LENGTH] = CRGB::Black;
      leds[start_pos + AB_LENGTH + C_LENGTH + A_LENGTH] = CRGB::Black;
      leds[start_pos + AB_LENGTH + C_LENGTH + A_LENGTH + A_LENGTH] = CRGB::Black;
      break;
    case 9:
      //a,b,c
      for (int i=start_pos+1; i < (start_pos + AB_LENGTH + C_LENGTH); i++) {
        leds[i] = colour;
      }
      leds[start_pos + A_LENGTH + A_LENGTH] = CRGB::Black;
      //d
      for (int i=(start_pos + AB_LENGTH + C_LENGTH + 1); i < (start_pos + AB_LENGTH + C_LENGTH + A_LENGTH); i++) {
        leds[i] = colour;
      }
      //f,g
      for (int i=(start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH); i < (start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH + C_LENGTH + C_LENGTH); i++) {
        leds[i] = colour;
      }
      leds[start_pos + AB_LENGTH + C_LENGTH + A_LENGTH + A_LENGTH -1] = colour;
      break;
    case CHARACTER_A:
      //a,b,c,d,e
      for (int i=start_pos; i < (start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH); i++) {
        leds[i] = colour;
      }
      leds[start_pos + A_LENGTH + A_LENGTH] = CRGB::Black;
      leds[start_pos + AB_LENGTH + C_LENGTH] = CRGB::Black;
      //g
      for (int i=(start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH + C_LENGTH); i < (start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH + C_LENGTH + C_LENGTH); i++) {
        leds[i] = colour;
      }
      break;
    case CHARACTER_b:
      //a
      for (int i=(start_pos + 1); i < (start_pos + A_LENGTH); i++) {
        leds[i] = colour;
      }
      //d,e,f,g
      for (int i=(start_pos + AB_LENGTH + C_LENGTH); i < (start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH + C_LENGTH + C_LENGTH); i++) {
        leds[i] = colour;
      }
      break;
    case CHARACTER_c:
      //e
      for (int i=(start_pos + AB_LENGTH + C_LENGTH + A_LENGTH + 1); i < (start_pos + AB_LENGTH + C_LENGTH + A_LENGTH + A_LENGTH); i++) {
        leds[i] = colour;
      }
      //f,g
      for (int i=(start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH); i < (start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH + C_LENGTH + C_LENGTH); i++) {
        leds[i] = colour;
      }
      break;
    case CHARACTER_C:
      //c,d,e,f
      for (int i=(start_pos + AB_LENGTH); i < (start_pos + A_LENGTH + A_LENGTH + 1 + C_LENGTH + AB_LENGTH + C_LENGTH); i++) {
        leds[i] = colour;
      }
      leds[start_pos + AB_LENGTH + C_LENGTH] = CRGB::Black;
      leds[start_pos + AB_LENGTH + C_LENGTH + A_LENGTH + A_LENGTH] = CRGB::Black;
      break;
    case CHARACTER_E:
        //c,d,e,f,g
        for (int i=(start_pos + AB_LENGTH); i < (start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH + C_LENGTH + C_LENGTH); i++) {
        leds[i] = colour;
      }
      break;
    case CHARACTER_h:
      //a
      for (int i=start_pos; i < (start_pos + A_LENGTH); i++) {
        leds[i] = colour;
      }
      //d,e
      for (int i=(start_pos + AB_LENGTH + C_LENGTH); i < (start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH); i++) {
        leds[i] = colour;
      }
      //g
      for (int i=(start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH + C_LENGTH); i < (start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH + C_LENGTH + C_LENGTH); i++) {
        leds[i] = colour;
      }
      break;
    case CHARACTER_i:
      //a
      for (int i=start_pos; i < (start_pos + A_LENGTH); i++) {
        leds[i] = colour;
      }
      //dot
      leds[start_pos + A_LENGTH+3] = colour;
      leds[start_pos + A_LENGTH+4] = colour;
      break;
    case CHARACTER_L:
      //d,e,f
      for (int i=(start_pos + AB_LENGTH + C_LENGTH); i < (start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH + C_LENGTH); i++) {
        leds[i] = colour;
      }
      break;
    case CHARACTER_n:
      //a
      for (int i=(start_pos + 1); i < (start_pos + A_LENGTH); i++) {
        leds[i] = colour;
      }
      //e
      for (int i=(start_pos + AB_LENGTH + C_LENGTH + A_LENGTH + 1); i < (start_pos + AB_LENGTH + C_LENGTH + A_LENGTH + A_LENGTH); i++) {
        leds[i] = colour;
      }
      //g
      for (int i=(start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH + C_LENGTH); i < (start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH + C_LENGTH + C_LENGTH); i++) {
        leds[i] = colour;
      }
      break;
    case CHARACTER_o:
      //a
      for (int i=(start_pos + 1); i < (start_pos + A_LENGTH); i++) {
        leds[i] = colour;
      }
      //e
      for (int i=(start_pos + AB_LENGTH + C_LENGTH + A_LENGTH + 1); i < (start_pos + AB_LENGTH + C_LENGTH + A_LENGTH + A_LENGTH); i++) {
        leds[i] = colour;
      }
      //f,g
      for (int i=(start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH); i < (start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH + C_LENGTH + C_LENGTH); i++) {
        leds[i] = colour;
      }
      break;
    case CHARACTER_P:
      //b,c,d,e
      for (int i=(start_pos + A_LENGTH + 1); i < (start_pos + A_LENGTH + 1 + A_LENGTH + C_LENGTH + AB_LENGTH); i++) {
        leds[i] = colour;
      }
      leds[start_pos + A_LENGTH + A_LENGTH] = CRGB::Black;
      //g
      for (int i=(start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH + C_LENGTH); i < (start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH + C_LENGTH + C_LENGTH); i++) {
        leds[i] = colour;
      }
      break;
    case CHARACTER_r:
      //e
      for (int i=(start_pos + AB_LENGTH + C_LENGTH + A_LENGTH + 1); i < (start_pos + AB_LENGTH + C_LENGTH + A_LENGTH + A_LENGTH); i++) {
        leds[i] = colour;
      }
      //g
      for (int i=(start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH + C_LENGTH); i < (start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH + C_LENGTH + C_LENGTH); i++) {
        leds[i] = colour;
      }
      break;
    case CHARACTER_S:
      //a
      for (int i=(start_pos + 1); i < (start_pos + A_LENGTH); i++) {
        leds[i] = colour;
      }
      //c,d
      for (int i=(start_pos + AB_LENGTH); i < (start_pos + AB_LENGTH + C_LENGTH + A_LENGTH); i++) {
        leds[i] = colour;
      }
      //f,g
      for (int i=(start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH); i < (start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH + C_LENGTH + C_LENGTH); i++) {
        leds[i] = colour;
      }
      leds[start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH -2] = colour;
      leds[start_pos + AB_LENGTH -2] = colour;
      leds[start_pos + AB_LENGTH +C_LENGTH] = CRGB::Black;
      break;
    case CHARACTER_t:
      //d,e,f,g
      for (int i=(start_pos + AB_LENGTH + C_LENGTH); i < (start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH + C_LENGTH + C_LENGTH); i++) {
        leds[i] = colour;
      }
      leds[start_pos + AB_LENGTH + C_LENGTH + A_LENGTH + A_LENGTH] = CRGB::Black;
      leds[start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH + C_LENGTH] = CRGB::Black;
      leds[start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH + C_LENGTH + 1] = CRGB::Black;
      leds[start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH + C_LENGTH + 2] = CRGB::Black;
      leds[start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH + C_LENGTH + 3] = CRGB::Black;
      break;
    case CHARACTER_DEGREES:
      //b,c,d
      for (int i=(start_pos + A_LENGTH + 1); i < (start_pos + A_LENGTH + 1 + A_LENGTH + C_LENGTH + A_LENGTH); i++) {
        leds[i] = colour;
      }
      leds[start_pos + A_LENGTH + A_LENGTH] = CRGB::Black;
      leds[start_pos + AB_LENGTH + C_LENGTH] = CRGB::Black;
      //g
      for (int i=(start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH + C_LENGTH); i < (start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH + C_LENGTH + C_LENGTH); i++) {
        leds[i] = colour;
      }
      break;
    case CHARACTER_MINUS:
      //g
      for (int i=(start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH + C_LENGTH); i < (start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH + C_LENGTH + C_LENGTH); i++) {
        leds[i] = colour;
      }
      break;
    case CHARACTER_POINT:
      /* Decimal point layout is:
       *  b
       *  b
       *  b
       *  b
       *  b
       *  b
       *  a
       *  a
       * 
       *  a is 3 pixels long
       *  b is 26 pixels long
       */
      //lower dot
      for (int i = start_pos; i < (start_pos + 3); i++) {
        leds[i] = colour;
      }
      break;
    case CHARACTER_COLON:
      /* Colon layout is:
       *  e
       *  e
       *  d
       *  c
       *  c
       *  b
       *  a
       *  a
       * 
       *  a,e are 7 pixels long
       *  b,d are 3 pixels long
       *  c is 9 pixels long
       */
      //lower dot
      for (int i = (start_pos + 7); i < (start_pos + 7 + 3); i++) {
        leds[i] = colour;
      }
      //upper dot
      for (int i = (start_pos + 7 + 3 + 9); i < (start_pos + 7 + 3 + 9 + 3); i++) {
        leds[i] = colour;
      }
      break;
    case CHARACTER_EXCLAMATION:
      /* Exclamation layout is:
       *  c
       *  c
       *  c
       *  c
       *  b
       *  b
       *  a
       *  a
       * 
       *  a is 3 pixels long
       *  b is 5 pixels long
       *  c is 21 pixels long
       */
      //lower dot
      for (int i = start_pos; i < (start_pos + 3); i++) {
        leds[i] = colour;
      }
      //upper part
      for (int i = (start_pos + 3 + 5); i < (start_pos + 29); i++) {
        leds[i] = colour;
      }
      break;
    case CHARACTER_ERROR:
      //lower dot
      for (int i = start_pos; i < (start_pos + 3); i++) {
        leds[i] = colour;
      }
      //upper part
      for (int i = (start_pos + 3 + 5); i < (start_pos + 29); i++) {
        leds[i] = colour;
      }
      //upper part
      for (int i = (start_pos + AB_LENGTH + C_LENGTH); i < (start_pos + AB_LENGTH + C_LENGTH + 21); i++) {
        leds[i] = colour;
      }
      //final dot
      for (int i = (start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH - 3); i < (start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH); i++) {
        leds[i] = colour;
      }
      break;
    case CHARACTER_FILLED:  //draw this in CRGB::Black to blank out a digit
      //a,b,c,d,e,f,g
      for (int i=(start_pos); i < (start_pos + AB_LENGTH + C_LENGTH + AB_LENGTH + C_LENGTH + C_LENGTH); i++) {
        leds[i] = colour;
      }
      break;
  }
}
