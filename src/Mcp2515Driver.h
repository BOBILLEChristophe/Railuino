
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

#ifndef RAILUINO_MCP2515_DRIVER_H
#define RAILUINO_MCP2515_DRIVER_H

// Available on AVR (always) and on any board where the ACAN2515 library
// is installed, ESP32 included. With the Arduino IDE on ESP32, add
// #include <ACAN2515.h> to the sketch so that the IDE finds the library.
#if defined(ARDUINO_ARCH_AVR) || (defined(__has_include) && __has_include(<ACAN2515.h>))
#define RAILUINO_HAS_MCP2515 1

#include "CanDriver.h"
#include <ACAN2515.h> // https://github.com/pierremolinaro/acan2515

// Defaults of the MCP2515 wiring and quartz, used by Mcp2515Driver() and
// by TrackController on AVR when no driver is given. Override them
// without touching the library, for instance in platformio.ini:
//   build_flags = -D RAILUINO_MCP2515_QUARTZ=8000000UL -D RAILUINO_MCP2515_CS=53
// Many cheap MCP2515 modules carry an 8 MHz quartz: with the wrong value
// the bit rate is wrong and nothing is received.
#ifndef RAILUINO_MCP2515_QUARTZ
#define RAILUINO_MCP2515_QUARTZ 16000000UL
#endif
#ifndef RAILUINO_MCP2515_CS
#define RAILUINO_MCP2515_CS 10
#endif
#ifndef RAILUINO_MCP2515_INT
#define RAILUINO_MCP2515_INT 2
#endif

/**
 * CAN driver for an MCP2515 on the SPI bus, through the ACAN2515 library.
 * Works on AVR (Uno, Mega) and on ESP32. The INT pin must support
 * interrupts. One MCP2515 per sketch.
 *
 *   Mcp2515Driver can(5, 4, 8000000UL);  // CS, INT, quartz (ESP32 + 8 MHz module)
 *   TrackController ctrl(can, 0xDF24);
 */
class Mcp2515Driver : public CanDriver
{
public:
  explicit Mcp2515Driver(uint8_t csPin = RAILUINO_MCP2515_CS,
                         uint8_t intPin = RAILUINO_MCP2515_INT,
                         uint32_t quartz = RAILUINO_MCP2515_QUARTZ);

  bool begin(Print *log) override;
  bool send(const CanFrame &frame) override;
  bool receive(CanFrame &frame) override;

private:
  ACAN2515 mCan;
  uint32_t mQuartz;
  uint8_t mCsPin;
  uint8_t mIntPin;

  // The MCP2515 interrupt routine has no parameter: it reaches the
  // driver through this pointer, set by begin().
  static Mcp2515Driver *sInstance;
  static void isr();
};

#endif // MCP2515 available
#endif // RAILUINO_MCP2515_DRIVER_H
