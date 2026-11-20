/*
 * Railuino - S88-to-CAN-Bridge
 *
 * Reads physical S88 feedback modules wired to the board (via TrackReporterS88)
 * and broadcasts every contact state change onto the Märklin CAN bus as an
 * official S88 Event frame (command 0x11 / Marklin::Cmd::S88Event).
 * Also answers on-demand CAN S88 polling requests from a central unit or PC.
 *
 * Lit les modules de rétrosignalisation S88 câblés sur la carte (via TrackReporterS88)
 * et diffuse chaque changement d'état sur le bus CAN Märklin sous forme de trame
 * officielle S88 Event (commande 0x11 / Marklin::Cmd::S88Event).
 * Répond également aux requêtes CAN d'interrogation S88 à la demande.
 *
 * Automatically compatible with ESP32 (built-in TWAI controller)
 * and Arduino Uno / Mega (MCP2515 module over SPI bus).
 */

#if defined(ARDUINO_ARCH_AVR)
#include <ACAN2515.h> // Uno / Mega: lets the Arduino IDE find the MCP2515 library
#endif
#include <Railuino.h>

// ============================================================================
// HARDWARE CONFIGURATION BY ARCHITECTURE
// CONFIGURATION MATÉRIELLE SELON L'ARCHITECTURE
// ============================================================================
#if defined(ARDUINO_ARCH_ESP32)
// CAN pins of the ESP32 (built-in TWAI controller)
// Broches CAN de l'ESP32 (contrôleur TWAI intégré)
constexpr uint8_t CAN_RX = 22;
constexpr uint8_t CAN_TX = 23;

// S88 bus pins on ESP32 (avoid conflict with CAN_RX 22 and CAN_TX 23)
// Broches du bus S88 sur ESP32 (éviter tout conflit avec CAN_RX 22 et CAN_TX 23)
constexpr uint8_t S88_DATA  = 33;
constexpr uint8_t S88_CLOCK = 25;
constexpr uint8_t S88_LOAD  = 26;
constexpr uint8_t S88_RESET = 27;

#elif defined(ARDUINO_ARCH_AVR)
// Uno / Mega: MCP2515 module on the SPI bus (CS 10, INT 2).
// Check the frequency written on its quartz: 8000000UL (8 MHz) or 16000000UL (16 MHz).
constexpr uint8_t  MCP2515_CS     = 10;
constexpr uint8_t  MCP2515_INT    = 2;
constexpr uint32_t MCP2515_QUARTZ = 8000000UL; // 16000000UL for 16 MHz
Mcp2515Driver can(MCP2515_CS, MCP2515_INT, MCP2515_QUARTZ);

// S88 bus pins on Uno / Mega (free digital pins outside SPI 10..13 and INT 2)
// Broches du bus S88 sur Uno / Mega (broches libres hors SPI 10..13 et INT 2)
constexpr uint8_t S88_DATA  = 4;
constexpr uint8_t S88_CLOCK = 5;
constexpr uint8_t S88_LOAD  = 6;
constexpr uint8_t S88_RESET = 7;

#else
#error "Unsupported architecture: please use an ESP32, Arduino Uno or Arduino Mega."
#endif

// ============================================================================
// S88 & CAN PARAMETERS / PARAMÈTRES S88 & CAN
// ============================================================================
constexpr uint8_t  S88_MODULES          = 1;      // Number of 16-bit S88 modules (1..32)
constexpr uint16_t S88_DEVICE_ID        = 0x0001; // Module Device ID reported in CAN frame
constexpr uint32_t S88_POLL_INTERVAL_MS = 20;     // S88 scan period in ms (50 Hz)

constexpr uint32_t SERIAL_BAUD          = 115200;
constexpr uint32_t EXCHANGE_TIMEOUT_MS  = 500;
constexpr uint16_t NODE_HASH            = 0xDF24; // or 0 for an auto-generated hash

#if defined(ARDUINO_ARCH_AVR)
TrackController ctrl(can, NODE_HASH, false, EXCHANGE_TIMEOUT_MS);
#else
TrackController ctrl(NODE_HASH, false, EXCHANGE_TIMEOUT_MS);
#endif

TrackReporterS88 reporter(S88_MODULES, S88_DATA, S88_CLOCK, S88_LOAD, S88_RESET);

// Previous state storage: 2 bytes (16 bits) per S88 module
// Mémorisation de l'état précédent : 2 octets (16 bits) par module S88
uint8_t prevBytes[S88_MODULES * 2] = { 0 };
uint32_t lastScanMs = 0;

// ============================================================================
// SEND ONE S88 EVENT FRAME (COMMAND 0x11) ON THE CAN BUS
// ENVOI D'UNE TRAME S88 EVENT (COMMANDE 0x11) SUR LE BUS CAN
// ============================================================================
void sendS88Event(uint16_t contact, uint8_t oldState, uint8_t newState, bool isResponse = false)
{
  // Märklin CAN S88 Event payload (8 bytes):
  // data[0..1] : Device ID (module identifier)
  // data[2..3] : Contact number (1..512)
  // data[4]    : Previous state (0 or 1)
  // data[5]    : New state (0 = free, 1 = occupied)
  // data[6..7] : Time elapsed since previous change (in 10 ms units, 0 if unused)
  TrackMessage m = TrackMessage::make(
      Marklin::Cmd::S88Event,
      (static_cast<uint32_t>(S88_DEVICE_ID) << 16) | contact,
      8);
  m.response = isResponse;
  m.data[4]  = oldState;
  m.data[5]  = newState;
  m.data[6]  = 0x00;
  m.data[7]  = 0x00;

  ctrl.sendMessage(m);

  // Optional debug trace on Serial (same CSV format as CAN-S88-sniffer)
  // Trace optionnelle sur le port Série (même format CSV que CAN-S88-sniffer)
  Serial.print(F("TX;S88;"));
  Serial.print(contact);
  Serial.print(';');
  Serial.println(newState);
}

// ============================================================================
// ANSWER ON-DEMAND CAN S88 POLLING REQUESTS FROM CENTRAL UNIT / PC
// RÉPONSE AUX REQUÊTES CAN D'INTERROGATION S88 DE LA CENTRALE / DU PC
// ============================================================================
void onCanMessage(const TrackMessage &m, void *)
{
  if (m.response)
    return; // Ignore replies from other devices / Ignorer les réponses d'autres modules

  // Case 1: Single-contact query (Command 0x11, DLC = 4)
  // Cas 1 : Requête sur un contact précis (Commande 0x11, DLC = 4)
  if (m.command == Marklin::Cmd::S88Event && m.length == 4)
  {
    const uint16_t reqDeviceId = (static_cast<uint16_t>(m.data[0]) << 8) | m.data[1];
    const uint16_t contact     = (static_cast<uint16_t>(m.data[2]) << 8) | m.data[3];
    const uint16_t maxContacts = static_cast<uint16_t>(S88_MODULES) * 16;

    // Answer if Device ID matches ours (or 0 = broadcast) and contact is in our range
    if ((reqDeviceId == S88_DEVICE_ID || reqDeviceId == 0) && contact >= 1 && contact <= maxContacts)
    {
      const uint8_t byteIdx = (contact - 1) / 8;
      const uint8_t bit     = (contact - 1) % 8;
      const uint8_t state   = (prevBytes[byteIdx] >> bit) & 0x01;

      sendS88Event(contact, state, state, true);
    }
  }
  // Case 2: 16-bit module polling query (Command 0x10 / S88 Polling, DLC = 5)
  // Cas 2 : Requête sur un module complet de 16 bits (Commande 0x10, DLC = 5)
  else if (m.command == 0x10 && m.length == 5)
  {
    const uint16_t reqDeviceId = (static_cast<uint16_t>(m.data[0]) << 8) | m.data[1];
    const uint8_t  moduleNum   = m.data[4]; // 1-based module index (1..32)

    if ((reqDeviceId == S88_DEVICE_ID || reqDeviceId == 0) && moduleNum >= 1 && moduleNum <= S88_MODULES)
    {
      const uint8_t baseByte = (moduleNum - 1) * 2;
      TrackMessage reply = TrackMessage::make(
          0x10,
          (static_cast<uint32_t>(S88_DEVICE_ID) << 16) | (static_cast<uint16_t>(moduleNum) << 8),
          7);
      reply.response = true;
      reply.data[4]  = moduleNum;
      reply.data[5]  = prevBytes[baseByte + 1]; // High byte (contacts 9..16)
      reply.data[6]  = prevBytes[baseByte];     // Low byte (contacts 1..8)

      ctrl.sendMessage(reply);
    }
  }
}

// ============================================================================
// SCAN S88 CHAIN AND DETECT CHANGED CONTACTS
// SCRUTATION DE LA CHAÎNE S88 ET DÉTECTION DES CHANGEMENTS D'ÉTAT
// ============================================================================
void scanS88()
{
  reporter.refresh(S88_MODULES);

  const uint8_t byteCount = S88_MODULES * 2;
  for (uint8_t byteIdx = 0; byteIdx < byteCount; byteIdx++)
  {
    const uint8_t currentByte = reporter.getByte(byteIdx);
    const uint8_t changedBits = currentByte ^ prevBytes[byteIdx];

    if (changedBits != 0)
    {
      for (uint8_t bit = 0; bit < 8; bit++)
      {
        if (changedBits & (1 << bit))
        {
          // Märklin S88 contacts are 1-based (1..512)
          const uint16_t contact  = static_cast<uint16_t>(byteIdx) * 8 + bit + 1;
          const uint8_t  oldState = (prevBytes[byteIdx] >> bit) & 0x01;
          const uint8_t  newState = (currentByte >> bit) & 0x01;

          sendS88Event(contact, oldState, newState, false);
        }
      }
      prevBytes[byteIdx] = currentByte;
    }
  }
}

// ============================================================================
// SETUP & LOOP
// ============================================================================
void setup()
{
  Serial.begin(SERIAL_BAUD);
  while (!Serial) { ; }

  // Register callback to answer incoming S88 polling queries
  // Enregistrement du callback pour répondre aux interrogations S88 entrantes
  ctrl.onMessage(onCanMessage);

#if defined(ARDUINO_ARCH_ESP32)
  const bool ok = ctrl.begin(CAN_RX, CAN_TX);
#else
  const bool ok = ctrl.begin();
#endif

  if (!ok)
  {
    Serial.println(F("ERR;CAN_INIT_FAILED"));
    while (true) { ; }
  }

  // Initial read so already-occupied tracks are reported at boot
  // Lecture initiale pour signaler dès le démarrage les cantons déjà occupés
  scanS88();

  Serial.println(F("READY;S88-to-CAN-Bridge"));
}

void loop()
{
  // Keep CAN stack responsive (handles pings, polling queries and bus traffic)
  // Maintient la pile CAN active (gère les pings, requêtes de polling et le trafic)
  ctrl.poll();

  // Non-blocking periodic scan of the S88 shift registers
  // Scrutation périodique non-bloquante des registres à décalage S88
  const uint32_t now = millis();
  if (now - lastScanMs >= S88_POLL_INTERVAL_MS)
  {
    lastScanMs = now;
    scanS88();
  }
}
