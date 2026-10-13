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
 * Changes the direction of a locomotive every 10 seconds.
 * Change le sens de marche d'une locomotive toutes les 10 secondes.
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

constexpr uint16_t LOCO = ADDR_MFX + 7; // Change with your own address
constexpr uint32_t PAUSE = 10000;       // ms
constexpr bool DEBUG = false;

TrackController ctrl(0, DEBUG);

void printDirection()
{
  uint8_t direction;
  if (ctrl.getLocoDirection(LOCO, &direction))
  {
    Serial.print(F("Direction is "));
    Serial.println(direction == DIR_FORWARD ? F("forward") : F("reverse"));
  }
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
  Serial.println(F("Headlights on"));
  ctrl.setLocoFunction(LOCO, 0, 1);
}

void loop()
{
  ctrl.setLocoDirection(LOCO, DIR_FORWARD);
  printDirection();
  delay(PAUSE);
  ctrl.setLocoDirection(LOCO, DIR_REVERSE);
  printDirection();
  delay(PAUSE);
}
