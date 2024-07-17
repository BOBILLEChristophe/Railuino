
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

#ifndef TRACKCMESSAGE_H
#define TRACKCMESSAGE_H

#include <Arduino.h>

/**
 * Represents a message going through the Marklin CAN bus. More or
 * less a beautified version of the real CAN message. You normally
 * don't need to use this unless you want to experiment with the
 * protocol or extend the library. See the Marklin protocol
 * documentation for details. The TrackMessage is a Printable, so
 * it can be directly used in Serial.println(), for instance. It
 * can also be converted from a String.
 */
class TrackMessage
{
private:
public:
    /**
     * The priority number.
     */
    uint8_t prio;
    /**
     * The command number.
     */
    uint8_t command;
    /**
     * Whether this is a response to a request.
     */
    bool response;
    /**
     * The hash that is used for avoiding device/message collisions.
     */
    uint16_t hash;
    /**
     * The number of data bytes in the payload.
     */
    uint8_t length;
    /**
     * The actual message data bytes.
     */
    uint8_t data[8];

    /**
     * Clears the message, setting all values to zero. Provides for
     * easy recycling of TrackMessage objects.
     */
    void clear();
    /**
     * Prints the message to the given Print object, which could be a
     * Serial object, for instance. The message format looks like this
     *
     * HHHH R CC L DD DD DD DD DD DD DD DD
     *
     * with all numbers being hexadecimals and the data bytes being
     * optional beyond what the message length specifies. Exactly one
     * whitespace is inserted between different fields as a separator.
     */
    size_t printTo(Print &p) const;
    /**
     * Parses the message from the given String. Returns true on
     * success, false otherwise. The message must have exactly the
     * format that printTo creates. This includes each and every
     * whitespace. If the parsing fails the state of the object is
     * undefined afterwards, and a clear() is recommended.
     */
    bool parseFrom(String &s);

    static size_t printHex(Print &p, uint32_t hex, uint16_t digits);
    static uint8_t parseHex(String &s, uint8_t start, uint8_t end, bool *ok);
};

#endif // TRACKCMESSAGE_H