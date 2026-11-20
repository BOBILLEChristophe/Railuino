/*
 * Railuino - S88-Local-Monitor
 *
 * Copyright (C) 2026 christophe bobille
 *
 * Reads physical S88 feedback modules wired directly to the board
 * (without any CAN bus interface) and prints their 16-bit binary state
 * to the Serial Monitor whenever a contact changes.
 *
 * Lit des modules de rétrosignalisation S88 câblés directement sur la carte
 * (sans interface bus CAN) et affiche leur état binaire sur 16 bits dans le
 * moniteur série dès qu'un contact change d'état.
 *
 * Automatically compatible with ESP32 (ARDUINO_ARCH_ESP32)
 * and Arduino Uno / Mega (ARDUINO_ARCH_AVR).
 */

#include <Railuino.h>

// ============================================================================
// HARDWARE CONFIGURATION BY ARCHITECTURE
// CONFIGURATION MATÉRIELLE SELON L'ARCHITECTURE
// ============================================================================
#if defined(ARDUINO_ARCH_ESP32)
constexpr uint8_t S88_DATA  = 33;
constexpr uint8_t S88_CLOCK = 25;
constexpr uint8_t S88_LOAD  = 26;
constexpr uint8_t S88_RESET = 27;
#elif defined(ARDUINO_ARCH_AVR)
constexpr uint8_t S88_DATA  = 4;
constexpr uint8_t S88_CLOCK = 5;
constexpr uint8_t S88_LOAD  = 6;
constexpr uint8_t S88_RESET = 7;
#else
#error "Unsupported architecture: please use an ESP32, Arduino Uno or Arduino Mega."
#endif

// ============================================================================
// S88 PARAMETERS / PARAMÈTRES S88
// ============================================================================
constexpr uint8_t  S88_MODULES          = 2;      // Number of 16-bit S88 modules (1..32)
constexpr uint32_t S88_POLL_INTERVAL_MS = 20;     // Scan period in ms (50 Hz)
constexpr uint32_t SERIAL_BAUD          = 115200;

TrackReporterS88 reporter(S88_MODULES, S88_DATA, S88_CLOCK, S88_LOAD, S88_RESET);

uint8_t  prevBytes[S88_MODULES * 2] = { 0 };
uint32_t lastScanMs = 0;

// ============================================================================
// PRINT ALL MODULES IN GROUPED 16-BIT BINARY (e.g. "Mod 1: 0010 0000 1000 0000")
// AFFICHAGE BINAIRE GROUPÉ SUR 16 BITS DE TOUS LES MODULES
// ============================================================================
void printModulesState()
{
  for (uint8_t mod = 0; mod < S88_MODULES; mod++)
  {
    const uint8_t lowByte  = reporter.getByte(mod * 2);     // Contacts 1..8
    const uint8_t highByte = reporter.getByte(mod * 2 + 1); // Contacts 9..16
    const uint16_t word    = (static_cast<uint16_t>(highByte) << 8) | lowByte;

    Serial.print(F("Mod "));
    Serial.print(mod + 1);
    Serial.print(F(": "));

    // Print contacts 1 to 16 from left to right, grouped by 4
    // Affiche les contacts 1 à 16 de gauche à droite, groupés par 4
    for (uint8_t bit = 0; bit < 16; bit++)
    {
      if (bit > 0 && (bit % 4) == 0)
        Serial.print(' ');
      Serial.print((word >> bit) & 0x01);
    }
    Serial.print(F("   "));
  }
  Serial.println();
}

// ============================================================================
// SCAN S88 CHAIN AND REPORT CHANGES
// SCRUTATION DE LA CHAÎNE S88 ET AFFICHAGE DES CHANGEMENTS
// ============================================================================
void scanS88(bool forcePrint = false)
{
  reporter.refresh(S88_MODULES);

  bool anyChange = forcePrint;
  const uint8_t byteCount = S88_MODULES * 2;

  for (uint8_t byteIdx = 0; byteIdx < byteCount; byteIdx++)
  {
    const uint8_t currentByte = reporter.getByte(byteIdx);
    const uint8_t changedBits = currentByte ^ prevBytes[byteIdx];

    if (changedBits != 0)
    {
      anyChange = true;
      for (uint8_t bit = 0; bit < 8; bit++)
      {
        if (changedBits & (1 << bit))
        {
          const uint16_t contact  = static_cast<uint16_t>(byteIdx) * 8 + bit + 1;
          const uint8_t  newState = (currentByte >> bit) & 0x01;

          Serial.print(F("Contact "));
          Serial.print(contact);
          Serial.println(newState ? F(" -> OCCUPIED (1)") : F(" -> FREE (0)"));
        }
      }
      prevBytes[byteIdx] = currentByte;
    }
  }

  if (anyChange)
  {
    printModulesState();
  }
}

// ============================================================================
// SETUP & LOOP
// ============================================================================
void setup()
{
  Serial.begin(SERIAL_BAUD);
  while (!Serial) { ; }

  Serial.println(F("READY;S88-Local-Monitor"));
  scanS88(true); // Initial snapshot / État initial au démarrage
}

void loop()
{
  const uint32_t now = millis();
  if (now - lastScanMs >= S88_POLL_INTERVAL_MS)
  {
    lastScanMs = now;
    scanS88(false);
  }
}
