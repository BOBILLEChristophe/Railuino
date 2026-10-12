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
 * Looks for MFX locomotives by address: switches on functions 0 to 5 of
 * addresses 1 to 19, one after the other. Watch which locomotive lights up.
 * Cherche les locomotives MFX par adresse : allume les fonctions 0 à 5 des
 * adresses 1 à 19, l'une après l'autre. Regardez quelle loco s'allume.
 */

#include <Railuino.h>

// CAN pins on ESP32. Ignored on Uno/Mega, where the MCP2515 is on the SPI
// bus (see RAILUINO_MCP2515_* to change its CS/INT pins and quartz).
// Broches CAN sur ESP32. Ignorées sur Uno/Mega (MCP2515 sur le bus SPI).
const uint8_t CAN_RX = 22;
const uint8_t CAN_TX = 23;

const uint8_t NB_LOCOS = 20;
const uint32_t PAUSE = 5000; // ms
const bool DEBUG = false;

TrackController ctrl(0, DEBUG);

void setFunctions(uint16_t loco, uint8_t power)
{
  for (uint8_t f = 0; f < 6; f++)
    ctrl.setLocoFunction(loco, f, power);
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
  delay(PAUSE);
  for (uint8_t i = 1; i < NB_LOCOS; i++)
  {
    const uint16_t loco = ADDR_MFX + i;
    setFunctions(loco, 1);
    Serial.print(F("Loco address: "));
    Serial.println(i);
    delay(PAUSE);
    setFunctions(loco, 0);
  }
}
