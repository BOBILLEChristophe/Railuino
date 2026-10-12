
/*********************************************************************
 * Railuino - Hacking your Märklin
 *
 * Copyright (C) 2012 Joerg Pleumann
 * Copyright (C) 2024 – 2026 christophe bobille
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

 #ifndef TRACKCMESSAGE_H
 #define TRACKCMESSAGE_H
 
 #include <Arduino.h>
 #include <Printable.h>
 #include "MarklinProtocol.h"
 
 /**
  * Represents a message going through the Marklin CAN bus. More or
  * less a beautified version of the real CAN message. You normally
  * don't need to use this unless you want to experiment with the
  * protocol or extend the library. See the Marklin protocol
  * documentation for details. The TrackMessage is a Printable, so
  * it can be directly used in Serial.println(), for instance. It
  * can also be converted from a String.
  */
 class TrackMessage : public Printable
 {
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
      * Builds a message: command, UID (data[0..3]) and length; the other
      * fields are zero. For instance a speed command for MFX loco 7:
      *   TrackMessage m = TrackMessage::make(Marklin::Cmd::Speed, 0x4007, 6);
      *   m.data[4] = highByte(speed);
      *   m.data[5] = lowByte(speed);
      */
     static TrackMessage make(uint8_t command, uint32_t uid, uint8_t length);

     /**
      * Same, for a system command with its sub-command in data[4]
      * (length 5 at least).
      */
     static TrackMessage makeSystem(uint8_t subCommand, uint32_t uid = 0, uint8_t length = 5);

     /**
      * The 32 bit UID (or Loc-ID) in data[0..3], big endian.
      */
     uint32_t uid() const;
     void setUid(uint32_t uid);

     /**
      * The 29 bit CAN identifier: prio, command, response bit and hash.
      */
     uint32_t canId() const;

     /**
      * Fills prio, command, response and hash from a CAN identifier.
      */
     void setCanId(uint32_t id);
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
     size_t printTo(Print &p) const override;
     /**
      * Parses the message from the given String. Returns true on
      * success, false otherwise. The message must have exactly the
      * format that printTo creates. This includes each and every
      * whitespace. If the parsing fails the state of the object is
      * undefined afterwards, and a clear() is recommended.
      */
     bool parseFrom(String &s);
 
     static size_t printHex(Print &p, uint32_t hex, uint16_t digits);
     /**
      * Parses the hexadecimal digits s[start..end[ (at most 4 digits).
      * On error, sets *ok to false and returns 0; *ok is never set to
      * true, so one flag can collect the errors of several calls.
      */
     static uint16_t parseHex(const String &s, uint8_t start, uint8_t end, bool *ok);
 };
 
 #endif // TRACKCMESSAGE_H