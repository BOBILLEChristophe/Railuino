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
 * Switches the headlights of a locomotive on and off every 5 seconds.
 * Allume et éteint les feux d'une locomotive toutes les 5 secondes.
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

// MFX address (SID) of your locomotive. To find it, run the example
// 03.Mfx/MfxObserver, then register the loco on the MS2 (remove it and
// put it back on the track): the SID is printed.
// Adresse MFX (SID) de votre locomotive. Pour la trouver, lancez l'exemple
// 03.Mfx/MfxObserver, puis faites inscrire la loco sur la MS2 (supprimez-la
// et reposez-la sur la voie) : le SID s'affiche.
constexpr uint16_t SID = 7; // Change with your own SID

// MM2 or DCC locomotive: ADDR_MM2 + address or ADDR_DCC + address.
// Loco MM2 ou DCC : ADDR_MM2 + adresse ou ADDR_DCC + adresse.
constexpr uint16_t LOCO = ADDR_MFX + SID;

constexpr uint32_t PAUSE = 5000;        // ms
constexpr bool DEBUG = false;

#if defined(ARDUINO_ARCH_AVR)
TrackController ctrl(can, 0, DEBUG);
#else
TrackController ctrl(0, DEBUG);
#endif

void setLights(uint8_t power)
{
  ctrl.setLocoFunction(LOCO, 0, power);
  uint8_t b;
  if (ctrl.getLocoFunction(LOCO, 0, &b))
  {
    Serial.print(F("Lights are "));
    Serial.println(b ? F("on") : F("off"));
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
}

void loop()
{
  setLights(1);
  delay(PAUSE);
  setLights(0);
  delay(PAUSE);
}
