
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

public:
    /**
     * Creates a new TrackController with default values. This should
     * be fine for most use cases. Further configuration can be done
     * by using the init() method.
     */
    TrackController();
    /**
     * Creates a new TrackController with the given hash and debugging
     * flag. A zero hash will result in a unique hash begin generated.
     */
    TrackController(uint16_t hash, bool debug);
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
    void init(uint16_t hash, bool debug, bool loopback);

    /**
     * Queries the hash used by the TrackController.
     */
    // uint16_t getHash();

    /**
     * Reflects whether the TrackController is in debug mode,
     * where all messages are dumped to the Serial console.
     */
    // bool isDebug();

    /**
     * Reflects whether the TrackController is in debug mode,
     * where all messages are reflected by the CAN controller.
     */
    // bool isLoopback();

    /**
     * Initializes the CAN hardware and starts receiving CAN
     * messages. CAN messages are put into an internal buffer of
     * limited size, so they don't get lost, but you have to take
     * care of them in time. Otherwise the buffer might overflow.
     */
    void begin();

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
     * Sets the direction of the given locomotive. Valid directions
     * are those specified by the DIR_* constants. The return value
     * reflects whether the call was successful.
     */
    bool setLocoDirection(uint16_t address, uint8_t direction);

    /**
     * Toggles the direction of the given locomotive. This normally
     * includes a full stop.
     */
    bool toggleLocoDirection(uint16_t address);

    // bool setLocoSpeed(uint16_t address, uint16_t speed);
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
    bool setLocoFunction(uint16_t address, uint8_t function, uint8_t power);

    // bool toggleLocoFunction(uint16_t address, uint8_t function);
    // bool setAccessory(uint16_t address, uint8_t position, uint8_t power, uint16_t time);
    // bool setTurnout(uint16_t address, bool straight);

    /**
     * Queries the direction of the given locomotive and writes it
     * into the referenced byte. The return value indicates whether
     * the call was successful and the direction is valid.
     */
    bool getLocoDirection(uint16_t address, uint8_t *direction);

    // bool getLocoSpeed(uint16_t address, uint16_t *speed);
    // bool getLocoFunction(uint16_t address, uint8_t function, uint8_t *power);
    // bool getAccessory(uint16_t address, uint8_t *position, uint8_t *power);
    // bool writeConfig(uint16_t address, uint16_t number, uint8_t value);

    /**
     * Reads the given config number of the given locomotive into the
     * given value.
     */
    bool readConfig(uint16_t address, uint16_t number, uint8_t *value);

    // bool getVersion(uint8_t *high, uint8_t *low);
};

#endif // TRACKCONTROLLER_H