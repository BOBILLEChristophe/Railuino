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
 * WITHOUT an MS2: Railuino is the only central station (ESP32 or Arduino
 * + Gleisbox). It registers the MFX locomotives itself: Discovery ->
 * Bind -> Verify, 3 tries. Based on rocrail_mfx_discovery v0.4.
 * Do not run it with an MS2 on the bus (use MfxObserver).
 *
 * SANS MS2 : Railuino est la seule centrale. Il inscrit lui-même les
 * locos MFX. Basé sur rocrail_mfx_discovery v0.4. Ne pas l'utiliser
 * avec une MS2 sur le bus (voir MfxObserver).
 *
 * Serial: d = new discovery, l = list, r = every loco registers again,
 *         c = clear the table.
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

constexpr uint32_t SEARCH_TIME = 10000; // ms: the locos on the track answer
constexpr bool DEBUG = false;

#if defined(ARDUINO_ARCH_AVR)
TrackController ctrl(can, 0x2F39, DEBUG); // hash of v0.4
#else
TrackController ctrl(0x2F39, DEBUG); // hash of v0.4
#endif
MfxRegistry mfx(ctrl, MfxRegistry::Mode::Master);
uint8_t confirmed = 0; // locos confirmed since the start

void onNewAddress(uint32_t uid, uint16_t locId, void *)
{
  Serial.print(F("Loco "));
  Serial.print(uid, HEX);
  Serial.print(F(" confirmed, SID "));
  Serial.print(locId - ADDR_MFX); // the value to put in SID of the examples
  Serial.print(F(" (Loc-ID 0x"));
  Serial.print(locId, HEX);
  Serial.println(F(")"));
  confirmed++;
}

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    ;
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
  mfx.begin(); // Stop, wake-up, counter, Verify, Go, Discovery

  // The locos answer after the discovery, a few seconds later: wait for
  // them here, so that their lines come before the menu.
  // Les locos répondent après la recherche, quelques secondes plus tard :
  // on les attend ici, pour que leurs lignes viennent avant le menu.
  Serial.println(F("Searching for the locos on the track (10 s)..."));
  const uint32_t start = millis();
  while (millis() - start < SEARCH_TIME)
    ctrl.poll();
  Serial.print(confirmed);
  Serial.println(F(" loco(s) confirmed. Locos put on the track later are registered too."));
  Serial.println(F("d = discovery, l = list, r = register again, c = clear"));
}

void loop()
{
  ctrl.poll(); // receives the answers and runs Bind / Verify

  if (Serial.available())
  {
    switch (Serial.read())
    {
    case 'd':
      mfx.startDiscovery();
      break;
    case 'l':
      mfx.printTo(Serial);
      break;
    case 'r':
      mfx.forceReRegistration();
      Serial.println(F("Re-registration requested"));
      break;
    case 'c':
      mfx.clear();
      Serial.println(F("Table cleared"));
      break;
    }
  }
}
