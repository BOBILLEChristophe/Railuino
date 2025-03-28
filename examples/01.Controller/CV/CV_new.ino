/*********************************************************************
 * Railuino - Hacking your Märklin
 *
 * Copyright (C) 2012 Joerg Pleumann
 * Copyright (C) 2024 Christophe Bobille
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
* This exemple is the update of examples/01.Controller/CV/CV_new.ino
*/

#include "Config.h"
#include "TrackController.h"

const uint16_t LOCO = ADDR_MFX + 7; // Change with your own address

const bool DEBUG = false;
const uint64_t TIMEOUT = 500; // ms
const uint16_t HASH = 0x00;
const bool LOOPBACK = false;

TrackController ctrl(HASH, DEBUG, TIMEOUT, LOOPBACK);  // Instance de la classe TrackController, création de l'objet ctrl.

void showRegister(uint16_t, String);

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    ;

  ctrl.begin();
  Serial.print("Power on\n\n");
  ctrl.setPower(true);

  showRegister(1, "Address");
  showRegister(2, "Min. Voltage");
  showRegister(3, "Accel. time");
  showRegister(4, "Decel. time");
  showRegister(5, "Max. speed");
  showRegister(6, "Avg. speed");
  showRegister(7, "Version");
  showRegister(8, "Manufacturer");

  Serial.println("\n\nSystem stopped. Need to reset.");
}

void loop() 
{
}// Nothing to do

void showRegister(uint16_t i, String label)
{
  uint8_t b = 0;

  if (ctrl.readConfig(LOCO, i, &b))
    Serial.print("Register "+label+" : "+b+"\n");
}
