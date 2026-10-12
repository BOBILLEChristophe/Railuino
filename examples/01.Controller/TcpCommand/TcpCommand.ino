/*********************************************************************
 * Railuino - Hacking your Märklin
 *
 * Copyright (C) 2012 Joerg Pleumann
 * Copyright (C) 2024 Christophe Bobille
 * 
 * This example is free software; you can redistribute it and/or
 * modify it under the terms of the Creative Commons Zero License,
 * version 1.0, as published by the Creative Commons Organisation.
 * This effectively puts the file into the public domain.
 *
 * This example is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * LICENSE file for more details.
 */


/*
 * Same commands as SerialCommand, received over TCP (port 15731, ESP32 only).
 * Mêmes commandes que SerialCommand, reçues en TCP (port 15731, ESP32).
 */

#ifndef ARDUINO_ARCH_ESP32
#error "TcpCommand needs an ESP32 (WiFi) / TcpCommand nécessite un ESP32 (WiFi)"
#endif

#include <WiFi.h>
#include <Railuino.h>

// CAN pins / Broches CAN
const uint8_t CAN_RX = 22;
const uint8_t CAN_TX = 23;

const bool DEBUG = true;

TrackController ctrl(0xDF24, DEBUG);

// WiFi credentials: copy secrets.h.example to secrets.h (ignored by Git).
// Identifiants WiFi : copier secrets.h.example en secrets.h (ignoré par Git).
#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Copiez secrets.h.example en secrets.h et renseignez votre WiFi / Copy secrets.h.example to secrets.h"
#endif
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;

IPAddress ip(192, 168, 1, 103);
IPAddress gateway(192, 168, 1, 254);
IPAddress subnet(255, 255, 255, 0);
const uint port = 15731;
WiFiServer server(port);
WiFiClient client;

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    ;

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

  server.begin();

  ctrl.begin(CAN_RX, CAN_TX);
}

void loop()
{
  while (!client) // listen for incoming clients
    client = server.available();
  if (client.connected())
  {
    if (client.available())
    {
      String command = client.readStringUntil('\n');

      Serial.println(command);
      ctrl.handleUserCommands(command);
    }
  }
}

// if (!client.connected())
// {
//   Serial.println("Client disconnected.");
//   client.stop();
// }
