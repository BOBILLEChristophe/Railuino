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

#include <Railuino.h>

#if defined(ARDUINO_ARCH_ESP32)
// CAN pins of the ESP32 (TWAI controller).
// Broches CAN de l'ESP32 (contrôleur TWAI).
constexpr uint8_t CAN_RX = 22;
constexpr uint8_t CAN_TX = 23;
#endif
// Uno / Mega: MCP2515 module on the SPI bus, CS 10, INT 2, 16 MHz quartz
// (see RAILUINO_MCP2515_* to change them).
// Uno / Mega : module MCP2515 sur le bus SPI, CS 10, INT 2, quartz 16 MHz
// (voir RAILUINO_MCP2515_* pour les changer).

constexpr bool DEBUG = false;

TrackController ctrl(0x2F39, DEBUG); // hash of v0.4
MfxRegistry mfx(ctrl, MfxRegistry::Mode::Master);

void onNewAddress(uint32_t uid, uint16_t locId, void *)
{
  Serial.print(F("Loco "));
  Serial.print(uid, HEX);
  Serial.print(F(" confirmed, Loc-ID 0x"));
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
