/*********************************************************************
 * Railuino - Hacking your Märklin
 *
 * Copyright (C) 2012 Joerg Pleumann
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
 
#include "Config.h"
#include "TrackController.h"

const uint16_t LOCO = ADDR_MM2 + 78; 
const uint16_t TIME = 2000;
const bool DEBUG = true;

TrackController ctrl(0xdf24, DEBUG);

void setup() {
  Serial.begin(115200);
  while (!Serial);
  
  ctrl.begin();
  Serial.println("Power on");
  ctrl.setPower(true);
}

void loop() {
  byte b;
  
  for (uint8_t i = 0; i <= 4; i++) {
    Serial.print("Function ");
    Serial.print(i, DEC);
    
    ctrl.setLocoFunction(LOCO, i, 1);
    if (ctrl.getLocoFunction(LOCO, i, &b)) {
      Serial.print("(Function ");
      Serial.print(i, DEC);
      Serial.println(b ? " is on)" : " is off)");
    }
  
    delay(TIME);

    ctrl.setLocoFunction(LOCO, i, 0);
    if (ctrl.getLocoFunction(LOCO, i, &b)) {
      Serial.print("(Function ");
      Serial.print(i, DEC);
      Serial.println(b ? " is on)" : " is off)");
    }
    
    delay(TIME);
  }
}

