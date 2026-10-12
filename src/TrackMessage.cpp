
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

 #include "TrackMessage.h"

 /* -------------------------------------------------------------------
    TrackMessage::printHex
 -------------------------------------------------------------------  */
 
 size_t TrackMessage::printHex(Print &p, uint32_t hex, uint16_t digits)
 {
     size_t size = 0;
     String s = String(hex, HEX);
     for (uint16_t i = s.length(); i < digits; i++)
         size += p.print("0");
     size += p.print(s);
     return size;
 }
 
 /* -------------------------------------------------------------------
    TrackMessage::parseHex
 -------------------------------------------------------------------  */
 
 // Returns uint16_t: the hash has 4 hexadecimal digits. With uint8_t
 // (introduced in 0.91, it was int in 0.9) parseFrom() truncated the hash
 // to 8 bits, and the error value -1 became 255.
 uint16_t TrackMessage::parseHex(const String &s, uint8_t start, uint8_t end, bool *ok)
 {
     uint16_t value = 0;
 
     for (uint8_t i = start; i < end; i++)
     {
         char c = s.charAt(i);
         if (c >= '0' && c <= '9')
             value = 16 * value + c - '0';
         else if (c >= 'a' && c <= 'f')
             value = 16 * value + 10 + c - 'a';
         else if (c >= 'A' && c <= 'F')
             value = 16 * value + 10 + c - 'A';
         else
         {
             *ok = false;
             return 0;
         }
     }
     return value;
 }
 
 /* -------------------------------------------------------------------
    TrackMessage::clear
 -------------------------------------------------------------------  */
 
 void TrackMessage::clear()
 {
     prio = 0;
     command = 0;
     hash = 0;
     response = false;
     length = 0;
     memset(data, 0x00, 8);
 }
 
 /* -------------------------------------------------------------------
    TrackMessage::make / makeSystem
 -------------------------------------------------------------------  */

 TrackMessage TrackMessage::make(uint8_t command, uint32_t uid, uint8_t length)
 {
     TrackMessage m;
     m.clear();
     m.command = command;
     m.length = length > 8 ? 8 : length;
     m.setUid(uid);
     return m;
 }

 TrackMessage TrackMessage::makeSystem(uint8_t subCommand, uint32_t uid, uint8_t length)
 {
     TrackMessage m = make(Marklin::Cmd::System, uid, length < 5 ? 5 : length);
     m.data[4] = subCommand;
     return m;
 }

 /* -------------------------------------------------------------------
    TrackMessage::uid / setUid / canId / setCanId
 -------------------------------------------------------------------  */

 uint32_t TrackMessage::uid() const
 {
     return (static_cast<uint32_t>(data[0]) << 24) | (static_cast<uint32_t>(data[1]) << 16) |
            (static_cast<uint32_t>(data[2]) << 8) | data[3];
 }

 void TrackMessage::setUid(uint32_t uid)
 {
     data[0] = (uid >> 24) & 0xFF;
     data[1] = (uid >> 16) & 0xFF;
     data[2] = (uid >> 8) & 0xFF;
     data[3] = uid & 0xFF;
 }

 uint32_t TrackMessage::canId() const
 {
     return (static_cast<uint32_t>(prio & 0x0F) << 25) | (static_cast<uint32_t>(command) << 17) |
            (static_cast<uint32_t>(response ? 1 : 0) << 16) | hash;
 }

 void TrackMessage::setCanId(uint32_t id)
 {
     prio = (id >> 25) & 0x0F;
     command = (id >> 17) & 0xFF;
     response = (id >> 16) & 0x01;
     hash = id & 0xFFFF;
 }

 /* -------------------------------------------------------------------
    TrackMessage::printTo
 -------------------------------------------------------------------  */
 
 size_t TrackMessage::printTo(Print &p) const
 {
     size_t size = 0;
     size += printHex(p, hash, 4);
     size += p.print(response ? " R " : "   ");
     size += printHex(p, command, 2);
     size += p.print(" ");
     size += printHex(p, length, 1);
 
     for (int i = 0; i < length; i++)
     {
         size += p.print(" ");
         size += printHex(p, data[i], 2);
     }
     return size;
 }
 
 /* -------------------------------------------------------------------
    TrackMessage::parseFrom
 -------------------------------------------------------------------  */
 
 bool TrackMessage::parseFrom(String &s)
 {
     bool result = true;
     clear();
 
     if (s.length() < 11)
         return false;
 
     hash = parseHex(s, 0, 4, &result);
     response = s.charAt(5) != ' ';
     command = parseHex(s, 7, 9, &result);
     length = parseHex(s, 10, 11, &result);
 
     if (length > 8)
         return false;
 
     if (s.length() < static_cast<size_t>(11) + 3 * length)
         return false;
 
     for (int i = 0; i < length; i++)
         data[i] = parseHex(s, 12 + 3 * i, 12 + 3 * i + 2, &result);
 
     return result;
 }