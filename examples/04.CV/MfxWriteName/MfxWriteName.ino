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
 * Writes the name of an MFX locomotive into its decoder.
 * Écrit le nom d'une locomotive MFX dans son décodeur.
 *
 * 1. Reads and prints the current name.
 * 2. Type the new name in the serial monitor (1 to 16 characters, no
 *    accents), then Enter. Line ending of the monitor: "Newline".
 * 3. Writes it byte by byte, then reads it again.
 *
 * 1. Lit et affiche le nom actuel.
 * 2. Tapez le nouveau nom dans le moniteur série (1 à 16 caractères, sans
 *    accents), puis Entrée. Fin de ligne du moniteur : « Nouvelle ligne ».
 * 3. L'écrit octet par octet, puis le relit.
 *
 * The name is the array of type 0x18 of block 01 (index 1..16, ended by
 * a 0 if shorter). The sketch looks for it in block 01 only and writes
 * nothing if it does not find it (Krauss, "Description of the mfx Rail
 * format", 6.4).
 * Le nom est le tableau de type 0x18 du bloc 01. Le sketch le cherche
 * dans le bloc 01 seulement, et n'écrit rien s'il ne le trouve pas.
 *
 * The MS2 keeps its own copy of the name: it shows the new one only
 * after the loco has been removed from its list and registered again.
 * La MS2 garde sa propre copie du nom : elle n'affiche le nouveau
 * qu'après avoir retiré la loco de sa liste et l'avoir réinscrite.
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

constexpr uint16_t CV_BLOCKS = 4;       // block table (type 0x17), index 1..
constexpr uint8_t TYPE_NAME = 0x18;     // block 01: name, index 1..16
constexpr uint8_t MAX_NAME_LENGTH = 16; // (NAME_MAX is taken on ESP32)

#if defined(ARDUINO_ARCH_AVR)
TrackController ctrl(can, 0, DEBUG);
#else
TrackController ctrl(0, DEBUG);
#endif

uint16_t nameCv = 0; // CV of the name array, found in setup()

bool readByte(uint16_t cv, uint8_t index, uint8_t *value)
{
  return ctrl.readConfig(LOCO, cv, value, index);
}

// CV of the array of type 'type' in block 01, 0 if not found. Block 01
// ends where the next block starts (block table, index 1, x 4). Other
// blocks reuse the same types (block 05 also has a type 0x18: the
// speedometer), so the search stays in block 01. CV 0 is not read: the
// Gleisbox may refuse it.
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

// Reads the name into 'name' (MAX_NAME_LENGTH + 1 bytes). false if no answer.
bool readName(char *name)
{
  uint8_t n = 0;
  for (uint8_t i = 1; i <= MAX_NAME_LENGTH; i++)
  {
    uint8_t c;
    if (!readByte(nameCv, i, &c))
      return false;
    if (c == 0)
      break;
    name[n++] = (char)c;
  }
  name[n] = 0;
  return true;
}

void printName(const __FlashStringHelper *label)
{
  char name[MAX_NAME_LENGTH + 1];
  Serial.print(label);
  if (readName(name))
  {
    Serial.print('"');
    Serial.print(name);
    Serial.println('"');
  }
  else
    Serial.println(F("no answer"));
}

// Writes one byte. The result byte of the Gleisbox (0x80 written,
// 0x40 verified) is printed if it is not 0xC0, or with DEBUG.
bool writeByte(uint8_t index, uint8_t value)
{
  uint8_t result = 0;
  if (!ctrl.writeConfig(LOCO, nameCv, value, index, true, &result))
  {
    Serial.print(F("  index "));
    Serial.print(index);
    Serial.println(F(": no answer"));
    return false;
  }
  if (DEBUG || (result & 0xC0) != 0xC0)
  {
    Serial.print(F("  index "));
    Serial.print(index);
    Serial.print(F(": result 0x"));
    Serial.println(result, HEX);
  }
  return true;
}

void writeName(const char *name)
{
  const uint8_t n = strlen(name);
  Serial.print(F("Writing \""));
  Serial.print(name);
  Serial.println(F("\"..."));
  for (uint8_t i = 0; i < n; i++)
    if (!writeByte(i + 1, (uint8_t)name[i]))
      return;
  if (n < MAX_NAME_LENGTH && !writeByte(n + 1, 0)) // end of the name
    return;
  printName(F("Name now     : "));
}

// Name typed by the user: 1..16 printable ASCII characters.
bool validName(const char *s)
{
  const size_t n = strlen(s);
  if (n == 0 || n > MAX_NAME_LENGTH)
    return false;
  for (size_t i = 0; i < n; i++)
    if (s[i] < 32 || s[i] > 126)
      return false;
  return true;
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
  Serial.println(SID);
  nameCv = findInBlock01(TYPE_NAME);
  if (nameCv == 0)
  {
    Serial.println(F("Name array not found: nothing will be written. Need to reset."));
    return;
  }
  Serial.print(F("Name array   : CV "));
  Serial.println(nameCv);
  printName(F("Current name : "));
  Serial.println(F("Type the new name (1..16 characters, no accents), then Enter:"));
}

void loop()
{
  if (nameCv == 0)
    return;

  static char line[40];
  static uint8_t len = 0;
  while (Serial.available())
  {
    const char c = Serial.read();
    if (c == '\r' || c == '\n')
    {
      if (len == 0)
        continue; // "Both NL & CR": the second end of line
      line[len] = 0;
      len = 0;
      if (validName(line))
        writeName(line);
      else
        Serial.println(F("Refused: 1 to 16 characters, letters, digits, spaces, no accents."));
      Serial.println(F("Type another name, or reset."));
    }
    else if (len < sizeof(line) - 1)
      line[len++] = c;
  }
}
