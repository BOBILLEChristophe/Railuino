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
 * Looks for MFX locomotives by address: switches on functions 0 to 5 of
 * addresses 1 to 19, one after the other. Watch which locomotive lights up.
 * Cherche les locomotives MFX par adresse : allume les fonctions 0 à 5 des
 * adresses 1 à 19, l'une après l'autre. Regardez quelle loco s'allume.
 */

#include <Railuino.h>

#if defined(ARDUINO_ARCH_ESP32)
// CAN pins of the ESP32 (TWAI controller).
// Broches CAN de l'ESP32 (contrôleur TWAI).
constexpr uint8_t CAN_RX = 22;
constexpr uint8_t CAN_TX = 23;
#endif
// Uno / Mega: MCP2515 module on the SPI bus, CS 10, INT 2, 16 MHz quartz
// (see RAILUINO_MCP2515_* to change them).
// Uno / Mega : module MCP2515 sur le bus SPI, CS 10, INT 2, quartz 16 MHz
// (voir RAILUINO_MCP2515_* pour les changer).

constexpr uint8_t NB_LOCOS = 20;
constexpr uint32_t PAUSE = 5000; // ms
constexpr bool DEBUG = false;

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
#if defined(ARDUINO_ARCH_ESP32)
  ctrl.begin(CAN_RX, CAN_TX);
#else
  ctrl.begin(); // MCP2515 (SPI)
#endif
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
