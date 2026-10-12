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
 * Commands (addresses on 5 digits) / Commandes (adresses sur 5 chiffres) :
 *   power 1                      power on (0 = off)
 *   speed 16391 100              speed 100 for MFX loco 7 (0x4000 + 7)
 *   direction 16391 3            3 = toggle, 1 = forward, 2 = reverse
 *   function 16391 0 1           function 0 (lights) on
 *   accessory 12289 1 1 200      accessory 0x3001: position 1, power 1, 200 ms
 *   emergency 16391              emergency stop of one locomotive
 */

#include <Railuino.h>

// CAN pins on ESP32. Ignored on Uno/Mega, where the MCP2515 is on the SPI
// bus (see RAILUINO_MCP2515_* to change its CS/INT pins and quartz).
// Broches CAN sur ESP32. Ignorées sur Uno/Mega (MCP2515 sur le bus SPI).
const uint8_t CAN_RX = 22;
const uint8_t CAN_TX = 23;

const bool DEBUG = false;

TrackController ctrl(0, DEBUG);

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    ;
  ctrl.begin(CAN_RX, CAN_TX);
  Serial.println(F("Ready. Type a command, e.g. power 1"));
}

void loop()
{
  if (Serial.available())
  {
    String command = Serial.readStringUntil('\n');
    command.trim();
    Serial.println(command);
    ctrl.handleUserCommands(command);
  }
}
