
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

#include "CommandParser.h"
#include "TrackController.h"

#include <stdlib.h>

namespace
{
  const uint8_t MAX_WORDS = 6;

  // Case-insensitive comparison of a word with a command name.
  bool sameWord(const char *word, const char *name)
  {
    while (*word && *name)
    {
      char a = *word++;
      if (a >= 'A' && a <= 'Z')
        a = a - 'A' + 'a';
      if (a != *name++)
        return false;
    }
    return *word == 0 && *name == 0;
  }

  // Decimal number, or hexadecimal with 0x. No sign, no octal: "0300"
  // is 300. False if not a number, too large, or greater than 'max'.
  bool parseNumber(const char *word, uint32_t max, uint32_t &value)
  {
    if (word == nullptr || *word == 0)
      return false;
    uint32_t base = 10;
    if (word[0] == '0' && (word[1] == 'x' || word[1] == 'X'))
    {
      base = 16;
      word += 2;
      if (*word == 0)
        return false;
    }
    uint32_t v = 0;
    for (; *word != 0; word++)
    {
      const char c = *word;
      uint32_t digit;
      if (c >= '0' && c <= '9')
        digit = c - '0';
      else if (base == 16 && c >= 'a' && c <= 'f')
        digit = c - 'a' + 10;
      else if (base == 16 && c >= 'A' && c <= 'F')
        digit = c - 'A' + 10;
      else
        return false;
      if (v > (0xFFFFFFFFUL - digit) / base)
        return false; // more than 32 bits
      v = v * base + digit;
    }
    if (v > max)
      return false;
    value = v;
    return true;
  }

  void report(Print *out, bool ok, const __FlashStringHelper *error = nullptr)
  {
    if (out == nullptr)
      return;
    if (ok)
      out->println(F("ok"));
    else
    {
      out->print(F("error: "));
      out->println(error != nullptr ? error : F("no answer from the layout"));
    }
  }
}

CommandParser::CommandParser(TrackController &controller) : mCtrl(controller)
{
}

bool CommandParser::execute(const char *line, Print *out)
{
  if (line == nullptr)
    return false;

  // Copy the line and split it into words.
  char buffer[MAX_LINE + 1];
  uint8_t length = 0;
  while (line[length] != 0 && length < MAX_LINE)
  {
    buffer[length] = line[length];
    length++;
  }
  buffer[length] = 0;
  if (line[length] != 0)
  {
    report(out, false, F("line too long"));
    return false;
  }

  char *words[MAX_WORDS] = {nullptr};
  uint8_t count = 0;
  char *p = buffer;
  while (*p != 0)
  {
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
      *p++ = 0;
    if (*p == 0)
      break;
    if (count == MAX_WORDS)
    {
      report(out, false, F("too many parameters"));
      return false;
    }
    words[count++] = p;
    while (*p != 0 && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n')
      p++;
  }

  if (count == 0)
    return false; // empty line: nothing to do, nothing to report

  const char *cmd = words[0];
  uint32_t a = 0, b = 0, c = 0, d = 0;
  bool ok;

  if (sameWord(cmd, "help"))
  {
    if (out != nullptr)
    {
      out->println(F("power 1|0"));
      out->println(F("speed ADDR SPEED(0..1000)"));
      out->println(F("direction ADDR 1|2|3 (forward, reverse, toggle)"));
      out->println(F("function ADDR F 1|0"));
      out->println(F("accessory ADDR POS POWER [MS]"));
      out->println(F("turnout ADDR 1|0 (straight, round)"));
      out->println(F("emergency ADDR"));
      out->println(F("halt"));
      out->println(F("version"));
      out->println(F("ADDR: Loc-ID, e.g. 16391 or 0x4007 for MFX loco 7"));
    }
    return true;
  }

  if (sameWord(cmd, "power"))
  {
    if (count != 2 || !parseNumber(words[1], 1, a))
    {
      report(out, false, F("usage: power 1|0"));
      return false;
    }
    ok = mCtrl.setPower(a != 0);
  }
  else if (sameWord(cmd, "speed"))
  {
    if (count != 3 || !parseNumber(words[1], 0xFFFFFFFFUL, a) || !parseNumber(words[2], 1000, b))
    {
      report(out, false, F("usage: speed ADDR SPEED(0..1000)"));
      return false;
    }
    ok = mCtrl.setLocoSpeed(a, b);
  }
  else if (sameWord(cmd, "direction"))
  {
    if (count != 3 || !parseNumber(words[1], 0xFFFFFFFFUL, a) || !parseNumber(words[2], 3, b))
    {
      report(out, false, F("usage: direction ADDR 1|2|3"));
      return false;
    }
    ok = mCtrl.setLocoDirection(a, static_cast<uint8_t>(b));
  }
  else if (sameWord(cmd, "function"))
  {
    if (count != 4 || !parseNumber(words[1], 0xFFFFFFFFUL, a) || !parseNumber(words[2], 31, b) ||
        !parseNumber(words[3], 31, c))
    {
      report(out, false, F("usage: function ADDR F 1|0"));
      return false;
    }
    ok = mCtrl.setLocoFunction(a, b, c);
  }
  else if (sameWord(cmd, "accessory"))
  {
    if ((count != 4 && count != 5) || !parseNumber(words[1], 0xFFFFFFFFUL, a) ||
        !parseNumber(words[2], 255, b) || !parseNumber(words[3], 31, c) ||
        (count == 5 && !parseNumber(words[4], 0xFFFF, d)))
    {
      report(out, false, F("usage: accessory ADDR POS POWER [MS]"));
      return false;
    }
    ok = mCtrl.setAccessory(a, b, c, d);
  }
  else if (sameWord(cmd, "turnout"))
  {
    if (count != 3 || !parseNumber(words[1], 0xFFFFFFFFUL, a) || !parseNumber(words[2], 1, b))
    {
      report(out, false, F("usage: turnout ADDR 1|0"));
      return false;
    }
    ok = mCtrl.setTurnout(a, b != 0);
  }
  else if (sameWord(cmd, "emergency"))
  {
    if (count != 2 || !parseNumber(words[1], 0xFFFFFFFFUL, a))
    {
      report(out, false, F("usage: emergency ADDR"));
      return false;
    }
    ok = mCtrl.emergency(a);
  }
  else if (sameWord(cmd, "halt"))
  {
    ok = mCtrl.systemHalt(0);
  }
  else if (sameWord(cmd, "version"))
  {
    uint8_t high = 0, low = 0;
    ok = mCtrl.getVersion(&high, &low);
    if (ok && out != nullptr)
    {
      out->print(F("Gleisbox "));
      out->print(high);
      out->print('.');
      out->println(low);
    }
  }
  else
  {
    report(out, false, F("unknown command (type help)"));
    return false;
  }

  report(out, ok);
  return ok;
}
