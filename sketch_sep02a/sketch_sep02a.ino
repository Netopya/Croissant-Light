#include <Arduino.h>
#include <ArduinoJson.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WebSocketsClient.h>

#include "FastLED.h"

FASTLED_USING_NAMESPACE

#define SLACK_BOT_TOKEN "#"
#define WIFI_SSID "#"
#define WIFI_PASSWORD "#"

// if Slack changes their SSL fingerprint, you would need to update this:
#define SLACK_SSL_FINGERPRINT "AC 95 5A 58 B8 4E 0B CD B3 97 D2 88 68 F5 CA C1 0A 81 E3 6E" 

#define WORD_SEPERATORS "., \"'()[]<>;:-+&?!\n\t"

#define DATA_PIN    5
#define LED_TYPE    WS2811
#define COLOR_ORDER GRB
#define NUM_LEDS    64
CRGB leds[NUM_LEDS];

#define BRIGHTNESS          96
#define FRAMES_PER_SECOND  120

ESP8266WiFiMulti WiFiMulti;
WebSocketsClient webSocket;

const int ledPin = 4;
long nextCmdId = 1;
bool connected = false;
unsigned long lastPing = 0;

uint8_t gHue = 0; // rotating "base color" used by many of the patterns
int leadPos = NUM_LEDS;
CRGB rgb = CRGB(255, 255, 255);

void processSlackMessage(char *payload) {
  char *nextWord = NULL;
  Serial.println("processing message");

  for (nextWord = strtok(payload, WORD_SEPERATORS); nextWord; nextWord = strtok(NULL, WORD_SEPERATORS)) {
    if (strcasecmp(nextWord, "on") == 0) {
      digitalWrite(ledPin, HIGH); 
    }
    if (strcasecmp(nextWord, "off") == 0) {
      digitalWrite(ledPin, LOW); 
    }
    if (strcasecmp(nextWord, "error") == 0) {
      connectToSlack();
    }
    if (strcasecmp(nextWord, "go") == 0) {
      leadPos = 0;
    }
    if (strcasecmp(nextWord, "red") == 0) {
      leadPos = 0;
      rgb = CRGB(255, 0, 0);
    }
    if (strcasecmp(nextWord, "orange") == 0) {
      leadPos = 0;
      rgb = CRGB(255, 128, 0);
    }
    if (strcasecmp(nextWord, "yellow") == 0) {
      leadPos = 0;
      rgb = CRGB(255, 255, 0);
    }
    if (strcasecmp(nextWord, "green") == 0) {
      leadPos = 0;
      rgb = CRGB(0, 255, 0);
    }
    if (strcasecmp(nextWord, "blue") == 0) {
      leadPos = 0;
      rgb = CRGB(0, 0, 255);
    }
    if (strcasecmp(nextWord, "purple") == 0) {
      leadPos = 0;
      rgb = CRGB(128, 0, 128);
    }
  }
}

void sendPing() {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["type"] = "ping";
  root["id"] = nextCmdId++;
  String json;
  root.printTo(json);
  webSocket.sendTXT(json);
}

void webSocketEvent(WStype_t type, uint8_t *payload, size_t len) {
  switch (type) {
    case WStype_CONNECTED:
      Serial.printf("[WebSocket] Connected to: %s\n", payload);
      break;

    case WStype_TEXT:
      Serial.printf("[WebSocket] Message: %s\n", payload);
      processSlackMessage((char*)payload);
      // TODO: process message
      break;
  }
}

bool connectToSlack() {
  updateStatus(3);
  
  // Step 1: Find WebSocket address via RTM API (https://api.slack.com/methods/rtm.start)
  HTTPClient http;
  http.begin("https://slack.com/api/rtm.connect?token=" SLACK_BOT_TOKEN, SLACK_SSL_FINGERPRINT);
  int httpCode = http.GET();

  updateStatus(4);
  
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("HTTP GET failed with code %d\n", httpCode);
    return false;
  }

  Serial.printf("HTTP GET Code %d\n", httpCode);

  WiFiClient *client = http.getStreamPtr();
  client->find("wss:\\/\\/");
  String host = client->readStringUntil('\\');
  String path = client->readStringUntil('"');
  path.replace("\\/", "/");

  updateStatus(5);
  
  // Step 2: Open WebSocket connection and register event handler
  Serial.println("WebSocket Host=" + host + " Path=" + path);
  webSocket.beginSSL(host, 443, path, "", "");
  webSocket.onEvent(webSocketEvent);

  updateStatus(6);
  
  return true;
}

void rainbow() 
{
  // FastLED's built-in rainbow generator
  fill_rainbow( leds, NUM_LEDS, gHue, 7);
}

void updateStatus(int led){
  leds[led] = CRGB::HotPink;

  FastLED.show();
  FastLED.delay(1000/FRAMES_PER_SECOND);
}

void setup() {
  delay(3000); // 3 second delay for recovery

  // tell FastLED about the LED strip configuration
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);

  // set master brightness control
  FastLED.setBrightness(BRIGHTNESS);

  updateStatus(0);

  Serial.begin(115200);
  Serial.setDebugOutput(true);

  Serial.println("Starting Up");
  pinMode(ledPin, OUTPUT);

  WiFiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
  while (WiFiMulti.run() != WL_CONNECTED) {
    delay(100);
  }

  updateStatus(1);
  
  /*WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }*/
  Serial.println("");
  
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  
  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  updateStatus(2);
  //connectToSlack();
}

void loop() {
  webSocket.loop();

  if (connected) {
    // Send ping every 5 seconds, to keep the connection alive
    if (millis() - lastPing > 5000) {
      sendPing();
      lastPing = millis();
    }
  } else {
    // Try to connect / reconnect to slack
    connected = connectToSlack();
    if (!connected) {
      delay(500);
    }
  }

  // do some periodic updates
  EVERY_N_MILLISECONDS( 20 ) { gHue++; } // slowly cycle the "base color" through the rainbow
  int loopTime = 1000/FRAMES_PER_SECOND*4;
  EVERY_N_MILLISECONDS(loopTime) {
    ledLoop();
  }
  
}

void ledLoop() {
    fadeToBlackBy( leds, NUM_LEDS, 20);
    
    if(leadPos < NUM_LEDS) {
      leds[leadPos] += rgb;
      leadPos++;
    }
    //rainbow();
    
    FastLED.show();
}

