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

const uint16_t LOCO = ADDR_MFX + 7;        // Change with your own address
const uint32_t TIME = 1UL * 60UL * 1000UL; // 1 minute
const bool DEBUG = true;

TrackController ctrl(0xDF24, DEBUG);

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    ;
  ctrl.begin();
}

void loop()
{
  ctrl.handleUserCommands();
}
