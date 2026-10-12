

/*********************************************************************
 * Railuino - Hacking your Märklin
 *
 * Copyright (C) 2012 Joerg Pleumann
 * Copyright (C) 2024 – 2026 christophe bobille
 * Copyright (C) 2026 Uwe Monreal
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * LICENSE file for more details.
 */

#ifndef TRACKCONTROLLER_H
#define TRACKCONTROLLER_H

#include <Arduino.h>
#include "TrackMessage.h"
#include "Config.h"
#include "CanDriver.h"
#include "MarklinProtocol.h"

class MfxRegistry;

/**
 * Function called for every message received from the bus that is not
 * the answer to one of our own requests: commands sent by an MS2 or a
 * CS2, feedback events, pings... 'context' is the pointer given to
 * TrackController::onMessage(). Must not call TrackController methods
 * that wait for an answer (it is called from inside them).
 */
typedef void (*TrackMessageHandler)(const TrackMessage &message, void *context);

// ===================================================================
// === TrackController ===============================================
// ===================================================================

/**
 * Controls things on and connected to the track: locomotives,
 * turnouts and other accessories. While there are some low-level
 * methods for dealing with messages, you will normally want to use
 * the high-level methods that wrap most of the the nasty protocol
 * details. When addressing something, you have to tell the system the
 * type of address (or decoder) you are using by adding the proper
 * protocol base address. For instance, DCC locomotive 42 is properly
 * addressed as ADDR_DCC + 42.
 */
class TrackController
{

private:
  /**
   * The CAN controller used to talk to the layout.
   */
  CanDriver *mCan;

  /**
   * Stores the hash of our controller. This must not conflict with
   * hashes of other devices in the setup (most likely the MS2 and
   * the connector box).
   */
  uint16_t mHash;
  /**
   * Where traces go (all messages in debug mode, and errors). nullptr:
   * no output. Set to &Serial when debug is true, or with setLog().
   */
  Print *mLog;
  /**
   * Holds the loopback flag. When loopback is on, messages are
   * reflected by the CAN controller. No external communication
   * takes place. This is helpful for some test cases.
   */
  bool mLoopback;

  /**
   * Timeout (in ms) used by exchangeMessage() for ordinary commands.
   * uint32_t matches the type returned by millis().
   */
  uint32_t mTimeout;

  /**
   * Called for every received message that is not an awaited answer.
   */
  TrackMessageHandler mHandler;
  void *mHandlerContext;

  /**
   * MFX table fed with the received messages (nullptr: none).
   */
  MfxRegistry *mMfx;

  /**
   * Whether setPower(true) sends the MFX re-registration counter.
   */
  bool mMfxCounterOnPowerOn;

  /**
   * Non-blocking accessories (setAccessoryAsync): switch-off messages
   * waiting to be sent by poll().
   */
#if defined(ARDUINO_ARCH_AVR)
  static const uint8_t MAX_PENDING_ACC = 4;
#else
  static const uint8_t MAX_PENDING_ACC = 8;
#endif
  struct PendingAccessory
  {
    uint32_t address;
    uint32_t dueMs;
    uint8_t position;
    bool used;
  };
  PendingAccessory mPendingAcc[MAX_PENDING_ACC];
  bool mAccAsync;

  bool switchAccessoryOff(const uint32_t address, uint8_t position);
  void serviceAccessories(bool all);

  void logFrame(const char *direction, const CanFrame &frame);
  void dispatch(const TrackMessage &message);

public:
  /**
   * Default timeout (in ms) for ordinary commands.
   */
  static constexpr uint32_t DEFAULT_TIMEOUT = 1000;

  /**
   * Timeout (in ms) for reading and writing decoder CVs, which can
   * take several seconds on the programming track.
   */
  static constexpr uint32_t CV_TIMEOUT = 10000;

  /**
   * Creates a new TrackController with default values.
   */
  TrackController();

  /**
   * Creates a new TrackController with the given hash, debugging flag,
   * exchangeMessage timeout (in ms) and loopback flag. A zero hash
   * will result in a unique hash being generated.
   *
   * The first parameter is ALWAYS the hash, as in Railuino 0.9:
   *   TrackController ctrl(0xDF24);              // hash
   *   TrackController ctrl(0xDF24, true);        // hash, debug
   *   TrackController ctrl(0xDF24, true, 500);   // hash, debug, timeout
   *
   * Versions 0.95/0.951 also had TrackController(timeOut) and
   * TrackController(timeOut, debug). They were removed because
   * TrackController(0xDF24) silently selected the timeout constructor.
   * Use setTimeout() to change only the timeout.
   */
  explicit TrackController(uint16_t hash, bool debug = false,
                           uint32_t timeOut = DEFAULT_TIMEOUT,
                           bool loopback = false);

  /**
   * Same, with an explicit CAN driver, for instance an MCP2515 on an
   * ESP32 or a driver of your own:
   *   Mcp2515Driver can(5, 4, 8000000UL);  // CS, INT, quartz
   *   TrackController ctrl(can, 0xDF24);
   * Without a driver, TrackController uses the ESP32 TWAI controller
   * on ESP32 and an MCP2515 (RAILUINO_MCP2515_*) on AVR.
   */
  explicit TrackController(CanDriver &can, uint16_t hash = 0, bool debug = false,
                           uint32_t timeOut = DEFAULT_TIMEOUT,
                           bool loopback = false);
  /**
   * Is called when a TrackController is being destroyed. Does the
   * necessary cleanup. No need to call this manually.
   */
  ~TrackController();

  /**
   * Initializes the TrackController with the given values. This
   * should be called before begin, otherwise it will not take
   * effect. A zero hash will result in a unique hash begin
   * generated.
   */
  void init(uint16_t hash, bool debug, bool loopback, uint32_t timeOut = DEFAULT_TIMEOUT);

  /**
   * Changes the timeout (in ms) used by exchangeMessage() for
   * ordinary commands.
   */
  void setTimeout(uint32_t timeOut);

  /**
   * Queries the timeout (in ms) used for ordinary commands.
   */
  uint32_t getTimeout() const;

  /**
   * Queries the hash used by the TrackController.
   */
  uint16_t getHash() const;

  /**
   * Reflects whether the TrackController is in debug mode,
   * where all messages are dumped to the log (Serial by default).
   */
  bool isDebug() const;

  /**
   * Sends the traces (all messages and errors) to 'log' instead of
   * Serial, e.g. Serial1 or a TCP client; nullptr disables them.
   * Same as debug mode on, but with another output.
   */
  void setLog(Print *log);
  Print *getLog() const;

  /**
   * Registers a function called for every received message that is not
   * the answer to one of our requests (MS2 commands, feedback events...).
   * Messages are processed by poll() and while waiting for answers.
   *   void onCan(const TrackMessage &m, void *) { Serial.println(m); }
   *   ctrl.onMessage(onCan);
   */
  void onMessage(TrackMessageHandler handler, void *context = nullptr);

  /**
   * Reads every message waiting in the CAN driver and passes it to the
   * onMessage() handler, and sends the accessory switch-offs that are
   * due (see setAccessoryAsync()). Call it often from loop().
   */
  void poll();

  /**
   * Non-blocking accessories. false (default): setAccessory() and
   * setTurnout() wait for the end of the pulse (delay), as before.
   * true: they switch the coil on and return at once; the switch-off is
   * sent by poll() when the time is over, so loop() keeps running (web
   * server, other commands) during the pulse. poll() must then be called
   * often, or the coil stays on. Switching an accessory again while its
   * pulse runs switches the previous position off first. If more than
   * 8 pulses (4 on AVR) run at the same time, the call blocks as before.
   */
  void setAccessoryAsync(bool on);
  bool isAccessoryAsync() const { return mAccAsync; }

  /**
   * Number of accessory switch-offs waiting to be sent by poll().
   */
  uint8_t pendingAccessories() const;

  /**
   * Sends all the waiting switch-offs now (e.g. before end() or a
   * long blocking operation).
   */
  void flushAccessories();

  /**
   * Called by MfxRegistry::begin(): the registry then receives every
   * message, and poll() also runs its state machine.
   */
  void setMfxRegistry(MfxRegistry *registry);

  /**
   * Since 0.13, setPower(true) no longer sends the MFX re-registration
   * counter (value 3), which made the locomotives register again and
   * could make an MS2 give them new SIDs. true restores the old
   * behaviour.
   */
  void setMfxCounterOnPowerOn(bool on);

  /**
   * Reflects whether the TrackController is in loopback mode,
   * where all messages are reflected by the CAN controller.
   */
  bool isLoopback() const;

  /**
   * Initializes the CAN hardware and starts receiving CAN
   * messages. CAN messages are put into an internal buffer of
   * limited size, so they don't get lost, but you have to take
   * care of them in time. Otherwise the buffer might overflow.
   * The pins are those of the ESP32 TWAI controller; they are ignored
   * by the MCP2515 driver (SPI bus, see RAILUINO_MCP2515_* in
   * Mcp2515Driver.h).
   * Returns false if the CAN controller could not be configured or the
   * Gleisbox wake-up message could not be sent.
   */
  bool begin(const uint8_t can_rx_pin, const uint8_t can_tx_pin);

  /**
   * Same, keeping the pins of the driver (CAN_RX_PIN / CAN_TX_PIN from
   * Config.h for the default ESP32 driver).
   */
  bool begin();

  /**
   * Generates a new hash and makes sure it does not conflict
   * with those of other devices in the setup.
   */
  void generateHash();

  /**
   * Stops receiving messages from the CAN hardware. Clears
   * the internal buffer.
   */
  void end();

  /**
   * Starts an MFX discovery. Needs an MfxRegistry in master mode
   * (see MfxRegistry.h); returns false otherwise.
   */
  bool locoDiscovery();

  /**
   * Sends a message and reports true on success. Internal method.
   * Normally you don't want to use this, but the more convenient
   * methods below instead.
   */
  bool sendMessage(TrackMessage &message);

  /**
   * Receives an arbitrary message, if available, and reports true
   * on success. Does not block. Internal method. Normally you
   * don't want to use this, but the more convenient methods below
   * instead.
   */
  bool receiveMessage(TrackMessage &message);

  /**
   * Sends a message and waits for the corresponding response,
   * returning true on success. Blocks until either a message with
   * the same command ID and the response marker arrives or the
   * timeout (in ms) expires. All non-matching messages are
   * skipped. Internal method. Normally you don't want to use this,
   * but the more convenient methods below instead. 'out' and 'in'
   * may be the same object.
   */
  bool exchangeMessage(TrackMessage &out, TrackMessage &in, uint32_t timeout);

  /**
   * Tells whether 'in' is the response to the request 'out': same
   * command, response bit set and, for commands addressed to a
   * device (locomotive, accessory, system sub-command...), same UID
   * in data[0..3] and same sub-command in data[4].
   */
  static bool isResponseTo(const TrackMessage &out, const TrackMessage &in);

  /**
   * Controls power on the track. When passing false, all
   * locomotives will stop, but remember their previous directions
   * and speeds. When passing true, all locomotives will regain
   * their old directions and speeds. The system starts in
   * stopped mode in order to avoid accidents. The return value
   * reflects whether the call was successful.
   */
  bool setPower(bool power);

  /**
   *  Cette commande qui n'existait pas a l'origine a ete ajoutee
   *  a partir de la v-9.01
   *  Commande système (0x00, dans CAN-ID : 0x00)
   *  Arrêt d'urgence de la locomotive (0x02)
   *  Toutes les locomotives reçoivent l'ordre de s'arrêter (vitesse 0),
   *  y compris l'inertie de freinage. Le signal numérique reste sur les rails,
   *  mais aucune autre commande n'est envoyée sur les rails.
   *  L'énergie électrique reste disponible.
   */
  // See https://streaming.maerklin.de/public-media/cs2/cs2CAN-Protokoll-2_0.pdf -> 2.3 Commande : System Halt
  bool systemHalt(const uint32_t address);

  /**
   *  Cette commande qui n'existait pas a l'origine a ete ajoutee
   *  a partir de la v-9.01
   *  Commande système (0x00, dans CAN-ID : 0x00)
   *  Arrêt d'urgence de la locomotive (0x03)
   *  Arrêt d'urgence ou arrêt immédiat de la locomotive, selon le protocole de voie.
   *  Il faut spécifier une locomotive déjà ciblée par une commande.
   *  Si cette locomotive n'est pas déjà dans le cycle, elle ne sera pas prise en compte par cette commande.
   */

  /**
   * This command, which didn't exist originally, has been added from v-9.01
   * System command (0x00, in CAN-ID: 0x00)
   * Locomotive emergency stop (0x03)
   * Emergency stop or immediate stop of locomotive, depending on track protocol.
   * A locomotive already targeted by a command must be specified.
   * If this locomotive is not already in the cycle, it will not be taken into account by this command.
   */
  // See https://streaming.maerklin.de/public-media/cs2/cs2CAN-Protokoll-2_0.pdf -> 2.4 Commande : Arrêt d'urgence de la locomotive
  bool emergency(const uint32_t address);

  /**
   * Sets the direction of the given locomotive. Valid directions
   * are those specified by the DIR_* constants. The return value
   * reflects whether the call was successful.
   */
  bool setLocoDirection(const uint32_t address, uint8_t direction);
  bool setLocoDirection(const uint32_t address, Marklin::Direction direction);

  /**
   * Toggles the direction of the given locomotive. This normally
   * includes a full stop.
   */
  bool toggleLocoDirection(const uint32_t address);

  /**
   * Sets the speed of the given locomotive. Valid speeds are
   * 0 to 1023 (inclusive), though the connector box will limit
   * all speeds beyond 1000 to 1000. The return value reflects
   * whether the call was successful.
   */
  bool setLocoSpeed(const uint32_t address, uint16_t speed);
  // bool accelerateLoco(uint16_t address);
  // bool decelerateLoco(uint16_t address);

  /**
   * Sets the given function of the given locomotive (or simply a
   * function decoder). Valid functions are 0 to 31, with 0
   * normally denoting the head/backlight. Valid values are, again,
   * 0 ("off") to 31, although not all protocols support values
   * beyond 1 (which then means "on").  The return value reflects
   * whether the call was successful.
   */
  bool setLocoFunction(const uint32_t address, uint8_t function, uint8_t power);

  /**
   * Toggles the given function of the given locomotive. Valid
   * functions are 0 to 31, with 0 normally denoting the
   * head/backlight.
   */
  bool toggleLocoFunction(const uint32_t address, uint8_t function);

  /**
   * Switches the given magnetic accessory. Valid position values
   * are those denoted by the ACC_* constants. Valid power values
   * are 0 ("off") to 31 (inclusive) although not all protocols
   * support values beyond 1 (which then means "on"). The final
   * parameter specifies the time (in ms) for which the accessory
   * will be active. A time of 0 means the accessory will only be
   * switched on. Some magnetic accessories must not be active for
   * too long, because they might burn out. A good timeout for
   * Marklin turnouts seems to be 20 ms. The return value reflects
   * whether the call was successful. With setAccessoryAsync(true) the
   * call returns after switching on, and poll() switches off; the return
   * value then only covers the switching on.
   */
  bool setAccessory(const uint32_t address, uint8_t position, uint8_t power, uint16_t time);

  /**
   * Switches a turnout. This is actually a convenience function
   * around setAccessory() that uses default values for some
   * parameters. The return value reflects whether the call was
   * successful.
   */
  bool setTurnout(const uint32_t address, bool straight);

  /**
   * Queries the direction of the given locomotive and writes it
   * into the referenced uint8_t. The return value indicates whether
   * the call was successful and the direction is valid.
   */
  bool getLocoDirection(const uint32_t address, uint8_t *direction);

  /**
   * Queries the speed of the given locomotive and writes it
   * into the referenced uint8_t. The return value indicates whether
   * the call was successful and the speed is valid.
   */

  bool getLocoSpeed(const uint32_t address, uint16_t *speed);

  /**
   * Queries the given function of the given locomotive and writes
   * it into the referenced uint8_t. The return value indicates
   * whether the call was successful and the power is valid. Note
   * that the call will not reflect the original power value sent
   * to the function, but only 0 ("off") or 1 ("on").
   */
  bool getLocoFunction(const uint32_t address, uint8_t function, uint8_t *power);

  /**
   * Queries the given magnetic accessory's state and and writes
   * it into the referenced bytes. The return value indicates
   * whether the call was successful and the bytes are valid. Note
   * that the call will not reflect the original power value sent
   * to the function, but only 0 ("off") or 1 ("on").
   */
  bool getAccessory(const uint32_t address, uint8_t *position, uint8_t *power);

  /**
   * Writes the given value to the given config number of the given
   * locomotive.  The return value reflects whether the call was
   * successful.
   */
  /* -------------------------------------------------------------------
     TrackController::writeConfig

     See https://streaming.maerklin.de/public-media/cs2/cs2CAN-Protokoll-2_0.pdf -> 3.8 Commande : Écrire Config

  -------------------------------------------------------------------  */
  bool writeConfig(const uint32_t address, uint16_t number, uint8_t value);

  /**
   * Reads the given config number of the given locomotive into the
   * given value.
   */

  //!\\ Les décodeurs MFX de première génération ne sont pas conçus pour cela et pourraient être endommagés
  //!\\ First-generation MFX decoders are not designed for this purpose and could be damaged.

  // Command 0x07 "Read Config" of the CS2 CAN protocol
  // (https://streaming.maerklin.de/public-media/cs2/cs2CAN-Protokoll-2_0.pdf).
  // Uses CV_TIMEOUT (10 s).

  bool readConfig(const uint32_t address, uint16_t number, uint8_t *value);

  /**
   * Queries the software version of the track format processor
   * (Gleisbox, device type 0x0010 or 0x0011). Answers from other devices (MS2...)
   * are ignored. high and low may be nullptr.
   */
  bool getVersion(uint8_t *high, uint8_t *low);

  /**
   * Processes one text command received on the serial or TCP port.
   * Kept for compatibility: see CommandParser for the syntax, the
   * error messages and more commands.
   */
  void handleUserCommands(String command);
};

#endif // TRACKCONTROLLER_H
