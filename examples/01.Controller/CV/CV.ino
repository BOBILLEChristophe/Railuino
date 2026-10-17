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
 * Reads the first configuration bytes of a locomotive decoder.
 * Lit les premiers octets de configuration d'un décodeur.
 *
 * - MFX loco: read on the main track. The numbers address the MFX
 *   configuration space of the decoder, not DCC CVs: the values are
 *   printed as they are.
 * - DCC loco: read only on a programming track, which the Gleisbox does
 *   not have. The names of the DCC CVs are printed.
 * - MM2 loco: cannot be read.
 *
 * - Loco MFX : lecture sur la voie principale. Les numéros désignent
 *   l'espace de configuration MFX du décodeur, pas des CV DCC : les
 *   valeurs sont affichées telles quelles.
 * - Loco DCC : lecture seulement sur une voie de programmation, que la
 *   Gleisbox n'a pas. Les noms des CV DCC sont affichés.
 * - Loco MM2 : lecture impossible.
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

// DCC locomotive: ADDR_DCC + address.
// Loco DCC : ADDR_DCC + adresse.
constexpr uint16_t LOCO = ADDR_MFX + SID;

constexpr uint8_t COUNT = 8; // bytes / CVs read: 1..COUNT

constexpr bool DEBUG = false;

#if defined(ARDUINO_ARCH_AVR)
TrackController ctrl(can, 0, DEBUG);
#else
TrackController ctrl(0, DEBUG);
#endif

// Names of the first DCC CVs (NMRA). Not valid for an MFX decoder.
const char *const DCC_NAMES[] = {
    "Address", "Min. voltage", "Accel. time", "Decel. time",
    "Max. speed", "Mid. speed", "Version", "Manufacturer"};

bool isDcc(uint16_t loco) { return loco >= ADDR_DCC; }
bool isMfx(uint16_t loco) { return loco >= ADDR_MFX && loco < ADDR_DCC; }

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

  if (!isMfx(LOCO) && !isDcc(LOCO))
  {
    Serial.println(F("MM2 decoders cannot be read. Need to reset."));
    return;
  }
  if (isDcc(LOCO))
    Serial.println(F("DCC: needs a programming track (not with a Gleisbox)."));

  for (uint8_t number = 1; number <= COUNT; number++)
  {
    uint8_t value = 0;
    if (isMfx(LOCO))
    {
      Serial.print(F("MFX config "));
      Serial.print(number);
    }
    else
    {
      Serial.print(F("CV "));
      Serial.print(number);
      if (number <= sizeof(DCC_NAMES) / sizeof(DCC_NAMES[0]))
      {
        Serial.print(F(" ("));
        Serial.print(DCC_NAMES[number - 1]);
        Serial.print(F(")"));
      }
    }
    Serial.print(F(" : "));
    if (ctrl.readConfig(LOCO, number, &value))
    {
      Serial.print(value);
      Serial.print(F(" (0x"));
      Serial.print(value, HEX);
      Serial.println(F(")"));
    }
    else
      Serial.println(F("no answer"));
  }

  Serial.println(F("System stopped. Need to reset."));
}

void loop()
{
}
