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
 * This exemple is the update of examples/01.Controller/Headlight/Headlight_new.ino
 */

 #include "Config.h"
 #include "TrackController.h"
 
 const uint16_t LOCO = ADDR_MFX + 7; // Change with your own address
 
 const uint16_t PAUSE = 5UL * 1000UL;
 
 const bool DEBUG = false;
 const uint64_t TIMEOUT = 500; // ms
 const uint16_t HASH = 0x00;
 const bool LOOPBACK = false;
 
 TrackController ctrl(HASH, DEBUG, TIMEOUT, LOOPBACK); // Instance de la classe TrackController, création de l'objet ctrl.
 
 
 void setup() {
   Serial.begin(115200);
   while (!Serial);
 
   ctrl.begin();
   Serial.println("Power on");
   ctrl.setPower(true);
 }
 
 void loop() {
   byte b;
   
   ctrl.setLocoFunction(LOCO, 0, 1);
   if (ctrl.getLocoFunction(LOCO, 0, &b)) {
     Serial.print("Lights are ");
     Serial.println(b ? "on" : "off");
   }
   
   delay(PAUSE);
   
   ctrl.setLocoFunction(LOCO, 0, 0);
   if (ctrl.getLocoFunction(LOCO, 0, &b)) {
     Serial.print("Lights are ");
     Serial.println(b ? "on" : "off");
   }
   
   delay(PAUSE);
 }
 
 