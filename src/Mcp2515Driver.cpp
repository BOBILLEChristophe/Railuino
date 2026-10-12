
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

#include "Mcp2515Driver.h"

#if defined(RAILUINO_HAS_MCP2515)

Mcp2515Driver *Mcp2515Driver::sInstance = nullptr;

Mcp2515Driver::Mcp2515Driver(uint8_t csPin, uint8_t intPin, uint32_t quartz)
    : mCan(csPin, SPI, intPin), mQuartz(quartz), mCsPin(csPin), mIntPin(intPin)
{
}

void Mcp2515Driver::isr()
{
  if (sInstance != nullptr)
    sInstance->mCan.isr();
}

bool Mcp2515Driver::begin(Print *log)
{
  sInstance = this;
  SPI.begin();
  ACAN2515Settings settings(mQuartz, RAILUINO_CAN_BIT_RATE);
  const uint16_t errorCode = mCan.begin(settings, isr);

  if (errorCode != 0)
  {
    if (log != nullptr)
    {
      log->print(F("!!! MCP2515 configuration error 0x"));
      log->println(errorCode, HEX);
    }
    return false;
  }

  if (log != nullptr)
  {
    log->print(F("MCP2515, CS "));
    log->print(mCsPin);
    log->print(F(", INT "));
    log->print(mIntPin);
    log->print(F(", quartz "));
    log->print(mQuartz);
    log->println(F(" Hz"));
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

bool Mcp2515Driver::send(const CanFrame &frame)
{
  CANMessage m;
  m.id = frame.id;
  m.ext = frame.ext;
  m.len = frame.len;
  for (uint8_t i = 0; i < frame.len && i < 8; i++)
    m.data[i] = frame.data[i];
  return mCan.tryToSend(m);
}

bool Mcp2515Driver::receive(CanFrame &frame)
{
  CANMessage m;
  if (!mCan.receive(m))
    return false;
  frame.id = m.id;
  frame.ext = m.ext;
  frame.len = m.len > 8 ? 8 : m.len;
  for (uint8_t i = 0; i < frame.len; i++)
    frame.data[i] = m.data[i];
  return true;
}

#endif // RAILUINO_HAS_MCP2515
