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
 * This exemple is the update of examples/01.Controller/Test_address_loco/Test_address_loco.ino
 */

 // Versiuon 0.1 20/03/25

 #include "Config.h"
 #include "TrackController.h"
 
 // const uint16_t LOCO = ADDR_MFX + 8; // Change with your own address
 uint16_t LOCO = ADDR_MFX;
 const uint16_t PAUSE = 10UL * 1000UL;
 
 const bool DEBUG = false;
 const uint64_t TIMEOUT = 500; // ms
 const uint16_t HASH = 0x00;
 const bool LOOPBACK = false;
 
 TrackController ctrl(HASH, DEBUG, TIMEOUT, LOOPBACK); // Instance de la classe TrackController, création de l'objet ctrl.
 
 const uint8_t nbreLocos = 20;
 
 void setup()
 {
   Serial.begin(115200);
   while (!Serial)
     ;
 
 #if defined ARDUINO_ARCH_ESP32
   ctrl.begin(21, 22);
 #elif defined ARDUINO_ARCH_AVR
   ctrl.begin(255, 255);
 #endif
 
   Serial.println("Power on");
   ctrl.setPower(true);
 }
 
 void loop()
 {
   delay(5000);
   uint16_t LOCO = ADDR_MFX;
   for (uint8_t i = 1; i < nbreLocos; i++)
   {
     for (uint8_t j = 0; j < 6; j++)
       ctrl.setLocoFunction(LOCO + i, j, 1);
     Serial.print("Loco adresse : ");
     Serial.println(i);
     delay(5000);
     for (uint8_t j = 0; j < 6; j++)
       ctrl.setLocoFunction(LOCO + i, j, 0);
   }
 }
 