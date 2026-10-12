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
 * Switches the headlights of a locomotive on and off every 5 seconds.
 * Allume et éteint les feux d'une locomotive toutes les 5 secondes.
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

void setLights(uint8_t power)
{
  ctrl.setLocoFunction(LOCO, 0, power);
  uint8_t b;
  if (ctrl.getLocoFunction(LOCO, 0, &b))
  {
    Serial.print(F("Lights are "));
    Serial.println(b ? F("on") : F("off"));
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
}

void loop()
{
  setLights(1);
  delay(PAUSE);
  setLights(0);
  delay(PAUSE);
}
