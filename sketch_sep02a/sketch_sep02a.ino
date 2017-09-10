#include <Arduino.h>
#include <ArduinoJson.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WebSocketsClient.h>

#define SLACK_BOT_TOKEN "#"
#define WIFI_SSID "#"
#define WIFI_PASSWORD "#"

// if Slack changes their SSL fingerprint, you would need to update this:
#define SLACK_SSL_FINGERPRINT "AC 95 5A 58 B8 4E 0B CD B3 97 D2 88 68 F5 CA C1 0A 81 E3 6E" 

#define WORD_SEPERATORS "., \"'()[]<>;:-+&?!\n\t"

ESP8266WiFiMulti WiFiMulti;
WebSocketsClient webSocket;

const int ledPin = 4;
long nextCmdId = 1;
bool connected = false;
unsigned long lastPing = 0;

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
  // Step 1: Find WebSocket address via RTM API (https://api.slack.com/methods/rtm.start)
  HTTPClient http;
  http.begin("https://slack.com/api/rtm.connect?token=" SLACK_BOT_TOKEN, SLACK_SSL_FINGERPRINT);
  int httpCode = http.GET();

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

  // Step 2: Open WebSocket connection and register event handler
  Serial.println("WebSocket Host=" + host + " Path=" + path);
  webSocket.beginSSL(host, 443, path, "", "");
  webSocket.onEvent(webSocketEvent);
  return true;
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);

  Serial.println("Starting Up");
  pinMode(ledPin, OUTPUT);

  //WiFiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
  /*while (WiFiMulti.run() != WL_CONNECTED) {
    delay(100);
  }*/

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  
  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  
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
}
