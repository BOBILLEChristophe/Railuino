
/*********************************************************************
 * Railuino - Hacking your Märklin
 *
 * Copyright (C) 2012 Joerg Pleumann
 * Copyright (C) 2024 christophe bobille
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

#ifndef RAILUINO_CAN_DRIVER_H
#define RAILUINO_CAN_DRIVER_H

#include <Arduino.h>

/**
 * A raw CAN frame, independent of the CAN library used underneath.
 * Märklin uses extended (29 bit) identifiers at 250 kbit/s.
 */
struct CanFrame
{
  uint32_t id = 0;
  bool ext = true;
  uint8_t len = 0;
  uint8_t data[8] = {0};
};

/**
 * Interface between TrackController and a CAN controller.
 *
 * Implementations in this library:
 * - Esp32TwaiDriver: the CAN (TWAI) controller built into the ESP32,
 *   through the ACAN_ESP32 library.
 * - Mcp2515Driver: an MCP2515 on the SPI bus (Uno, Mega, and ESP32 too),
 *   through the ACAN2515 library.
 * A test double (MockCanDriver) is used by the unit tests in test/.
 *
 * To support another controller, derive from CanDriver and pass the
 * object to the TrackController constructor:
 *   MyDriver can;
 *   TrackController ctrl(can, 0xDF24);
 */
class CanDriver
{
public:
  /**
   * Starts the controller at 250 kbit/s. Configuration details are
   * printed to 'log' when it is not nullptr. Returns false on error.
   */
  virtual bool begin(Print *log) = 0;

  /**
   * Queues a frame for sending. Returns false if it could not be queued.
   */
  virtual bool send(const CanFrame &frame) = 0;

  /**
   * Gets a received frame, if any. Does not block.
   */
  virtual bool receive(CanFrame &frame) = 0;

  /**
   * Changes the Rx/Tx pins, for controllers that have such pins
   * (ESP32 TWAI). Must be called before begin(). Default: does nothing.
   */
  virtual void setPins(uint8_t rxPin, uint8_t txPin)
  {
    (void)rxPin;
    (void)txPin;
  }

protected:
  // Not virtual on purpose: drivers are never deleted through a
  // CanDriver pointer, and a virtual destructor would require
  // operator delete, which some AVR cores do not provide.
  ~CanDriver() = default;
};

/**
 * Märklin CAN bit rate.
 */
static const uint32_t RAILUINO_CAN_BIT_RATE = 250000UL;

#endif // RAILUINO_CAN_DRIVER_H
