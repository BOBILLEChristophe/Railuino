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
 * Switches a turnout every 5 seconds.
 * Change la position d'un aiguillage toutes les 5 secondes.
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

constexpr uint16_t TURNOUT = ADDR_ACC_MM2 + 1; // Change with your own address
constexpr uint32_t PAUSE = 5000;               // ms
constexpr bool DEBUG = false;

TrackController ctrl(0, DEBUG);

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
  // Blocking mode (default): setTurnout() waits for the end of the pulse.
  // With ctrl.setAccessoryAsync(true), it returns at once and ctrl.poll()
  // must be called often from loop() to switch the coil off.
}

void loop()
{
  Serial.println(F("Set turnout straight"));
  if (!ctrl.setTurnout(TURNOUT, true))
    Serial.println(F("  no answer"));
  delay(PAUSE);
  Serial.println(F("Set turnout round"));
  if (!ctrl.setTurnout(TURNOUT, false))
    Serial.println(F("  no answer"));
  delay(PAUSE);
}
