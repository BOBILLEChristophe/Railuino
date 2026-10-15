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
 * WITH an MS2 (or CS2) on the bus. The MS2 registers the MFX locomotives;
 * this sketch learns, from the Bind messages of the MS2, which address
 * (Loc-ID) each locomotive has, and keeps the table across restarts.
 * The locomotive is then driven by its UID, whatever SID the MS2 gave it.
 *
 * AVEC une MS2 (ou CS2) sur le bus. La MS2 inscrit les locos MFX ; ce
 * sketch apprend, d'après les messages Bind de la MS2, quelle adresse
 * (Loc-ID) a chaque loco, et garde la table après un redémarrage.
 * La loco se pilote alors par son UID, quel que soit le SID donné par la MS2.
 *
 * To fill the table: register the locos on the MS2 (new loco, or remove
 * and put back on the track) while this sketch runs.
 * Serial: l = list, c = clear the table, f = lights of LOCO_UID on/off.
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

// UID of one of your MFX locos (printed when the MS2 registers it).
constexpr uint32_t LOCO_UID = 0xFFE9B194UL;

#if defined(ARDUINO_ARCH_AVR)
TrackController ctrl(can, 0, false);
#else
TrackController ctrl(0, false);
#endif
MfxRegistry mfx(ctrl); // observer mode: sends nothing

void onNewAddress(uint32_t uid, uint16_t locId, void *)
{
  Serial.print(F("Loco "));
  Serial.print(uid, HEX);
  Serial.print(F(" -> Loc-ID 0x"));
  Serial.print(locId, HEX);
  Serial.print(F(" ("));
  Serial.print(locId);
  Serial.println(F(")"));
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
  mfx.setAutoSave(true); // NVS on ESP32, EEPROM on AVR
  mfx.onChange(onNewAddress);
  mfx.begin();
  mfx.printTo(Serial);
  Serial.println(F("l = list, c = clear, f = lights of LOCO_UID"));
}

void loop()
{
  ctrl.poll(); // receives the MS2 messages and updates the table

  if (Serial.available())
  {
    static bool lights = false;
    switch (Serial.read())
    {
    case 'l':
      mfx.printTo(Serial);
      break;
    case 'c':
      mfx.clear();
      Serial.println(F("Table cleared"));
      break;
    case 'f':
    {
      const uint16_t locId = mfx.locIdOf(LOCO_UID);
      if (locId == 0)
        Serial.println(F("LOCO_UID unknown: register it on the MS2"));
      else
      {
        lights = !lights;
        ctrl.setLocoFunction(locId, 0, lights ? 1 : 0);
        Serial.println(lights ? F("Lights on") : F("Lights off"));
      }
      break;
    }
    }
  }
}
