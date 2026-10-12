
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

#ifndef RAILUINO_ESP32_TWAI_DRIVER_H
#define RAILUINO_ESP32_TWAI_DRIVER_H

#if defined(ARDUINO_ARCH_ESP32)

#include "CanDriver.h"
#include "Config.h"

/**
 * CAN driver for the controller built into the ESP32 (TWAI), through
 * the ACAN_ESP32 library. It needs a CAN transceiver (SN65HVD230...).
 * The ESP32 has one TWAI controller: create only one Esp32TwaiDriver.
 *
 *   Esp32TwaiDriver can(22, 23);        // Rx, Tx
 *   TrackController ctrl(can, 0xDF24);
 */
class Esp32TwaiDriver : public CanDriver
{
public:
  explicit Esp32TwaiDriver(uint8_t rxPin = CAN_RX_PIN, uint8_t txPin = CAN_TX_PIN);

  bool begin(Print *log) override;
  bool send(const CanFrame &frame) override;
  bool receive(CanFrame &frame) override;
  void setPins(uint8_t rxPin, uint8_t txPin) override;

private:
  uint8_t mRxPin;
  uint8_t mTxPin;
};

#endif // ARDUINO_ARCH_ESP32
#endif // RAILUINO_ESP32_TWAI_DRIVER_H
