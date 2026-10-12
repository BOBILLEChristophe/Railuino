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
 * Runs a locomotive for 5 seconds, then stops it.
 * Fait rouler une locomotive pendant 5 secondes, puis l'arrête.
 */

#include <Railuino.h>

// CAN pins on ESP32. Ignored on Uno/Mega, where the MCP2515 is on the SPI
// bus (see RAILUINO_MCP2515_* to change its CS/INT pins and quartz).
// Broches CAN sur ESP32. Ignorées sur Uno/Mega (MCP2515 sur le bus SPI).
const uint8_t CAN_RX = 22;
const uint8_t CAN_TX = 23;

const uint16_t LOCO = ADDR_MFX + 7; // Change with your own address
const uint16_t SPEED = 100;         // 0..1000
const uint32_t TIME = 5000;         // ms
const bool DEBUG = false;

TrackController ctrl(0, DEBUG);

void printSpeed()
{
  uint16_t speed;
  if (ctrl.getLocoSpeed(LOCO, &speed))
  {
    Serial.print(F("(Speed is "));
    Serial.print(speed);
    Serial.println(F(")"));
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
  Serial.println(F("Headlights on"));
  ctrl.setLocoFunction(LOCO, 0, 1);
  Serial.println(F("Direction forward"));
  ctrl.setLocoDirection(LOCO, DIR_FORWARD);

  Serial.print(F("Setting speed to "));
  Serial.println(SPEED);
  ctrl.setLocoSpeed(LOCO, SPEED);
  printSpeed();
  delay(TIME);

  Serial.println(F("Stop"));
  ctrl.setLocoSpeed(LOCO, 0);
  printSpeed();
  Serial.println(F("System stopped. Need to reset."));
}

void loop()
{
}
