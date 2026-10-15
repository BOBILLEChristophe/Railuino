/*********************************************************************
 * Railuino - Hacking your Märklin
 *
 * Copyright (C) 2012 Joerg Pleumann
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
 * Prints what the other devices (MS2, CS2...) send on the bus, with the
 * locomotive commands decoded. Drive a locomotive from the MS2 to read
 * its current address (Loc-ID), e.g. after the MS2 gave it a new MFX SID.
 *
 * Affiche ce que les autres appareils (MS2, CS2...) envoient sur le bus,
 * avec les commandes de locomotive décodées. Pilotez une loco depuis la
 * MS2 pour lire son adresse actuelle (Loc-ID), par exemple après que la
 * MS2 lui a donné un nouveau SID MFX.
 *
 *   8B23 R 04 6 00 00 40 07 00 64   loco 0x4007 (16391) speed 100
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

#if defined(ARDUINO_ARCH_AVR)
TrackController ctrl(can, 0, false);
#else
TrackController ctrl(0, false);
#endif

void printLoco(const TrackMessage &m)
{
  Serial.print(F("   loco 0x"));
  Serial.print(m.uid(), HEX);
  Serial.print(F(" ("));
  Serial.print(m.uid());
  Serial.print(F(")"));
}

void onCanMessage(const TrackMessage &m, void *)
{
  Serial.print(m); // raw message: hash, R (response), command, length, data

  if (m.response)
  {
    Serial.println();
    return;
  }

  switch (m.command)
  {
  case Marklin::Cmd::Speed:
    if (m.length == 6)
    {
      printLoco(m);
      Serial.print(F(" speed "));
      Serial.print((static_cast<uint16_t>(m.data[4]) << 8) | m.data[5]);
    }
    break;
  case Marklin::Cmd::Direction:
    if (m.length == 5)
    {
      printLoco(m);
      Serial.print(F(" direction "));
      Serial.print(m.data[4]);
    }
    break;
  case Marklin::Cmd::Function:
    if (m.length == 6)
    {
      printLoco(m);
      Serial.print(F(" function "));
      Serial.print(m.data[4]);
      Serial.print(m.data[5] ? F(" on") : F(" off"));
    }
    break;
  case Marklin::Cmd::Accessory:
    if (m.length >= 6)
    {
      Serial.print(F("   accessory 0x"));
      Serial.print(m.uid(), HEX);
      Serial.print(F(" position "));
      Serial.print(m.data[4]);
    }
    break;
  case Marklin::Cmd::System:
    if (m.length >= 5 && m.data[4] == Marklin::SysCmd::Go)
      Serial.print(F("   power on"));
    else if (m.length >= 5 && m.data[4] == Marklin::SysCmd::Stop)
      Serial.print(F("   power off"));
    break;
  case Marklin::Cmd::Ping:
    Serial.print(F("   ping"));
    break;
  }
  Serial.println();
}

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    ;
  ctrl.onMessage(onCanMessage);
#if defined(ARDUINO_ARCH_ESP32)
  ctrl.begin(CAN_RX, CAN_TX);
#else
  ctrl.begin(); // MCP2515 (SPI)
#endif
  Serial.println(F("Listening to the CAN bus..."));
}

void loop()
{
  ctrl.poll();
}
