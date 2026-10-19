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
 * Changes the main settings of an MFX decoder from the serial monitor:
 * sound volume, maximum speed, acceleration and braking.
 * Modifie les principaux réglages d'un décodeur MFX depuis le moniteur
 * série : volume sonore, vitesse maximale, accélération et freinage.
 *
 * Commands (line ending "Newline") / Commandes (fin de ligne
 * « Nouvelle ligne ») :
 *   l        list the settings / affiche les réglages
 *   v 180    volume 0..255
 *   m 200    maximum speed / vitesse maximale 0..255
 *   a 28     acceleration / accélération 0..255 (x 0.25 s: 28 = 7 s)
 *   b 15     braking / freinage 0..255 (x 0.25 s: 15 = 3.75 s)
 *
 * The places are found in the decoder itself (block table): volume in
 * block 08 + 1 (type 0x10), index 1; acceleration and braking in block
 * 05 + 2 (type 0x13), index 1 and 2; speeds in block 05 + 3 (type 0x17),
 * index 1 (start) and 2 (max). The type is checked before every write:
 * nothing is written if it does not match (Krauss, "Description of the
 * mfx Rail format", 6.3-6.4).
 * Les emplacements sont retrouvés dans le décodeur (table des blocs). Le
 * type est vérifié avant chaque écriture : rien n'est écrit s'il ne
 * correspond pas.
 *
 * The MS2 keeps its own copy of these settings: it shows the new values
 * only after the loco has been removed from its list and registered again.
 * La MS2 garde sa propre copie de ces réglages : elle n'affiche les
 * nouvelles valeurs qu'après avoir retiré la loco de sa liste et l'avoir
 * réinscrite.
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

constexpr uint16_t CV_BLOCKS = 4;       // block table (type 0x17), index 1..
constexpr uint8_t BLOCK_DRIVING = 0x05; // driving characteristics
constexpr uint8_t BLOCK_SOUND = 0x08;   // sound
constexpr uint8_t MAX_BLOCKS = 16;

// One setting: array (CV) found at start, its expected type, its index.
struct Setting
{
  char key;
  const char *name;
  uint16_t cv; // 0 if not found
  uint8_t type;
  uint8_t index;
};

Setting settings[] = {
    {'v', "Volume      ", 0, 0x10, 1}, // block 08 + 1
    {'m', "Max. speed  ", 0, 0x17, 2}, // block 05 + 3
    {'a', "Acceleration", 0, 0x13, 1}, // block 05 + 2
    {'b', "Braking     ", 0, 0x13, 2}, // block 05 + 2
};

bool readByte(uint16_t cv, uint8_t index, uint8_t *value)
{
  return ctrl.readConfig(LOCO, cv, value, index);
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

// Keeps the array only if its type (index 0) is the expected one.
uint16_t checked(uint16_t cv, uint8_t type)
{
  uint8_t t;
  return (cv != 0 && readByte(cv, 0, &t) && t == type) ? cv : 0;
}

void findSettings()
{
  const uint16_t driving = findBlock(BLOCK_DRIVING);
  const uint16_t sound = findBlock(BLOCK_SOUND);
  settings[0].cv = sound ? checked(sound + 1, 0x10) : 0;
  settings[1].cv = driving ? checked(driving + 3, 0x17) : 0;
  settings[2].cv = driving ? checked(driving + 2, 0x13) : 0;
  settings[3].cv = settings[2].cv;
}

void printValue(const Setting &s, uint8_t value)
{
  Serial.print(value);
  if (s.key == 'a' || s.key == 'b')
  {
    Serial.print(F("  ("));
    Serial.print(value * 0.25, 2);
    Serial.print(F(" s)"));
  }
  else if (s.key == 'v')
  {
    Serial.print(F(" / 255  ("));
    Serial.print((value * 100 + 127) / 255);
    Serial.print(F(" %)"));
  }
  Serial.println();
}

void printSetting(const Setting &s)
{
  Serial.print(s.name);
  Serial.print(F(" : "));
  uint8_t value;
  if (s.cv == 0)
    Serial.println(F("not found"));
  else if (readByte(s.cv, s.index, &value))
    printValue(s, value);
  else
    Serial.println(F("no answer"));
}

void printAll()
{
  for (const Setting &s : settings)
    printSetting(s);
}

void writeSetting(Setting &s, long value)
{
  if (value < 0 || value > 255)
  {
    Serial.println(F("Refused: value 0..255"));
    return;
  }
  if (s.cv == 0)
  {
    Serial.print(s.name);
    Serial.println(F(" : not found, nothing written"));
    return;
  }
  uint8_t result = 0;
  if (!ctrl.writeConfig(LOCO, s.cv, (uint8_t)value, s.index, true, &result))
  {
    Serial.println(F("No answer: nothing written?"));
    return;
  }
  if (DEBUG || (result & 0xC0) != 0xC0)
  {
    Serial.print(F("  result 0x")); // 0x80 written, 0x40 verified
    Serial.println(result, HEX);
  }
  printSetting(s); // read again
}

void printHelp()
{
  Serial.println(F("l = list, v 0..255 = volume, m 0..255 = max. speed,"));
  Serial.println(F("a 0..255 = acceleration, b 0..255 = braking (x 0.25 s)"));
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

  Serial.print(F("MFX decoder SID "));
  Serial.print(SID);
  Serial.println(F(": looking for the settings..."));
  findSettings();
  printAll();
  printHelp();
}

void loop()
{
  static char line[20];
  static uint8_t len = 0;
  while (Serial.available())
  {
    const char c = Serial.read();
    if (c != '\r' && c != '\n')
    {
      if (len < sizeof(line) - 1)
        line[len++] = c;
      continue;
    }
    if (len == 0)
      continue; // "Both NL & CR": the second end of line
    line[len] = 0;
    len = 0;

    const char key = line[0];
    if (key == 'l')
    {
      printAll();
      continue;
    }
    bool done = false;
    for (Setting &s : settings)
      if (s.key == key)
      {
        char *end;
        const long value = strtol(line + 1, &end, 10);
        if (end == line + 1)
          Serial.println(F("Refused: a value is missing, e.g. v 180"));
        else
          writeSetting(s, value);
        done = true;
      }
    if (!done)
      printHelp();
  }
}
