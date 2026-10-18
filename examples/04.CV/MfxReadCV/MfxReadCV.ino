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
 * Reads the main settings of an MFX decoder, like the MS2 does:
 * name, manufacturer, acceleration, braking, start and maximum speed,
 * sound volume. Read only: nothing is written to the decoder.
 * Lit les principaux réglages d'un décodeur MFX, comme la MS2 :
 * nom, fabricant, accélération, freinage, vitesses mini et maxi,
 * volume sonore. Lecture seule : rien n'est écrit dans le décodeur.
 *
 * An MFX decoder has no DCC-like CVs. Its configuration is made of
 * arrays (CV number + index 0..63) grouped in blocks; the block table
 * (CV 4) tells where each block starts, and index 0 of each array gives
 * its type. The sketch finds the places itself, so it works whatever the
 * decoder (Stefan Krauss, "Description of the mfx Rail format", 6.3-6.4).
 * Un décodeur MFX n'a pas de CV comme en DCC. Sa configuration est faite
 * de tableaux (numéro de CV + index 0..63) regroupés en blocs ; la table
 * des blocs (CV 4) indique où commence chaque bloc, et l'index 0 de chaque
 * tableau donne son type. Le sketch trouve lui-même les emplacements :
 * il fonctionne quel que soit le décodeur.
 *
 * Setup / Montage : Gleisbox + Railuino, MS2 unplugged / MS2 débranchée.
 * Find the SID with 03.Mfx/MfxMaster. Trouvez le SID avec 03.Mfx/MfxMaster.
 *
 * Warning / Attention : first-generation MFX decoders are not designed for
 * this and could be damaged. Les décodeurs MFX de première génération ne
 * sont pas conçus pour cela et pourraient être endommagés.
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

// MFX address (SID) of your locomotive, given by 03.Mfx/MfxMaster.
// Adresse MFX (SID) de votre locomotive, donnée par 03.Mfx/MfxMaster.
constexpr uint16_t SID = 5; // Change with your own SID
constexpr uint16_t LOCO = ADDR_MFX + SID;
constexpr bool DEBUG = false;

#if defined(ARDUINO_ARCH_AVR)
TrackController ctrl(can, 0, DEBUG);
#else
TrackController ctrl(0, DEBUG);
#endif

// Places of the configuration (Krauss 6.4)
constexpr uint16_t CV_BLOCKS = 4;       // block table (type 0x17), index 1..
constexpr uint8_t TYPE_MAKER = 0x10;    // block 01: manufacturer, index 1..4
constexpr uint8_t TYPE_NAME = 0x18;     // block 01: name, index 1..16
constexpr uint8_t BLOCK_DRIVING = 0x05; // driving characteristics
constexpr uint8_t BLOCK_SOUND = 0x08;   // sound
constexpr uint8_t MAX_BLOCKS = 16;
constexpr uint8_t MAX_NAME_LENGTH = 16; // (NAME_MAX is taken on ESP32)

bool readByte(uint16_t cv, uint8_t index, uint8_t *value)
{
  return ctrl.readConfig(LOCO, cv, value, index);
}

// CV of the array of type 'type' in block 01, 0 if not found. Block 01
// ends where the next block starts (block table, index 1, x 4). Other
// blocks reuse the same types (block 05 also has a type 0x18), so the
// search stays in block 01. CV 0 is not read: the Gleisbox may refuse it.
uint16_t findInBlock01(uint8_t type)
{
  uint16_t end = 16; // if the block table cannot be read
  uint8_t next;
  if (readByte(CV_BLOCKS, 1, &next) && next != 0)
    end = uint16_t(next) * 4;
  for (uint16_t cv = 1; cv < end && cv < 64; cv++)
  {
    uint8_t t;
    if (readByte(cv, 0, &t) && t == type)
      return cv;
  }
  return 0;
}

// CV of the first array of the block of type 'type', 0 if not found.
uint16_t findBlock(uint8_t type)
{
  for (uint8_t i = 1; i <= MAX_BLOCKS; i++)
  {
    uint8_t start;
    if (!readByte(CV_BLOCKS, i, &start) || start == 0)
      break;
    const uint16_t cv = uint16_t(start) * 4; // blocks start on a multiple of 4
    uint8_t blockType;                       // block description: 01 BB VV 00 GG LL
    if (readByte(cv, 1, &blockType) && blockType == type)
      return cv;
  }
  return 0;
}

void printName()
{
  Serial.print(F("Name         : "));
  const uint16_t cv = findInBlock01(TYPE_NAME);
  if (cv == 0)
  {
    Serial.println(F("not found"));
    return;
  }
  char name[MAX_NAME_LENGTH + 1];
  uint8_t n = 0;
  for (uint8_t i = 1; i <= MAX_NAME_LENGTH; i++)
  {
    uint8_t c;
    if (!readByte(cv, i, &c) || c == 0)
      break;
    name[n++] = (char)c;
  }
  name[n] = 0;
  Serial.println(n ? name : "(none)");
}

void printMaker()
{
  Serial.print(F("Manufacturer : "));
  const uint16_t cv = findInBlock01(TYPE_MAKER);
  uint32_t id = 0;
  for (uint8_t i = 1; cv != 0 && i <= 4; i++)
  {
    uint8_t b = 0;
    if (!readByte(cv, i, &b))
    {
      Serial.println(F("no answer"));
      return;
    }
    id = (id << 8) | b;
  }
  if (cv == 0)
  {
    Serial.println(F("not found"));
    return;
  }
  Serial.print(F("0x"));
  Serial.print(id, HEX);
  if (id == 0x83)
    Serial.println(F(" (Maerklin / Trix)"));
  else if (id == 0x97)
    Serial.println(F(" (ESU)"));
  else
    Serial.println();
}

void printDriving()
{
  const uint16_t block = findBlock(BLOCK_DRIVING);
  if (block == 0)
  {
    Serial.println(F("Driving block: not found"));
    return;
  }
  Serial.print(F("Driving block: CV "));
  Serial.println(block);

  // Block 05 + 2, type 0x13: acceleration AA, braking BB (x 0.25 s)
  uint8_t type, a, b;
  if (readByte(block + 2, 0, &type) && type == 0x13 &&
      readByte(block + 2, 1, &a) && readByte(block + 2, 2, &b))
  {
    Serial.print(F("Acceleration : "));
    Serial.print(a * 0.25, 2);
    Serial.print(F(" s  (value "));
    Serial.print(a);
    Serial.println(F(")"));
    Serial.print(F("Braking      : "));
    Serial.print(b * 0.25, 2);
    Serial.print(F(" s  (value "));
    Serial.print(b);
    Serial.println(F(")"));
  }
  else
    Serial.println(F("Acceleration / braking: not found"));

  // Block 05 + 3, type 0x17: Vmin MM, Vmax AA (0..255)
  if (readByte(block + 3, 0, &type) && type == 0x17 &&
      readByte(block + 3, 1, &a) && readByte(block + 3, 2, &b))
  {
    Serial.print(F("Start speed  : "));
    Serial.println(a);
    Serial.print(F("Max. speed   : "));
    Serial.println(b);
  }
  else
    Serial.println(F("Speed curve: not found"));
}

void printSound()
{
  // Block 08 + 1, type 0x10: volume in index 1 (0..255)
  const uint16_t block = findBlock(BLOCK_SOUND);
  if (block == 0)
  {
    Serial.println(F("Volume       : no sound block (decoder without sound?)"));
    return;
  }
  uint8_t type, volume;
  if (readByte(block + 1, 0, &type) && type == 0x10 &&
      readByte(block + 1, 1, &volume))
  {
    Serial.print(F("Volume       : "));
    Serial.print(volume);
    Serial.print(F(" / 255  ("));
    Serial.print((volume * 100 + 127) / 255);
    Serial.print(F(" %)  [sound block: CV "));
    Serial.print(block);
    Serial.println(F("]"));
  }
  else
    Serial.println(F("Volume       : not found"));
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
  ctrl.setPower(true);
  Serial.print(F("Reading MFX decoder SID "));
  Serial.print(SID);
  Serial.println(F("..."));

  printName();
  printMaker();
  printDriving();
  printSound();

  Serial.println(F("Done. Need to reset."));
}

void loop()
{
}
