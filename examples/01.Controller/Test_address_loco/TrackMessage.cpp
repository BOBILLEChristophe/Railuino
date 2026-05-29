
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
 
 uint8_t TrackMessage::parseHex(String &s, uint8_t start, uint8_t end, bool *ok)
 {
     uint8_t value = 0;
 
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
             return -1;
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