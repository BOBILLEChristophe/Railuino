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
 * Runs a locomotive for 5 seconds, then stops it.
 * Fait rouler une locomotive pendant 5 secondes, puis l'arrête.
 */

#if defined(ARDUINO_ARCH_AVR)
#include <ACAN2515.h> // Uno / Mega: lets the Arduino IDE find the MCP2515 library
#endif
#include <Railuino.h>

#if defined(ARDUINO_ARCH_ESP32)
// CAN pins of the ESP32 (TWAI controller).
// Broches CAN de l'ESP32 (contrôleur TWAI).
constexpr uint8_t CAN_RX = 22;
constexpr uint8_t CAN_TX = 23;
#endif
#if defined(ARDUINO_ARCH_AVR)
// Uno / Mega: MCP2515 module on the SPI bus, CS 10, INT 2. Set the
// frequency written on its quartz: most cheap modules have 8 MHz.
// Uno / Mega : module MCP2515 sur le bus SPI, CS 10, INT 2. Indiquez la
// fréquence écrite sur son quartz : 8 MHz sur la plupart des modules.
constexpr uint32_t MCP2515_QUARTZ = 16000000UL; // 8000000UL for 8 MHz
Mcp2515Driver can(10, 2, MCP2515_QUARTZ);       // CS, INT, quartz
#endif

// MFX address (SID) of your locomotive. To find it:
// - with an MS2: run the example 03.Mfx/MfxObserver, then register the
//   loco on the MS2 (remove it and put it back on the track);
// - without an MS2 (Gleisbox only): run the example 03.Mfx/MfxMaster,
//   which registers the loco and gives it its SID (5, 6...).
// The SID is printed; the loco keeps it until it is registered again.
// Adresse MFX (SID) de votre locomotive. Pour la trouver :
// - avec une MS2 : lancez l'exemple 03.Mfx/MfxObserver, puis faites
//   inscrire la loco sur la MS2 (supprimez-la et reposez-la sur la voie) ;
// - sans MS2 (Gleisbox seule) : lancez l'exemple 03.Mfx/MfxMaster, qui
//   inscrit la loco et lui donne son SID (5, 6...).
// Le SID s'affiche ; la loco le garde jusqu'à une nouvelle inscription.
constexpr uint16_t SID = 7; // Change with your own SID

// MM2 or DCC locomotive: ADDR_MM2 + address or ADDR_DCC + address.
// Loco MM2 ou DCC : ADDR_MM2 + adresse ou ADDR_DCC + adresse.
constexpr uint16_t LOCO = ADDR_MFX + SID;

constexpr uint16_t SPEED = 100;         // 0..1000
constexpr uint32_t TIME = 5000;         // ms
constexpr bool DEBUG = false;

#if defined(ARDUINO_ARCH_AVR)
TrackController ctrl(can, 0, DEBUG);
#else
TrackController ctrl(0, DEBUG);
#endif

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
#if defined(ARDUINO_ARCH_ESP32)
  ctrl.begin(CAN_RX, CAN_TX);
#else
  ctrl.begin(); // MCP2515 (SPI)
#endif

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
