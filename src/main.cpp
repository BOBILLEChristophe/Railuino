#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include "Config.h"
#include "TrackController.h"
#include <ACAN_ESP32.h>

const uint16_t LOCO = ADDR_MFX + 7;
const bool DEBUG = true;

byte cBuffer[13];
byte sBuffer[13];

TrackController ctrl(0xDF24, DEBUG);

const char* ssid = "**********";
const char* password = "**********";


IPAddress ip(192, 168, 1, 103);
IPAddress gateway(192, 168, 1, 254);
IPAddress subnet(255, 255, 255, 0);
const uint port = 80;

WebServer server(port);

void handleRoot() {
  File file = SPIFFS.open("/index.html", "r");
  if (!file) {
    server.send(500, "text/plain", "File not found");
    return;
  }
  size_t sent = server.streamFile(file, "text/html");
  file.close();
}

void handleCss() {
  File file = SPIFFS.open("/style.css", "r");
  if (!file) {
    server.send(500, "text/plain", "File not found");
    return;
  }
  size_t sent = server.streamFile(file, "text/css");
  file.close();
}

void handleJs() {
  File file = SPIFFS.open("/script.js", "r");
  if (!file) {
    server.send(500, "text/plain", "File not found");
    return;
  }
  size_t sent = server.streamFile(file, "text/javascript");
  file.close();
}

void handleImage1() {
  File file = SPIFFS.open("/image1.jpg", "r");
  if (!file) {
    server.send(500, "text/plain", "File not found");
    return;
  }
  size_t sent = server.streamFile(file, "image/jpg");
  file.close();
}

void handleImage2() {
  File file = SPIFFS.open("/image2.jpg", "r");
  if (!file) {
    server.send(500, "text/plain", "File not found");
    return;
  }
  size_t sent = server.streamFile(file, "image/jpg");
  file.close();
}

void handleImage3() {
  File file = SPIFFS.open("/image3.jpg", "r");
  if (!file) {
    server.send(500, "text/plain", "File not found");
    return;
  }
  size_t sent = server.streamFile(file, "image/jpg");
  file.close();
}

void handleImage4() {
  File file = SPIFFS.open("/image4.jpg", "r");
  if (!file) {
    server.send(500, "text/plain", "File not found");
    return;
  }
  size_t sent = server.streamFile(file, "image/jpg");
  file.close();
}

void handleSetPower() {
  ctrl.setPower(true);
  server.send(200, "text/plain", "Power set");
}

void handleSetStop() {
  ctrl.setLocoSpeed(LOCO, 0);
  server.send(200, "text/plain", "Stop");
}

void handleSetEmergency() {
  // Implementation de l'emergency ici
  server.send(200, "text/plain", "Emergency");
}

void handleSetDirection() {
  ctrl.toggleLocoDirection(LOCO);
  server.send(200, "text/plain", "Direction toggled");
}

void handleSetSpeed() {
  if (server.hasArg("speed")) {
    int speed = server.arg("speed").toInt();
    ctrl.setLocoSpeed(LOCO, speed);
    server.send(200, "text/plain", "Speed set");
  } else {
    server.send(400, "text/plain", "Speed parameter missing");
  }
}

void handleSetAddress() {
  if (server.hasArg("address")) {
    int address = server.arg("address").toInt();
    // Gérer l'adresse comme nécessaire
    server.send(200, "text/plain", "Address set");
  } else {
    server.send(400, "text/plain", "Address parameter missing");
  }
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

void setup() {
  Serial.begin(115200);
  if (!SPIFFS.begin(true)) {
    Serial.println("An error has occurred while mounting SPIFFS");
    return;
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
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




  server.on("/setPower", HTTP_POST, handleSetPower);
  server.on("/setStop", HTTP_POST, handleSetStop);
  server.on("/setEmergency", HTTP_POST, handleSetEmergency);
  server.on("/setDirection", HTTP_POST, handleSetDirection);
  server.on("/setSpeed", HTTP_POST, handleSetSpeed);
  server.on("/setAddress", HTTP_POST, handleSetAddress);
  server.onNotFound(handleNotFound);

  server.begin();
  ctrl.begin();
}

void loop() {
  server.handleClient();
}