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
 * MFX MASTER: Railuino is the only central station (ESP32 or Arduino +
 * Gleisbox, NO MS2 on the bus). It registers the MFX locomotives itself
 * and gives each one a SID (from 5).
 * MFX MASTER : Railuino est la seule centrale (ESP32 ou Arduino +
 * Gleisbox, PAS de MS2 sur le bus). Il inscrit lui-même les locomotives
 * MFX et donne un SID à chacune (à partir de 5).
 *
 * - New locos: Discovery -> Bind -> Verify, 3 tries.
 * - Known locos (saved in memory): checked with a Verify, without a new
 *   Bind, so they keep their SID. "confirmed" = on the track,
 *   "unconfirmed" = known but did not answer (not on the track). An
 *   unconfirmed loco is checked again every few seconds.
 * - Locos inscrites : contrôlées par un Verify, sans nouveau Bind ; elles
 *   gardent leur SID. « confirmed » = sur la voie, « unconfirmed » =
 *   connue mais n'a pas répondu (pas sur la voie). Une loco
 *   « unconfirmed » est contrôlée de nouveau toutes les quelques secondes.
 *
 * Do not run it with an MS2 on the bus (use MfxObserver): both would
 * give SIDs to the same locomotives.
 * Ne pas l'utiliser avec une MS2 sur le bus (voir MfxObserver) : les deux
 * donneraient des SID aux mêmes locomotives.
 *
 * Serial: l = list, d = new discovery, v = check the known locos again,
 *         r = every loco registers again, c = clear the table.
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

constexpr uint32_t MIN_WAIT = 10000; // ms: the locos on the track answer
constexpr uint32_t MAX_WAIT = 30000; // ms: at most, before the menu
constexpr bool DEBUG = false;

#if defined(ARDUINO_ARCH_AVR)
TrackController ctrl(can, 0x2F39, DEBUG); // hash of rocrail_mfx_discovery v0.4
#else
TrackController ctrl(0x2F39, DEBUG); // hash of rocrail_mfx_discovery v0.4
#endif
MfxRegistry mfx(ctrl, MfxRegistry::Mode::Master);

// Called when a loco is confirmed (new, or known and on the track).
// Appelée quand une loco est confirmée (nouvelle, ou connue et sur la voie).
void onNewAddress(uint32_t uid, uint16_t locId, void *)
{
  Serial.print(F("Loco UID 0x"));
  Serial.print(uid, HEX);
  Serial.print(F(" - SID "));
  Serial.print(locId - ADDR_MFX); // the value to put in SID of the examples
  Serial.print(F(" - Loc-ID 0x"));
  Serial.println(locId, HEX);
}

void printMenu()
{
  Serial.println(F("l = list, d = discovery, v = check again, r = register again, c = clear"));
}

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    ;
  Serial.println();
  Serial.println(F("RAILUINO - MFX MASTER"));

#if defined(ARDUINO_ARCH_ESP32)
  const bool canOk = ctrl.begin(CAN_RX, CAN_TX);
#else
  const bool canOk = ctrl.begin(); // MCP2515 (SPI)
#endif
  if (!canOk)
  {
    Serial.println(F("CAN configuration error"));
    for (;;)
      delay(1000);
  }

  mfx.setAutoSave(true); // same SID for a loco after a restart
  mfx.onChange(onNewAddress);
  Serial.println(F("Start sequence (about 6 s)..."));
  mfx.begin(); // Stop, wake-up, counter, Verify, Go, then checks / discovery

  // Waits for the answers, so that the loco lines come before the menu:
  // at least MIN_WAIT, longer while locos are being checked or registered.
  // Attend les réponses, pour que les lignes des locos précèdent le menu.
  Serial.println(F("Searching for the locos on the track..."));
  const uint32_t start = millis();
  while (millis() - start < MAX_WAIT &&
         (millis() - start < MIN_WAIT || mfx.busy()))
    ctrl.poll();

  mfx.printTo(Serial);
  Serial.println(F("Locos put on the track later are registered too."));
  printMenu();
}

void loop()
{
  ctrl.poll(); // receives the answers, runs Bind / Verify and the checks

  if (Serial.available())
  {
    switch (Serial.read())
    {
    case 'l':
      mfx.printTo(Serial);
      break;
    case 'd':
      mfx.startDiscovery();
      Serial.println(F("Discovery started"));
      break;
    case 'v':
      if (mfx.verifyKnown())
        Serial.println(F("Checking the known locos..."));
      else
        Serial.println(F("No known loco"));
      break;
    case 'r':
      mfx.forceReRegistration();
      Serial.println(F("Re-registration requested"));
      break;
    case 'c':
      mfx.clear();
      Serial.println(F("Table cleared"));
      break;
    case '\r':
    case '\n':
      break;
    default:
      printMenu();
      break;
    }
  }
}
