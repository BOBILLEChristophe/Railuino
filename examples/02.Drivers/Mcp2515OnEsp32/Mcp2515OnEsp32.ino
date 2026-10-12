/*********************************************************************
 * Railuino - Hacking your Märklin
 *
 * Copyright (C) 2026 christophe bobille
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
 * An ESP32 with an MCP2515 module on its SPI bus instead of its built-in
 * CAN controller: the driver is created in the sketch and given to
 * TrackController. Switches the track power on and off every 5 seconds.
 *
 * Un ESP32 avec un module MCP2515 sur son bus SPI au lieu de son
 * contrôleur CAN intégré : le pilote est créé dans le sketch et passé à
 * TrackController.
 *
 * Wiring / Câblage (ESP32 VSPI): SCK 18, MISO 19, MOSI 23, CS 5, INT 4.
 */

#ifndef ARDUINO_ARCH_ESP32
#error "This example is for an ESP32 / Cet exemple est pour un ESP32"
#endif

#include <ACAN2515.h> // makes the MCP2515 driver available (Arduino IDE)
#include <Railuino.h>

const uint8_t MCP2515_CS = 5;
const uint8_t MCP2515_INT = 4;
const uint32_t MCP2515_QUARTZ = 8000000UL; // 8 MHz on most cheap modules

const uint32_t PAUSE = 5000; // ms
const bool DEBUG = true;

Mcp2515Driver can(MCP2515_CS, MCP2515_INT, MCP2515_QUARTZ);
TrackController ctrl(can, 0, DEBUG); // hash 0 = generated

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    ;
  if (!ctrl.begin())
    Serial.println(F("CAN not started: check the wiring and the quartz"));
}

void loop()
{
  Serial.println(F("Power on"));
  ctrl.setPower(true);
  delay(PAUSE);
  Serial.println(F("Power off"));
  ctrl.setPower(false);
  delay(PAUSE);
}
