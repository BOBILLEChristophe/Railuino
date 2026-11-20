/*
 * Railuino - CAN-S88-sniffer
 *
 * Copyright (C) 2026 christophe bobille
 *
 * Écoute les événements de rétrosignalisation S88 (commande 0x11) circulant
 * sur le bus CAN Märklin et les transmet sur le port Série (USB) à 115200 bauds
 * sous forme CSV : S88;<contact>;<nouvel_etat>
 *
 * Compatible automatiquement avec ESP32 (contrôleur TWAI intégré)
 * et Arduino Uno / Mega (module MCP2515 sur bus SPI).
 *
 * Listens to S88 feedback events (command 0x11) circulating
 * on the Märklin CAN bus and forwards them over the Serial port (USB) at 115200 baud
 * in CSV format: S88;<contact>;<new_state>
 *
 * Automatically compatible with ESP32 (built-in TWAI controller)
 * and Arduino Uno / Mega (MCP2515 module over SPI bus).

 */

#if defined(ARDUINO_ARCH_AVR)
#include <ACAN2515.h> // Uno / Mega : permet à l'IDE Arduino de trouver la bibliothèque MCP2515
#endif
#include <Railuino.h>

// ============================================================================
// CONFIGURATION MATÉRIELLE SELON L'ARCHITECTURE
// ============================================================================
#if defined(ARDUINO_ARCH_ESP32)
// Broches CAN de l'ESP32 (contrôleur TWAI intégré)
constexpr uint8_t CAN_RX = 22;
constexpr uint8_t CAN_TX = 23;
#elif defined(ARDUINO_ARCH_AVR)
// Uno / Mega : module MCP2515 sur le bus SPI (CS 10, INT 2).
// Vérifiez la fréquence inscrite sur le quartz : 8000000UL (8 MHz) ou 16000000UL (16 MHz).
constexpr uint8_t  MCP2515_CS     = 10;
constexpr uint8_t  MCP2515_INT    = 2;
constexpr uint32_t MCP2515_QUARTZ = 8000000UL; // 16000000UL pour un quartz 16 MHz
Mcp2515Driver can(MCP2515_CS, MCP2515_INT, MCP2515_QUARTZ);
#else
#error "Architecture non supportée : utilisez un ESP32, Arduino Uno ou Arduino Mega."
#endif

constexpr uint32_t SERIAL_BAUD         = 115200;
constexpr uint32_t EXCHANGE_TIMEOUT_MS = 500;
constexpr uint16_t NODE_HASH           = 0xDF24; // ou 0 pour un hash généré automatiquement

#if defined(ARDUINO_ARCH_AVR)
TrackController ctrl(can, NODE_HASH, false, EXCHANGE_TIMEOUT_MS);
#else
TrackController ctrl(NODE_HASH, false, EXCHANGE_TIMEOUT_MS);
#endif

// ============================================================================
// CALLBACK DE RÉCEPTION DES TRAMES CAN
// ============================================================================
void onCanMessage(const TrackMessage &m, void *)
{
  // Commande 0x11 = Marklin::Cmd::S88Event (événement de rétrosignalisation S88)
  if (m.command == Marklin::Cmd::S88Event && m.length == 8)
  {
    // Extraction des données selon la spécification CAN Märklin :
    // m.data[0..1] : Device ID (identifiant du module)
    // m.data[2..3] : Numéro du contact (1 à 512)
    // m.data[4]    : Ancien état (0 ou 1)
    // m.data[5]    : Nouvel état (0 = libre, 1 = occupé)
    const uint16_t device  = (static_cast<uint16_t>(m.data[0]) << 8) | m.data[1];
    const uint16_t contact  = (static_cast<uint16_t>(m.data[2]) << 8) | m.data[3];
    const uint8_t  oldState = m.data[4];
    const uint8_t  newState = m.data[5];
    (void)oldState; // Évite un avertissement de compilation si non affiché

    // Envoi vers l'application extérieure sur le port Série (format CSV simple)
    Serial.print(F("S88 : "));
    Serial.print(F("Device : "));
    Serial.print(device);
    Serial.print(F("Contact : "));
    Serial.print(contact);
    Serial.print(';');
    Serial.println(newState);
  }
}


// ============================================================================
// SETUP & LOOP
// ============================================================================
void setup()
{
  Serial.begin(SERIAL_BAUD);
  while (!Serial) { ; }

  // Enregistrement de la fonction callback avant le démarrage du bus
  ctrl.onMessage(onCanMessage);

  // Démarrage du contrôleur CAN (spécifique ESP32 TWAI vs AVR MCP2515)
#if defined(ARDUINO_ARCH_ESP32)
  const bool ok = ctrl.begin(CAN_RX, CAN_TX);
#else
  const bool ok = ctrl.begin();
#endif

  if (!ok)
  {
    Serial.println(F("ERR;CAN_INIT_FAILED"));
    while (true) { ; } // Blocage en cas d'erreur de câblage ou de quartz MCP2515
  }

  Serial.println(F("READY;CAN-S88-sniffer"));
}

void loop()
{
  // Indispensable : lit la file de réception CAN et déclenche onCanMessage()
  ctrl.poll();
}
