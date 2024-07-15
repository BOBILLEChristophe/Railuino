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


#include "Config.h"
#include "TrackController.h"
//----------------------------------------------------------------------------------------
//   Include files
//----------------------------------------------------------------------------------------

#include <ACAN_ESP32.h> // https://github.com/pierremolinaro/acan-esp32.git
#include <Arduino.h>
#include <WiFi.h>

const uint16_t LOCO = ADDR_MFX + 7; // Change with your own address
const bool DEBUG = true;

byte cBuffer[13]; // CAN buffer
byte sBuffer[13]; // Serial buffer

TrackController ctrl(0xDF24, DEBUG);

//----------------------------------------------------------------------------------------
//  TCP/WIFI-ETHERNET
//----------------------------------------------------------------------------------------

const char *ssid = "**********";
const char *password = "**********";
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

  ctrl.begin();
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
