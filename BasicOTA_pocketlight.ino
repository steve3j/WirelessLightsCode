

/*
  Rui Santos
  Complete project details
   - Arduino IDE: https://RandomNerdTutorials.com/esp8266-nodemcu-ota-over-the-air-arduino/
   - VS Code: https://RandomNerdTutorials.com/esp8266-nodemcu-ota-over-the-air-vs-code/

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*/

// Import required libraries
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "LittleFS.h"
#include <Arduino_JSON.h>
#include <AsyncElegantOTA.h>
#include <Adafruit_NeoPixel.h>
#include <AceButton.h>
#include <Ticker.h>
using namespace ace_button;

// Replace with your network credentials
const char* ssid = "Frontier9200";
const char* password = "5488291889";

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Create a WebSocket object
AsyncWebSocket ws("/ws");

// Set number of outputs
//#define NUM_OUTPUTS 4
#define PIXEL_PIN   2
#define PIXEL_COUNT 7
const int ANALOG_PIN = A0;
const int BUTTON_PIN =  12; //gnd is tied to pin 8

AceButton button(BUTTON_PIN);
// Forward reference to prevent Arduino compiler becoming confused.
void handleEvent(AceButton*, uint8_t, uint8_t);

Ticker voltageMonitorTicker;
Ticker brightnessAdjustTicker;

// Declare our NeoPixel strip object:
Adafruit_NeoPixel strip = Adafruit_NeoPixel(PIXEL_COUNT, PIXEL_PIN, NEO_GRB + NEO_KHZ800);
// Argument 1 = Number of pixels in NeoPixel strip
// Argument 2 = Arduino pin number (most are valid)
// Argument 3 = Pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)

// Assign each GPIO to an output
//int outputGPIOs[NUM_OUTPUTS] = {4, 5, 13, 14};
int ledState = 0;
int analogValue = 0;

// Initialize LittleFS
void initLittleFS() {
  if (!LittleFS.begin()) {
    Serial.println("An error has occurred while mounting LittleFS");
  }
  Serial.println("LittleFS mounted successfully");
}

// Initialize WiFi
void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
}

String getOutputStates() {
  JSONVar myArray;
  //  for (int i = 0; i < NUM_OUTPUTS; i++) {
  //    myArray["gpios"][i]["output"] = String(outputGPIOs[i]);
  //    myArray["gpios"][i]["state"] = String(digitalRead(outputGPIOs[i]));
  //  }
  myArray["gpios"][0]["output"] = String(PIXEL_PIN);
  //  myArray["gpios"][0]["state"] = String(strip.getPixelColor(0);
  myArray["gpios"][0]["state"] = ledState;
  myArray["gpios"][0]["color"][0] = (uint8_t)(strip.getPixelColor(0) >> 16);
  myArray["gpios"][0]["color"][1] = (uint8_t)(strip.getPixelColor(0) >> 8);
  myArray["gpios"][0]["color"][2] = (uint8_t)(strip.getPixelColor(0));
  myArray["voltage"] = getVoltage();
  String jsonString = JSON.stringify(myArray);
  return jsonString;
}

String getDeviceStates() {
  JSONVar myArray;
  //  for (int i = 0; i < NUM_OUTPUTS; i++) {
  //    myArray["gpios"][i]["output"] = String(outputGPIOs[i]);
  //    myArray["gpios"][i]["state"] = String(digitalRead(outputGPIOs[i]));
  //  }
  //  myArray["gpios"][0]["output"] = String(PIXEL_PIN);
  //  myArray["gpios"][0]["state"] = String(strip.getPixelColor(0);
  //  myArray["gpios"][0]["state"] = ledState;
  //  myArray["gpios"][0]["color"][0] = (uint8_t)(strip.getPixelColor(0)>>16);
  //  myArray["gpios"][0]["color"][1] = (uint8_t)(strip.getPixelColor(0)>>8);
  //  myArray["gpios"][0]["color"][2] = (uint8_t)(strip.getPixelColor(0));
  myArray["voltage"] = getVoltage();
  String jsonString = JSON.stringify(myArray);
  return jsonString;
}

void notifyClients(String state) {
  ws.textAll(state);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;

    if (strcmp((char*)data, "states") == 0) {
      notifyClients(getOutputStates());
    }
    else {
      int gpio = atoi((char*)data);
      //      digitalWrite(gpio, !digitalRead(gpio));
      toggleLED();
      notifyClients(getOutputStates());
    }
  }
}

uint8 brightnessColor[3] = {254, 254, 254};

void toggleLED() {

  if (ledState == 0) {
    led_set(brightnessColor[0], brightnessColor[1], brightnessColor[2]);
    ledState = 1;
  } else {
    led_set(0, 0, 0);
    ledState = 0;
  }
}

bool brightnessDirection = false;
bool longPressed = false;

void longPress() {
  longPressed = true;
}

void adjustBrightness() {
  if (!longPressed){
    return;
  }
  if (!ledState){
    return;
  }
  if (brightnessDirection == true) {
    if (brightnessColor[0] < 254) {
      brightnessColor[0] = brightnessColor[0] + 1;
      brightnessColor[1] = brightnessColor[1] + 1;
      brightnessColor[2] = brightnessColor[2] + 1;
    }
  }
  else if (brightnessColor[0] > 1) {
    brightnessColor[0] = brightnessColor[0] - 1;
    brightnessColor[1] = brightnessColor[1] - 1;
    brightnessColor[2] = brightnessColor[2] - 1;
  }

  led_set(brightnessColor[0], brightnessColor[1], brightnessColor[2]);
}

void adjustBrightnessDirection() {
  brightnessDirection = !brightnessDirection;
  longPressed = false;
}

void led_set(uint8 R, uint8 G, uint8 B) {
  for (int i = 0; i < PIXEL_COUNT; i++) {
    strip.setPixelColor(i, strip.Color(R, G, B));
    strip.show();
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void setup() {
  // Serial port for debugging purposes
  Serial.begin(115200);

  // Set GPIOs as outputs
  //  for (int i = 0; i < NUM_OUTPUTS; i++) {
  //    pinMode(outputGPIOs[i], OUTPUT);
  //  }
  strip.begin(); // Initialize NeoPixel strip object (REQUIRED)
  led_set(0, 0, 0);
  ledState = 0;
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Configure the ButtonConfig with the event handler, and enable all higher
  // level events.
  ButtonConfig* buttonConfig = button.getButtonConfig();
  buttonConfig->setEventHandler(handleEvent);
  buttonConfig->setFeature(ButtonConfig::kFeatureClick);
  buttonConfig->setFeature(ButtonConfig::kFeatureDoubleClick);
  buttonConfig->setFeature(ButtonConfig::kFeatureLongPress);
  buttonConfig->setFeature(ButtonConfig::kFeatureSuppressAfterLongPress);
  buttonConfig->setFeature(ButtonConfig::kFeatureRepeatPress);

  button.setEventHandler(handleEvent);

  voltageMonitorTicker.attach(10, handleVoltages);
  brightnessAdjustTicker.attach_ms(10, adjustBrightness);
  initLittleFS();
  initWiFi();
  initWebSocket();

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/index.html", "text/html", false);
  });

  server.serveStatic("/", LittleFS, "/");

  // Start ElegantOTA
  AsyncElegantOTA.begin(&server);

  // Start server
  server.begin();
}

void loop() {
  ws.cleanupClients();
  button.check();
  //  Serial.println(getVoltage());
}

float getVoltage() {
  int analogReading = analogRead(ANALOG_PIN);
  //  VanalogReading/1024 = (Vbatt * 100) / 450
  //  (Vread*450)/(1024*100) = Vbatt
  //  float Vout = ((float)analogReading * 0.00439453125);
  float Vout = analogReading * 4.45 / 1024.0;
  return Vout;
}

void handleEvent(AceButton*, uint8_t eventType, uint8_t) {
  switch (eventType) {
    case AceButton::kEventReleased:
      toggleLED();
      notifyClients(getOutputStates());
      break;
    case AceButton::kEventLongPressed:
      longPress();
      notifyClients(getOutputStates());
      break;
    case AceButton::kEventLongReleased:
      adjustBrightnessDirection();
      notifyClients(getOutputStates());
      break;
  }
}

void handleVoltages()
{
  notifyClients(getDeviceStates());
}
