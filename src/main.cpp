#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include "Config.h"
#include "TrackController.h"
#include <ACAN_ESP32.h>

bool powerState = false; // Variable globale pour suivre l'état de l'alimentation
const bool DEBUG = true;

byte cBuffer[13];
byte sBuffer[13];

TrackController ctrl(0xDF24, DEBUG);

const char *ssid = "**********";
const char *password = "**********";

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



bool togglePower()
{
    powerState = !powerState;
    return ctrl.setPower(powerState);
}

void handleSetPower()
{
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
    if (server.hasArg("address"))
    {
        uint16_t address = server.arg("address").toInt();
        const bool ok = ctrl.setLocoSpeed(address, 0);
        if (ok)
            server.send(200, "text/plain", "Stop");
        else
            server.send(400, "text/plain", "Server error");
    }
}

void handleSetSystemHalt()
{

    if (server.hasArg("address"))
    {
        uint16_t address = server.arg("address").toInt();
        ctrl.systemHalt(address);
        server.send(200, "text/plain", "SystemHalt");
    }
    else
        server.send(400, "text/plain", "Address parameter missing");
}

void handleSetDirection()
{
    if (server.hasArg("address"))
    {
        uint16_t address = server.arg("address").toInt();
        ctrl.toggleLocoDirection(address);
        server.send(200, "text/plain", "Direction toggled");
    }
    else
        server.send(400, "text/plain", "Address parameter missing");
}


void handleSetSpeed()
{
    uint16_t address = 0;
    uint16_t speed = 0;
    if (server.hasArg("address") && server.hasArg("speed"))
    {
        address = server.arg("address").toInt();
        speed = server.arg("speed").toInt();
        ctrl.setLocoSpeed(address, speed);
        server.send(200, "text/plain", "Speed set");
    }
    else
        server.send(400, "text/plain", "Speed parameter missing");
}

void handleSetAddress()
{
    if (server.hasArg("address"))
    {
        uint16_t address = server.arg("address").toInt();
        server.send(200, "text/plain", "Address set");
    }
    else
        server.send(400, "text/plain", "Address parameter missing");
}

void handleSetFunction()
{
    if (server.hasArg("address") && server.hasArg("function") && server.hasArg("power"))
    {
        uint16_t address = server.arg("address").toInt();
        uint8_t function = server.arg("function").toInt();
        uint8_t power = server.arg("power").toInt();
        ctrl.setLocoFunction(address, function, power);
        server.send(200, "text/plain", "Function set");
    }
    else
        server.send(400, "text/plain", "Function parameter missing");
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
    server.onNotFound(handleNotFound);

    server.begin();
    ctrl.begin();
}

void loop()
{
    server.handleClient();
}