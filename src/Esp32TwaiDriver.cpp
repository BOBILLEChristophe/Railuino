
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

#if defined(ARDUINO_ARCH_ESP32)

#include "Esp32TwaiDriver.h"
#include <ACAN_ESP32.h> // https://github.com/pierremolinaro/acan-esp32

Esp32TwaiDriver::Esp32TwaiDriver(uint8_t rxPin, uint8_t txPin)
    : mRxPin(rxPin), mTxPin(txPin)
{
}

void Esp32TwaiDriver::setPins(uint8_t rxPin, uint8_t txPin)
{
  mRxPin = rxPin;
  mTxPin = txPin;
}

bool Esp32TwaiDriver::begin(Print *log)
{
  ACAN_ESP32_Settings settings(RAILUINO_CAN_BIT_RATE);
  settings.mRxPin = static_cast<gpio_num_t>(mRxPin);
  settings.mTxPin = static_cast<gpio_num_t>(mTxPin);
  const uint32_t errorCode = ACAN_ESP32::can.begin(settings);

  if (errorCode != 0)
  {
    if (log != nullptr)
    {
      log->print(F("!!! ESP32 CAN configuration error 0x"));
      log->println(errorCode, HEX);
    }
    return false;
  }

  if (log != nullptr)
  {
    log->print(F("ESP32 CAN, Rx "));
    log->print(mRxPin);
    log->print(F(", Tx "));
    log->println(mTxPin);
    log->print(F("Bit Rate prescaler: "));
    log->println(settings.mBitRatePrescaler);
    log->print(F("Triple Sampling: "));
    log->println(settings.mTripleSampling ? F("yes") : F("no"));
    log->print(F("Actual bit rate: "));
    log->print(settings.actualBitRate());
    log->println(F(" bit/s"));
    log->print(F("Exact bit rate ? "));
    log->println(settings.exactBitRate() ? F("yes") : F("no"));
    log->print(F("Sample point: "));
    log->print(settings.samplePointFromBitStart());
    log->println(F("%"));
  }
  return true;
}

bool Esp32TwaiDriver::send(const CanFrame &frame)
{
  CANMessage m;
  m.id = frame.id;
  m.ext = frame.ext;
  m.len = frame.len;
  for (uint8_t i = 0; i < frame.len && i < 8; i++)
    m.data[i] = frame.data[i];
  return ACAN_ESP32::can.tryToSend(m);
}

bool Esp32TwaiDriver::receive(CanFrame &frame)
{
  CANMessage m;
  if (!ACAN_ESP32::can.receive(m))
    return false;
  frame.id = m.id;
  frame.ext = m.ext;
  frame.len = m.len > 8 ? 8 : m.len;
  for (uint8_t i = 0; i < frame.len; i++)
    frame.data[i] = m.data[i];
  return true;
}

#endif // ARDUINO_ARCH_ESP32
