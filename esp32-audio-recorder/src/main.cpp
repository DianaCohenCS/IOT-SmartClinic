/*
Audio Recorder on-board (esp32) web server.
Display the playlist (FS content).
Upload audio files (MP3, WAV in different qualities) to local storage (flash).
Play any file, through the GUI.
Record your audio from Microphone through GUI – directly to local storage (flash).
Ability to play the last recording, through the GUI.
Ability to delete any audio file?
Our infrastructure encapsulates the common functionality for FileSystem and Microphone sampling

Test setup:
  - ESP32 (CH9102)
  - Micriphone (INMP441)
  - Speaker attached to DAC (MAX98357A)
  - Arduino IDE 2.3.3 / PlatformIO on VSCodium
  - Board manager: esp32 v3.0.6 by Espressif Systems

Test scenario:
  Play the WAV file, previously recorded to local FS (flash)

  TODO: replace...
  1. Upload the sketch and open Serial Monitor.
  2. Press the RESET button on your esp32.
  3. Play the WAV file from the local FS.
*/

// Import the required libraries
#include <Arduino.h> // for PlatformIO

#include <ESPAsyncWebServer.h> // includes AsyncTCP and WiFi when ESP32 is defined
#include <ESPmDNS.h>           // to find our device via host-name
#include <AsyncJson.h>         // let's try sending messages in json format

#include <SPIFFS.h> // TODO: later use our wrapper fsFLASH.h

// our definitions and wrappers from Diana-audio-utils
#include "secrets.h"

// Set LED GPIO
#define BUILTIN_LED 2
// Stores LED state
String ledState;

// Replace with your network credentials, defined in secrets.h
// const char* ssid = "REPLACE_WITH_YOUR_SSID";
// const char* password = "REPLACE_WITH_YOUR_PASSWORD";
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// put function declarations here:
String processor(const String &);

void setup()
{
  // put your setup code here, to run once:

  // Serial port for debugging purposes
  Serial.begin(115200);
  Serial.println("Booted");

  // HW SETUP
  pinMode(BUILTIN_LED, OUTPUT);

  // FS INIT
  if (!SPIFFS.begin(true)) // Initialize SPIFFS
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
  Serial.println("SPIFFS mounted");

  // WIFI INIT
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  // while (WiFi.waitForConnectResult() != WL_CONNECTED)
  // {
  //   Serial.println("Connection Failed! Rebooting....");
  //   delay(5000);
  //   ESP.restart();
  // }
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  // connected
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // DNS INIT
  MDNS.begin("demo-server");

  // WEB SERVER SETUP

  // CORS handlers
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, PUT");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "*");

  // define "led" endpoint, to get the json input
  server.addHandler(new AsyncCallbackJsonWebHandler("/led", [](AsyncWebServerRequest *request, JsonVariant &json)
                                                    {
    const JsonObject &jsonObj = json.as<JsonObject>();
    if (jsonObj["on"])
    {
      Serial.println("Turn on LED");
      digitalWrite(BUILTIN_LED, HIGH);
    }
    else
    {
      Serial.println("Turn off LED");
      digitalWrite(BUILTIN_LED, LOW);
    }
    request->send(200, "OK"); })); // don't forget to send the response

  // // serve content from SPIFFS
  // server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

  // Route for root / web page, using processor() to read the led state
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/index.html", String(), false, processor); });

  // Route to load style.css file
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/style.css", "text/css"); });

  // Route to load script.js file
  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/script.js", "application/javascript"); });

  // Route to set GPIO to HIGH
  server.on("/on", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    digitalWrite(BUILTIN_LED, HIGH);
    request->send(SPIFFS, "/index.html", String(), false, processor); });

  // Route to set GPIO to LOW
  server.on("/off", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    digitalWrite(BUILTIN_LED, LOW);
    request->send(SPIFFS, "/index.html", String(), false, processor); });

  // handle not found, except for OPTIONS request
  server.onNotFound([](AsyncWebServerRequest *request)
                    {
    if (request->method() == HTTP_OPTIONS) // preflight request
    {
      request->send(200);
    }
    else
    {
      Serial.println("Not found");
      request->send(404, "Not found");
    } });

  // START WEB SERVER
  server.begin();
  Serial.println("Server started");
}

void loop()
{
  // put your main code here, to run repeatedly:
}

// put function definitions here:

// Replaces placeholder with LED state value
String processor(const String &var)
{
  Serial.printf("%s: ", var);
  if (var == "STATE")
  {
    if (digitalRead(BUILTIN_LED))
    {
      ledState = "ON";
    }
    else
    {
      ledState = "OFF";
    }
    Serial.println(ledState);
    return ledState;
  }
  return String();
}