/*
 * Web interface for the layout (ESP32 only): power, speed, direction,
 * functions. The pages are in data/: upload them with
 * PlatformIO > Platform > Upload Filesystem Image.
 *
 * MFX locomotives can be given by their UID in index.html
 * (data-uid="FFE9B194"): the address is then looked up in an MfxRegistry
 * that follows the registrations made by the MS2, so the page keeps
 * working when the MS2 gives the locomotive a new SID. data-address is
 * used until the registry knows the UID. GET /locos lists the table.
 * Interface web du réseau (ESP32 uniquement). Les pages sont dans data/ :
 * les téléverser avec PlatformIO > Platform > Upload Filesystem Image.
 */

#ifndef ARDUINO_ARCH_ESP32
#error "WebControl needs an ESP32 (WiFi) / WebControl nécessite un ESP32 (WiFi)"
#endif

#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <Railuino.h>

// CAN pins / Broches CAN
const uint8_t CAN_RX = 22;
const uint8_t CAN_TX = 23;

bool powerState = false; // Variable globale pour suivre l'état de l'alimentation

byte cBuffer[13];
byte sBuffer[13];

const bool DEBUG = true;
const uint32_t TIMEOUT = 500; // ms
const uint16_t HASH = 0xDF24;
const bool LOOPBACK = false;

TrackController ctrl(HASH, DEBUG, TIMEOUT, LOOPBACK);

// MFX table (UID -> address), learned from the MS2 and kept in NVS.
MfxRegistry mfx(ctrl);

// WiFi credentials live in secrets.h (same folder), ignored by Git.
// Copy secrets.h.example to secrets.h and fill it in.
// Identifiants WiFi dans secrets.h (même dossier), ignoré par Git.
#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Copiez secrets.h.example en secrets.h (dossier WebControl) et renseignez votre WiFi / Copy secrets.h.example to secrets.h"
#endif
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;

IPAddress ip(192, 168, 1, 103);
IPAddress gateway(192, 168, 1, 254);
IPAddress subnet(255, 255, 255, 0);
const uint port = 80;

WebServer server(port);

void handleRoot()
{
  File file = SPIFFS.open("/index.html", "r");
  if (!file)
  {
    server.send(500, "text/plain", "File not found");
    return;
  }
  size_t sent = server.streamFile(file, "text/html");
  file.close();
}

void handleCss()
{
  File file = SPIFFS.open("/style.css", "r");
  if (!file)
  {
    server.send(500, "text/plain", "File not found");
    return;
  }
  size_t sent = server.streamFile(file, "text/css");
  file.close();
}

void handleJs()
{
  File file = SPIFFS.open("/script.js", "r");
  if (!file)
  {
    server.send(500, "text/plain", "File not found");
    return;
  }
  size_t sent = server.streamFile(file, "text/javascript");
  file.close();
}

void handleImage1()
{
  File file = SPIFFS.open("/image1.jpg", "r");
  if (!file)
  {
    server.send(500, "text/plain", "File not found");
    return;
  }
  size_t sent = server.streamFile(file, "image/jpg");
  file.close();
}

void handleImage2()
{
  File file = SPIFFS.open("/image2.jpg", "r");
  if (!file)
  {
    server.send(500, "text/plain", "File not found");
    return;
  }
  size_t sent = server.streamFile(file, "image/jpg");
  file.close();
}

void handleImage3()
{
  File file = SPIFFS.open("/image3.jpg", "r");
  if (!file)
  {
    server.send(500, "text/plain", "File not found");
    return;
  }
  size_t sent = server.streamFile(file, "image/jpg");
  file.close();
}

void handleImage4()
{
  File file = SPIFFS.open("/image4.jpg", "r");
  if (!file)
  {
    server.send(500, "text/plain", "File not found");
    return;
  }
  size_t sent = server.streamFile(file, "image/jpg");
  file.close();
}

void handleFavinco()
{
  File file = SPIFFS.open("/favicon.ico", "r");
  if (!file)
  {
    server.send(500, "text/plain", "File not found");
    return;
  }
  size_t sent = server.streamFile(file, "image/x-icon");
  file.close();
}

// Address of the locomotive of a request: from its UID when the page
// gives one and the registry knows it, otherwise from "address". A UID
// not known yet is recorded with the given address; the next Bind of the
// MS2 corrects it if needed. Returns 0 if no address can be found.
uint16_t requestAddress()
{
  const uint16_t address = server.hasArg("address") ? server.arg("address").toInt() : 0;
  if (!server.hasArg("uid"))
    return address;

  const uint32_t uid = strtoul(server.arg("uid").c_str(), nullptr, 16);
  const uint16_t locId = mfx.locIdOf(uid);
  if (locId != 0)
    return locId;
  if (address != 0)
    mfx.assign(uid, address);
  return address;
}

void sendResult(bool ok, const char *text, uint16_t address)
{
  String answer = String(text) + " (" + String(address) + ")";
  server.send(ok ? 200 : 503, "text/plain", ok ? answer : String("No answer from the layout") + " (" + String(address) + ")");
}

// GET /locos: the MFX table, as JSON.
void handleLocos()
{
  String json = "[";
  for (uint8_t i = 0; i < mfx.size(); i++)
  {
    const MfxRegistry::Entry &e = mfx.entry(i);
    char uid[9];
    snprintf(uid, sizeof(uid), "%08lX", static_cast<unsigned long>(e.uid));
    if (i > 0)
      json += ",";
    json += "{\"uid\":\"" + String(uid) + "\",\"address\":" + String(0x4000 + e.sid) +
            ",\"state\":\"" + MfxRegistry::stateName(e.state) + "\"}";
  }
  json += "]";
  server.send(200, "application/json", json);
}

void handleSetPower()
{
  auto togglePower = [&]() -> bool
  {
    powerState = !powerState;
    return ctrl.setPower(powerState);
  };

  if (togglePower())
  {
    if (powerState)
      server.send(200, "text/plain", "true");
    else
      server.send(200, "text/plain", "false");
  }
  else
    server.send(200, "text/plain", "Error power function");
}

void handleSetStop()
{
  const uint16_t address = requestAddress();
  if (address == 0)
  {
    server.send(400, "text/plain", "Address parameter missing");
    return;
  }
  sendResult(ctrl.setLocoSpeed(address, 0), "Stop", address);
}

void handleSetSystemHalt()
{
  sendResult(ctrl.systemHalt(0), "SystemHalt", 0);
}

void handleSetDirection()
{
  const uint16_t address = requestAddress();
  if (address == 0)
  {
    server.send(400, "text/plain", "Address parameter missing");
    return;
  }
  sendResult(ctrl.toggleLocoDirection(address), "Direction toggled", address);
}

void handleSetSpeed()
{
  const uint16_t address = requestAddress();
  if (address == 0 || !server.hasArg("speed"))
  {
    server.send(400, "text/plain", "Speed parameter missing");
    return;
  }
  const uint16_t speed = server.arg("speed").toInt();
  sendResult(ctrl.setLocoSpeed(address, speed), "Speed set", address);
}

void handleSetAddress()
{
  const uint16_t address = requestAddress();
  if (address == 0)
  {
    server.send(400, "text/plain", "Address parameter missing");
    return;
  }
  sendResult(true, "Address set", address);
}

void handleSetFunction()
{
  const uint16_t address = requestAddress();
  if (address == 0 || !server.hasArg("function") || !server.hasArg("power"))
  {
    server.send(400, "text/plain", "Function parameter missing");
    return;
  }
  const uint8_t function = server.arg("function").toInt();
  const uint8_t power = server.arg("power").toInt();
  sendResult(ctrl.setLocoFunction(address, function, power), "Function set", address);
}

void handleNotFound()
{
  server.send(404, "text/plain", "Not found");
}

void setup()
{
  Serial.begin(115200);
  if (!SPIFFS.begin(true))
  {
    Serial.print("An error has occurred while mounting SPIFFS\n");
    return;
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/style.css", handleCss);
  server.on("/script.js", handleJs);
  server.on("/image1.jpg", handleImage1);
  server.on("/image2.jpg", handleImage2);
  server.on("/image3.jpg", handleImage3);
  server.on("/image4.jpg", handleImage4);
  server.on("/favicon.ico", handleFavinco);

  server.on("/setPower", HTTP_POST, handleSetPower);
  server.on("/setStop", HTTP_POST, handleSetStop);
  server.on("/setSystemHalt", HTTP_POST, handleSetSystemHalt);
  server.on("/setDirection", HTTP_POST, handleSetDirection);
  server.on("/setSpeed", HTTP_POST, handleSetSpeed);
  server.on("/setAddress", HTTP_POST, handleSetAddress);
  server.on("/setFunction", HTTP_POST, handleSetFunction);
  server.on("/locos", HTTP_GET, handleLocos);
  server.onNotFound(handleNotFound);

  server.begin();
  ctrl.begin(CAN_RX, CAN_TX);

  mfx.setAutoSave(true); // the table survives a restart
  mfx.begin();           // observer: learns the addresses given by the MS2
  mfx.printTo(Serial);
}

void loop()
{
  server.handleClient();
  ctrl.poll(); // MS2 messages -> MFX table
}