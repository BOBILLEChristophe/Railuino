/*********************************************************************
 * Railuino - Hacking your Märklin
 *
 * Copyright (C) 2012 Joerg Pleumann
 * Copyright (C) 2024 christophe bobille
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
 * Controls the layout from the serial monitor (115200 baud, newline).
 * Pilote le réseau depuis le moniteur série (115200 bauds, fin de ligne LF).
 *
 * Type "help" for the list of commands. Addresses are Loc-IDs, in decimal
 * or hexadecimal: 16391 or 0x4007 for MFX loco 7.
 *   power 1
 *   speed 0x4007 300
 *   function 0x4007 0 1
 *   direction 0x4007 3
 *   accessory 0x3001 1 1 200
 * Each command answers "ok" or "error: ...".
 */

#include <Railuino.h>

// CAN pins on ESP32. Ignored on Uno/Mega, where the MCP2515 is on the SPI
// bus (see RAILUINO_MCP2515_* to change its CS/INT pins and quartz).
// Broches CAN sur ESP32. Ignorées sur Uno/Mega (MCP2515 sur le bus SPI).
const uint8_t CAN_RX = 22;
const uint8_t CAN_TX = 23;

const bool DEBUG = false;

TrackController ctrl(0, DEBUG);
CommandParser parser(ctrl);

char line[CommandParser::MAX_LINE + 1];
uint8_t length = 0;

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    ;
  ctrl.begin(CAN_RX, CAN_TX);
  Serial.println(F("Ready. Type a command, e.g. power 1 (help for the list)"));
}

void loop()
{
  // Reads a line without String, to save RAM on a Uno.
  while (Serial.available())
  {
    const char c = Serial.read();
    if (c == '\n' || c == '\r')
    {
      if (length > 0)
      {
        line[length] = 0;
        Serial.println(line);
        parser.execute(line, &Serial);
        length = 0;
      }
    }
    else if (length < CommandParser::MAX_LINE)
      line[length++] = c;
  }
  ctrl.poll(); // keeps the CAN receive queue empty
}
