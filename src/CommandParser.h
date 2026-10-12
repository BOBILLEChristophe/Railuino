
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

#ifndef RAILUINO_COMMAND_PARSER_H
#define RAILUINO_COMMAND_PARSER_H

#include <Arduino.h>

class TrackController;

/**
 * Executes text commands, typically received on the serial port or a
 * TCP connection, on a TrackController.
 *
 * Words are separated by spaces; numbers are decimal or hexadecimal
 * (0x...). Addresses are Loc-IDs: 16391 or 0x4007 for MFX loco 7.
 *
 *   power 1|0                      track power on / off
 *   speed ADDR SPEED               speed 0..1000
 *   direction ADDR DIR             1 forward, 2 reverse, 3 toggle
 *   function ADDR F 1|0            function F (0 = lights) on / off
 *   accessory ADDR POS POWER [MS]  switch an accessory, off after MS ms
 *   turnout ADDR 1|0               turnout straight (1) / round (0)
 *   emergency ADDR                 emergency stop of one locomotive
 *   halt                           all locomotives stop
 *   version                        Gleisbox software version
 *   help                           list of commands
 *
 * The result ("ok" or "error: ...") is printed to 'out' if not nullptr.
 *
 *   CommandParser parser(ctrl);
 *   parser.execute("speed 0x4007 300", &Serial);
 */
class CommandParser
{
public:
  /**
   * Longest accepted line, in characters.
   */
  static const uint8_t MAX_LINE = 64;

  explicit CommandParser(TrackController &controller);

  /**
   * Executes one command line. Returns true if the command is known,
   * its parameters valid and the layout acknowledged it.
   */
  bool execute(const char *line, Print *out = nullptr);

private:
  TrackController &mCtrl;
};

#endif // RAILUINO_COMMAND_PARSER_H
