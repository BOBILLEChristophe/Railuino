

/*********************************************************************
 * Railuino - Hacking your Märklin
 *
 * Copyright (C) 2012 Joerg Pleumann
 * Copyright (C) 2024 christophe bobille
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

 #ifndef TRACKCONTROLLER_H
 #define TRACKCONTROLLER_H
 
 #include <Arduino.h>
 #include "TrackMessage.h"
 #include "Config.h"
 
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
    * Stores the hash of our controller. This must not conflict with
    * hashes of other devices in the setup (most likely the MS2 and
    * the connector box).
    */
   uint16_t mHash;
   /**
    * Stores the debug flag. When debugging is on, all outgoing and
    * incoming messages are printed to the Serial console.
    */
   bool mDebug;
   /**
    * Holds the loopback flag. When loopback is on, messages are
    * reflected by the CAN controller. No external communication
    * takes place. This is helpful for some test cases.
    */
   bool mLoopback;
 
   uint64_t mTimeout;
 
 public:
   /**
    * Creates a new TrackController with default values.
    */
   TrackController();
 
   /**
    * Creates a new TrackController with modification of the default exchangeMessage timeout
    */
   TrackController(uint64_t timeOut);
 
   /**
    * Creates a new TrackController with the given hash and debugging
    * flag and changing the default exchangeMessage timeout.
    * A zero hash will result in a unique hash begin generated.
    */
   TrackController(uint16_t hash, bool debug, uint64_t timeOut);
 
   /**
    * Creates a new TrackController with the given hash and debugging
    * flag. A zero hash will result in a unique hash begin generated.
    */
   TrackController(uint16_t hash, bool debug, uint64_t timeOut, bool loopback);
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
   void init(uint16_t hash, bool debug, bool loopback, uint64_t timeOut = 1000);
 
   /**
    * Queries the hash used by the TrackController.
    */
   uint16_t getHash();
 
   /**
    * Reflects whether the TrackController is in debug mode,
    * where all messages are dumped to the Serial console.
    */
   bool isDebug();
 
   /**
    * Reflects whether the TrackController is in debug mode,
    * where all messages are reflected by the CAN controller.
    */
   bool isLoopback();
 
   /**
    * Initializes the CAN hardware and starts receiving CAN
    * messages. CAN messages are put into an internal buffer of
    * limited size, so they don't get lost, but you have to take
    * care of them in time. Otherwise the buffer might overflow.
    * default CAN Rx pin is GPIO_NUM_5 and can_tx_pin is GPIO_NUM_4
    */
 
 
   // void begin(const gpio_num_t can_rx_pin = GPIO_NUM_5,
   //            const gpio_num_t can_tx_pin = GPIO_NUM_4);
 
   void begin(const byte can_rx_pin = 5,
              const byte can_tx_pin = 4);
 
   /**
    * Generates a new hash and makes sure it does not conflict
    * with those of other devices in the setup.
    */
   void generateHash();
 
   /**
    * Stops receiving messages from the CAN hardware. Clears
    * the internal buffer.
    */
   // void end();
 
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
   bool exchangeMessage(TrackMessage &out, TrackMessage &in, uint16_t timeout);
 
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
   bool systemHalt(const uint16_t address);
 
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
   bool emergency(const uint16_t address);
 
   /**
    * Sets the direction of the given locomotive. Valid directions
    * are those specified by the DIR_* constants. The return value
    * reflects whether the call was successful.
    */
   bool setLocoDirection(const uint16_t address, uint8_t direction);
 
   /**
    * Toggles the direction of the given locomotive. This normally
    * includes a full stop.
    */
   bool toggleLocoDirection(const uint16_t address);
 
   /**
    * Sets the speed of the given locomotive. Valid speeds are
    * 0 to 1023 (inclusive), though the connector box will limit
    * all speeds beyond 1000 to 1000. The return value reflects
    * whether the call was successful.
    */
   bool setLocoSpeed(const uint16_t address, uint16_t speed);
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
   bool setLocoFunction(const uint16_t address, uint8_t function, uint8_t power);
 
   /**
    * Toggles the given function of the given locomotive. Valid
    * functions are 0 to 31, with 0 normally denoting the
    * head/backlight.
    */
   bool toggleLocoFunction(const uint16_t address, uint8_t function);
 
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
    * whether the call was successful.
    */
   bool setAccessory(const uint16_t address, uint8_t position, uint8_t power, uint16_t time);
 
   /**
    * Switches a turnout. This is actually a convenience function
    * around setAccessory() that uses default values for some
    * parameters. The return value reflects whether the call was
    * successful.
    */
   bool setTurnout(const uint16_t address, bool straight);
 
   /**
    * Queries the direction of the given locomotive and writes it
    * into the referenced byte. The return value indicates whether
    * the call was successful and the direction is valid.
    */
   bool getLocoDirection(const uint16_t address, uint8_t *direction);
 
   /**
    * Queries the speed of the given locomotive and writes it
    * into the referenced byte. The return value indicates whether
    * the call was successful and the speed is valid.
    */
 
   bool getLocoSpeed(const uint16_t address, uint16_t *speed);
 
   /**
    * Queries the given function of the given locomotive and writes
    * it into the referenced byte. The return value indicates
    * whether the call was successful and the power is valid. Note
    * that the call will not reflect the original power value sent
    * to the function, but only 0 ("off") or 1 ("on").
    */
   bool getLocoFunction(const uint16_t address, uint8_t function, uint8_t *power);
 
   /**
    * Queries the given magnetic accessory's state and and writes
    * it into the referenced bytes. The return value indicates
    * whether the call was successful and the bytes are valid. Note
    * that the call will not reflect the original power value sent
    * to the function, but only 0 ("off") or 1 ("on").
    */
   bool getAccessory(const uint16_t address, uint8_t *position, uint8_t *power);
 
   /**
    * Writes the given value to the given config number of the given
    * locomotive.  The return value reflects whether the call was
    * successful.
    */
   /* -------------------------------------------------------------------
      TrackController::writeConfig
 
      See https://streaming.maerklin.de/public-media/cs2/cs2CAN-Protokoll-2_0.pdf -> 3.8 Commande : Écrire Config
 
   -------------------------------------------------------------------  */
   bool writeConfig(const uint16_t address, uint16_t number, uint8_t value);
 
   /**
    * Reads the given config number of the given locomotive into the
    * given value.
    */
 
   //!\\ Les décodeurs MFX de première génération ne sont pas conçus pour cela et pourraient être endommagés
   //!\\ First-generation MFX decoders are not designed for this purpose and could be damaged.
 
   // See https://streaming.maerklin.de/public-media/cs2/cs2CAN-Protokoll-2_0.pdf -> 2.8 Befehl: Fast Read für mfx SID - Adresse
 
   bool readConfig(const uint16_t address, uint16_t number, uint8_t *value);
 
   /**
    * Queries the software version of the track format processor.
    */
   bool getVersion();
   //bool getVersion(uint8_t *high, uint8_t *low);
 
   /**
    * Processes commands received on the serial or TCP port
    */
   void handleUserCommands(String);
 };
 
 #endif // TRACKCONTROLLER_H
 