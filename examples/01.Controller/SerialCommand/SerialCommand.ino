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
 * This exemple is the update of examples/01.Controller/SerialCommand/SerialCommand.ino
 */

/*
* Exemple :
    power 1
    speed 16391 100
    function 16391 0 1
*/

#include "Config.h"
#include "TrackController.h"

const bool DEBUG = false;
const uint64_t TIMEOUT = 500; // ms
const uint16_t HASH = 0x00;
const bool LOOPBACK = false;

TrackController ctrl(HASH, DEBUG, TIMEOUT, LOOPBACK); // Instance de la classe TrackController, création de l'objet ctrl.

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    ;
  ctrl.begin();
}

void loop()
{
  if (Serial.available())
  {
    String command = Serial.readStringUntil('\n');
    Serial.println(command);
    ctrl.handleUserCommands(command);
  }
}
