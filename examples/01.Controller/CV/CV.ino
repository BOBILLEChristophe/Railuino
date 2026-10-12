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
 * Reads the first configuration variables (CVs) of a locomotive decoder.
 * Lit les premières variables de configuration (CV) d'un décodeur.
 *
 * Warning / Attention : first-generation MFX decoders are not designed for
 * this and could be damaged. Les décodeurs MFX de première génération ne
 * sont pas conçus pour cela et pourraient être endommagés.
 */

#include <Railuino.h>

// CAN pins on ESP32. Ignored on Uno/Mega, where the MCP2515 is on the SPI
// bus (see RAILUINO_MCP2515_* to change its CS/INT pins and quartz).
// Broches CAN sur ESP32. Ignorées sur Uno/Mega (MCP2515 sur le bus SPI).
const uint8_t CAN_RX = 22;
const uint8_t CAN_TX = 23;

const uint16_t LOCO = ADDR_MFX + 7; // Change with your own address
const bool DEBUG = false;

TrackController ctrl(0, DEBUG);

void showRegister(uint16_t number, const __FlashStringHelper *label)
{
  uint8_t value = 0;
  Serial.print(F("Register "));
  Serial.print(label);
  Serial.print(F(" : "));
  if (ctrl.readConfig(LOCO, number, &value))
    Serial.println(value);
  else
    Serial.println(F("no answer"));
}

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    ;
  ctrl.begin(CAN_RX, CAN_TX);
  Serial.println(F("Power on"));
  ctrl.setPower(true);

  showRegister(1, F("Address"));
  showRegister(2, F("Min. Voltage"));
  showRegister(3, F("Accel. time"));
  showRegister(4, F("Decel. time"));
  showRegister(5, F("Max. speed"));
  showRegister(6, F("Avg. speed"));
  showRegister(7, F("Version"));
  showRegister(8, F("Manufacturer"));

  Serial.println(F("System stopped. Need to reset."));
}

void loop()
{
}
