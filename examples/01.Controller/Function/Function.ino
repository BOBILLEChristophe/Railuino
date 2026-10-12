/*********************************************************************
 * Railuino - Hacking your Märklin
 *
 * Copyright (C) 2012 Joerg Pleumann
 * Copyright (C) 2024 – 2026 christophe bobille
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
 * Switches functions 0 to 12 of a locomotive on and off, one by one.
 * Active puis désactive une à une les fonctions 0 à 12 d'une locomotive.
 */

#include <Railuino.h>

// CAN pins on ESP32. Ignored on Uno/Mega, where the MCP2515 is on the SPI
// bus (see RAILUINO_MCP2515_* to change its CS/INT pins and quartz).
// Broches CAN sur ESP32. Ignorées sur Uno/Mega (MCP2515 sur le bus SPI).
const uint8_t CAN_RX = 22;
const uint8_t CAN_TX = 23;

const uint16_t LOCO = ADDR_MFX + 7; // Change with your own address
const uint32_t PAUSE = 5000;        // ms
const bool DEBUG = false;

TrackController ctrl(0, DEBUG);

void setFunction(uint8_t function, uint8_t power)
{
  ctrl.setLocoFunction(LOCO, function, power);
  uint8_t b;
  if (ctrl.getLocoFunction(LOCO, function, &b))
  {
    Serial.print(F("Function "));
    Serial.print(function);
    Serial.println(b ? F(" is on") : F(" is off"));
  }
}

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    ;
  ctrl.begin(CAN_RX, CAN_TX);
  Serial.println(F("Power on"));
  ctrl.setPower(true);

  for (uint8_t i = 0; i <= 12; i++)
  {
    setFunction(i, 1);
    delay(PAUSE);
    setFunction(i, 0);
    delay(PAUSE);
  }

  Serial.println(F("Power off"));
  ctrl.setPower(false);
  Serial.println(F("System stopped. Need to reset."));
}

void loop()
{
}
